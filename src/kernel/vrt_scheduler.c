#include "vrt_scheduler.h"
#include "vrt_port.h"
#include "vrt_config.h"

#include <stddef.h>
#include <stdbool.h>

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

    /*
     * The idle task must always remain runnable.
     *
     * Do not explicitly yield here. A tight idle loop gives us the simplest
     * possible bootstrap while the voluntary context-switch path is being
     * validated.
     */
    for (;;)
    {
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
 *
 * The ready queue is maintained in insertion order.
 *
 * We perform round-robin selection by starting immediately after the current
 * task and wrapping around to the queue head.
 *
 * The current task normally remains in the queue while RUNNING.
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
     * No current task or idle is running:
     * simply choose the first runnable task.
     */
    if (scheduler->currentTask == NULL ||
        scheduler->currentTask == scheduler->idleTask)
    {
        return vrt_scheduler_find_ready_task(scheduler);
    }

    /*
     * Start immediately after the current task.
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

    /*
     * Clear scheduler state.
     */
    vrt_list_init(
        &scheduler->readyQueue);

    scheduler->currentTask = NULL;
    scheduler->idleTask = NULL;

    scheduler->tickCount = 0U;
    scheduler->taskCount = 0U;

    scheduler->running = false;

    /*
     * ------------------------------------------------------------------------
     * Initialize idle task
     * ------------------------------------------------------------------------
     */

    vrt_task_init(
        &vrt_idle_task,
        vrt_idle_task_entry,
        NULL,
        0U,
        vrt_idle_stack,
        VRT_STACK_SIZE,
        "idle");

    /*
     * Reserve task ID 0 for idle.
     */
    vrt_idle_task.id = 0U;

    vrt_idle_task.isIdle = true;

    vrt_idle_task.state =
        VRT_TASK_READY;

    scheduler->idleTask =
        &vrt_idle_task;

    /*
     * Idle is the initial selected task until the scheduler starts.
     */
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

    /*
     * Idle is managed internally.
     */
    if (task->isIdle)
    {
        return false;
    }

    /*
     * taskCount excludes idle.
     */
    if (scheduler->taskCount >= VRT_MAX_TASKS)
    {
        return false;
    }

    /*
     * Only freshly initialized READY tasks can be added.
     */
    if (task->state != VRT_TASK_READY)
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
 *
 * IMPORTANT:
 *
 * This function does NOT save or restore CPU state.
 *
 * It only changes:
 *
 *     task state
 *     scheduler->currentTask
 *
 * The architecture layer is responsible for the actual register/stack
 * transition.
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

    /*
     * Find another runnable task.
     */
    vrt_task_t *next =
        vrt_scheduler_find_next_ready_task(
            scheduler);

    /*
     * ------------------------------------------------------------------------
     * Nothing else is READY
     * ------------------------------------------------------------------------
     */

    if (next == NULL)
    {
        /*
         * Continue the current user task if it is still runnable.
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
     * A different READY task exists
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

    /*
     * If we're switching away from idle, idle remains available conceptually
     * but is not part of the user ready queue.
     */
    if (current == scheduler->idleTask &&
        current != next)
    {
        current->state =
            VRT_TASK_READY;
    }

    /*
     * Select the next task.
     */
    next->state =
        VRT_TASK_RUNNING;

    scheduler->currentTask =
        next;
}

/*
 * ============================================================================
 * Scheduler tick
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

    if (!scheduler->running)
    {
        return;
    }

    /*
     * For now this is the same selection mechanism used by voluntary
     * scheduling.
     *
     * The architecture-specific interrupt/preemption path will be enabled
     * after the voluntary context-switch path is validated.
     */
    vrt_scheduler_schedule(
        scheduler);
}

/*
 * ============================================================================
 * Scheduler start
 * ============================================================================
 *
 * This is the only place that performs the initial architecture bootstrap.
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

    /*
     * Select the first user task.
     */
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
        /*
         * No user task exists -> idle.
         */
        if (scheduler->idleTask == NULL)
        {
            return;
        }

        scheduler->idleTask->state =
            VRT_TASK_RUNNING;

        scheduler->currentTask =
            scheduler->idleTask;
    }

    scheduler->running = true;

    /*
     * ------------------------------------------------------------------------
     * Architecture bootstrap
     * ------------------------------------------------------------------------
     *
     * This does not return.
     */
    vrt_port_start_first_task(
        scheduler->currentTask->sp);

    /*
     * Defensive fallback.
     */
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