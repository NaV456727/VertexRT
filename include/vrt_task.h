#ifndef VRT_TASK_H
#define VRT_TASK_H

#include "vrt_types.h"
#include "vrt_list.h"
#include "vrt_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        VRT_TASK_READY = 0,
        VRT_TASK_RUNNING,
        VRT_TASK_BLOCKED,
        VRT_TASK_SUSPENDED

    } vrt_task_state_t;

    typedef void (*vrt_task_function_t)(void *argument);

    typedef struct vrt_task
    {
        /* Used by scheduler */
        vrt_list_node_t node;

        /* Task Identity */
        uint32_t id;
        char name[VRT_TASK_NAME_LENGTH];

        /* Task Entry Function */
        vrt_task_function_t entry;

        /* User Argument */
        void *argument;

        /* Priority */
        uint8_t priority;

        /* Current State */
        vrt_task_state_t state;

        /* Stack Information */
        uint32_t *sp;
        uint32_t *stackStart;
        uint32_t *stackEnd;
        uint32_t stackSize;

    } vrt_task_t;

    void vrt_task_init(
        vrt_task_t *task,
        vrt_task_function_t function,
        void *argument,
        uint8_t priority,
        uint32_t *stackStart,
        uint32_t stackSize,
        const char *name);

    /**
     * @brief Voluntarily yield the processor.
     *
     * The current task gives up the CPU so another
     * runnable task may execute.
     */
    void vrt_task_yield(void);

#ifdef __cplusplus
}
#endif

#endif