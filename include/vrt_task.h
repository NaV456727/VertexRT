#ifndef VRT_TASK_H
#define VRT_TASK_H

#include "vrt_types.h"
#include "vrt_list.h"
#include "vrt_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*=========================================================
     * Task State
     *=========================================================*/

    typedef enum
    {
        VRT_TASK_READY,
        VRT_TASK_RUNNING,
        VRT_TASK_BLOCKED,
        VRT_TASK_SUSPENDED,
        VRT_TASK_TERMINATED

    } vrt_task_state_t;

    /*=========================================================
     * Task Entry Function
     *=========================================================*/

    typedef void (*vrt_task_function_t)(void *argument);

    /*=========================================================
     * Task Control Block
     *=========================================================*/

    typedef struct vrt_task
    {
        /* Task identification */
        uint32_t id;

        /* Task name */
        char name[VRT_TASK_NAME_LENGTH];

        /* Task entry function */
        vrt_task_function_t entry;

        /* Argument passed to task */
        void *argument;

        /* Scheduling priority */
        uint8_t priority;

        /* Current task state */
        vrt_task_state_t state;

        /* Task stack */
        uint32_t *stackStart;
        uint32_t *stackEnd;
        uint32_t stackSize;

        /* Saved stack pointer */
        uint32_t *sp;

        /* Scheduler list node */
        vrt_list_node_t node;
        vrt_list_node_t waitNode;

        bool isIdle;

    } vrt_task_t;

    /*=========================================================
     * Task API
     *=========================================================*/

    /* Initialize a task */
    void vrt_task_init(
        vrt_task_t *task,
        vrt_task_function_t entry,
        void *argument,
        uint8_t priority,
        uint32_t *stackStart,
        uint32_t stackSize,
        const char *name);

    /* Voluntarily yield CPU */
    void vrt_task_yield(void);

    /* Terminate current task */
    void vrt_task_exit(void);

    /* Suspend a task */
    void vrt_task_suspend(vrt_task_t *task);

    /* Resume a suspended task */
    void vrt_task_resume(vrt_task_t *task);

#ifdef __cplusplus
}
#endif

#endif /* VRT_TASK_H */