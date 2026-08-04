#include "vrt_scheduler.h"

/*=========================================================
 * Private Functions
 *========================================================*/

static vrt_task_t *vrt_scheduler_select_next_task(
    vrt_scheduler_t *scheduler);

static vrt_task_t *vrt_scheduler_get_first_task(
    vrt_scheduler_t *scheduler);

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

static vrt_task_t *vrt_scheduler_get_first_task(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return NULL;
    }

    if (vrt_list_is_empty(&scheduler->readyQueue))
    {
        return NULL;
    }

    vrt_list_node_t *node = scheduler->readyQueue.head;

    return (vrt_task_t *)node->owner;
}

bool vrt_scheduler_add_task(
    vrt_scheduler_t *scheduler,
    vrt_task_t *task)
{
    if (scheduler == NULL || task == NULL)
    {
        return false;
    }

    task->state = VRT_TASK_READY;

    task->node.next = NULL;
    task->node.prev = NULL;

    vrt_list_node_t *current =
        scheduler->readyQueue.head;

    while (current != NULL)
    {
        vrt_task_t *existingTask =
            (vrt_task_t *)current->owner;

        if (task->priority >
            existingTask->priority)
        {
            if (!vrt_list_insert_before(
                    &scheduler->readyQueue,
                    current,
                    &task->node))
            {
                return false;
            }

            scheduler->taskCount++;

            return true;
        }

        current = current->next;
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

static vrt_task_t *vrt_scheduler_select_next_task(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return NULL;
    }

    if (scheduler->currentTask == NULL)
    {
        return NULL;
    }

    vrt_list_node_t *nextNode =
        scheduler->currentTask->node.next;

    if (nextNode == NULL)
    {
        nextNode = scheduler->readyQueue.head;
    }

    return (vrt_task_t *)nextNode->owner;
}

void vrt_scheduler_start(vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    if (vrt_list_is_empty(&scheduler->readyQueue))
    {
        return;
    }

    scheduler->currentTask =
        vrt_scheduler_get_first_task(scheduler);

    if (scheduler->currentTask == NULL)
    {
        return;
    }

    scheduler->currentTask->state = VRT_TASK_RUNNING;
    scheduler->running = true;

    /* TODO: Perform the first context switch */
}

void vrt_scheduler_schedule(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    if (!scheduler->running)
    {
        return;
    }

    if (scheduler->currentTask == NULL)
    {
        return;
    }

    vrt_task_t *nextTask =
        vrt_scheduler_select_next_task(scheduler);

    if (nextTask == NULL)
    {
        return;
    }

    vrt_task_t *current = scheduler->currentTask;

    current->state = VRT_TASK_READY;

    scheduler->currentTask = nextTask;

    nextTask->state = VRT_TASK_RUNNING;
}

void vrt_scheduler_tick(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return;
    }

    if (!scheduler->running)
    {
        return;
    }

    scheduler->tickCount++;

    vrt_scheduler_schedule(scheduler);
}