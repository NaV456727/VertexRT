#ifndef VRT_PORT_H
#define VRT_PORT_H

#include "vrt_types.h"
#include "vrt_task.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*=========================================================
 * Stack Configuration
 *=========================================================*/

/*
 * ESP32 Xtensa stack alignment.
 */
#define VRT_STACK_ALIGNMENT 16U

/*
 * Align an address downwards.
 */
#define VRT_ALIGN_DOWN(addr, alignment) \
    (((addr)) & ~((alignment) - 1U))

    /*=========================================================
     * Stack Initialization
     *=========================================================*/

    /**
     * @brief Initialize a task's initial CPU context.
     *
     * @param stackTop Pointer to the top of the task stack.
     * @param entry Task entry function.
     * @param argument Task argument.
     *
     * @return Initial saved stack pointer.
     */
    uint32_t *vrt_port_stack_init(
        uint32_t *stackTop,
        vrt_task_function_t entry,
        void *argument);

    /*=========================================================
     * Context Switching
     *=========================================================*/

    /**
     * @brief Start execution of the first task.
     *
     * This function does not return.
     */
    void vrt_port_start_first_task(void);

    /**
     * @brief Switch from the current task to the task
     * selected by the scheduler.
     */
    void vrt_port_switch_context(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_PORT_H */