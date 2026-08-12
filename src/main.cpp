#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

static vrt_task_t taskA;
static vrt_task_t taskB;

static uint32_t stackA[VRT_STACK_SIZE];
static uint32_t stackB[VRT_STACK_SIZE];

static volatile uint32_t countA = 0;
static volatile uint32_t countB = 0;

/*
 * Keep the first context-switch test completely independent
 * of Arduino's underlying FreeRTOS scheduler.
 *
 * No delay()
 * No vTaskDelay()
 * No Arduino yield()
 *
 * Just burn some CPU cycles and explicitly yield through VertexRT.
 */

static void vertex_delay_cycles(uint32_t cycles)
{
    for (volatile uint32_t i = 0; i < cycles; ++i)
    {
        __asm__ volatile("nop");
    }
}

/* =========================================================
 * Task A
 * ========================================================= */

static void task_a(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========== TASK A STARTED ==========");

    for (;;)
    {
        ++countA;

        Serial.print("A: ");
        Serial.println(countA);

        vertex_delay_cycles(300000);

        Serial.println("A: yielding");

        vrt_task_yield();
    }
}

/* =========================================================
 * Task B
 * ========================================================= */

static void task_b(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========== TASK B STARTED ==========");

    for (;;)
    {
        ++countB;

        Serial.print("B: ");
        Serial.println(countB);

        vertex_delay_cycles(300000);

        Serial.println("B: yielding");

        vrt_task_yield();
    }
}

/* =========================================================
 * Setup
 * ========================================================= */

void setup()
{
    Serial.begin(115200);

    /*
     * Give the serial peripheral time to initialise.
     * This occurs before VertexRT starts, so it does not
     * affect the task context-switch test.
     */
    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("VERTEXRT TWO-TASK SWITCH TEST");
    Serial.println("====================================");

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == nullptr)
    {
        Serial.println("ERROR: scheduler instance is NULL.");

        for (;;)
        {
        }
    }

    /* -----------------------------------------------------
     * Scheduler initialization
     * ----------------------------------------------------- */

    Serial.println("Initializing scheduler...");

    vrt_scheduler_init(scheduler);

    Serial.println("Scheduler initialized.");

    /* -----------------------------------------------------
     * Task A initialization
     * ----------------------------------------------------- */

    Serial.println("Initializing Task A...");

    vrt_task_init(
        &taskA,
        task_a,
        nullptr,
        1,
        stackA,
        VRT_STACK_SIZE,
        "taskA");

    if (taskA.sp == nullptr)
    {
        Serial.println("ERROR: Task A stack initialization failed.");

        for (;;)
        {
        }
    }

    Serial.printf(
        "Task A SP = %p\n",
        (void *)taskA.sp);

    Serial.println("Task A initialized.");

    /* -----------------------------------------------------
     * Task B initialization
     * ----------------------------------------------------- */

    Serial.println("Initializing Task B...");

    vrt_task_init(
        &taskB,
        task_b,
        nullptr,
        1,
        stackB,
        VRT_STACK_SIZE,
        "taskB");

    if (taskB.sp == nullptr)
    {
        Serial.println("ERROR: Task B stack initialization failed.");

        for (;;)
        {
        }
    }

    Serial.printf(
        "Task B SP = %p\n",
        (void *)taskB.sp);

    Serial.println("Task B initialized.");

    /* -----------------------------------------------------
     * Add tasks to VertexRT ready queue
     * ----------------------------------------------------- */

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskA))
    {
        Serial.println("ERROR: Could not add Task A.");

        for (;;)
        {
        }
    }

    Serial.println("Task A added.");

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskB))
    {
        Serial.println("ERROR: Could not add Task B.");

        for (;;)
        {
        }
    }

    Serial.println("Task B added.");

    /*
     * The underlying Arduino FreeRTOS task remains active
     * here only until this function transfers control to
     * VertexRT.
     */
    Serial.println();
    Serial.println("Starting VertexRT scheduler...");
    Serial.println();

    vrt_scheduler_start(scheduler);

    /*
     * vrt_scheduler_start() must never return.
     */
    Serial.println(
        "ERROR: VertexRT scheduler returned!");

    for (;;)
    {
    }
}

/* =========================================================
 * Arduino loop
 * ========================================================= */

void loop()
{
    /*
     * VertexRT should own execution after scheduler start.
     *
     * Reaching loop() means something went wrong.
     */
    Serial.println(
        "ERROR: Arduino loop() is running.");

    for (;;)
    {
    }
}