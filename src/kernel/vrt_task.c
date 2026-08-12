#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_port.h"

#include <string.h>

/*=========================================================
 * Private Variables
 *=========================================================*/

/*
 * Simple monotonically increasing task ID.
 *
 * Task IDs start at 1.
 * ID 0 is reserved for the idle task.
 */
static uint32_t g_next_task_id = 1U;

/*=========================================================
 * Task Initialization
 *=========================================================*/

void vrt_task_init(
    vrt_task_t *task,
    vrt_task_function_t entry,
    void *argument,
    uint8_t priority,
    uint32_t *stackStart,
    uint32_t stackSize,
    const char *name)
{
    if (task == NULL || entry == NULL || stackStart == NULL ||
        stackSize == 0U || name == NULL)
    {
        return;
    }

    task->waitNode.owner = task;
    task->waitNode.next = NULL;
    task->waitNode.prev = NULL;

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

    task->sp = vrt_port_stack_init(task->stackEnd, task->entry, task->argument);

    if (task->sp == NULL)
    {
        task->state = VRT_TASK_TERMINATED;
        return;
    }

    task->node.owner = task;
    task->node.next = NULL;
    task->node.prev = NULL;

    strncpy(task->name, name, VRT_TASK_NAME_LENGTH - 1U);
    task->name[VRT_TASK_NAME_LENGTH - 1U] = '\0';
}

/*=========================================================
 * Task Yield
 *=========================================================*/

void vrt_task_yield(void)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL || !scheduler->running)
    {
        return;
    }

    vrt_task_t *self =
        scheduler->currentTask;

    if (self == NULL)
    {
        return;
    }

    /*
     * Scheduler selection happens in C.
     *
     * The architecture layer then saves self->sp and
     * restores scheduler->currentTask->sp.
     */
    vrt_scheduler_schedule(scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL || next == self)
    {
        return;
    }

    vrt_port_switch_context(
        &self->sp,
        next->sp);
}

/*=========================================================
 * Task Exit
 *=========================================================*/

void vrt_task_exit(void)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    vrt_task_t *task =
        scheduler->currentTask;

    if (task == NULL || task == scheduler->idleTask)
    {
        return;
    }

    task->state =
        VRT_TASK_TERMINATED;

    vrt_list_remove(
        &scheduler->readyQueue,
        &task->node);

    if (scheduler->taskCount > 0U)
    {
        scheduler->taskCount--;
    }

    /*
     * The current task is no longer runnable.
     */
    scheduler->currentTask = NULL;

    /*
     * Select another runnable task.
     *
     * If none exists, scheduler_schedule() selects idle.
     */
    vrt_scheduler_schedule(scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    if (next == NULL)
    {
        /*
         * There is nowhere valid to go.
         * This should never happen because the idle task exists.
         */
        for (;;)
        {
        }
    }

    /*
     * Do NOT save the terminating task's context.
     *
     * Directly restore the next task.
     */
    vrt_port_restore_context(next->sp);
}

/*=========================================================
 * Task Suspend
 *=========================================================*/

void vrt_task_suspend(vrt_task_t *task)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL || task == NULL)
    {
        return;
    }

    if (task->state != VRT_TASK_READY &&
        task->state != VRT_TASK_RUNNING)
    {
        return;
    }

    if (task != scheduler->currentTask)
    {
        vrt_list_remove(
            &scheduler->readyQueue,
            &task->node);
    }

    task->state = VRT_TASK_SUSPENDED;

    if (scheduler->currentTask == task)
    {
        scheduler->currentTask = NULL;
        vrt_scheduler_schedule(scheduler);

        vrt_task_t *next =
            scheduler->currentTask;

        if (next != NULL)
        {
            vrt_port_restore_context(next->sp);
        }

        for (;;)
        {
        }
    }
}

/*=========================================================
 * Task Resume
 *=========================================================*/

void vrt_task_resume(vrt_task_t *task)
{
    vrt_scheduler_t *scheduler = vrt_scheduler_get_instance();

    if (scheduler == NULL || task == NULL)
    {
        return;
    }
    if (task->state != VRT_TASK_SUSPENDED)
    {
        return;
    }
    if (!vrt_list_push_back(&scheduler->readyQueue, &task->node))
    {
        return;
    }
    task->state = VRT_TASK_READY;
}