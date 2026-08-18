#include "vrt_queue.h"
#include "vrt_scheduler.h"
#include "vrt_freertos_backend.h"

#include <string.h>

/*
 * ============================================================================
 * Internal helpers
 * ============================================================================
 */

static vrt_task_t *
vrt_queue_current_task(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return NULL;
    }

    vrt_task_t *task =
        scheduler->currentTask;

    if (task == NULL ||
        task == scheduler->idleTask ||
        task->isIdle)
    {
        return NULL;
    }

    return task;
}

/*
 * Select the highest-priority READY task.
 */
static vrt_task_t *
vrt_queue_find_highest_ready(
    vrt_scheduler_t *scheduler)
{
    if (scheduler == NULL)
    {
        return NULL;
    }

    vrt_task_t *best =
        NULL;

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
            if (best == NULL ||
                task->priority > best->priority)
            {
                best = task;
            }
        }

        node =
            node->next;
    }

    return best;
}

/*
 * Block the current VertexRT task on a queue wait list.
 *
 * Returns false if there is no replacement task.
 */
static bool
vrt_queue_block_current(
    vrt_scheduler_t *scheduler,
    vrt_list_t *waitQueue)
{
    if (scheduler == NULL ||
        waitQueue == NULL)
    {
        return false;
    }

    vrt_task_t *current =
        vrt_queue_current_task(
            scheduler);

    if (current == NULL)
    {
        return false;
    }

    /*
     * Remove from the READY queue.
     */
    vrt_list_remove(
        &scheduler->readyQueue,
        &current->node);

    /*
     * Mark blocked.
     */
    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Put task on the queue's wait list.
     */
    if (!vrt_list_push_back(
            waitQueue,
            &current->waitNode))
    {
        current->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        return false;
    }

    /*
     * The current task can no longer remain selected.
     */
    scheduler->currentTask =
        NULL;

    /*
     * Find another runnable task.
     */
    vrt_task_t *next =
        vrt_queue_find_highest_ready(
            scheduler);

    /*
     * No replacement task exists.
     *
     * Roll back instead of leaving the system without
     * a runnable backing task.
     */
    if (next == NULL)
    {
        vrt_list_remove(
            waitQueue,
            &current->waitNode);

        current->state =
            VRT_TASK_RUNNING;

        vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        scheduler->currentTask =
            current;

        return false;
    }

    next->state =
        VRT_TASK_RUNNING;

    scheduler->currentTask =
        next;

    /*
     * Perform the actual FreeRTOS-backed switch.
     *
     * This suspends the backing task that called us.
     */
    vrt_freertos_backend_switch_to(
        next);

    return true;
}

/*
 * Wake the first task waiting on a queue.
 */
static void
vrt_queue_wake_one(
    vrt_scheduler_t *scheduler,
    vrt_list_t *waitQueue)
{
    if (scheduler == NULL ||
        waitQueue == NULL ||
        vrt_list_is_empty(waitQueue))
    {
        return;
    }

    vrt_list_node_t *node =
        waitQueue->head;

    if (node == NULL)
    {
        return;
    }

    vrt_task_t *task =
        (vrt_task_t *)node->owner;

    if (task == NULL)
    {
        return;
    }

    /*
     * Remove from queue wait list.
     */
    vrt_list_remove(
        waitQueue,
        &task->waitNode);

    /*
     * Make READY.
     */
    task->state =
        VRT_TASK_READY;

    /*
     * Add to scheduler READY queue.
     */
    if (!vrt_list_push_back(
            &scheduler->readyQueue,
            &task->node))
    {
        /*
         * Restore BLOCKED state if insertion failed.
         */
        task->state =
            VRT_TASK_BLOCKED;

        vrt_list_push_back(
            waitQueue,
            &task->waitNode);

        return;
    }

    /*
     * IMPORTANT:
     *
     * The VertexRT task is READY, but its FreeRTOS backing
     * task may still be suspended because it was previously
     * blocked on this queue.
     *
     * Make the backing task runnable again.
     */
    vrt_freertos_backend_resume_task(
        task);

    /*
     * If the woken task has higher priority than the
     * currently executing task, immediately preempt.
     */
    vrt_task_t *current =
        scheduler->currentTask;

    if (current == NULL ||
        current == scheduler->idleTask ||
        task->priority > current->priority)
    {
        if (current != NULL &&
            current != scheduler->idleTask)
        {
            current->state =
                VRT_TASK_READY;
        }

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
 * Queue initialization
 * ============================================================================
 */

void vrt_queue_init(
    vrt_queue_t *queue,
    void *buffer,
    size_t itemSize,
    size_t capacity)
{
    if (queue == NULL ||
        buffer == NULL ||
        itemSize == 0U ||
        capacity == 0U)
    {
        return;
    }

    queue->buffer =
        (uint8_t *)buffer;

    queue->itemSize =
        itemSize;

    queue->capacity =
        capacity;

    queue->head =
        0U;

    queue->tail =
        0U;

    queue->count =
        0U;

    vrt_list_init(
        &queue->sendWaitQueue);

    vrt_list_init(
        &queue->receiveWaitQueue);
}

/*
 * ============================================================================
 * Queue state
 * ============================================================================
 */

bool vrt_queue_is_empty(
    const vrt_queue_t *queue)
{
    if (queue == NULL)
    {
        return true;
    }

    return queue->count == 0U;
}

bool vrt_queue_is_full(
    const vrt_queue_t *queue)
{
    if (queue == NULL)
    {
        return false;
    }

    return queue->count >=
           queue->capacity;
}

size_t vrt_queue_count(
    const vrt_queue_t *queue)
{
    if (queue == NULL)
    {
        return 0U;
    }

    return queue->count;
}

/*
 * ============================================================================
 * Queue send
 * ============================================================================
 *
 * If the queue is full, the current task blocks until a receiver
 * removes an item.
 * ============================================================================
 */

bool vrt_queue_send(
    vrt_queue_t *queue,
    const void *item)
{
    if (queue == NULL ||
        item == NULL)
    {
        return false;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return false;
    }

    for (;;)
    {
        /*
         * Space available.
         */
        if (!vrt_queue_is_full(queue))
        {
            uint8_t *destination =
                queue->buffer +
                (queue->tail *
                 queue->itemSize);

            memcpy(
                destination,
                item,
                queue->itemSize);

            queue->tail++;

            if (queue->tail >=
                queue->capacity)
            {
                queue->tail = 0U;
            }

            queue->count++;

            /*
             * Wake a receiver waiting for data.
             */
            vrt_queue_wake_one(
                scheduler,
                &queue->receiveWaitQueue);

            return true;
        }

        /*
         * Queue is full.
         *
         * Block the sender until a receiver creates space.
         */
        if (!vrt_queue_block_current(
                scheduler,
                &queue->sendWaitQueue))
        {
            return false;
        }

        /*
         * When this task is resumed, loop back and try
         * the send again.
         */
    }
}

/*
 * ============================================================================
 * Queue receive
 * ============================================================================
 *
 * If the queue is empty, the current task blocks until a sender
 * adds an item.
 * ============================================================================
 */

bool vrt_queue_receive(
    vrt_queue_t *queue,
    void *item)
{
    if (queue == NULL ||
        item == NULL)
    {
        return false;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return false;
    }

    for (;;)
    {
        /*
         * Data available.
         */
        if (!vrt_queue_is_empty(queue))
        {
            uint8_t *source =
                queue->buffer +
                (queue->head *
                 queue->itemSize);

            memcpy(
                item,
                source,
                queue->itemSize);

            queue->head++;

            if (queue->head >=
                queue->capacity)
            {
                queue->head = 0U;
            }

            queue->count--;

            /*
             * Wake a sender waiting for space.
             */
            vrt_queue_wake_one(
                scheduler,
                &queue->sendWaitQueue);

            return true;
        }

        /*
         * Queue is empty.
         *
         * Block the receiver until a sender supplies data.
         */
        if (!vrt_queue_block_current(
                scheduler,
                &queue->receiveWaitQueue))
        {
            return false;
        }

        /*
         * When this task is resumed, retry the receive.
         */
    }
}