#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_port.h"

#include <string.h>

/*=========================================================
 * Private Variables
 *========================================================*/

/*
 * Simple monotonically increasing task ID.
 *
 * Task IDs start at 1.
 * ID 0 is reserved as an invalid/unassigned ID.
 */
static uint32_t g_next_task_id = 1U;

/*=========================================================
 * Task Initialization
 *========================================================*/

void vrt_task_init(
    vrt_task_t *task,
    vrt_task_function_t entry,
    void *argument,
    uint8_t priority,
    uint32_t *stackStart,
    uint32_t stackSize,
    const char *name)
{

    task->waitNode.owner = task;
    task->waitNode.next = NULL;
    task->waitNode.prev = NULL;

    if (task == NULL ||
        entry == NULL ||
        stackStart == NULL ||
        stackSize == 0U ||
        name == NULL)
    {
        return;
    }

    /*-----------------------------------------------------
     * Task Identity
     *-----------------------------------------------------*/

    task->id = g_next_task_id++;

    /*
     * Prevent ID 0 from ever being assigned if the
     * counter wraps around.
     */
    if (g_next_task_id == 0U)
    {
        g_next_task_id = 1U;
    }

    /*-----------------------------------------------------
     * Task Entry
     *-----------------------------------------------------*/

    task->entry = entry;
    task->argument = argument;

    /*-----------------------------------------------------
     * Scheduling Information
     *-----------------------------------------------------*/

    task->priority = priority;
    task->state = VRT_TASK_READY;

    /*-----------------------------------------------------
     * Stack Information
     *-----------------------------------------------------*/

    task->stackStart = stackStart;
    task->stackSize = stackSize;

    /*
     * stackSize is expressed in uint32_t words.
     *
     * Example:
     *
     *     uint32_t stack[1024];
     *
     *     stackStart = stack
     *     stackSize  = 1024
     *
     * Therefore stackEnd points one element past
     * the usable stack memory.
     */
    task->stackEnd = stackStart + stackSize;

    /*-----------------------------------------------------
     * Initial CPU Context
     *-----------------------------------------------------*/

    task->sp = vrt_port_stack_init(
        task->stackEnd,
        task->entry,
        task->argument);

    if (task->sp == NULL)
    {
        task->state = VRT_TASK_TERMINATED;
        return;
    }

    /*-----------------------------------------------------
     * Scheduler List Node
     *-----------------------------------------------------*/

    task->node.owner = task;
    task->node.next = NULL;
    task->node.prev = NULL;

    /*-----------------------------------------------------
     * Task Name
     *-----------------------------------------------------*/

    strncpy(
        task->name,
        name,
        VRT_TASK_NAME_LENGTH - 1U);

    task->name[VRT_TASK_NAME_LENGTH - 1U] = '\0';
}

/*=========================================================
 * Task Yield
 *=========================================================*/

void vrt_task_yield(void)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Ask the scheduler to select the next runnable task.
     *
     * The actual CPU context switch will eventually be
     * performed by the architecture-specific port layer.
     */
    vrt_scheduler_schedule(scheduler);
}

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

    if (task == NULL)
    {
        return;
    }

    if (task == scheduler->idleTask)
    {
        return;
    }

    /*
     * The task is no longer runnable.
     */
    task->state = VRT_TASK_TERMINATED;

    /*
     * Remove it from the ready queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &task->node);

    /*
     * This task is no longer an active task.
     */
    if (scheduler->taskCount > 0)
    {
        scheduler->taskCount--;
    }

    /*
     * There is currently no running task.
     */
    scheduler->currentTask = NULL;

    /*
     * Ask the scheduler to select another task.
     */
    vrt_scheduler_schedule(scheduler);
}

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

    /*
     * Remove the task from the ready queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &task->node);

    /*
     * Mark it as suspended.
     */
    task->state = VRT_TASK_SUSPENDED;

    /*
     * If the current task suspended itself,
     * there is no longer a running task.
     */
    if (scheduler->currentTask == task &&
        task->state == VRT_TASK_SUSPENDED)
    {
        scheduler->currentTask = NULL;

        vrt_scheduler_schedule(scheduler);
    }
}

void vrt_task_resume(vrt_task_t *task)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL || task == NULL)
    {
        return;
    }

    /*
     * Only suspended tasks can be resumed.
     */
    if (task->state != VRT_TASK_SUSPENDED)
    {
        return;
    }

    /*
     * Put it back into the ready queue first.
     */
    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node))
    {
        return;
    }

    /*
     * Only mark the task READY after
     * successful insertion.
     */
    task->state = VRT_TASK_READY;
}