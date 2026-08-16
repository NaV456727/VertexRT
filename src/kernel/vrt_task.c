#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_port.h"

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
        for (;;)
        {
        }
    }

    vrt_task_t *current =
        scheduler->currentTask;

    if (current == NULL ||
        current == scheduler->idleTask)
    {
        for (;;)
        {
        }
    }

    /*
     * Mark terminated.
     */
    current->state =
        VRT_TASK_TERMINATED;

    /*
     * Remove from ready queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    if (scheduler->taskCount > 0U)
    {
        scheduler->taskCount--;
    }

    /*
     * Tell scheduler there is no currently-running task.
     */
    scheduler->currentTask =
        NULL;

    /*
     * Select another runnable task.
     */
    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL)
    {
        for (;;)
        {
        }
    }

    /*
     * Use the already-proven context switch path.
     *
     * The terminated task's context may be saved, but it can never
     * be scheduled again.
     */
    vrt_port_switch_context(
        &current->sp,
        next->sp);

    /*
     * Should never execute.
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

void vrt_task_suspend(vrt_task_t *task)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL ||
        task == NULL)
    {
        return;
    }

    if (task->state != VRT_TASK_READY &&
        task->state != VRT_TASK_RUNNING)
    {
        return;
    }

    /*
     * Remove from ready queue.
     *
     * This is only valid if the task is actually linked there.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &task->node);

    task->state =
        VRT_TASK_SUSPENDED;

    /*
     * If this is the current task, switch away from it.
     */
    if (scheduler->currentTask == task)
    {
        scheduler->currentTask =
            NULL;

        vrt_scheduler_schedule(
            scheduler);

        vrt_task_t *next =
            scheduler->currentTask;

        if (next != NULL &&
            next != task)
        {
            vrt_port_switch_context(
                &task->sp,
                next->sp);
        }
    }
}

/*
 * ============================================================================
 * Task resume
 * ============================================================================
 */

void vrt_task_resume(vrt_task_t *task)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL ||
        task == NULL)
    {
        return;
    }

    if (task->state != VRT_TASK_SUSPENDED)
    {
        return;
    }

    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node))
    {
        return;
    }

    task->state =
        VRT_TASK_READY;
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
        !scheduler->running)
    {
        return;
    }

    vrt_task_t *current =
        scheduler->currentTask;

    if (current == NULL ||
        current == scheduler->idleTask)
    {
        return;
    }

    /*
     * A zero-tick delay is simply a yield.
     */
    if (ticks == 0U)
    {
        vrt_task_yield();
        return;
    }

    /*
     * Remove current task from runnable queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    /*
     * Compute wake-up tick.
     *
     * Unsigned wraparound is intentional and the tick comparison in
     * vrt_scheduler_tick() uses signed-difference semantics.
     */
    current->wakeTick =
        scheduler->tickCount + ticks;

    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Put task on delayed queue.
     */
    if (!vrt_list_push_back(
            &scheduler->delayedQueue,
            &current->waitNode))
    {
        /*
         * If insertion failed, restore runnable state.
         */
        current->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        return;
    }

    /*
     * Current task is no longer runnable.
     */
    scheduler->currentTask =
        NULL;

    /*
     * Select another runnable task.
     */
    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL)
    {
        /*
         * Idle should always be available.
         */
        for (;;)
        {
        }
    }

    /*
     * Save current context and switch to next.
     */
    vrt_port_switch_context(
        &current->sp,
        next->sp);
}