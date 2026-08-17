#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_preempt_timer.h"
#include "vrt_freertos_backend.h"

static vrt_task_t taskLow;
static vrt_task_t taskMedium;
static vrt_task_t taskHigh;

static uint32_t stackLow[VRT_STACK_SIZE];
static uint32_t stackMedium[VRT_STACK_SIZE];
static uint32_t stackHigh[VRT_STACK_SIZE];

static volatile uint32_t lowRuns = 0;
static volatile uint32_t mediumRuns = 0;
static volatile uint32_t highRuns = 0;

static volatile bool highBlocked = false;
static volatile bool mediumBlocked = false;
static volatile bool highWoke = false;
static volatile bool passed = false;

static const char *current_name()
{
    vrt_scheduler_t *s =
        vrt_scheduler_get_instance();

    if (s == NULL ||
        s->currentTask == NULL)
    {
        return "NULL";
    }

    return s->currentTask->name;
}

/*
 * LOW = priority 1
 */
static void task_low(void *arg)
{
    (void)arg;

    Serial.println(
        "========== LOW TASK STARTED ==========");

    for (;;)
    {
        if (passed)
        {
            /*
             * Do nothing after the acceptance test.
             */
            for (;;)
            {
                delay(1000);
            }
        }

        lowRuns++;

        Serial.printf(
            "LOW: run=%lu tick=%lu priority=%u current=%s\n",
            (unsigned long)lowRuns,
            (unsigned long)vrt_scheduler_get_instance()->tickCount,
            (unsigned int)taskLow.priority,
            current_name());

        /*
         * Acceptance point:
         *
         * HIGH has blocked.
         * MEDIUM has blocked.
         * HIGH has subsequently woken.
         *
         * Therefore LOW was successfully preempted by HIGH.
         */
        if (!passed &&
            highBlocked &&
            mediumBlocked &&
            highWoke)
        {
            passed = true;

            Serial.println();
            Serial.println(
                "====================================");
            Serial.println(
                "STEP 19 PASSED");
            Serial.println(
                "Priority scheduling is working.");
            Serial.println(
                "HIGH (3) > MEDIUM (2) > LOW (1)");
            Serial.println(
                "HIGH blocked -> MEDIUM ran.");
            Serial.println(
                "MEDIUM blocked -> LOW ran.");
            Serial.println(
                "HIGH woke -> HIGH preempted LOW.");
            Serial.println(
                "====================================");
        }

        for (volatile uint32_t i = 0;
             i < 500000;
             ++i)
        {
        }
    }
}

/*
 * MEDIUM = priority 2
 */
static void task_medium(void *arg)
{
    (void)arg;

    Serial.println(
        "========== MEDIUM TASK STARTED ==========");

    for (;;)
    {
        if (passed)
        {
            for (;;)
            {
                delay(1000);
            }
        }

        mediumRuns++;

        Serial.printf(
            "MED: run=%lu tick=%lu priority=%u current=%s\n",
            (unsigned long)mediumRuns,
            (unsigned long)vrt_scheduler_get_instance()->tickCount,
            (unsigned int)taskMedium.priority,
            current_name());

        /*
         * HIGH has already blocked.
         * Let MEDIUM run, then block it so LOW can run.
         */
        if (!mediumBlocked &&
            highBlocked &&
            mediumRuns >= 4)
        {
            Serial.println();
            Serial.println(
                "MED: delaying for 20 ticks...");

            mediumBlocked = true;

            vrt_task_delay(20);

            Serial.println(
                "MED: woke.");
        }

        for (volatile uint32_t i = 0;
             i < 500000;
             ++i)
        {
        }
    }
}

/*
 * HIGH = priority 3
 */
static void task_high(void *arg)
{
    (void)arg;

    Serial.println(
        "========== HIGH TASK STARTED ==========");

    for (;;)
    {
        if (passed)
        {
            for (;;)
            {
                delay(1000);
            }
        }

        highRuns++;

        Serial.printf(
            "HIGH: run=%lu tick=%lu priority=%u current=%s\n",
            (unsigned long)highRuns,
            (unsigned long)vrt_scheduler_get_instance()->tickCount,
            (unsigned int)taskHigh.priority,
            current_name());

        /*
         * HIGH blocks once.
         *
         * Expected:
         *
         *     HIGH -> MEDIUM
         */
        if (!highBlocked &&
            highRuns == 3)
        {
            Serial.println();
            Serial.println(
                "HIGH: delaying for 30 ticks...");

            highBlocked = true;

            vrt_task_delay(30);

            highWoke = true;

            Serial.println(
                "HIGH: woke.");
        }

        for (volatile uint32_t i = 0;
             i < 500000;
             ++i)
        {
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(
        "====================================");
    Serial.println(
        "VertexRT STEP 19");
    Serial.println(
        "Priority scheduling test");
    Serial.println(
        "====================================");

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    Serial.println(
        "Initializing scheduler...");

    vrt_scheduler_init(
        scheduler);

    Serial.println(
        "Scheduler initialized.");

    Serial.println(
        "Initializing LOW task...");

    vrt_task_init(
        &taskLow,
        task_low,
        NULL,
        1,
        stackLow,
        VRT_STACK_SIZE,
        "low");

    Serial.printf(
        "LOW SP = %p priority=%u\n",
        (void *)taskLow.sp,
        (unsigned int)taskLow.priority);

    Serial.println(
        "Initializing MEDIUM task...");

    vrt_task_init(
        &taskMedium,
        task_medium,
        NULL,
        2,
        stackMedium,
        VRT_STACK_SIZE,
        "medium");

    Serial.printf(
        "MEDIUM SP = %p priority=%u\n",
        (void *)taskMedium.sp,
        (unsigned int)taskMedium.priority);

    Serial.println(
        "Initializing HIGH task...");

    vrt_task_init(
        &taskHigh,
        task_high,
        NULL,
        3,
        stackHigh,
        VRT_STACK_SIZE,
        "high");

    Serial.printf(
        "HIGH SP = %p priority=%u\n",
        (void *)taskHigh.sp,
        (unsigned int)taskHigh.priority);

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskLow))
    {
        Serial.println(
            "ERROR: failed to add LOW.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskMedium))
    {
        Serial.println(
            "ERROR: failed to add MEDIUM.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskHigh))
    {
        Serial.println(
            "ERROR: failed to add HIGH.");
        return;
    }

    Serial.println(
        "LOW added.");
    Serial.println(
        "MEDIUM added.");
    Serial.println(
        "HIGH added.");

    Serial.println();
    Serial.println(
        "Initializing VertexRT timer ISR...");

    if (!vrt_preempt_timer_init())
    {
        Serial.println(
            "ERROR: timer initialization failed.");
        return;
    }

    Serial.println(
        "Timer ISR initialized.");

    if (!vrt_preempt_timer_start())
    {
        Serial.println(
            "ERROR: timer start failed.");
        return;
    }

    Serial.println(
        "Timer ISR started.");

    Serial.println();
    Serial.println(
        "Starting FreeRTOS-backed VertexRT...");

    Serial.println(
        "Initial logical task = LOW");

    Serial.println(
        "Priority order:");

    Serial.println(
        "HIGH   = 3");
    Serial.println(
        "MEDIUM = 2");
    Serial.println(
        "LOW    = 1");

    /*
     * Use the established scheduler start path.
     * Do not manually modify scheduler->running/currentTask.
     */
    vrt_scheduler_start(
        scheduler);
}

void loop()
{
    delay(1000);
}