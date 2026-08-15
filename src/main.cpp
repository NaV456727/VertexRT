#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"

/*
 * ============================================================================
 * VertexRT STEP 11
 * Cooperative task delay / manual tick test
 * ============================================================================
 *
 * Expected:
 *
 *     A starts
 *     A delays for 5 ticks
 *     B runs
 *     B advances ticks 1..5
 *     A becomes READY
 *     B yields
 *     A resumes
 *
 * No hardware timer is used in this step.
 * ============================================================================
 */

static vrt_task_t taskA;
static vrt_task_t taskB;

static uint32_t stackA[VRT_STACK_SIZE];
static uint32_t stackB[VRT_STACK_SIZE];

static volatile bool aHasResumed = false;

/*
 * ============================================================================
 * Task A
 * ============================================================================
 */

static void task_a(void *argument)
{
    (void)argument;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    Serial.println();
    Serial.println("========== TASK A STARTED ==========");

    Serial.printf(
        "A: current tick = %lu\n",
        (unsigned long)scheduler->tickCount);

    Serial.println(
        "A: delaying for 5 ticks");

    vrt_task_delay(5);

    /*
     * We should NOT reach this point until the kernel has advanced
     * five ticks and woken A.
     */
    Serial.println();
    Serial.printf(
        "A: resumed at tick %lu\n",
        (unsigned long)scheduler->tickCount);

    if (scheduler->tickCount < 5U)
    {
        Serial.println(
            "ERROR: A resumed too early!");

        for (;;)
        {
        }
    }

    aHasResumed = true;

    Serial.println();
    Serial.println(
        "========== STEP 11 DELAY PASSED ==========");

    for (;;)
    {
        Serial.println("A: alive");
        vrt_task_yield();
    }
}

/*
 * ============================================================================
 * Task B
 * ============================================================================
 */

static void task_b(void *argument)
{
    (void)argument;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    Serial.println();
    Serial.println("========== TASK B STARTED ==========");

    for (uint32_t i = 0; i < 7U; ++i)
    {
        Serial.printf(
            "B: tick before = %lu\n",
            (unsigned long)scheduler->tickCount);

        vrt_scheduler_tick(
            scheduler);

        Serial.printf(
            "B: tick after  = %lu\n",
            (unsigned long)scheduler->tickCount);

        if (taskA.state == VRT_TASK_BLOCKED)
        {
            Serial.println(
                "B: A is BLOCKED");
        }
        else
        {
            Serial.printf(
                "B: A state = %d\n",
                (int)taskA.state);
        }

        /*
         * Once A wakes, let it run.
         */
        if (aHasResumed)
        {
            break;
        }

        vrt_task_yield();
    }

    /*
     * A should wake at tick 5.
     */
    for (;;)
    {
        vrt_task_yield();

        if (aHasResumed)
        {
            Serial.println(
                "B: A has successfully resumed");

            for (;;)
            {
                delay(1000);
            }
        }
    }
}

/*
 * ============================================================================
 * Setup
 * ============================================================================
 */

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("VertexRT STEP 11");
    Serial.println("Task delay / manual tick test");
    Serial.println("====================================");

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == nullptr)
    {
        Serial.println(
            "ERROR: scheduler instance is NULL.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Initializing scheduler...");

    vrt_scheduler_init(
        scheduler);

    Serial.println(
        "Scheduler initialized.");

    /*
     * ------------------------------------------------------------------------
     * Task A
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "Initializing Task A...");

    vrt_task_init(
        &taskA,
        task_a,
        nullptr,
        1,
        stackA,
        VRT_STACK_SIZE,
        "taskA");

    Serial.printf(
        "Task A SP = %p\n",
        (void *)taskA.sp);

    /*
     * ------------------------------------------------------------------------
     * Task B
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "Initializing Task B...");

    vrt_task_init(
        &taskB,
        task_b,
        nullptr,
        1,
        stackB,
        VRT_STACK_SIZE,
        "taskB");

    Serial.printf(
        "Task B SP = %p\n",
        (void *)taskB.sp);

    if (taskA.sp == nullptr ||
        taskB.sp == nullptr)
    {
        Serial.println(
            "ERROR: task initialization failed.");

        while (true)
        {
            delay(1000);
        }
    }

    /*
     * ------------------------------------------------------------------------
     * Add tasks
     * ------------------------------------------------------------------------
     */

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskA))
    {
        Serial.println(
            "ERROR: could not add Task A");

        while (true)
        {
        }
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskB))
    {
        Serial.println(
            "ERROR: could not add Task B");

        while (true)
        {
        }
    }

    Serial.println(
        "Task A added.");

    Serial.println(
        "Task B added.");

    /*
     * ------------------------------------------------------------------------
     * Start
     * ------------------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "Starting VertexRT scheduler...");

    vrt_scheduler_start(
        scheduler);

    /*
     * Should never return.
     */

    for (;;)
    {
    }
}

void loop()
{
}