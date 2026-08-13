#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_port.h"

#include <string.h>

/* ============================================================================
 * Private state
 * ========================================================================== */

static uint32_t g_next_task_id = 1U;

/* ============================================================================
 * Task initialization
 * ========================================================================== */

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

    memset(task, 0, sizeof(*task));

    /*
     * Wait-list node.
     */
    task->waitNode.owner = task;
    task->waitNode.next = NULL;
    task->waitNode.prev = NULL;

    /*
     * Ready-queue node.
     */
    task->node.owner = task;
    task->node.next = NULL;
    task->node.prev = NULL;

    /*
     * Basic task metadata.
     */
    task->id = g_next_task_id++;

    if (g_next_task_id == 0U)
    {
        g_next_task_id = 1U;
    }

    task->entry = entry;
    task->argument = argument;
    task->priority = priority;

    task->state = VRT_TASK_READY;
    task->isIdle = false;

    task->stackStart = stackStart;
    task->stackSize = stackSize;
    task->stackEnd = stackStart + stackSize;

    /*
     * ESP32 FreeRTOS pxPortInitialiseStack() expects a pointer to the
     * LAST valid stack word, not the one-past-the-end pointer.
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
        task->state = VRT_TASK_TERMINATED;
        return;
    }

    /*
     * Task name.
     */
    strncpy(
        task->name,
        name,
        VRT_TASK_NAME_LENGTH - 1U);

    task->name[VRT_TASK_NAME_LENGTH - 1U] = '\0';
}

/* ============================================================================
 * Task yield
 * ========================================================================== */

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
     * The scheduler only selects the next task.
     *
     * It does not touch CPU context.
     */
    vrt_scheduler_schedule(scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL ||
        next == current)
    {
        return;
    }

    /*
     * IMPORTANT:
     *
     * The assembly routine saves the current task's XtSolFrame through
     * &current->sp and restores next->sp.
     *
     * It does not return to this C call site immediately; the old task
     * continues here only when another task later switches back to it.
     */
    vrt_port_switch_context(
        &current->sp,
        next->sp);
}

/* ============================================================================
 * Task exit
 * ========================================================================== */

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
        /*
         * The idle task must never terminate.
         */
        for (;;)
        {
        }
    }

    /*
     * Mark the task dead before scheduler selection.
     */
    current->state =
        VRT_TASK_TERMINATED;

    /*
     * It must no longer be present in the ready queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    if (scheduler->taskCount > 0U)
    {
        scheduler->taskCount--;
    }

    /*
     * There is no valid context to save for the terminating task.
     */
    scheduler->currentTask = NULL;

    /*
     * Pick another runnable task.
     */
    vrt_scheduler_schedule(scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL)
    {
        /*
         * This should be impossible because idle exists.
         */
        for (;;)
        {
        }
    }

    /*
     * IMPORTANT:
     *
     * Do NOT call vrt_port_switch_context().
     *
     * The terminating task's context must never be saved.
     */
    vrt_port_restore_context(next->sp);

    /*
     * Defensive fallback.
     *
     * vrt_port_restore_context() is non-returning in the normal case.
     */
    for (;;)
    {
    }
}

/* ============================================================================
 * Task suspend
 * ========================================================================== */

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
     * Remove from the runnable queue first.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &task->node);

    task->state =
        VRT_TASK_SUSPENDED;

    /*
     * If another task is being suspended, nothing more is required.
     *
     * If this is the current task, force a scheduling decision.
     */
    if (scheduler->currentTask == task)
    {
        vrt_scheduler_schedule(scheduler);

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

/* ============================================================================
 * Task resume
 * ========================================================================== */

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