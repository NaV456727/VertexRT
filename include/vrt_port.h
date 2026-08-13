#ifndef VRT_PORT_H
#define VRT_PORT_H

#include "vrt_task.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Build the initial Xtensa task frame.
     *
     * Returns a pointer to the task's initial stack/context pointer.
     */
    uint32_t *vrt_port_stack_init(
        uint32_t *stackTop,
        vrt_task_function_t entry,
        void *argument);

    /*
     * Start the first task.
     *
     * Does not return.
     */
    void vrt_port_start_first_task(uint32_t *sp)
        __attribute__((noreturn));

    /*
     * Save the current task and restore the selected next task.
     *
     * saveSpOut = &currentTask->sp
     * restoreSp = nextTask->sp
     *
     * Returns only when the saved task is scheduled again.
     */
    void vrt_port_switch_context(
        uint32_t **saveSpOut,
        uint32_t *restoreSp);

    /*
     * Restore a task without saving the current context.
     *
     * Used when a task terminates.
     *
     * Does not return.
     */
    void vrt_port_restore_context(uint32_t *sp)
        __attribute__((noreturn));

    /*
     * Internal low-level Xtensa solicited context switch.
     *
     * Arguments are passed through the architecture-owned handoff
     * variables in vrt_port.c.
     *
     * Does not have a C argument list.
     */
    void vrt_port_yield_switch(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_PORT_H */