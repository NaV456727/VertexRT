#include "vrt_task.h"
#include "vrt_scheduler.h"

#include <string.h>

void vrt_task_init(
    vrt_task_t *task,
    vrt_task_function_t entry,
    void *argument,
    uint8_t priority,
    uint32_t *stackStart,
    uint32_t stackSize,
    const char *name)

{
    if (task == NULL || entry == NULL || stackStart == NULL || name == NULL)
    {
        return;
    }

    task->id = 0; // Assign a unique ID as needed

    task->entry = entry;

    task->argument = argument;

    task->priority = priority;

    task->state = VRT_TASK_READY;

    task->sp = stackStart;
    task->stackStart = stackStart;
    task->stackEnd = stackStart + stackSize - 1;
    task->stackSize = stackSize;

    task->node.owner = task;
    task->node.next = NULL;
    task->node.prev = NULL;

    strncpy(task->name, name, VRT_TASK_NAME_LENGTH - 1);
    task->name[VRT_TASK_NAME_LENGTH - 1] = '\0';
}

void vrt_task_yield(void)
{
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    vrt_scheduler_schedule(scheduler);
}