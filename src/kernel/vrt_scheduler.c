#include "vrt_scheduler.h"
#include "vrt_port.h"
#include "vrt_config.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * ============================================================================
 * Global scheduler
 * ============================================================================
 */

static vrt_scheduler_t vrt_scheduler;

/*
 * ============================================================================
 * Idle task
 * ============================================================================
 */

static vrt_task_t vrt_idle_task;

static uint32_t
    vrt_idle_stack[VRT_STACK_SIZE];

/*
 * ============================================================================
 * Idle task entry
 * ============================================================================
 */

static void vrt_idle_task_entry(void *argument)
{
    (void)argument;

    for (;;)
    {
        /*
         * Keep the idle task as the simplest possible task.
         */
    }
}

/*
 * ============================================================================
 * Find first READY task
 * ============================================================================
 */

static vrt_task_t *
vrt_scheduler_find_ready_task(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return NULL;
    }

    vrt_list_node_t *node =
        scheduler->readyQueue.head;

    while (node != NULL)
    {
        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task->state == VRT_TASK_READY)
        {
            return task;
        }

        node = node->next;
    }

    return NULL;
}

/*
 * ============================================================================
 * Find next READY task
 * ============================================================================
 */

static vrt_task_t *
vrt_scheduler_find_next_ready_task(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return NULL;
    }

    if (scheduler->readyQueue.head == NULL)
    {
        return NULL;
    }

    /*
     * No current task or idle is running.
     */
    if (scheduler->currentTask == NULL ||
        scheduler->currentTask == scheduler->idleTask)
    {
        return vrt_scheduler_find_ready_task(
            scheduler);
    }

    /*
     * Start immediately after current.
     */
    vrt_list_node_t *node =
        scheduler->currentTask->node.next;

    while (node != NULL)
    {
        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task != scheduler->currentTask &&
            task->state == VRT_TASK_READY)
        {
            return task;
        }

        node = node->next;
    }

    /*
     * Wrap around.
     */
    node =
        scheduler->readyQueue.head;

    while (node != NULL)
    {
        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task != scheduler->currentTask &&
            task->state == VRT_TASK_READY)
        {
            return task;
        }

        node = node->next;
    }

    return NULL;
}

/*
 * ============================================================================
 * Scheduler initialization
 * ============================================================================
 */

void vrt_scheduler_init(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    vrt_list_init(
        &scheduler->readyQueue);

    vrt_list_init(
        &scheduler->delayedQueue);

    scheduler->currentTask =
        NULL;

    scheduler->idleTask =
        NULL;

    scheduler->tickCount =
        0U;

    scheduler->taskCount =
        0U;

    scheduler->running =
        false;

    /*
     * Initialize idle task.
     */
    vrt_task_init(
        &vrt_idle_task,
        vrt_idle_task_entry,
        NULL,
        0U,
        vrt_idle_stack,
        VRT_STACK_SIZE,
        "idle");

    vrt_idle_task.id =
        0U;

    vrt_idle_task.isIdle =
        true;

    vrt_idle_task.state =
        VRT_TASK_READY;

    scheduler->idleTask =
        &vrt_idle_task;

    scheduler->currentTask =
        scheduler->idleTask;
}

/*
 * ============================================================================
 * Add task
 * ============================================================================
 */

bool vrt_scheduler_add_task(
    vrt_scheduler_t *scheduler,
    vrt_task_t *task)
{
    if (scheduler == NULL ||
        task == NULL)
    {
        return false;
    }

    if (task->isIdle)
    {
        return false;
    }

    if (scheduler->taskCount >=
        VRT_MAX_TASKS)
    {
        return false;
    }

    if (task->state !=
        VRT_TASK_READY)
    {
        return false;
    }

    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node))
    {
        return false;
    }

    scheduler->taskCount++;

    return true;
}

/*
 * ============================================================================
 * Scheduler selection
 * ============================================================================
 */

void vrt_scheduler_schedule(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    vrt_task_t *current =
        scheduler->currentTask;

    vrt_task_t *next =
        vrt_scheduler_find_next_ready_task(
            scheduler);

    /*
     * ------------------------------------------------------------------------
     * No other READY task.
     * ------------------------------------------------------------------------
     */

    if (next == NULL)
    {
        /*
         * Keep current task if it remains runnable.
         */
        if (current != NULL &&
            current != scheduler->idleTask &&
            (current->state == VRT_TASK_RUNNING ||
             current->state == VRT_TASK_READY))
        {
            current->state =
                VRT_TASK_RUNNING;

            scheduler->currentTask =
                current;

            return;
        }

        /*
         * Otherwise use idle.
         */
        if (scheduler->idleTask != NULL)
        {
            scheduler->idleTask->state =
                VRT_TASK_RUNNING;

            scheduler->currentTask =
                scheduler->idleTask;
        }

        return;
    }

    /*
     * ------------------------------------------------------------------------
     * Different READY task exists.
     * ------------------------------------------------------------------------
     */

    if (current != NULL &&
        current != scheduler->idleTask &&
        current != next &&
        current->state == VRT_TASK_RUNNING)
    {
        current->state =
            VRT_TASK_READY;
    }

    if (current == scheduler->idleTask &&
        current != next)
    {
        current->state =
            VRT_TASK_READY;
    }

    next->state =
        VRT_TASK_RUNNING;

    scheduler->currentTask =
        next;
}

/*
 * ============================================================================
 * Scheduler tick
 * ============================================================================
 *
 * Cooperative/manual tick for Step 11.
 *
 * One call advances the kernel by exactly one tick.
 *
 * Blocked tasks whose wakeTick has arrived are moved from delayedQueue back
 * to readyQueue.
 * ============================================================================
 */

void vrt_scheduler_tick(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    scheduler->tickCount++;

    /*
     * Walk the delayed queue.
     */
    vrt_list_node_t *node =
        scheduler->delayedQueue.head;

    while (node != NULL)
    {
        /*
         * Save next before removing the current node.
         */
        vrt_list_node_t *nextNode =
            node->next;

        vrt_task_t *task =
            (vrt_task_t *)node->owner;

        if (task != NULL &&
            task->state == VRT_TASK_BLOCKED)
        {
            /*
             * Signed-difference comparison handles uint32_t wraparound.
             *
             * Task is due when:
             *
             *     currentTick - wakeTick >= 0
             */
            int32_t remaining =
                (int32_t)(task->wakeTick -
                          scheduler->tickCount);

            if (remaining <= 0)
            {
                /*
                 * Remove from delayed queue.
                 */
                vrt_list_remove(
                    &scheduler->delayedQueue,
                    &task->waitNode);

                /*
                 * Make runnable.
                 */
                task->state =
                    VRT_TASK_READY;

                /*
                 * Reinsert into ready queue.
                 */
                vrt_list_push_back(
                    &scheduler->readyQueue,
                    &task->node);
            }
        }

        node =
            nextNode;
    }
}

/*
 * ============================================================================
 * Scheduler start
 * ============================================================================
 */

void vrt_scheduler_start(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    if (scheduler->running)
    {
        return;
    }

    vrt_task_t *first =
        vrt_scheduler_find_ready_task(
            scheduler);

    if (first != NULL)
    {
        first->state =
            VRT_TASK_RUNNING;

        scheduler->currentTask =
            first;
    }
    else
    {
        if (scheduler->idleTask == NULL)
        {
            return;
        }

        scheduler->idleTask->state =
            VRT_TASK_RUNNING;

        scheduler->currentTask =
            scheduler->idleTask;
    }

    scheduler->running =
        true;

    vrt_port_start_first_task(
        scheduler->currentTask->sp);

    for (;;)
    {
    }
}

/*
 * ============================================================================
 * Scheduler instance
 * ============================================================================
 */

vrt_scheduler_t *
vrt_scheduler_get_instance(void)
{
    return &vrt_scheduler;
}