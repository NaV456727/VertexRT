#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_preempt_timer.h"
#include "vrt_freertos_backend.h"
#include "vrt_sync.h"
#include "vrt_config.h"

static vrt_task_t taskA;
static vrt_task_t taskB;

static uint32_t stackA[VRT_STACK_SIZE];
static uint32_t stackB[VRT_STACK_SIZE];

static vrt_mutex_t testMutex;
static vrt_sem_t testSemaphore;

static volatile uint32_t aRuns = 0U;
static volatile uint32_t bRuns = 0U;

static volatile bool
    mutexPassed = false;

static volatile bool
    semaphorePassed = false;

static volatile bool
    step18Passed = false;

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
    Serial.println(
        "========== TASK A STARTED ==========");

    for (;;)
    {
        aRuns++;

        Serial.printf(
            "A: run=%lu tick=%lu current=%s\n",
            (unsigned long)aRuns,
            (unsigned long)scheduler->tickCount,
            scheduler->currentTask != NULL
                ? scheduler->currentTask->name
                : "NULL");

        /*
         * ------------------------------------------------------------
         * Mutex test
         * ------------------------------------------------------------
         */

        if (!mutexPassed &&
            aRuns == 2U)
        {
            Serial.println();
            Serial.println(
                "A: locking mutex...");

            vrt_mutex_lock(
                &testMutex);

            Serial.println(
                "A: mutex locked.");

            /*
             * Hold the mutex long enough for B to try to
             * acquire it.
             */
            for (volatile uint32_t i = 0U;
                 i < 1200000U;
                 ++i)
            {
            }

            Serial.println(
                "A: unlocking mutex...");

            vrt_mutex_unlock(
                &testMutex);

            Serial.println(
                "A: mutex unlocked.");
        }

        /*
         * ------------------------------------------------------------
         * Semaphore signal
         * ------------------------------------------------------------
         */

        if (aRuns == 5U)
        {
            Serial.println();
            Serial.println(
                "A: signaling semaphore...");

            vrt_sem_signal(
                &testSemaphore);
        }

        /*
         * Step 18 complete only after both tests have succeeded.
         */
        if (!step18Passed &&
            mutexPassed &&
            semaphorePassed)
        {
            step18Passed = true;

            Serial.println();
            Serial.println(
                "====================================");
            Serial.println(
                "STEP 18 PASSED");
            Serial.println(
                "====================================");

            Serial.println(
                "Mutex lock/block/unlock worked.");

            Serial.println(
                "Semaphore wait/signal worked.");

            Serial.println(
                "Both synchronization primitives");
            Serial.println(
                "are integrated with VertexRT");
            Serial.println(
                "blocking and FreeRTOS backing tasks.");

            Serial.println(
                "====================================");
        }

        for (volatile uint32_t i = 0U;
             i < 500000U;
             ++i)
        {
        }
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
    Serial.println(
        "========== TASK B STARTED ==========");

    for (;;)
    {
        bRuns++;

        Serial.printf(
            "B: run=%lu tick=%lu current=%s\n",
            (unsigned long)bRuns,
            (unsigned long)scheduler->tickCount,
            scheduler->currentTask != NULL
                ? scheduler->currentTask->name
                : "NULL");

        /*
         * ------------------------------------------------------------
         * Mutex test
         * ------------------------------------------------------------
         *
         * B attempts to acquire the mutex while A owns it.
         * This should block B.
         */

        if (!mutexPassed &&
            aRuns >= 2U &&
            bRuns == 2U)
        {
            Serial.println();
            Serial.println(
                "B: attempting mutex lock...");

            vrt_mutex_lock(
                &testMutex);

            Serial.println(
                "B: acquired mutex.");

            mutexPassed = true;

            vrt_mutex_unlock(
                &testMutex);
        }

        /*
         * ------------------------------------------------------------
         * Semaphore test
         * ------------------------------------------------------------
         *
         * Initially count == 0, so B should block here.
         */

        if (!semaphorePassed &&
            bRuns == 4U)
        {
            Serial.println();
            Serial.println(
                "B: waiting on semaphore...");

            vrt_sem_wait(
                &testSemaphore);

            /*
             * Execution resumes here after A signals.
             */
            Serial.println(
                "B: semaphore received.");

            semaphorePassed = true;
        }

        for (volatile uint32_t i = 0U;
             i < 400000U;
             ++i)
        {
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
        "VertexRT STEP 18");
    Serial.println(
        "Mutex / semaphore test");
    Serial.println(
        "====================================");

    /*
     * ------------------------------------------------------------
     * Scheduler
     * ------------------------------------------------------------
     */

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    vrt_scheduler_init(
        scheduler);

    /*
     * ------------------------------------------------------------
     * Synchronization primitives
     * ------------------------------------------------------------
     */

    vrt_mutex_init(
        &testMutex);

    vrt_sem_init(
        &testSemaphore,
        false);

    /*
     * ------------------------------------------------------------
     * Task A
     * ------------------------------------------------------------
     */

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
     * ------------------------------------------------------------
     * Task B
     * ------------------------------------------------------------
     */

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
     * ------------------------------------------------------------
     * Add tasks
     * ------------------------------------------------------------
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
     * ------------------------------------------------------------
     * Timer
     * ------------------------------------------------------------
     */

    if (!vrt_preempt_timer_init())
    {
        Serial.println(
            "ERROR: timer initialization failed.");

        while (true)
        {
            delay(1000);
        }
    }

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
     * ------------------------------------------------------------
     * Start Task A
     * ------------------------------------------------------------
     */

    taskA.state =
        VRT_TASK_RUNNING;

    taskB.state =
        VRT_TASK_READY;

    scheduler->currentTask =
        &taskA;

    scheduler->running =
        true;

    Serial.println();
    Serial.println(
        "Starting FreeRTOS-backed VertexRT...");

    vrt_freertos_backend_start(
        &taskA);
}

void loop()
{
    delay(1000);
}