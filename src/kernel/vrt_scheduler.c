#include "vrt_scheduler.h"
#include "vrt_port.h"
#include "vrt_config.h"

#include <stddef.h>

/*=========================================================
 * Global Scheduler
 *=========================================================*/

static vrt_scheduler_t vrt_scheduler;

/*=========================================================
 * Scheduler Initialization
 *=========================================================*/

void vrt_scheduler_init(vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    vrt_list_init(&scheduler->readyQueue);

    scheduler->currentTask = NULL;
    scheduler->idleTask = NULL;

    scheduler->tickCount = 0;
    scheduler->taskCount = 0;

    scheduler->running = false;
}

/*=========================================================
 * Add Task
 *=========================================================*/

bool vrt_scheduler_add_task(
    vrt_scheduler_t *scheduler,
    vrt_task_t *task)
{
    if (scheduler == NULL || task == NULL)
    {
        return false;
    }

    /*
     * Do not exceed the configured task limit.
     */
    if (scheduler->taskCount >= VRT_MAX_TASKS)
    {
        return false;
    }

    /*
     * Only READY tasks may enter the ready queue.
     */
    if (task->state != VRT_TASK_READY)
    {
        return false;
    }

    /*
     * Assign task ID.
     *
     * IDs are assigned sequentially from zero.
     */
    task->id = scheduler->taskCount;

    /*
     * Insert task into the ready queue.
     */
    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node))
    {
        return false;
    }

    scheduler->taskCount++;

    /*
     * If this is the first task, select it.
     *
     * It remains READY until the scheduler starts.
     */
    if (scheduler->currentTask == NULL)
    {
        scheduler->currentTask = task;
    }

    return true;
}

/*=========================================================
 * Scheduler Selection
 *=========================================================*/

void vrt_scheduler_schedule(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Nothing to schedule.
     */
    if (vrt_list_is_empty(&scheduler->readyQueue))
    {
        scheduler->currentTask = NULL;
        return;
    }

    /*
     * If there is no current task, select the first
     * task in the ready queue.
     */
    if (scheduler->currentTask == NULL)
    {
        vrt_list_node_t *node =
            scheduler->readyQueue.head;

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

        next->state = VRT_TASK_RUNNING;
        scheduler->currentTask = next;

        return;
    }

    /*
     * The current task must be running before it
     * can be rotated.
     */
    if (scheduler->currentTask->state != VRT_TASK_RUNNING)
    {
        scheduler->currentTask->state = VRT_TASK_RUNNING;
        return;
    }

    /*
     * Move current task:
     *
     *     RUNNING -> READY
     *
     * and move it to the back of the ready queue.
     */
    vrt_task_t *current =
        scheduler->currentTask;

    current->state = VRT_TASK_READY;

    if (!vrt_list_remove(
            &scheduler->readyQueue,
            &current->node))
    {
        /*
         * Queue corruption or inconsistent task state.
         *
         * Restore the state rather than continuing.
         */
        current->state = VRT_TASK_RUNNING;
        return;
    }

    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node))
    {
        /*
         * If reinsertion fails, keep the current task
         * marked as running.
         */
        current->state = VRT_TASK_RUNNING;
        return;
    }

    /*
     * The task at the front of the queue is now the
     * next task to run.
     */
    vrt_list_node_t *node =
        scheduler->readyQueue.head;

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
     * Update task states.
     */
    next->state = VRT_TASK_RUNNING;

    scheduler->currentTask = next;

    /*
     * IMPORTANT:
     *
     * At this stage we only select the next task.
     *
     * Actual CPU context switching will eventually
     * happen here through the architecture port.
     */
}

/*=========================================================
 * Scheduler Tick
 *=========================================================*/

void vrt_scheduler_tick(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Increment scheduler tick count.
     */
    scheduler->tickCount++;

    /*
     * Do not schedule until the scheduler has started.
     */
    if (!scheduler->running)
    {
        return;
    }

    /*
     * Every system tick currently represents a
     * scheduling opportunity.
     */
    vrt_scheduler_schedule(scheduler);
}

/*=========================================================
 * Scheduler Start
 *=========================================================*/

void vrt_scheduler_start(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Scheduler is already running.
     */
    if (scheduler->running)
    {
        return;
    }

    /*
     * There must be a task available.
     */
    if (scheduler->currentTask == NULL)
    {
        return;
    }

    /*
     * Mark scheduler as running.
     */
    scheduler->running = true;

    /*
     * The initially selected task becomes RUNNING.
     */
    scheduler->currentTask->state =
        VRT_TASK_RUNNING;

    /*
     * Transfer execution to the architecture-specific
     * first-task startup routine.
     *
     * This function should not return.
     */
    vrt_port_start_first_task();

    /*
     * We intentionally do not put any normal execution
     * after this call. Once the port is implemented,
     * control is transferred directly to the task.
     */
}

/*=========================================================
 * Scheduler Instance
 *=========================================================*/

vrt_scheduler_t *vrt_scheduler_get_instance(void)
{
    return &vrt_scheduler;
}