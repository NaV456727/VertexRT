#include "vrt_port.h"
#include "vrt_port_frame.h"
#include "esp_attr.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/*
 * ============================================================================
 * Context-switch handoff state
 * ============================================================================
 *
 * These variables are consumed by vrt_port_yield_switch() and the
 * ISR-preemption path in vrt_port_asm.S.
 *
 * vrt_port_current_sp:
 *
 *     Address of the current task's SP field.
 *
 * vrt_port_next_sp:
 *
 *     Saved SP of the next task to restore.
 *
 * Global storage is used because the low-level Xtensa context code performs
 * register-window operations and cannot rely on normal argument registers
 * remaining valid.
 */

volatile uint32_t **vrt_port_current_sp = NULL;
volatile uint32_t *vrt_port_next_sp = NULL;

/*
 * ============================================================================
 * ESP32 FreeRTOS initial stack builder
 * ============================================================================
 *
 * VertexRT deliberately reuses the ESP32 FreeRTOS port's existing
 * pxPortInitialiseStack() implementation.
 *
 * This gives VertexRT the same Xtensa initial task-frame ABI used by the
 * working ESP32 FreeRTOS implementation.
 */

extern uint32_t *pxPortInitialiseStack(
    uint32_t *topOfStack,
    void (*taskFunction)(void *),
    void *parameter);

/*
 * ============================================================================
 * Assembly entry points
 * ============================================================================
 */

extern void vrt_port_yield_switch(void);

extern void vrt_port_restore_context_asm(
    uint32_t *sp);

/*
 * ============================================================================
 * Initial task stack
 * ============================================================================
 */

uint32_t *vrt_port_stack_init(
    uint32_t *stackTop,
    vrt_task_function_t entry,
    void *argument)
{
    if (stackTop == NULL ||
        entry == NULL)
    {
        return NULL;
    }

    return pxPortInitialiseStack(
        stackTop,
        (void (*)(void *))entry,
        argument);
}

/*
 * ============================================================================
 * Voluntary context switch
 * ============================================================================
 *
 * The scheduler has already selected the next task.
 *
 * This function only publishes:
 *
 *     &currentTask->sp
 *     nextTask->sp
 *
 * and enters the low-level Xtensa switch routine.
 */

void vrt_port_switch_context(
    uint32_t **saveSpOut,
    uint32_t *restoreSp)
{
    if (saveSpOut == NULL ||
        restoreSp == NULL)
    {
        return;
    }

    /*
     * Address where the newly-created current-task frame SP will be stored.
     */
    vrt_port_current_sp =
        (volatile uint32_t **)saveSpOut;

    /*
     * SP of the selected next task.
     */
    vrt_port_next_sp =
        (volatile uint32_t *)restoreSp;

    /*
     * Perform the actual architecture-level context switch.
     *
     * This returns only when the original task is scheduled again.
     */
    vrt_port_yield_switch();
}

/*
 * ============================================================================
 * Prepare ISR-driven preemption
 * ============================================================================
 *
 * Called by the scheduler after it has selected the next task.
 *
 * currentSpOut:
 *
 *     &currentTask->sp
 *
 * nextSp:
 *
 *     nextTask->sp
 *
 * No CPU context is switched here.
 *
 * The low-level Xtensa ISR path consumes these values.
 */

void IRAM_ATTR vrt_port_prepare_preemption(
    uint32_t **currentSpOut,
    uint32_t *nextSp)
{
    if (currentSpOut == NULL ||
        nextSp == NULL)
    {
        return;
    }

    vrt_port_current_sp =
        (volatile uint32_t **)currentSpOut;

    vrt_port_next_sp =
        (volatile uint32_t *)nextSp;
}

/*
 * ============================================================================
 * Restore context without saving current context
 * ============================================================================
 *
 * Used when a task terminates.
 *
 * The current task must not be saved because it is already TERMINATED.
 *
 * The assembly backend is expected to perform a one-way transfer and never
 * return.
 */

void vrt_port_restore_context(uint32_t *sp)
{
    if (sp == NULL)
    {
        /*
         * This should never happen because VertexRT maintains an idle task.
         */
        for (;;)
        {
        }
    }

    /*
     * One-way architecture handoff.
     */
    vrt_port_restore_context_asm(sp);

    /*
     * Defensive fallback.
     *
     * The assembly backend is declared non-returning by design.
     */
    abort();
}