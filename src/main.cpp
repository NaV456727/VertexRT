#include <Arduino.h>

extern "C"
{
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_sync.h"
#include "vrt_freertos_backend.h"
#include "vrt_config.h"
}

/*
 * ========================================================================
 * Scheduler / synchronization objects
 * ========================================================================
 */

static vrt_scheduler_t *scheduler = nullptr;
static vrt_sem_t testSem;

static vrt_task_t timeoutTask;

static uint32_t __attribute__((aligned(VRT_STACK_ALIGNMENT)))
timeoutStack[VRT_STACK_SIZE];

/*
 * ========================================================================
 * Test state
 * ========================================================================
 */

static volatile bool timeoutReturned = false;
static volatile bool timeoutResult = true;

/*
 * ========================================================================
 * Timeout task
 * ========================================================================
 */

static void timeout_task(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("============================================================");
    Serial.println("VERTEXRT STEP 5C");
    Serial.println("SEMAPHORE TIMEOUT CLEANUP TEST");
    Serial.println("============================================================");

    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    Serial.print("Current task    : ");

    if (current != nullptr)
        Serial.println(current->name);
    else
        Serial.println("NULL");

    Serial.print("Semaphore count : ");
    Serial.println(testSem.count);

    Serial.print("Backend handle  : ");
    Serial.println(
        (uintptr_t)testSem.backendHandle,
        HEX);

    Serial.print("Timed waiter    : ");
    Serial.println(
        (uintptr_t)testSem.timedWaiter,
        HEX);

    Serial.print("Wait queue empty: ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue) ? "YES" : "NO");

    Serial.println();
    Serial.println("Calling vrt_sem_wait_timeout(&testSem, 20)...");
    Serial.println("Expected result: TIMEOUT");

    uint32_t startUs = micros();

    timeoutResult =
        vrt_sem_wait_timeout(
            &testSem,
            20U);

    uint32_t endUs = micros();

    uint32_t elapsedUs =
        endUs - startUs;

    timeoutReturned = true;

    Serial.println();
    Serial.println("============================================================");
    Serial.println("TIMEOUT RETURNED");
    Serial.println("============================================================");

    Serial.print("Result          : ");

    if (timeoutResult)
        Serial.println("ACQUIRED");
    else
        Serial.println("TIMEOUT");

    Serial.print("Elapsed us      : ");
    Serial.println(elapsedUs);

    Serial.print("Elapsed ms      : ");
    Serial.println(elapsedUs / 1000U);

    Serial.print("Semaphore count : ");
    Serial.println(testSem.count);

    Serial.print("Timed waiter    : ");
    Serial.println(
        (uintptr_t)testSem.timedWaiter,
        HEX);

    Serial.print("Wait queue empty: ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue) ? "YES" : "NO");

    /*
     * ================================================================
     * CHECK 1: timeout actually occurred
     * ================================================================
     */

    bool timeoutPass =
        (!timeoutResult &&
         elapsedUs >= 150000U);

    Serial.println();
    Serial.print("Timeout occurred : ");

    if (timeoutPass)
        Serial.println("PASS");
    else
        Serial.println("FAIL");

    /*
     * ================================================================
     * CHECK 2: timedWaiter must be cleared
     * ================================================================
     */

    bool waiterCleared =
        (testSem.timedWaiter == nullptr);

    Serial.print("Timed waiter cleared : ");

    if (waiterCleared)
        Serial.println("PASS");
    else
        Serial.println("FAIL");

    /*
     * ================================================================
     * CHECK 3: semaphore wait queue must be empty
     * ================================================================
     */

    bool queueCleared =
        vrt_list_is_empty(&testSem.waitQueue);

    Serial.print("Wait queue cleared : ");

    if (queueCleared)
        Serial.println("PASS");
    else
        Serial.println("FAIL");

    /*
     * ================================================================
     * CHECK 4: task must no longer be blocked
     * ================================================================
     */

    bool taskRunnable =
        (timeoutTask.state == VRT_TASK_RUNNING ||
         timeoutTask.state == VRT_TASK_READY);

    Serial.print("Task left BLOCKED state : ");

    if (taskRunnable)
        Serial.println("PASS");
    else
        Serial.println("FAIL");

    /*
     * ================================================================
     * Now signal the semaphore.
     *
     * There must be no stale waiter to wake.
     * ================================================================
     */

    Serial.println();
    Serial.println("------------------------------------------------------------");
    Serial.println("POST-TIMEOUT SIGNAL TEST");
    Serial.println("------------------------------------------------------------");

    Serial.println(
        "Calling vrt_sem_signal() after timeout...");

    vrt_sem_signal(
        &testSem);

    Serial.println(
        "vrt_sem_signal() returned");

    Serial.print("Semaphore count : ");
    Serial.println(testSem.count);

    Serial.print("Timed waiter    : ");
    Serial.println(
        (uintptr_t)testSem.timedWaiter,
        HEX);

    Serial.print("Wait queue empty: ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue) ? "YES" : "NO");

    /*
     * Since there is no waiter anymore, signal() should leave the
     * semaphore available.
     */
    bool signalStored =
        (testSem.count == 1U);

    Serial.print("Post-timeout signal stored : ");

    if (signalStored)
        Serial.println("PASS");
    else
        Serial.println("FAIL");

    /*
     * No waiter should have been recreated.
     */
    bool noStaleWaiter =
        (testSem.timedWaiter == nullptr &&
         vrt_list_is_empty(&testSem.waitQueue));

    Serial.print("No stale waiter : ");

    if (noStaleWaiter)
        Serial.println("PASS");
    else
        Serial.println("FAIL");

    /*
     * ================================================================
     * FINAL RESULT
     * ================================================================
     */

    bool finalPass =
        timeoutPass &&
        waiterCleared &&
        queueCleared &&
        taskRunnable &&
        signalStored &&
        noStaleWaiter;

    Serial.println();
    Serial.println("============================================================");
    Serial.println("STEP 5C RESULT");
    Serial.println("============================================================");

    if (finalPass)
    {
        Serial.println(
            "STEP 5C DIAGNOSTIC: PASS");
    }
    else
    {
        Serial.println(
            "STEP 5C DIAGNOSTIC: FAIL");
    }

    Serial.println(
        "============================================================");

    for (;;)
    {
        vrt_task_yield();
    }
}

/*
 * ========================================================================
 * Setup
 * ========================================================================
 */

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("============================================================");
    Serial.println("VertexRT v0.2 STEP 5C");
    Serial.println("SEMAPHORE TIMEOUT CLEANUP TEST");
    Serial.println("============================================================");

    scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == nullptr)
    {
        Serial.println(
            "ERROR: scheduler instance is NULL");

        return;
    }

    Serial.print("Using scheduler instance = ");
    Serial.println(
        (uintptr_t)scheduler,
        HEX);

    vrt_scheduler_init(
        scheduler);

    Serial.println(
        "Scheduler initialized");

    /*
     * Empty semaphore.
     */
    vrt_sem_init(
        &testSem,
        false);

    Serial.println(
        "Semaphore initialized");

    Serial.print(
        "Backend semaphore = ");

    Serial.println(
        (uintptr_t)testSem.backendHandle,
        HEX);

    /*
     * Timeout task.
     */
    vrt_task_init(
        &timeoutTask,
        timeout_task,
        nullptr,
        2U,
        timeoutStack,
        VRT_STACK_SIZE,
        "timeoutTask");

    if (timeoutTask.sp == nullptr)
    {
        Serial.println(
            "ERROR: timeout task SP is NULL");

        return;
    }

    Serial.println(
        "Timeout task initialized");

    if (!vrt_scheduler_add_task(
            scheduler,
            &timeoutTask))
    {
        Serial.println(
            "ERROR: failed to add timeout task");

        return;
    }

    Serial.println(
        "Timeout task added");

    Serial.println();
    Serial.println("Starting VertexRT scheduler...");
    Serial.println();

    vrt_scheduler_start(
        scheduler);
}

void loop()
{
}