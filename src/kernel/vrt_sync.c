#include "vrt_sync.h"
#include "vrt_scheduler.h"
#include "vrt_freertos_backend.h"

#include <stddef.h>

/*
 * ============================================================================
 * Binary Semaphore
 * ============================================================================
 */

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

/*
 * ============================================================================
 * Semaphore Wait
 * ============================================================================
 */

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

    /*
     * Use the task that is actually executing.
     */
    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

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
     * Keep scheduler state synchronized with
     * the actual executing task.
     */
    scheduler->currentTask =
        current;

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
     * Block current task.
     */
    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Remove from the runnable queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    /*
     * Add to semaphore wait queue.
     */
    if (!vrt_list_push_back(
            &sem->waitQueue,
            &current->waitNode))
    {
        /*
         * Roll back if the wait queue insertion failed.
         */
        current->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        return;
    }

    /*
     * Select another runnable task.
     */
    scheduler->currentTask =
        current;

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    /*
     * Switch the actual FreeRTOS execution context.
     */
    if (next != NULL &&
        next != current)
    {
        vrt_freertos_backend_switch_to(
            next);
    }

    /*
     * Suspend the actual backing task.
     *
     * This task resumes when vrt_sem_signal()
     * wakes it.
     */
    vrt_freertos_backend_block_current();
}

/*
 * ============================================================================
 * Semaphore Signal
 * ============================================================================
 */

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
         * Make runnable.
         */
        task->state =
            VRT_TASK_READY;

        /*
         * Return task to ready queue.
         */
        if (!vrt_list_push_back(
                &scheduler->readyQueue,
                &task->node))
        {
            /*
             * Roll back if insertion failed.
             */
            task->state =
                VRT_TASK_BLOCKED;

            vrt_list_push_back(
                &sem->waitQueue,
                &task->waitNode);

            return;
        }

        /*
         * Wake the actual FreeRTOS backing task.
         */
        vrt_freertos_backend_wake_task(
            task);

        /*
         * Direct ownership transfer.
         *
         * Semaphore remains unavailable because the
         * woken task is effectively receiving it.
         */
        sem->count = 0U;

        return;
    }

    /*
     * Nobody is waiting.
     *
     * Make the semaphore available.
     */
    sem->count = 1U;
}

/*
 * ============================================================================
 * Mutex Initialization
 * ============================================================================
 */

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

/*
 * ============================================================================
 * Mutex Lock
 * ============================================================================
 */

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

    /*
     * Use the actual executing VertexRT task.
     */
    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

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
     * Keep scheduler state synchronized.
     */
    scheduler->currentTask =
        current;

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
     * Recursive mutexes are not supported yet.
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
     * Remove from ready queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    /*
     * Add to mutex wait queue.
     */
    if (!vrt_list_push_back(
            &mutex->waitQueue,
            &current->waitNode))
    {
        /*
         * Roll back on failure.
         */
        current->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        return;
    }

    /*
     * Select another runnable task.
     */
    scheduler->currentTask =
        current;

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    /*
     * Switch actual FreeRTOS execution.
     */
    if (next != NULL &&
        next != current)
    {
        vrt_freertos_backend_switch_to(
            next);
    }

    /*
     * Block the actual backing task.
     */
    vrt_freertos_backend_block_current();
}

/*
 * ============================================================================
 * Mutex Unlock
 * ============================================================================
 */

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

    /*
     * Use the actual executing task.
     */
    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    if (current == NULL)
    {
        return;
    }

    scheduler->currentTask =
        current;

    /*
     * Only the owner may unlock.
     */
    if (mutex->owner != current)
    {
        return;
    }

    /*
     * If another task is waiting, transfer ownership.
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
         * Make runnable.
         */
        next->state =
            VRT_TASK_READY;

        /*
         * Add to ready queue.
         */
        if (!vrt_list_push_back(
                &scheduler->readyQueue,
                &next->node))
        {
            /*
             * Roll back on failure.
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
        mutex->owner =
            next;

        mutex->locked =
            true;

        /*
         * Wake actual FreeRTOS backing task.
         */
        vrt_freertos_backend_wake_task(
            next);

        return;
    }

    /*
     * Nobody is waiting.
     *
     * Fully release mutex.
     */
    mutex->owner =
        NULL;

    mutex->locked =
        false;
}