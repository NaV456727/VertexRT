#ifndef VRT_SCHEDULER_H
#define VRT_SCHEDULER_H

#include "vrt_types.h"
#include "vrt_list.h"
#include "vrt_task.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*=========================================================
     * Scheduler Object
     *========================================================*/
    typedef struct vrt_scheduler
    {
        /* Ready Queue */
        vrt_list_t readyQueue;

        /* Currently Running Task */
        vrt_task_t *currentTask;

        /* Idle Task */
        vrt_task_t *idleTask;

        /* System Tick Counter */
        uint32_t tickCount;

        /* Number of Registered Tasks */
        uint32_t taskCount;

        /* Scheduler State */
        bool running;

    } vrt_scheduler_t;

    /*=========================================================
     * Initialization
     *========================================================*/

    /**
     * @brief Initialize the scheduler.
     */
    void vrt_scheduler_init(vrt_scheduler_t *scheduler);

    /*=========================================================
     * Task Management
     *========================================================*/

    /**
     * @brief Add a task to the scheduler.
     *
     * @param scheduler Scheduler instance.
     * @param task Task to register.
     *
     * @return true on success.
     * @return false on failure.
     */
    bool vrt_scheduler_add_task(
        vrt_scheduler_t *scheduler,
        vrt_task_t *task);

    /*=========================================================
     * Scheduler Control
     *========================================================*/

    /**
     * @brief Start the scheduler.
     */
    void vrt_scheduler_start(vrt_scheduler_t *scheduler);

    /**
     * @brief Schedule the next runnable task.
     *
     * Selects the next task according to the
     * scheduling policy and updates the current task.
     *
     * @param scheduler Pointer to the scheduler.
     */
    void vrt_scheduler_schedule(
        vrt_scheduler_t *scheduler);

    /**
     * @brief Get the active scheduler instance.
     *
     * @return Pointer to the active scheduler.
     */
    vrt_scheduler_t *vrt_scheduler_get_instance(void);

    /**
     * @brief Process one system tick.
     *
     * Called by the hardware timer interrupt.
     *
     * @param scheduler Pointer to the scheduler.
     */
    void vrt_scheduler_tick(
        vrt_scheduler_t *scheduler);

#ifdef __cplusplus
}
#endif

#endif /* VRT_SCHEDULER_H */