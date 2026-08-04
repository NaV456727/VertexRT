#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

vrt_scheduler_t scheduler;

vrt_task_t blinkTask;

uint32_t blinkStack[256];

/* Test Task */
void blink(void *argument)
{
    Serial.println("Blink Task Started");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("VertexRT Booting...");

    /* Initialize Scheduler */
    vrt_scheduler_init(&scheduler);

    /* Initialize Task */
    vrt_task_init(
        &blinkTask,
        blink,
        NULL,
        1,
        blinkStack,
        256,
        "Blink");

    /* Register Task */
    if (!vrt_scheduler_add_task(&scheduler, &blinkTask))
    {
        Serial.println("Failed to register task.");
        return;
    }

    Serial.println("Task Registered.");

    /* Start Scheduler */
    vrt_scheduler_start(&scheduler);

    if (scheduler.currentTask != NULL)
    {
        Serial.print("Current Task: ");
        Serial.println(scheduler.currentTask->name);

        /*
         * Temporary:
         * Execute the task manually.
         * Later this will be replaced by the first
         * context switch.
         */
        scheduler.currentTask->entry(
            scheduler.currentTask->argument);
    }
    else
    {
        Serial.println("No current task.");
    }

    Serial.println("Setup Complete.");
}

void loop()
{
    /*
     * Empty for now.
     * The scheduler will eventually take control.
     */
}