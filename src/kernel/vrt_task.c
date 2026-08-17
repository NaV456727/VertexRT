#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_port.h"
#include "vrt_freertos_backend.h"

#include <string.h>

/*
 * ============================================================================
 * Private state
 * ============================================================================
 */

static uint32_t g_next_task_id = 1U;

/*
 * ============================================================================
 * Task initialization
 * ============================================================================
 */

void vrt_task_init(
    vrt_task_t *task,
    vrt_task_function_t entry,
    void *argument,
    uint8_t priority,
    uint32_t *stackStart,
    uint32_t stackSize,
    const char *name)
{
    if (task == NULL ||
        entry == NULL ||
        stackStart == NULL ||
        stackSize == 0U ||
        name == NULL)
    {
        return;
    }

    memset(
        task,
        0,
        sizeof(*task));

    /*
     * Wait/delay node.
     */
    task->waitNode.owner = task;
    task->waitNode.next = NULL;
    task->waitNode.prev = NULL;

    /*
     * Ready queue node.
     */
    task->node.owner = task;
    task->node.next = NULL;
    task->node.prev = NULL;

    /*
     * Basic metadata.
     */
    task->id =
        g_next_task_id++;

    if (g_next_task_id == 0U)
    {
        g_next_task_id = 1U;
    }

    task->entry = entry;
    task->argument = argument;
    task->priority = priority;

    task->state =
        VRT_TASK_READY;

    task->isIdle =
        false;

    task->stackStart =
        stackStart;

    task->stackSize =
        stackSize;

    task->stackEnd =
        stackStart + stackSize;

    /*
     * No delay initially.
     */
    task->wakeTick = 0U;

    /*
     * ESP32 FreeRTOS pxPortInitialiseStack() expects the LAST valid
     * stack word.
     */
    uint32_t *stackTop =
        task->stackEnd - 1U;

    task->sp =
        vrt_port_stack_init(
            stackTop,
            task->entry,
            task->argument);

    if (task->sp == NULL)
    {
        task->state =
            VRT_TASK_TERMINATED;

        return;
    }

    /*
     * Task name.
     */
    strncpy(
        task->name,
        name,
        VRT_TASK_NAME_LENGTH - 1U);

    task->name[VRT_TASK_NAME_LENGTH - 1U] =
        '\0';
}

/*
 * ============================================================================
 * Task yield
 * ============================================================================
 */

void vrt_task_yield(void)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL ||
        !scheduler->running)
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
     * A hardware tick may have requested preemption.
     *
     * Clear the request now because we are entering the safe
     * architecture-level context-switch boundary.
     */
    if (vrt_scheduler_preemption_pending(
            scheduler))
    {
        vrt_scheduler_clear_preemption(
            scheduler);
    }

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL ||
        next == current)
    {
        return;
    }

    vrt_port_switch_context(
        &current->sp,
        next->sp);
}

/*
 * ============================================================================
 * Task exit
 * ============================================================================
 */

void vrt_task_exit(void)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    if (current == NULL ||
        current == scheduler->idleTask ||
        current->isIdle)
    {
        return;
    }

    /*
     * ------------------------------------------------------------------------
     * Mark terminated.
     * ------------------------------------------------------------------------
     */

    current->state =
        VRT_TASK_TERMINATED;

    /*
     * ------------------------------------------------------------------------
     * Remove from ready queue.
     * ------------------------------------------------------------------------
     *
     * A running task remains in readyQueue in the current scheduler model.
     */

    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    /*
     * Keep task count consistent.
     */
    if (scheduler->taskCount > 0U)
    {
        scheduler->taskCount--;
    }

    scheduler->currentTask =
        current;

    /*
     * ------------------------------------------------------------------------
     * Select another runnable task.
     * ------------------------------------------------------------------------
     *
     * scheduler->currentTask still points to the terminated task,
     * so scheduler_schedule() will select another READY task.
     */

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    /*
     * ------------------------------------------------------------------------
     * Leave the current FreeRTOS backing task.
     * ------------------------------------------------------------------------
     */

    if (next != NULL &&
        next != current)
    {
        vrt_freertos_backend_exit_current(
            next);
    }

    /*
     * Should never return.
     */
    for (;;)
    {
    }
}

/*
 * ============================================================================
 * Task suspend
 * ============================================================================
 */

void vrt_task_suspend(
    vrt_task_t *task)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL ||
        task == NULL)
    {
        return;
    }

    /*
     * Idle task cannot be suspended.
     */
    if (task == scheduler->idleTask ||
        task->isIdle)
    {
        return;
    }

    /*
     * Already suspended.
     */
    if (task->state ==
        VRT_TASK_SUSPENDED)
    {
        return;
    }

    /*
     * Remove from ready queue if present.
     */
    if (task->state ==
            VRT_TASK_READY ||
        task->state ==
            VRT_TASK_RUNNING)
    {
        vrt_list_remove(
            &scheduler->readyQueue,
            &task->node);
    }

    task->state =
        VRT_TASK_SUSPENDED;

    /*
     * Suspend the actual FreeRTOS backing task.
     */
    vrt_freertos_backend_suspend_task(
        task);

    /*
     * If the suspended task was the currently
     * executing VertexRT task, select another task.
     */
    if (scheduler->currentTask == task)
    {
        vrt_task_t *previous =
            task;

        (void)previous;

        /*
         * Clear current selection temporarily so
         * scheduler_schedule() searches for another
         * READY task instead of retaining this task.
         */
        scheduler->currentTask =
            scheduler->idleTask;

        scheduler->idleTask->state =
            VRT_TASK_RUNNING;

        vrt_scheduler_schedule(
            scheduler);

        if (scheduler->currentTask != NULL &&
            scheduler->currentTask != task &&
            scheduler->currentTask !=
                scheduler->idleTask)
        {
            vrt_freertos_backend_switch_to(
                scheduler->currentTask);
        }
    }
}

/*
 * ============================================================================
 * Task resume
 * ============================================================================
 */

void vrt_task_resume(
    vrt_task_t *task)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL ||
        task == NULL)
    {
        return;
    }

    if (task->state !=
        VRT_TASK_SUSPENDED)
    {
        return;
    }

    /*
     * Make the task READY again.
     */
    task->state =
        VRT_TASK_READY;

    vrt_list_push_back(
        &scheduler->readyQueue,
        &task->node);

    /*
     * Make the backing FreeRTOS task runnable.
     */
    vrt_freertos_backend_resume_task(
        task);

    /*
     * If the scheduler currently has no user task,
     * allow the resumed task to become current.
     */
    if (scheduler->currentTask ==
            NULL ||
        scheduler->currentTask ==
            scheduler->idleTask)
    {
        vrt_scheduler_schedule(
            scheduler);

        if (scheduler->currentTask ==
            task)
        {
            vrt_freertos_backend_switch_to(
                task);
        }
    }
}

/*
 * ============================================================================
 * Task delay
 * ============================================================================
 *
 * Block the current task until:
 *
 *     scheduler->tickCount + ticks
 *
 * For this cooperative implementation, another task is responsible for
 * advancing the tick.
 * ============================================================================
 */

void vrt_task_delay(
    uint32_t ticks)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL ||
        scheduler->currentTask == NULL)
    {
        return;
    }

    vrt_task_t *task =
        vrt_freertos_backend_get_current_task();

    if (task == NULL)
    {
        return;
    }

    scheduler->currentTask =
        task;

    /*
     * Zero-delay is simply a normal yield.
     */
    if (ticks == 0U)
    {
        vrt_task_yield();
        return;
    }

    /*
     * Do not allow the idle task to block.
     */
    if (task == scheduler->idleTask ||
        task->isIdle)
    {
        return;
    }

    /*
     * Record the wake-up tick.
     */
    task->wakeTick =
        scheduler->tickCount + ticks;

    /*
     * Mark the task blocked.
     */
    task->state =
        VRT_TASK_BLOCKED;

    /*
     * Remove from the ready queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &task->node);

    /*
     * Add to delayed queue.
     */
    vrt_list_push_back(
        &scheduler->delayedQueue,
        &task->waitNode);

    vrt_scheduler_schedule(
        scheduler);

    vrt_freertos_backend_switch_to(
        scheduler->currentTask);

    vrt_freertos_backend_block_current();
}