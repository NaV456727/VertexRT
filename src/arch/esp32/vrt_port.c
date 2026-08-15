#include "vrt_port.h"
#include "vrt_port_frame.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/*
 * ============================================================================
 * Context-switch handoff state
 * ============================================================================
 *
 * These variables are consumed by vrt_port_yield_switch() in
 * vrt_port_asm.S.
 *
 * vrt_port_current_sp:
 *
 *     Address of the current task's SP field.
 *
 * vrt_port_next_sp:
 *
 *     Saved SP of the next task to restore.
 *
 * The values are kept in global storage because the low-level Xtensa
 * switching code performs register-window spilling and cannot rely on the
 * normal argument registers surviving that operation.
 */

volatile uint32_t **vrt_port_current_sp = NULL;
volatile uint32_t *vrt_port_next_sp = NULL;

/*
 * ============================================================================
 * ESP32 FreeRTOS initial stack builder
 * ============================================================================
 *
 * VertexRT deliberately uses the ESP32 Arduino/ESP-IDF FreeRTOS port's
 * existing pxPortInitialiseStack() implementation.
 *
 * This means the initial task frame uses the same ABI and frame construction
 * as the working FreeRTOS implementation already linked into libfreertos.a.
 *
 * In particular, we do not reproduce the Xtensa initial-frame construction
 * ourselves.
 */

extern uint32_t *pxPortInitialiseStack(
    uint32_t *topOfStack,
    void (*taskFunction)(void *),
    void *parameter);

/*
 * ============================================================================
 * Assembly entry points
 * ============================================================================
 *
 * vrt_port_yield_switch()
 *
 *     Low-level voluntary context-switch implementation.
 *
 *     It consumes:
 *
 *         vrt_port_current_sp
 *         vrt_port_next_sp
 *
 *     and performs the actual Xtensa register/stack transition.
 *
 *
 * vrt_port_restore_context_asm()
 *
 *     Low-level restore-only implementation used when a task terminates.
 *
 *     It receives the target task's saved SP in A10.
 */

extern void vrt_port_yield_switch(void);

extern void vrt_port_restore_context_asm(
    uint32_t *sp);

/*
 * ============================================================================
 * Initial task stack
 * ============================================================================
 *
 * Build the initial task frame using the ESP32 FreeRTOS implementation.
 *
 * stackTop is the LAST valid word of the task's stack array. This is the
 * convention expected by pxPortInitialiseStack(), and vrt_task_init()
 * deliberately passes the stack in this form.
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
 * Public VertexRT architecture interface:
 *
 *     vrt_port_switch_context(
 *         &currentTask->sp,
 *         nextTask->sp
 *     );
 *
 * The scheduler has already selected the next task.
 *
 * This function therefore does NOT select tasks and does NOT manipulate
 * scheduler state.
 *
 * It only publishes the stack handoff information and enters the low-level
 * Xtensa context-switch routine.
 *
 * The low-level routine returns only when the current task is scheduled again.
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
     * Publish where the current task's newly-created solicited frame SP
     * must be stored.
     */
    vrt_port_current_sp =
        (volatile uint32_t **)saveSpOut;

    /*
     * Publish the SP of the task that should be restored.
     */
    vrt_port_next_sp =
        (volatile uint32_t *)restoreSp;

    /*
     * Enter the low-level Xtensa switch.
     *
     * This is expected to return only when the task that called
     * vrt_port_switch_context() is eventually restored.
     */
    vrt_port_yield_switch();
}

/*
 * ============================================================================
 * Restore context without saving the current context
 * ============================================================================
 *
 * Used by vrt_task_exit().
 *
 * The current task is already marked TERMINATED and must never have its
 * context saved.
 *
 * The assembly backend is therefore expected to perform a one-way transfer
 * to the selected task and NEVER return.
 */

void vrt_port_restore_context(uint32_t *sp)
{
    if (sp == NULL)
    {
        /*
         * There is no valid task to restore.
         *
         * This should never happen because VertexRT keeps an idle task
         * available.
         */
        for (;;)
        {
        }
    }

    /*
     * One-way architecture handoff.
     *
     * The assembly backend must not return.
     */
    vrt_port_restore_context_asm(sp);

    /*
     * Defensive fallback.
     *
     * Reaching this point means the architecture backend unexpectedly
     * returned. Do not allow execution to continue in the terminated task's
     * C stack.
     */
    abort();
}