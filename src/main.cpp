#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_preempt_timer.h"
#include "vrt_freertos_backend.h"
#include "vrt_config.h"

static vrt_task_t taskA;
static vrt_task_t taskB;

static uint32_t stackA[VRT_STACK_SIZE];
static uint32_t stackB[VRT_STACK_SIZE];

static volatile uint32_t counterA = 0;
static volatile uint32_t counterB = 0;

static volatile bool
    taskAStarted = false;

static volatile bool
    taskBStarted = false;

static volatile bool
    stepPassed = false;

/*
 * ============================================================================
 * Task A
 * ============================================================================
 *
 * IMPORTANT:
 *
 * No vrt_task_yield().
 *
 * This task deliberately performs CPU work continuously.
 *
 * Preemption must therefore come from the hardware timer -> FreeRTOS ISR
 * scheduling path.
 * ============================================================================
 */

static void task_a(void *argument)
{
    (void)argument;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    taskAStarted = true;

    Serial.println();
    Serial.println(
        "========== TASK A STARTED ==========");

    for (;;)
    {
        counterA++;

        if (counterA % 1000000U == 0U)
        {
            Serial.printf(
                "A: counter=%lu tick=%lu current=%s\n",
                (unsigned long)counterA,
                (unsigned long)scheduler->tickCount,
                scheduler->currentTask != NULL
                    ? scheduler->currentTask->name
                    : "NULL");
        }

        /*
         * Detect that B has actually executed while A
         * was continuously running.
         */
        if (!stepPassed &&
            taskBStarted &&
            counterA >= 1000000U)
        {
            stepPassed = true;

            Serial.println();
            Serial.println(
                "====================================");
            Serial.println(
                "STEP 14A PREEMPTION PASSED");
            Serial.println(
                "====================================");

            Serial.println(
                "Task A was executing without a");
            Serial.println(
                "VertexRT voluntary yield.");

            Serial.println(
                "Task B was started by the");
            Serial.println(
                "FreeRTOS-backed preemption path.");

            Serial.println();
        }
    }
}

/*
 * ============================================================================
 * Task B
 * ============================================================================
 *
 * Same deliberate CPU-bound behavior as Task A.
 * ============================================================================
 */

static void task_b(void *argument)
{
    (void)argument;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    taskBStarted = true;

    Serial.println();
    Serial.println(
        "========== TASK B STARTED ==========");

    for (;;)
    {
        counterB++;

        if (counterB % 1000000U == 0U)
        {
            Serial.printf(
                "B: counter=%lu tick=%lu current=%s\n",
                (unsigned long)counterB,
                (unsigned long)scheduler->tickCount,
                scheduler->currentTask != NULL
                    ? scheduler->currentTask->name
                    : "NULL");
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
    Serial.println(
        "====================================");
    Serial.println(
        "VertexRT STEP 14A");
    Serial.println(
        "FreeRTOS-backed preemption test");
    Serial.println(
        "====================================");

    /*
     * ------------------------------------------------------------------------
     * Scheduler
     * ------------------------------------------------------------------------
     */

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

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

    /*
     * ------------------------------------------------------------------------
     * Add VertexRT tasks
     * ------------------------------------------------------------------------
     *
     * vrt_scheduler_add_task() now also creates the corresponding
     * FreeRTOS backing task.
     * ------------------------------------------------------------------------
     */

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskA))
    {
        Serial.println(
            "ERROR: failed to add Task A.");

        while (true)
        {
            delay(1000);
        }
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskB))
    {
        Serial.println(
            "ERROR: failed to add Task B.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Task A added.");

    Serial.println(
        "Task B added.");

    /*
     * ------------------------------------------------------------------------
     * Hardware timer
     * ------------------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "Initializing VertexRT timer ISR...");

    if (!vrt_preempt_timer_init())
    {
        Serial.println(
            "ERROR: timer initialization failed.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Timer ISR initialized.");

    if (!vrt_preempt_timer_start())
    {
        Serial.println(
            "ERROR: timer start failed.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Timer ISR started.");

    /*
     * ------------------------------------------------------------------------
     * Select first VertexRT task
     * ------------------------------------------------------------------------
     *
     * We do NOT use vrt_scheduler_start(), because that enters the old
     * raw Xtensa context-switch path.
     *
     * Instead, select the first logical task and let the FreeRTOS-backed
     * task become the actual execution context.
     * ------------------------------------------------------------------------
     */

    taskA.state =
        VRT_TASK_RUNNING;

    scheduler->currentTask =
        &taskA;

    scheduler->running =
        true;

    /*
     * Notify Task A's FreeRTOS backing task.
     *
     * The backing task was already created by
     * vrt_scheduler_add_task().
     */

    vrt_freertos_backend_on_preemption(
        &taskA);

    Serial.println();
    Serial.println(
        "Starting FreeRTOS-backed VertexRT...");

    Serial.println(
        "Task A is the initial logical task.");

    /*
     * setup() returns.
     *
     * Arduino/FreeRTOS continues running the backing tasks.
     */
}

/*
 * ============================================================================
 * Arduino loop
 * ============================================================================
 */

void loop()
{
    /*
     * VertexRT execution is handled by the FreeRTOS-backed tasks.
     */
    delay(1000);
}