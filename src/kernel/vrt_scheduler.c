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

/*
 * The idle task runs whenever no user task is READY.
 *
 * For now the idle task simply yields continuously.
 *
 * Once the architecture-specific context switching
 * mechanism is implemented, this will provide the
 * processor with a permanent lowest-priority task.
 */
static void vrt_idle_task_entry(void *argument)
{
    (void)argument;

    while (true)
    {
        vrt_task_yield();
    }
}

/*=========================================================
 * Internal: Select First Ready Task
 *=========================================================*/

/*
 * Find the first READY task in the ready queue.
 *
 * Returns NULL when no user task is READY.
 */
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
 * Internal: Select Next Ready Task
 *=========================================================*/

/*
 * Find the next READY task after the current task.
 *
 * The search wraps around the ready queue.
 */
static vrt_task_t *vrt_scheduler_find_next_ready_task(
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
     * If current task is NULL or is the idle task,
     * simply select the first READY user task.
     */
    if (scheduler->currentTask == NULL ||
        scheduler->currentTask == scheduler->idleTask)
    {
        return vrt_scheduler_find_ready_task(scheduler);
    }

    /*
     * Start searching after the current task.
     */
    vrt_list_node_t *node =
        scheduler->currentTask->node.next;

    /*
     * Search from current->next to the end.
     */
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

    /*
     * Wrap around and search from the beginning.
     */
    node = scheduler->readyQueue.head;

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
 * Initialization
 *=========================================================*/

void vrt_scheduler_init(vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Initialize ready queue.
     *
     * IMPORTANT:
     *
     * The idle task is not inserted into this queue.
     */
    vrt_list_init(&scheduler->readyQueue);

    scheduler->currentTask = NULL;
    scheduler->idleTask = NULL;

    scheduler->tickCount = 0;
    scheduler->taskCount = 0;

    scheduler->running = false;

    /*=====================================================
     * Initialize Idle Task
     *=====================================================*/

    vrt_task_init(
        &vrt_idle_task,
        vrt_idle_task_entry,
        NULL,
        0,
        vrt_idle_stack,
        VRT_STACK_SIZE,
        "idle");

    /*
     * Mark the task as kernel-owned idle task.
     */
    vrt_idle_task.isIdle = true;

    /*
     * Idle task is always available.
     */
    vrt_idle_task.state = VRT_TASK_READY;

    /*
     * ID zero is reserved for the idle task.
     */
    vrt_idle_task.id = 0;

    scheduler->idleTask = &vrt_idle_task;

    /*
     * The scheduler starts with the idle task selected.
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

    /*
     * The idle task cannot be registered as a normal
     * user task.
     */
    if (task->isIdle)
    {
        return false;
    }

    /*
     * Do not exceed the configured USER task limit.
     *
     * The idle task does not count toward this limit.
     */
    if (scheduler->taskCount >= VRT_MAX_TASKS)
    {
        return false;
    }

    /*
     * Only READY tasks can enter the ready queue.
     */
    if (task->state != VRT_TASK_READY)
    {
        return false;
    }

    /*
     * Add task to ready queue.
     */
    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node))
    {
        return false;
    }

    scheduler->taskCount++;

    /*
     * If the scheduler is not running and the idle task
     * is currently selected, the first user task becomes
     * the initial task.
     */
    if (!scheduler->running &&
        scheduler->currentTask == scheduler->idleTask)
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
     * Put the currently running user task back
     * into the READY state.
     *
     * The idle task is never placed in the ready queue.
     */
    if (scheduler->currentTask != NULL &&
        scheduler->currentTask != scheduler->idleTask)
    {
        if (scheduler->currentTask->state ==
            VRT_TASK_RUNNING)
        {
            scheduler->currentTask->state =
                VRT_TASK_READY;
        }
    }

    /*
     * Find the next READY user task.
     */
    vrt_task_t *next =
        vrt_scheduler_find_next_ready_task(scheduler);

    /*
     * No READY user task exists.
     * Run the idle task.
     */
    if (next == NULL)
    {
        if (scheduler->idleTask == NULL)
        {
            scheduler->currentTask = NULL;
            return;
        }

        scheduler->idleTask->state =
            VRT_TASK_RUNNING;

        scheduler->currentTask =
            scheduler->idleTask;

        return;
    }

    /*
     * A user task was found.
     */
    next->state = VRT_TASK_RUNNING;

    scheduler->currentTask = next;
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
     * Increment kernel tick counter.
     */
    scheduler->tickCount++;

    /*
     * Scheduler has not started yet.
     */
    if (!scheduler->running)
    {
        return;
    }

    /*
     * Every tick is currently a scheduling opportunity.
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
     * Do not start twice.
     */
    if (scheduler->running)
    {
        return;
    }

    /*
     * Select a READY user task if one exists.
     *
     * Otherwise the idle task is used.
     */
    vrt_task_t *first =
        vrt_scheduler_find_ready_task(scheduler);

    if (first != NULL)
    {
        scheduler->currentTask = first;
    }
    else
    {
        scheduler->currentTask = scheduler->idleTask;
    }

    if (scheduler->currentTask == NULL)
    {
        return;
    }

    /*
     * Mark scheduler as running.
     */
    scheduler->running = true;

    /*
     * Mark first task as running.
     */
    scheduler->currentTask->state =
        VRT_TASK_RUNNING;

    /*
     * Transfer control to the architecture-specific
     * first-task startup routine.
     *
     * This function is expected not to return.
     */
    vrt_port_start_first_task();
}

/*=========================================================
 * Scheduler Instance
 *=========================================================*/

vrt_scheduler_t *vrt_scheduler_get_instance(void)
{
    return &vrt_scheduler;
}