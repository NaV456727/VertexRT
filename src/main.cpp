#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

/*=========================================================
 * Scheduler
 *=========================================================*/

vrt_scheduler_t scheduler;

/*=========================================================
 * Tasks
 *=========================================================*/

vrt_task_t task1;
vrt_task_t task2;

/*=========================================================
 * Stacks
 *=========================================================*/

uint32_t task1Stack[256];
uint32_t task2Stack[256];

/*=========================================================
 * Task 1
 *=========================================================*/

void task1_entry(void *argument)
{
    (void)argument;

    while (true)
    {
        Serial.println("Task 1 running");

        vrt_task_yield();
    }
}

/*=========================================================
 * Task 2
 *=========================================================*/

void task2_entry(void *argument)
{
    (void)argument;

    while (true)
    {
        Serial.println("Task 2 running");

        vrt_task_yield();
    }
}

/*=========================================================
 * Setup
 *=========================================================*/

void setup()
{
    Serial.begin(115200);

    while (!Serial)
    {
        ;
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("VertexRT Context Switch Test");
    Serial.println("==============================");

    /* Initialize scheduler */
    vrt_scheduler_init(&scheduler);

    /* Initialize task 1 */
    vrt_task_init(
        &task1,
        task1_entry,
        NULL,
        1,
        task1Stack,
        256,
        "Task 1");

    /* Initialize task 2 */
    vrt_task_init(
        &task2,
        task2_entry,
        NULL,
        1,
        task2Stack,
        256,
        "Task 2");

    /* Register tasks */
    if (!vrt_scheduler_add_task(
            &scheduler,
            &task1))
    {
        Serial.println("ERROR: Failed to add Task 1");
        while (true)
        {
        }
    }

    if (!vrt_scheduler_add_task(
            &scheduler,
            &task2))
    {
        Serial.println("ERROR: Failed to add Task 2");
        while (true)
        {
        }
    }

    Serial.println("Tasks created.");
    Serial.println("Starting scheduler...");

    /*
     * This should NOT return.
     *
     * The port starts Task 1 directly from its
     * initialized CPU context.
     */
    vrt_scheduler_start(&scheduler);

    /*
     * We should never reach here.
     */
    Serial.println("ERROR: Scheduler returned!");

    while (true)
    {
    }
}

/*=========================================================
 * Arduino loop
 *=========================================================*/

void loop()
{
    /*
     * We should never reach Arduino loop once
     * VertexRT takes control of the CPU.
     */
}