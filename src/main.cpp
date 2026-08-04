#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

/*=========================================================
 * Global Objects
 *========================================================*/

vrt_scheduler_t scheduler;

vrt_task_t blinkTask;

uint32_t blinkStack[256];

/*=========================================================
 * Task Functions
 *========================================================*/

void blink(void *argument)
{
    Serial.println("Blink Task Running");
}

/*=========================================================
 * Arduino Setup
 *========================================================*/

void setup()
{
    Serial.begin(115200);

    while (!Serial)
    {
        ;
    }

    Serial.println();
    Serial.println("=================================");
    Serial.println("VertexRT Scheduler Tick Test");
    Serial.println("=================================");

    /* Initialize scheduler */
    vrt_scheduler_init(&scheduler);

    /* Initialize task */
    vrt_task_init(
        &blinkTask,
        blink,
        NULL,
        1,
        blinkStack,
        256,
        "Blink Task");

    /* Register task */
    if (!vrt_scheduler_add_task(
            &scheduler,
            &blinkTask))
    {
        Serial.println("ERROR: Failed to register task.");

        while (1)
        {
        }
    }

    /* Start scheduler */
    vrt_scheduler_start(&scheduler);

    Serial.print("Current Task: ");
    Serial.println(scheduler.currentTask->name);

    Serial.println();
    Serial.println("Starting Tick Simulation...");
}

/*=========================================================
 * Arduino Loop
 *========================================================*/

void loop()
{
    delay(1000);

    vrt_scheduler_tick(&scheduler);

    Serial.print("Tick: ");
    Serial.print(scheduler.tickCount);

    Serial.print(" | Running: ");
    Serial.println(scheduler.currentTask->name);

    /* Temporary
       Execute current task manually.
       Later this will happen through
       context switching.
    */
    scheduler.currentTask->entry(
        scheduler.currentTask->argument);
}