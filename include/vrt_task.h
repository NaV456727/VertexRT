#ifndef VRT_TASK_H
#define VRT_TASK_H

#include <stdint.h>
#include <stdbool.h>

#include "vrt_list.h"
#include "vrt_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Task entry function.
     */
    typedef void (*vrt_task_function_t)(void *argument);

    /*
     * Task states.
     *
     * Keep the existing numeric values:
     *
     *     READY      = 0
     *     RUNNING    = 1
     *     BLOCKED    = 2
     *     SUSPENDED  = 3
     *     TERMINATED = 4
     */
    typedef enum
    {
        VRT_TASK_READY = 0,
        VRT_TASK_RUNNING = 1,
        VRT_TASK_BLOCKED = 2,
        VRT_TASK_SUSPENDED = 3,
        VRT_TASK_TERMINATED = 4
    } vrt_task_state_t;

    typedef struct vrt_task vrt_task_t;

    /*
     * Task control block.
     */
    struct vrt_task
    {
        /*
         * Wait/delay list node.
         */
        vrt_list_node_t waitNode;

        /*
         * Ready queue node.
         */
        vrt_list_node_t node;

        /*
         * Task metadata.
         */
        uint32_t id;

        vrt_task_function_t entry;
        void *argument;

        uint8_t priority;

        vrt_task_state_t state;

        bool isIdle;

        /*
         * Stack information.
         */
        uint32_t *stackStart;
        uint32_t stackSize;
        uint32_t *stackEnd;

        /*
         * Saved architecture context.
         */
        uint32_t *sp;

        /*
         * Tick at which a blocked task becomes runnable.
         */
        uint32_t wakeTick;

        /*
         * Task name.
         */
        char name[VRT_TASK_NAME_LENGTH];
    };

    /*
     * ========================================================================
     * Task lifecycle
     * ========================================================================
     */

    void vrt_task_init(
        vrt_task_t *task,
        vrt_task_function_t entry,
        void *argument,
        uint8_t priority,
        uint32_t *stackStart,
        uint32_t stackSize,
        const char *name);

    /*
     * ========================================================================
     * Cooperative scheduling
     * ========================================================================
     */

    void vrt_task_yield(void);

    /*
     * ========================================================================
     * Task termination
     * ========================================================================
     */

    void vrt_task_exit(void);

    /*
     * ========================================================================
     * Suspend / resume
     * ========================================================================
     */

    void vrt_task_suspend(
        vrt_task_t *task);

    void vrt_task_resume(
        vrt_task_t *task);

    /*
     * ========================================================================
     * Delay / blocking
     * ========================================================================
     *
     * Block the current task for the specified number of scheduler ticks.
     */
    void vrt_task_delay(
        uint32_t ticks);

#ifdef __cplusplus
}
#endif

#endif /* VRT_TASK_H */