#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_port.h"
#include "vrt_freertos_backend.h"
#include "vrt_critical.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    task->waitNode.list = NULL;

    /*
     * Ready queue node.
     */
    task->node.owner = task;
    task->node.next = NULL;
    task->node.prev = NULL;
    task->node.list = NULL;

    /*
     * Basic metadata.
     */
    task->id =
        g_next_task_id++;

    if (g_next_task_id == 0U)
    {
        g_next_task_id = 1U;
    }

    task->entry =
        entry;

    task->argument =
        argument;

    task->priority =
        priority;

    task->basePriority =
        priority;

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
    task->wakeTick =
        0U;

    /*
     * ESP32 pxPortInitialiseStack() expects the LAST valid
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

    task->eventWaitBits = 0U;
    task->eventWaitResult = 0U;
    task->eventWaitForAll = false;
    task->eventClearOnExit = false;
    task->timedWaitActive = false;
}

/*
 * ============================================================================
 * Stack monitoring
 * ============================================================================
 */

/*
 * Return the minimum free stack space that has remained for this
 * VertexRT task's FreeRTOS backing task.
 */
size_t vrt_task_stack_high_water_mark(
    const vrt_task_t *task)
{
    return vrt_freertos_backend_stack_free(
        task);
}

/*
 * Return the peak stack usage in bytes.
 *
 * FreeRTOS high-water mark = minimum free stack ever observed.
 * Therefore:
 *
 *     peak used = total stack - minimum free
 */
size_t vrt_task_stack_used(
    const vrt_task_t *task)
{
    if (task == NULL)
    {
        return 0U;
    }

    size_t freeHighWater =
        vrt_task_stack_high_water_mark(
            task);

    size_t totalStackBytes =
        2048U * sizeof(StackType_t);

    if (freeHighWater >= totalStackBytes)
    {
        return 0U;
    }

    return totalStackBytes -
           freeHighWater;
}

/*
 * Return the minimum remaining stack headroom.
 *
 * This is the FreeRTOS high-water-mark value.
 */
size_t vrt_task_stack_free(
    const vrt_task_t *task)
{
    return vrt_task_stack_high_water_mark(
        task);
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

    vrt_freertos_backend_switch_to(
        next);
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
        scheduler->currentTask;

    if (current == NULL ||
        current == scheduler->idleTask ||
        current->isIdle)
    {
        return;
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
     * Select another runnable task.
     */
    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next != NULL &&
        next != current)
    {
        vrt_freertos_backend_exit_current(
            next);
    }

    /*
     * A terminated task must never continue executing.
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
     * Determine whether this is the currently executing task.
     */
    bool isCurrent =
        (scheduler->currentTask == task);

    /*
     * Remove from READY queue if present.
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

    /*
     * Mark the task suspended.
     */
    task->state =
        VRT_TASK_SUSPENDED;

    /*
     * If another task is being suspended, the currently
     * executing task does not need to be switched.
     */
    if (!isCurrent)
    {
        vrt_freertos_backend_suspend_task(
            task);

        return;
    }

    /*
     * Current task is suspending itself.
     */
    scheduler->currentTask =
        NULL;

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL ||
        next == task)
    {
        /*
         * Roll back suspension if no replacement task exists.
         */
        task->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node);

        scheduler->currentTask =
            task;

        return;
    }

    /*
     * Switch to replacement task.
     */
    vrt_freertos_backend_switch_to(
        next);
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
     * Make task READY in VertexRT.
     */
    task->state =
        VRT_TASK_READY;

    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node))
    {
        /*
         * Roll back if READY queue insertion fails.
         */
        task->state =
            VRT_TASK_SUSPENDED;

        return;
    }

    /*
     * Determine currently executing VertexRT task.
     */
    vrt_task_t *current =
        scheduler->currentTask;

    /*
     * If there is no current task, or idle is executing,
     * the resumed task can become current.
     */
    if (current == NULL ||
        current == scheduler->idleTask)
    {
        vrt_scheduler_schedule(
            scheduler);

        if (scheduler->currentTask ==
            task)
        {
            vrt_freertos_backend_switch_to(
                task);
        }

        return;
    }

    /*
     * Only preempt if the resumed task has higher priority.
     */
    if (task->priority >
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
}

/*
 * ============================================================================
 * Task delay
 * ============================================================================
 */

void vrt_task_delay(
    uint32_t ticks)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Zero delay is simply a yield.
     */
    if (ticks == 0U)
    {
        vrt_task_yield();
        return;
    }

    /*
     * Enter the kernel critical section BEFORE reading
     * tickCount or modifying scheduler state.
     */
    vrt_kernel_critical_enter();

    /*
     * Re-read the current task while protected.
     */
    vrt_task_t *task =
        scheduler->currentTask;

    if (task == NULL)
    {
        vrt_kernel_critical_exit();
        return;
    }

    /*
     * Idle task cannot block.
     */
    if (task == scheduler->idleTask ||
        task->isIdle)
    {
        vrt_kernel_critical_exit();
        return;
    }

    /*
     * Calculate wake-up tick atomically.
     */
    task->wakeTick =
        scheduler->tickCount + ticks;

    /*
     * Mark task blocked.
     */
    task->state =
        VRT_TASK_BLOCKED;

    /*
     * Remove task from READY queue.
     */
    if (!vrt_list_remove(
            &scheduler->readyQueue,
            &task->node))
    {
        task->state =
            VRT_TASK_RUNNING;

        vrt_kernel_critical_exit();
        return;
    }

    /*
     * Add task to delayed queue.
     */
    if (!vrt_list_push_back(
            &scheduler->delayedQueue,
            &task->waitNode))
    {
        /*
         * Roll back if delayed queue insertion failed.
         */
        task->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node);

        vrt_kernel_critical_exit();
        return;
    }

    /*
     * The blocked task cannot remain current.
     */
    scheduler->currentTask =
        NULL;

    /*
     * Select highest-priority READY task.
     */
    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    /*
     * No replacement task available.
     *
     * Roll back the complete delay operation.
     */
    if (next == NULL ||
        next == task)
    {
        vrt_list_remove(
            &scheduler->delayedQueue,
            &task->waitNode);

        task->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node);

        scheduler->currentTask =
            task;

        vrt_kernel_critical_exit();
        return;
    }

    /*
     * Leave critical section before physical task switch.
     */
    vrt_kernel_critical_exit();

    vrt_freertos_backend_switch_to(
        next);
}