#include "vrt_port.h"
#include "vrt_port_frame.h"

#include <stdint.h>
#include <stddef.h>

/*
 * ============================================================================
 * Low-level context-switch handoff state
 * ============================================================================
 *
 * These variables are consumed by vrt_port_yield_switch() in
 * vrt_port_asm.S.
 *
 * vrt_port_current_sp:
 *     Address of the current task's saved SP field.
 *
 * vrt_port_next_sp:
 *     Saved SP of the task that should be restored.
 */

volatile uint32_t **vrt_port_current_sp = NULL;
volatile uint32_t *vrt_port_next_sp = NULL;

/*
 * ============================================================================
 * ESP32 FreeRTOS stack initializer
 * ============================================================================
 *
 * Use the ESP32 FreeRTOS implementation already provided by libfreertos.a.
 *
 * This gives VertexRT the exact Xtensa initial task-frame ABI used by the
 * framework.
 */

extern uint32_t *pxPortInitialiseStack(
    uint32_t *topOfStack,
    void (*taskFunction)(void *),
    void *parameter);

/*
 * ============================================================================
 * Internal assembly entry points
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

    /*
     * vrt_task_init() passes the LAST valid word of the stack.
     *
     * pxPortInitialiseStack() expects exactly the same convention.
     */
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
 * Called by:
 *
 *     vrt_task_yield()
 *
 * with:
 *
 *     saveSpOut = &currentTask->sp
 *     restoreSp = nextTask->sp
 *
 * The assembly routine performs the actual Xtensa context transition.
 *
 * It returns only when the current task is scheduled again.
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
     * Publish the current task SP destination.
     *
     * The assembly routine will write the newly-created XtSolFrame SP
     * through this pointer.
     */
    vrt_port_current_sp =
        (volatile uint32_t **)saveSpOut;

    /*
     * Publish the next task's saved SP.
     */
    vrt_port_next_sp =
        (volatile uint32_t *)restoreSp;

    /*
     * Enter the low-level Xtensa switch.
     *
     * This function returns only after this task is restored later.
     */
    vrt_port_yield_switch();
}

/*
 * ============================================================================
 * Restore context without saving the current context
 * ============================================================================
 *
 * Used when a task terminates.
 */

void vrt_port_restore_context(uint32_t *sp)
{
    if (sp == NULL)
    {
        for (;;)
        {
        }
    }

    /*
     * The assembly backend performs the actual restoration and does not
     * return.
     */
    vrt_port_restore_context_asm(sp);

    /*
     * Defensive fallback.
     */
    for (;;)
    {
    }
}