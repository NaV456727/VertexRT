#include "vrt_freertos_backend.h"
#include "esp_attr.h"

#include "vrt_scheduler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    vrt_task_t *vrtTask;
    TaskHandle_t handle;
} vrt_freertos_binding_t;

static vrt_freertos_binding_t
    bindings[VRT_MAX_TASKS];

static uint32_t
    binding_count = 0U;

static bool
    backend_initialized = false;

static void
vrt_freertos_task_entry(void *argument)
{
    vrt_task_t *task =
        (vrt_task_t *)argument;

    if (task == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    for (;;)
    {
        /*
         * Wait until VertexRT selects this task.
         */
        while (vrt_scheduler_get_instance()->currentTask != task)
        {
            ulTaskNotifyTake(
                pdTRUE,
                portMAX_DELAY);
        }

        /*
         * VertexRT selected this task.
         */
        task->state =
            VRT_TASK_RUNNING;

        /*
         * Run the VertexRT task entry.
         *
         * The task entry is expected to remain alive.
         */
        if (task->entry != NULL)
        {
            task->entry(task->argument);
        }

        /*
         * If the entry function returns, terminate the
         * backing FreeRTOS task.
         */
        task->state =
            VRT_TASK_TERMINATED;

        vTaskDelete(NULL);
    }
}

bool vrt_freertos_backend_init(void)
{
    binding_count = 0U;

    backend_initialized = true;

    return true;
}

bool vrt_freertos_backend_register_task(
    vrt_task_t *task)
{
    if (!backend_initialized ||
        task == NULL)
    {
        return false;
    }

    if (binding_count >= VRT_MAX_TASKS)
    {
        return false;
    }

    for (uint32_t i = 0;
         i < binding_count;
         ++i)
    {
        if (bindings[i].vrtTask == task)
        {
            return true;
        }
    }

    TaskHandle_t handle = NULL;

    BaseType_t result =
        xTaskCreate(
            vrt_freertos_task_entry,
            task->name,
            2048,
            task,
            1,
            &handle);

    if (result != pdPASS)
    {
        return false;
    }

    bindings[binding_count].vrtTask =
        task;

    bindings[binding_count].handle =
        handle;

    binding_count++;

    return true;
}

void vrt_freertos_backend_start(void)
{
    /*
     * FreeRTOS is already running under Arduino.
     *
     * Nothing needs to be started explicitly here.
     */
}

void IRAM_ATTR vrt_freertos_backend_on_preemption(
    vrt_task_t *next)
{
    if (!backend_initialized ||
        next == NULL)
    {
        return;
    }

    for (uint32_t i = 0;
         i < binding_count;
         ++i)
    {
        if (bindings[i].vrtTask == next)
        {
            BaseType_t higherPriorityTaskWoken =
                pdFALSE;

            vTaskNotifyGiveFromISR(
                bindings[i].handle,
                &higherPriorityTaskWoken);

            if (higherPriorityTaskWoken)
            {
                portYIELD_FROM_ISR();
            }

            return;
        }
    }
}