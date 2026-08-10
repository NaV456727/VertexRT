#include "vrt_sync.h"
#include "vrt_scheduler.h"

#include <stddef.h>

/*=========================================================
 * Binary Semaphore
 *=========================================================*/

void vrt_sem_init(
    vrt_sem_t *sem,
    bool initialState)
{
    if (sem == NULL)
    {
        return;
    }

    sem->count =
        initialState ? 1U : 0U;

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
     * Idle task must never block.
     */
    if (current == scheduler->idleTask)
    {
        return;
    }

    /*
     * Semaphore available.
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
     * Put the task on the semaphore wait queue.
     */
    if (!vrt_list_push_back(
            &sem->waitQueue,
            &current->waitNode))
    {
        /*
         * Failed to queue the task.
         *
         * Restore READY state.
         */
        current->state =
            VRT_TASK_READY;

        return;
    }

    /*
     * Current task is no longer runnable.
     */
    scheduler->currentTask = NULL;

    /*
     * Select another task.
     *
     * Actual CPU switching will be performed
     * by the port layer once context switching
     * is connected.
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

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Wake the first waiting task.
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
         * Remove from semaphore wait queue.
         */
        vrt_list_remove(
            &sem->waitQueue,
            &task->waitNode);

        /*
         * Put task back into READY state.
         */
        task->state =
            VRT_TASK_READY;

        /*
         * Return task to scheduler ready queue.
         */
        if (!vrt_list_push_back(
                &scheduler->readyQueue,
                &task->node))
        {
            /*
             * Ready queue insertion failed.
             *
             * Restore BLOCKED state and put the
             * task back onto the semaphore queue.
             */
            task->state =
                VRT_TASK_BLOCKED;

            vrt_list_push_back(
                &sem->waitQueue,
                &task->waitNode);

            return;
        }

        /*
         * Semaphore ownership is transferred directly
         * to the woken task.
         *
         * Therefore count remains zero.
         */
        return;
    }

    /*
     * Nobody is waiting.
     *
     * Make the semaphore available.
     */
    sem->count = 1U;
}

/*=========================================================
 * Mutex Initialization
 *=========================================================*/

void vrt_mutex_init(
    vrt_mutex_t *mutex)
{
    if (mutex == NULL)
    {
        return;
    }

    mutex->locked = false;
    mutex->owner = NULL;

    vrt_list_init(
        &mutex->waitQueue);
}

/*=========================================================
 * Mutex Lock
 *=========================================================*/

void vrt_mutex_lock(
    vrt_mutex_t *mutex)
{
    if (mutex == NULL)
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
     * Idle task cannot own a mutex.
     */
    if (current == scheduler->idleTask)
    {
        return;
    }

    /*
     * Mutex is free.
     */
    if (!mutex->locked)
    {
        mutex->locked = true;
        mutex->owner = current;

        return;
    }

    /*
     * Current task already owns this mutex.
     *
     * We are not implementing recursive mutexes yet.
     */
    if (mutex->owner == current)
    {
        return;
    }

    /*
     * Mutex is owned by another task.
     *
     * Block current task.
     */
    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Add task to mutex wait queue.
     */
    if (!vrt_list_push_back(
            &mutex->waitQueue,
            &current->waitNode))
    {
        current->state =
            VRT_TASK_READY;

        return;
    }

    /*
     * Current task is no longer running.
     */
    scheduler->currentTask = NULL;

    /*
     * Select another task.
     */
    vrt_scheduler_schedule(
        scheduler);
}

/*=========================================================
 * Mutex Unlock
 *=========================================================*/

void vrt_mutex_unlock(
    vrt_mutex_t *mutex)
{
    if (mutex == NULL)
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
     * Only the owner may unlock the mutex.
     */
    if (mutex->owner != current)
    {
        return;
    }

    /*
     * If another task is waiting, transfer
     * ownership directly to it.
     */
    if (!vrt_list_is_empty(
            &mutex->waitQueue))
    {
        vrt_list_node_t *node =
            mutex->waitQueue.head;

        if (node == NULL)
        {
            return;
        }

        vrt_task_t *next =
            (vrt_task_t *)node->owner;

        if (next == NULL)
        {
            return;
        }

        /*
         * Remove from mutex wait queue.
         */
        vrt_list_remove(
            &mutex->waitQueue,
            &next->waitNode);

        /*
         * Return task to READY state.
         */
        next->state =
            VRT_TASK_READY;

        /*
         * Return task to scheduler queue.
         */
        if (!vrt_list_push_back(
                &scheduler->readyQueue,
                &next->node))
        {
            /*
             * Failed to make task runnable.
             * Keep it blocked and preserve ownership.
             */
            next->state =
                VRT_TASK_BLOCKED;

            vrt_list_push_back(
                &mutex->waitQueue,
                &next->waitNode);

            return;
        }

        /*
         * Transfer ownership directly.
         */
        mutex->owner = next;
        mutex->locked = true;

        return;
    }

    /*
     * Nobody is waiting.
     *
     * Completely release mutex.
     */
    mutex->owner = NULL;
    mutex->locked = false;
}