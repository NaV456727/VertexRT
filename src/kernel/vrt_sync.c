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
         * The task is now READY from VertexRT's perspective.
         *
         * Do not directly resume its FreeRTOS backing task.
         * Let VertexRT decide whether it should run now.
         */
        sem->count = 0U;

        /*
         * Check whether the awakened task should immediately
         * preempt the current task.
         */
        vrt_task_t *current =
            scheduler->currentTask;

        if (current != NULL &&
            task->priority >
                current->priority)
        {
            current->state =
                VRT_TASK_READY;

            task->state =
                VRT_TASK_RUNNING;

            scheduler->currentTask =
                task;

            vrt_freertos_backend_switch_to(
                task);
        }

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
         * The task is READY in VertexRT.
         *
         * Select it immediately if it outranks the
         * currently executing task.
         */
        vrt_task_t *current =
            scheduler->currentTask;

        if (current != NULL &&
            next->priority >
                current->priority)
        {
            current->state =
                VRT_TASK_READY;

            next->state =
                VRT_TASK_RUNNING;

            scheduler->currentTask =
                next;

            vrt_freertos_backend_switch_to(
                next);
        }

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

/*
 * ============================================================================
 * Event Group
 * ============================================================================
 */

void vrt_event_group_init(
    vrt_event_group_t *group)
{
    if (group == NULL)
    {
        return;
    }

    group->bits = 0U;

    vrt_list_init(
        &group->waitQueue);
}

/*
 * ============================================================================
 * Event Group condition check
 * ============================================================================
 */

static bool
vrt_event_group_condition_met(
    uint32_t currentBits,
    uint32_t requestedBits,
    bool waitForAll)
{
    if (requestedBits == 0U)
    {
        return true;
    }

    if (waitForAll)
    {
        return (currentBits & requestedBits) ==
               requestedBits;
    }

    return (currentBits & requestedBits) !=
           0U;
}

/*
 * ============================================================================
 * Event Group Wait
 * ============================================================================
 */

uint32_t vrt_event_group_wait_bits(
    vrt_event_group_t *group,
    uint32_t bits,
    bool waitForAll,
    bool clearOnExit)
{
    if (group == NULL ||
        bits == 0U)
    {
        return 0U;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return 0U;
    }

    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    if (current == NULL ||
        current == scheduler->idleTask)
    {
        return 0U;
    }

    scheduler->currentTask =
        current;

    /*
     * Check whether the event is already available.
     */
    if (vrt_event_group_condition_met(
            group->bits,
            bits,
            waitForAll))
    {
        uint32_t matchedBits =
            group->bits & bits;

        if (clearOnExit)
        {
            group->bits &=
                ~matchedBits;
        }

        return matchedBits;
    }

    /*
     * Store the wait condition.
     */
    current->eventWaitBits =
        bits;

    current->eventWaitResult =
        0U;

    current->eventWaitForAll =
        waitForAll;

    current->eventClearOnExit =
        clearOnExit;

    /*
     * Block the VertexRT task.
     */
    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Remove it from the READY queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    /*
     * Add it to the event-group wait queue.
     */
    if (!vrt_list_push_back(
            &group->waitQueue,
            &current->waitNode))
    {
        current->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        return 0U;
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
     * Switch to the selected VertexRT task.
     */
    if (next != NULL &&
        next != current)
    {
        vrt_freertos_backend_switch_to(
            next);
    }

    /*
     * We resume here after our event condition
     * has been satisfied.
     */
    uint32_t result =
        current->eventWaitResult;

    /*
     * Clear temporary wait state.
     */
    current->eventWaitBits =
        0U;

    current->eventWaitResult =
        0U;

    current->eventWaitForAll =
        false;

    current->eventClearOnExit =
        false;

    return result;
}

/*
 * ============================================================================
 * Find highest-priority READY task
 * ============================================================================
 */

static vrt_task_t *
vrt_event_group_find_best_ready(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return NULL;
    }

    vrt_task_t *best =
        NULL;

    vrt_list_node_t *node =
        scheduler->readyQueue.head;

    while (node != NULL)
    {
        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task->state == VRT_TASK_READY)
        {
            if (best == NULL ||
                task->priority > best->priority)
            {
                best =
                    task;
            }
        }

        node =
            node->next;
    }

    return best;
}

/*
 * ============================================================================
 * Event Group Set Bits
 * ============================================================================
 */

void vrt_event_group_set_bits(
    vrt_event_group_t *group,
    uint32_t bits)
{
    if (group == NULL ||
        bits == 0U)
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
     * Set the event bits first.
     */
    group->bits |= bits;

    /*
     * Current task executing this operation.
     */
    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    /*
     * Keep the scheduler's logical current task
     * synchronized with the actual executing task.
     */
    if (current != NULL)
    {
        scheduler->currentTask =
            current;
    }

    /*
     * ------------------------------------------------------------------------
     * Wake EVERY waiter whose condition is now satisfied.
     * ------------------------------------------------------------------------
     */

    vrt_list_node_t *node =
        group->waitQueue.head;

    while (node != NULL)
    {
        /*
         * Save the next node before removing the current one.
         */
        vrt_list_node_t *nextNode =
            node->next;

        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task->state == VRT_TASK_BLOCKED)
        {
            bool satisfied =
                vrt_event_group_condition_met(
                    group->bits,
                    task->eventWaitBits,
                    task->eventWaitForAll);

            if (satisfied)
            {
                /*
                 * Bits that satisfied this task.
                 */
                uint32_t matchedBits =
                    group->bits &
                    task->eventWaitBits;

                /*
                 * Save result for wait_bits().
                 */
                task->eventWaitResult =
                    matchedBits;

                /*
                 * Remove from event wait queue.
                 */
                vrt_list_remove(
                    &group->waitQueue,
                    &task->waitNode);

                /*
                 * Make READY.
                 */
                task->state =
                    VRT_TASK_READY;

                /*
                 * Return to scheduler READY queue.
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
                        &group->waitQueue,
                        &task->waitNode);

                    node =
                        nextNode;

                    continue;
                }

                /*
                 * Clear matched bits for this waiter if requested.
                 */
                if (task->eventClearOnExit)
                {
                    group->bits &=
                        ~matchedBits;
                }
            }
        }

        node =
            nextNode;
    }

    /*
     * ------------------------------------------------------------------------
     * Choose the highest-priority READY task.
     * ------------------------------------------------------------------------
     */

    current =
        scheduler->currentTask;

    if (current == NULL)
    {
        /*
         * No logical current task.
         *
         * The next scheduler boundary will select one.
         */
        return;
    }

    vrt_task_t *best =
        NULL;

    node =
        scheduler->readyQueue.head;

    while (node != NULL)
    {
        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task != current &&
            task->state == VRT_TASK_READY)
        {
            if (best == NULL ||
                task->priority > best->priority)
            {
                best =
                    task;
            }
        }

        node =
            node->next;
    }

    /*
     * ------------------------------------------------------------------------
     * Preempt only if a READY task has higher priority.
     * ------------------------------------------------------------------------
     */

    if (best != NULL &&
        best->priority >
            current->priority)
    {
        current->state =
            VRT_TASK_READY;

        best->state =
            VRT_TASK_RUNNING;

        scheduler->currentTask =
            best;

        vrt_freertos_backend_switch_to(
            best);
    }
}