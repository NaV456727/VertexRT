#include "vrt_sync.h"
#include "vrt_scheduler.h"
#include "vrt_freertos_backend.h"
#include "vrt_critical.h"

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

    sem->backendHandle =
        NULL;

    sem->timedWaiter =
        NULL;

    vrt_list_init(
        &sem->waitQueue);

    if (!vrt_freertos_backend_sem_init(
            &sem->backendHandle,
            initialState))
    {
        sem->backendHandle =
            NULL;
    }
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

        if (sem->backendHandle != NULL)
        {
            (void)vrt_freertos_backend_sem_take(
                sem->backendHandle);
        }

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

bool vrt_sem_wait_timeout(
    vrt_sem_t *sem,
    uint32_t timeoutTicks)
{
    if (sem == NULL)
    {
        return false;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return false;
    }

    /*
     * Use the task that is physically executing.
     */
    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    if (current == NULL)
    {
        return false;
    }

    /*
     * Idle task must never block.
     */
    if (current == scheduler->idleTask)
    {
        return false;
    }

    /*
     * Keep scheduler state synchronized with the actual
     * executing VertexRT task.
     */
    scheduler->currentTask =
        current;

    /*
     * ========================================================================
     * Immediate acquisition
     * ========================================================================
     *
     * The semaphore is already available.
     */
    if (sem->count > 0U)
    {
        sem->count = 0U;

        if (sem->backendHandle != NULL)
        {
            (void)vrt_freertos_backend_sem_take(
                sem->backendHandle);
        }

        return true;
    }

    /*
     * ========================================================================
     * Zero timeout
     * ========================================================================
     *
     * Non-blocking attempt.
     */
    if (timeoutTicks == 0U)
    {
        return false;
    }

    /*
     * ========================================================================
     * Enter VertexRT critical section
     * ========================================================================
     */

    vrt_kernel_critical_enter();

    /*
     * Recheck availability after entering the critical section.
     */
    if (sem->count > 0U)
    {
        sem->count = 0U;

        if (sem->backendHandle != NULL)
        {
            (void)vrt_freertos_backend_sem_take(
                sem->backendHandle);
        }

        vrt_kernel_critical_exit();

        return true;
    }

    /*
     * ========================================================================
     * Mark this task as a timed waiter
     * ========================================================================
     *
     * Multiple timed waiters are allowed.
     */
    current->timedWaitActive =
        true;

    /*
     * Keep timedWaiter as a reference to the first waiter in the queue.
     *
     * It is NOT used as a one-waiter restriction.
     */
    if (sem->timedWaiter == NULL)
    {
        sem->timedWaiter =
            current;
    }

    /*
     * ========================================================================
     * Block current VertexRT task
     * ========================================================================
     */

    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Remove current task from the READY queue.
     */
    if (!vrt_list_remove(
            &scheduler->readyQueue,
            &current->node))
    {
        current->state =
            VRT_TASK_RUNNING;

        current->timedWaitActive =
            false;

        sem->timedWaiter =
            NULL;

        vrt_kernel_critical_exit();

        return false;
    }

    /*
     * Add current task to the semaphore wait queue.
     */
    if (!vrt_list_push_back(
            &sem->waitQueue,
            &current->waitNode))
    {
        current->state =
            VRT_TASK_RUNNING;

        current->timedWaitActive =
            false;

        (void)vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        /*
         * Rebuild timedWaiter reference.
         */
        if (!vrt_list_is_empty(
                &sem->waitQueue))
        {
            vrt_list_node_t *node =
                sem->waitQueue.head;

            if (node != NULL)
            {
                sem->timedWaiter =
                    (vrt_task_t *)node->owner;
            }
            else
            {
                sem->timedWaiter =
                    NULL;
            }
        }
        else
        {
            sem->timedWaiter =
                NULL;
        }

        vrt_kernel_critical_exit();

        return false;
    }

    /*
     * ========================================================================
     * Select another runnable VertexRT task
     * ========================================================================
     */

    scheduler->currentTask =
        current;

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    /*
     * ========================================================================
     * No replacement task available
     * ========================================================================
     */

    if (next == NULL ||
        next == current)
    {
        (void)vrt_list_remove(
            &sem->waitQueue,
            &current->waitNode);

        current->state =
            VRT_TASK_RUNNING;

        current->timedWaitActive =
            false;

        (void)vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        scheduler->currentTask =
            current;

        /*
         * Rebuild timedWaiter reference.
         */
        if (!vrt_list_is_empty(
                &sem->waitQueue))
        {
            vrt_list_node_t *node =
                sem->waitQueue.head;

            if (node != NULL)
            {
                sem->timedWaiter =
                    (vrt_task_t *)node->owner;
            }
            else
            {
                sem->timedWaiter =
                    NULL;
            }
        }
        else
        {
            sem->timedWaiter =
                NULL;
        }

        vrt_kernel_critical_exit();

        return false;
    }

    /*
     * ========================================================================
     * Leave VertexRT critical section
     * ========================================================================
     *
     * We must not hold the VertexRT critical section while performing
     * the native FreeRTOS blocking operation.
     */
    vrt_kernel_critical_exit();

    /*
     * ========================================================================
     * Native timed blocking
     * ========================================================================
     *
     * Every VertexRT task now has its own native timed-wait semaphore.
     *
     * This prevents FreeRTOS from waking a different VertexRT waiter.
     */
    bool acquired =
        vrt_freertos_backend_block_current_timed(
            current,
            next,
            timeoutTicks);

    /*
     * ========================================================================
     * Returned from native blocking
     * ========================================================================
     *
     * We get here because:
     *
     *   1. This task was explicitly signaled, or
     *   2. Its timeout expired.
     */
    vrt_kernel_critical_enter();

    /*
     * Remove this task from the VertexRT semaphore wait queue if it
     * is still present.
     *
     * If sem_signal() already removed it, this does nothing.
     */
    if (current->waitNode.list ==
        &sem->waitQueue)
    {
        (void)vrt_list_remove(
            &sem->waitQueue,
            &current->waitNode);
    }

    /*
     * This task is no longer a timed waiter.
     */
    current->timedWaitActive =
        false;

    /*
     * ========================================================================
     * Rebuild timedWaiter
     * ========================================================================
     */

    if (!vrt_list_is_empty(
            &sem->waitQueue))
    {
        vrt_list_node_t *node =
            sem->waitQueue.head;

        if (node != NULL)
        {
            sem->timedWaiter =
                (vrt_task_t *)node->owner;
        }
        else
        {
            sem->timedWaiter =
                NULL;
        }
    }
    else
    {
        sem->timedWaiter =
            NULL;
    }

    /*
     * If the native wait succeeded, the semaphore token was consumed
     * by this task.
     */
    if (acquired)
    {
        sem->count =
            0U;
    }

    /*
     * ========================================================================
     * Return task to READY state
     * ========================================================================
     */

    current->state =
        VRT_TASK_READY;

    if (current->node.list == NULL)
    {
        (void)vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);
    }

    /*
     * Restore current task identity.
     */
    current->state =
        VRT_TASK_RUNNING;

    scheduler->currentTask =
        current;

    vrt_kernel_critical_exit();

    return acquired;
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
     * ========================================================================
     * Find the highest-priority blocked waiter.
     *
     * Do NOT simply use waitQueue.head.
     *
     * VertexRT semaphore wakeup is priority-based.
     * ========================================================================
     */

    vrt_task_t *selectedTask =
        NULL;

    vrt_list_node_t *node =
        sem->waitQueue.head;

    while (node != NULL)
    {
        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task->state == VRT_TASK_BLOCKED)
        {
            if (selectedTask == NULL ||
                task->priority >
                    selectedTask->priority)
            {
                selectedTask =
                    task;
            }
        }

        node =
            node->next;
    }

    /*
     * ========================================================================
     * No waiter.
     *
     * Store a semaphore token.
     * ========================================================================
     */

    if (selectedTask == NULL)
    {
        sem->count =
            1U;

        if (sem->backendHandle != NULL)
        {
            (void)vrt_freertos_backend_sem_give(
                sem->backendHandle);
        }

        return;
    }

    /*
     * ========================================================================
     * Remember whether this task is blocked in a timed native wait.
     * ========================================================================
     */

    bool isTimed =
        selectedTask->timedWaitActive;

    /*
     * Remove selected task from the VertexRT semaphore wait queue.
     */
    if (!vrt_list_remove(
            &sem->waitQueue,
            &selectedTask->waitNode))
    {
        return;
    }

    /*
     * This task is no longer a timed waiter.
     */
    selectedTask->timedWaitActive =
        false;

    /*
     * ========================================================================
     * Rebuild the diagnostic timedWaiter pointer.
     * ========================================================================
     */

    if (!vrt_list_is_empty(
            &sem->waitQueue))
    {
        vrt_list_node_t *nextNode =
            sem->waitQueue.head;

        if (nextNode != NULL)
        {
            sem->timedWaiter =
                (vrt_task_t *)nextNode->owner;
        }
        else
        {
            sem->timedWaiter =
                NULL;
        }
    }
    else
    {
        sem->timedWaiter =
            NULL;
    }

    /*
     * ========================================================================
     * Wake the selected task's PRIVATE native semaphore.
     *
     * This is the key change.
     *
     * Previously all timed waiters shared sem->backendHandle, allowing
     * FreeRTOS to wake a different waiter from the one VertexRT selected.
     * ========================================================================
     */

    if (isTimed)
    {
        if (!vrt_freertos_backend_wake_timed_task(
                selectedTask))
        {
            /*
             * Native wake failed.
             *
             * Roll back VertexRT state.
             */
            selectedTask->timedWaitActive =
                true;

            selectedTask->state =
                VRT_TASK_BLOCKED;

            (void)vrt_list_push_back(
                &sem->waitQueue,
                &selectedTask->waitNode);

            if (sem->timedWaiter == NULL)
            {
                sem->timedWaiter =
                    selectedTask;
            }

            return;
        }
    }

    /*
     * ========================================================================
     * Make selected task READY.
     * ========================================================================
     */

    selectedTask->state =
        VRT_TASK_READY;

    /*
     * Put it back into the scheduler READY queue.
     */
    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &selectedTask->node))
    {
        /*
         * Roll back if READY queue insertion fails.
         */
        selectedTask->state =
            VRT_TASK_BLOCKED;

        if (isTimed)
        {
            selectedTask->timedWaitActive =
                true;
        }

        (void)vrt_list_push_back(
            &sem->waitQueue,
            &selectedTask->waitNode);

        if (sem->timedWaiter == NULL)
        {
            sem->timedWaiter =
                selectedTask;
        }

        return;
    }

    /*
     * The semaphore token is consumed by the selected waiter.
     */
    sem->count =
        0U;

    /*
     * ========================================================================
     * Preempt current task if the selected waiter has higher priority.
     * ========================================================================
     */

    vrt_task_t *current =
        scheduler->currentTask;

    if (current != NULL &&
        selectedTask->priority >
            current->priority)
    {
        current->state =
            VRT_TASK_READY;

        selectedTask->state =
            VRT_TASK_RUNNING;

        scheduler->currentTask =
            selectedTask;

        vrt_freertos_backend_switch_to(
            selectedTask);
    }
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

    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    if (current == NULL)
    {
        return;
    }

    if (current == scheduler->idleTask)
    {
        return;
    }

    scheduler->currentTask =
        current;

    if (!mutex->locked)
    {
        mutex->locked = true;
        mutex->owner = current;

        return;
    }

    if (mutex->owner == current)
    {
        return;
    }

    current->state =
        VRT_TASK_BLOCKED;

    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    if (!vrt_list_push_back(
            &mutex->waitQueue,
            &current->waitNode))
    {
        current->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        return;
    }

    if (mutex->owner != NULL &&
        current->priority >
            mutex->owner->priority)
    {
        mutex->owner->priority =
            current->priority;
    }

    scheduler->currentTask =
        current;

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

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

    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    if (current == NULL)
    {
        return;
    }

    scheduler->currentTask =
        current;

    if (mutex->owner != current)
    {
        return;
    }

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

        vrt_list_remove(
            &mutex->waitQueue,
            &next->waitNode);

        next->state =
            VRT_TASK_READY;

        if (!vrt_list_push_back(
                &scheduler->readyQueue,
                &next->node))
        {
            next->state =
                VRT_TASK_BLOCKED;

            vrt_list_push_back(
                &mutex->waitQueue,
                &next->waitNode);

            return;
        }

        current->priority =
            current->basePriority;

        mutex->owner =
            next;

        mutex->locked =
            true;

        if (next->priority >
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

    current->priority =
        current->basePriority;

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