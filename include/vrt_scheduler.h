#ifndef VRT_SCHEDULER_H
#define VRT_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

#include "vrt_list.h"
#include "vrt_task.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct vrt_scheduler vrt_scheduler_t;

    struct vrt_scheduler
    {
        /*
         * Runnable tasks.
         */
        vrt_list_t readyQueue;

        /*
         * Tasks blocked waiting for their wake tick.
         */
        vrt_list_t delayedQueue;

        /*
         * Currently selected task.
         */
        vrt_task_t *currentTask;

        /*
         * Internal idle task.
         */
        vrt_task_t *idleTask;

        /*
         * Kernel tick counter.
         */
        uint32_t tickCount;

        /*
         * Number of user tasks.
         */
        uint32_t taskCount;

        /*
         * Scheduler running flag.
         */
        bool running;
    };

    /*
     * ========================================================================
     * Initialization
     * ========================================================================
     */

    void vrt_scheduler_init(
        vrt_scheduler_t *scheduler);

    /*
     * ========================================================================
     * Task management
     * ========================================================================
     */

    bool vrt_scheduler_add_task(
        vrt_scheduler_t *scheduler,
        vrt_task_t *task);

    /*
     * ========================================================================
     * Scheduling
     * ========================================================================
     */

    void vrt_scheduler_schedule(
        vrt_scheduler_t *scheduler);

    /*
     * ========================================================================
     * Tick
     * ========================================================================
     *
     * Advance the kernel by one tick.
     *
     * This is intentionally independent of a hardware timer for now.
     */
    void vrt_scheduler_tick(
        vrt_scheduler_t *scheduler);

    /*
     * ========================================================================
     * Start
     * ========================================================================
     */

    void vrt_scheduler_start(
        vrt_scheduler_t *scheduler);

    /*
     * ========================================================================
     * Global instance
     * ========================================================================
     */

    vrt_scheduler_t *
    vrt_scheduler_get_instance(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_SCHEDULER_H */