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
 *========================================================*/

/* ESP32 stacks must be 16-byte aligned */
#define VRT_STACK_ALIGNMENT 16U

/* Align an address downwards */
#define VRT_ALIGN_DOWN(addr, alignment) \
    (((addr)) & ~((alignment) - 1U))

    /*=========================================================
     * Stack Initialization
     *========================================================*/

    /**
     * @brief Initialize a task's stack.
     *
     * Creates the initial CPU context for a new task.
     *
     * @param stackTop Pointer to the top of the task stack.
     * @param entry Task entry function.
     * @param argument Task argument.
     *
     * @return Pointer to the initialized stack pointer.
     */
    uint32_t *vrt_port_stack_init(
        uint32_t *stackTop,
        vrt_task_function_t entry,
        void *argument);

#ifdef __cplusplus
}
#endif

#endif /* VRT_PORT_H */