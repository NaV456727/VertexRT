#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

/*
 * --------------------------------------------------------------------------
 * STEP 6
 * Single-task bootstrap test
 * --------------------------------------------------------------------------
 */

static vrt_task_t testTask;

static uint32_t testStack[VRT_STACK_SIZE];

static void bootstrap_task(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========== BOOTSTRAP TASK STARTED ==========");

    for (;;)
    {
        Serial.println("BOOTSTRAP TASK RUNNING");

        delay(1000);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("VertexRT STEP 6");
    Serial.println("Single-task bootstrap test");
    Serial.println("====================================");

    /*
     * ----------------------------------------------------------------------
     * Scheduler
     * ----------------------------------------------------------------------
     */

    Serial.println("Initializing scheduler...");

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == nullptr)
    {
        Serial.println("ERROR: scheduler instance is NULL.");

        while (true)
        {
            delay(1000);
        }
    }

    vrt_scheduler_init(scheduler);

    Serial.println("Scheduler initialized.");

    /*
     * ----------------------------------------------------------------------
     * Task
     * ----------------------------------------------------------------------
     */

    Serial.println("Initializing test task...");

    vrt_task_init(
        &testTask,
        bootstrap_task,
        nullptr,
        1,
        testStack,
        VRT_STACK_SIZE,
        "testTask");

    Serial.printf(
        "Task SP = %p\n",
        (void *)testTask.sp);

    if (testTask.sp == nullptr)
    {
        Serial.println("ERROR: Task stack initialization failed.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println("Task initialized.");

    /*
     * ----------------------------------------------------------------------
     * Dump the initial Xtensa frame
     * ----------------------------------------------------------------------
     *
     * IMPORTANT:
     * The task stack was created using the FreeRTOS Xtensa stack
     * initializer. We are only inspecting the resulting memory here.
     */

    uint32_t *p = testTask.sp;

    Serial.println();
    Serial.println("=== INITIAL TASK FRAME ===");

    Serial.printf(
        "SP   = %p\n",
        (void *)p);

    Serial.printf(
        "[00] = 0x%08lx  EXIT\n",
        (unsigned long)p[0]);

    Serial.printf(
        "[01] = 0x%08lx  PC\n",
        (unsigned long)p[1]);

    Serial.printf(
        "[02] = 0x%08lx  PS\n",
        (unsigned long)p[2]);

    Serial.printf(
        "[03] = 0x%08lx  A0\n",
        (unsigned long)p[3]);

    Serial.printf(
        "[04] = 0x%08lx  A1\n",
        (unsigned long)p[4]);

    Serial.printf(
        "[05] = 0x%08lx  A2\n",
        (unsigned long)p[5]);

    Serial.printf(
        "[06] = 0x%08lx  A3\n",
        (unsigned long)p[6]);

    Serial.printf(
        "[09] = 0x%08lx  A6\n",
        (unsigned long)p[9]);

    Serial.printf(
        "[10] = 0x%08lx  A7\n",
        (unsigned long)p[10]);

    Serial.println("==========================");

    /*
     * ----------------------------------------------------------------------
     * Add task
     * ----------------------------------------------------------------------
     */

    Serial.println("Adding test task...");

    if (!vrt_scheduler_add_task(
            scheduler,
            &testTask))
    {
        Serial.println("ERROR: Could not add test task.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println("Task added.");

    /*
     * ----------------------------------------------------------------------
     * Start scheduler
     * ----------------------------------------------------------------------
     */

    Serial.println();
    Serial.println("Starting VertexRT scheduler...");

    vrt_scheduler_start(scheduler);

    /*
     * We should never return here.
     */

    Serial.println(
        "ERROR: VertexRT scheduler returned unexpectedly.");

    while (true)
    {
        delay(1000);
    }
}

void loop()
{
    /*
     * The scheduler should take control before loop() is reached.
     */

    Serial.println(
        "ERROR: Arduino loop() is running.");

    delay(1000);
}