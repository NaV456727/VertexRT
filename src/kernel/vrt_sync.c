#include "vrt_sync.h"
#include "vrt_scheduler.h"

#include <stddef.h>

/*=========================================================
 * Semaphore Initialization
 *=========================================================*/

void vrt_sem_init(
    vrt_sem_t *sem,
    bool initialState)
{
    if (sem == NULL)
    {
        return;
    }

    /*
     * Binary semaphore can only contain
     * 0 or 1.
     */
    sem->count =
        initialState ? 1U : 0U;

    /*
     * No tasks are waiting initially.
     */
    vrt_list_init(
        &sem->waitQueue);
}

/*=========================================================
 * Semaphore Wait
 *=========================================================*/

void vrt_sem_wait(
    vrt_sem_t *sem)
{
    if (sem == NULL)
    {
        return;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    vrt_task_t *current =
        scheduler->currentTask;

    if (current == NULL)
    {
        return;
    }

    /*
     * The idle task must never block on
     * a synchronization primitive.
     */
    if (current == scheduler->idleTask)
    {
        return;
    }

    /*
     * Semaphore is available.
     *
     * Acquire it immediately.
     */
    if (sem->count > 0U)
    {
        sem->count = 0U;
        return;
    }

    /*
     * Semaphore unavailable.
     *
     * Block the current task.
     */
    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Put the task into the semaphore's
     * waiting queue.
     */
    if (!vrt_list_push_back(
            &sem->waitQueue,
            &current->waitNode))
    {
        /*
         * If insertion failed, restore
         * the task to READY.
         */
        current->state =
            VRT_TASK_READY;

        return;
    }

    /*
     * The current task is no longer runnable.
     */
    scheduler->currentTask = NULL;

    /*
     * Select another runnable task.
     *
     * This only changes scheduler state
     * for now. The actual context switch
     * will happen through the port layer.
     */
    vrt_scheduler_schedule(
        scheduler);
}

/*=========================================================
 * Semaphore Signal
 *=========================================================*/

void vrt_sem_signal(
    vrt_sem_t *sem)
{
    if (sem == NULL)
    {
        return;
    }

    /*
     * If there are blocked tasks waiting for
     * this semaphore, wake the first one.
     */
    if (!vrt_list_is_empty(
            &sem->waitQueue))
    {
        vrt_list_node_t *node =
            sem->waitQueue.head;

        if (node == NULL)
        {
            return;
        }

        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task == NULL)
        {
            return;
        }

        /*
         * Remove task from semaphore wait queue.
         */
        vrt_list_remove(
            &sem->waitQueue,
            &task->waitNode);

        /*
         * Make task runnable again.
         */
        task->state =
            VRT_TASK_READY;

        /*
         * Put it back into the scheduler
         * ready queue.
         */
        if (!vrt_list_push_back(
                &vrt_scheduler_get_instance()->readyQueue,
                &task->node))
        {
            /*
             * If the task could not be returned to the
             * ready queue, keep it blocked.
             */
            task->state =
                VRT_TASK_BLOCKED;

            vrt_list_push_back(
                &sem->waitQueue,
                &task->waitNode);

            return;
        }

        return;
    }

    /*
     * Nobody is waiting.
     *
     * Make semaphore available.
     */
    sem->count = 1U;
}