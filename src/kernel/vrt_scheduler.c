#include "vrt_scheduler.h"
#include "vrt_port.h"
#include "vrt_config.h"

#include <stddef.h>
#include <stdbool.h>

/*=========================================================
 * Global Scheduler
 *=========================================================*/

static vrt_scheduler_t vrt_scheduler;

/*=========================================================
 * Idle Task
 *=========================================================*/

static vrt_task_t vrt_idle_task;
static uint32_t vrt_idle_stack[VRT_STACK_SIZE];

/*=========================================================
 * Idle Task Function
 *=========================================================*/

static void vrt_idle_task_entry(void *argument)
{
    (void)argument;

    while (true)
    {
        /*
         * The idle task should not continuously perform
         * an architecture context switch.
         *
         * Just remain alive.
         */
    }
}

/*=========================================================
 * Find First READY Task
 *=========================================================*/

static vrt_task_t *vrt_scheduler_find_ready_task(
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

/*=========================================================
 * Find Next READY Task
 *=========================================================*/

static vrt_task_t *vrt_scheduler_find_next_ready_task(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL ||
        scheduler->readyQueue.head == NULL)
    {
        return NULL;
    }

    /*
     * No current task or currently idle:
     * start from the head of the ready queue.
     */
    if (scheduler->currentTask == NULL ||
        scheduler->currentTask == scheduler->idleTask)
    {
        return vrt_scheduler_find_ready_task(scheduler);
    }

    /*
     * The current task may be RUNNING and may not have a node
     * in the ready queue. Therefore do NOT use:
     *
     *     currentTask->node.next
     *
     * Start from the queue head and find the first READY task
     * that is different from the current task.
     */
    vrt_list_node_t *node =
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

/*=========================================================
 * Initialization
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

    scheduler->tickCount = 0U;
    scheduler->taskCount = 0U;

    scheduler->running = false;

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

    vrt_idle_task.isIdle = true;

    /*
     * Idle task is always available.
     */
    vrt_idle_task.state = VRT_TASK_READY;

    /*
     * Reserve ID 0 for idle.
     */
    vrt_idle_task.id = 0U;

    scheduler->idleTask = &vrt_idle_task;

    /*
     * Before the scheduler starts, idle is selected.
     */
    scheduler->currentTask = scheduler->idleTask;
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

    if (task->isIdle)
    {
        return false;
    }

    if (scheduler->taskCount >= VRT_MAX_TASKS)
    {
        return false;
    }

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

    /*
     * Do NOT change currentTask here.
     *
     * Scheduler start is responsible for selecting
     * the first task.
     */
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

    vrt_task_t *current =
        scheduler->currentTask;

    vrt_task_t *next =
        vrt_scheduler_find_next_ready_task(scheduler);

    /*
     * No different READY task exists.
     */
    if (next == NULL)
    {
        /*
         * Keep the current task running if it is still valid.
         */
        if (current != NULL &&
            current != scheduler->idleTask &&
            current->state == VRT_TASK_RUNNING)
        {
            return;
        }

        /*
         * Otherwise fall back to idle.
         */
        if (scheduler->idleTask != NULL)
        {
            if (current != NULL &&
                current != scheduler->idleTask &&
                current->state == VRT_TASK_RUNNING)
            {
                current->state = VRT_TASK_READY;
            }

            scheduler->idleTask->state =
                VRT_TASK_RUNNING;

            scheduler->currentTask =
                scheduler->idleTask;
        }

        return;
    }

    /*
     * Move the old task back to READY only if it was
     * actually running and is not idle.
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
     * Select the new task.
     */
    next->state =
        VRT_TASK_RUNNING;

    scheduler->currentTask =
        next;
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

    scheduler->tickCount++;

    if (!scheduler->running)
    {
        return;
    }
    /*
     * Preemptive switching is not implemented yet.
     *
     * Only account for the tick.
     */
    return;
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

    if (scheduler->running)
    {
        return;
    }

    /*
     * Select the first runnable task.
     */
    vrt_task_t *first =
        vrt_scheduler_find_ready_task(scheduler);

    if (first != NULL)
    {
        scheduler->currentTask = first;
        first->state = VRT_TASK_RUNNING;
    }
    else
    {
        /*
         * No user task exists.
         * Start the idle task instead.
         */
        if (scheduler->idleTask == NULL)
        {
            return;
        }

        scheduler->currentTask =
            scheduler->idleTask;

        scheduler->idleTask->state =
            VRT_TASK_RUNNING;
    }

    scheduler->running = true;

    /*
     * Architecture bootstrap.
     *
     * Does not return.
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

/*=========================================================
 * Scheduler Instance
 *=========================================================*/

vrt_scheduler_t *vrt_scheduler_get_instance(void)
{
    return &vrt_scheduler;
}