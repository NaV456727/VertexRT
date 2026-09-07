#include <Arduino.h>

extern "C"
{
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_sync.h"
#include "vrt_freertos_backend.h"
#include "vrt_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

/*
 * ========================================================================
 * Scheduler / semaphore
 * ========================================================================
 */

static vrt_scheduler_t *scheduler = nullptr;
static vrt_sem_t testSem;

static vrt_task_t lowTask;
static vrt_task_t highTask;
static vrt_task_t controllerTask;

static uint32_t __attribute__((aligned(VRT_STACK_ALIGNMENT)))
lowStack[VRT_STACK_SIZE];

static uint32_t __attribute__((aligned(VRT_STACK_ALIGNMENT)))
highStack[VRT_STACK_SIZE];

static uint32_t __attribute__((aligned(VRT_STACK_ALIGNMENT)))
controllerStack[VRT_STACK_SIZE];

/*
 * ========================================================================
 * Test state
 * ========================================================================
 */

static volatile bool lowStarted = false;
static volatile bool highStarted = false;

static volatile bool lowCompleted = false;
static volatile bool highCompleted = false;

static volatile bool lowAcquired = false;
static volatile bool highAcquired = false;

static volatile bool controllerStarted = false;

static volatile uint32_t acquisitionOrder = 0U;

/*
 * ========================================================================
 * LOW PRIORITY WAITER
 *
 * Priority = 2
 *
 * LOW intentionally enters the semaphore wait queue FIRST.
 * ========================================================================
 */

static void low_task(void *argument)
{
    (void)argument;

    lowStarted = true;

    Serial.println();
    Serial.println("------------------------------------------------------------");
    Serial.println("LOW PRIORITY WAITER STARTED");
    Serial.println("------------------------------------------------------------");

    Serial.println("Priority = 2");
    Serial.println("LOW enters wait queue first");
    Serial.println("Waiting on semaphore...");

    bool result =
        vrt_sem_wait_timeout(
            &testSem,
            50U);

    lowCompleted = true;

    if (result)
    {
        lowAcquired = true;

        if (acquisitionOrder == 0U)
        {
            acquisitionOrder = 1U;
        }

        Serial.println();
        Serial.println("LOW PRIORITY WAITER ACQUIRED");
    }
    else
    {
        Serial.println();
        Serial.println("LOW PRIORITY WAITER TIMED OUT");
    }

    Serial.print("Low result    : ");
    Serial.println(
        result ? "ACQUIRED" : "TIMEOUT");

    Serial.print("Low priority  : ");
    Serial.println(
        lowTask.priority);

    Serial.print("Wait queue empty : ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue)
            ? "YES"
            : "NO");

    /*
     * LOW must terminate so the controller can run.
     */
    vrt_task_exit();
}

/*
 * ========================================================================
 * HIGH PRIORITY WAITER
 *
 * Priority = 3
 *
 * HIGH is initially suspended.
 * Controller resumes it AFTER LOW is already blocked.
 * ========================================================================
 */

static void high_task(void *argument)
{
    (void)argument;

    highStarted = true;

    Serial.println();
    Serial.println("------------------------------------------------------------");
    Serial.println("HIGH PRIORITY WAITER STARTED");
    Serial.println("------------------------------------------------------------");

    Serial.println("Priority = 3");
    Serial.println("HIGH enters wait queue SECOND");
    Serial.println("Waiting on semaphore...");

    bool result =
        vrt_sem_wait_timeout(
            &testSem,
            50U);

    highCompleted = true;

    if (result)
    {
        highAcquired = true;

        if (acquisitionOrder == 0U)
        {
            acquisitionOrder = 2U;
        }

        Serial.println();
        Serial.println("HIGH PRIORITY WAITER ACQUIRED");
    }
    else
    {
        Serial.println();
        Serial.println("HIGH PRIORITY WAITER TIMED OUT");
    }

    Serial.print("High result   : ");
    Serial.println(
        result ? "ACQUIRED" : "TIMEOUT");

    Serial.print("High priority : ");
    Serial.println(
        highTask.priority);

    Serial.print("Wait queue empty : ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue)
            ? "YES"
            : "NO");

    /*
     * HIGH must terminate so the controller can run.
     */
    vrt_task_exit();
}

/*
 * ========================================================================
 * CONTROLLER
 *
 * Priority = 1
 *
 * Sequence:
 *
 * 1. LOW blocks first.
 * 2. Controller gets CPU.
 * 3. Controller resumes HIGH.
 * 4. HIGH blocks second.
 * 5. Controller signals once.
 * 6. HIGH must acquire first.
 * 7. Controller signals again.
 * 8. LOW must acquire second.
 * ========================================================================
 */

static void controller_task(void *argument)
{
    (void)argument;

    controllerStarted = true;

    Serial.println();
    Serial.println("------------------------------------------------------------");
    Serial.println("CONTROLLER TASK STARTED");
    Serial.println("------------------------------------------------------------");

    /*
     * Give LOW enough time to enter the semaphore wait queue.
     */
    vTaskDelay(
        pdMS_TO_TICKS(20));

    Serial.println();
    Serial.println("============================================================");
    Serial.println("BEFORE RESUMING HIGH");
    Serial.println("============================================================");

    Serial.print("Low started      : ");
    Serial.println(
        lowStarted ? "YES" : "NO");

    Serial.print("Low completed    : ");
    Serial.println(
        lowCompleted ? "YES" : "NO");

    Serial.print("High started     : ");
    Serial.println(
        highStarted ? "YES" : "NO");

    Serial.print("High completed   : ");
    Serial.println(
        highCompleted ? "YES" : "NO");

    Serial.print("Wait queue empty : ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue)
            ? "YES"
            : "NO");

    Serial.print("Timed waiter     : ");
    Serial.println(
        (uintptr_t)testSem.timedWaiter,
        HEX);

    /*
     * LOW must already be waiting.
     */
    if (!lowStarted ||
        lowCompleted ||
        vrt_list_is_empty(&testSem.waitQueue))
    {
        Serial.println();
        Serial.println(
            "ERROR: LOW did not enter wait queue correctly");
    }

    /*
     * Resume HIGH.
     */
    Serial.println();
    Serial.println(
        "CONTROLLER: resuming HIGH priority waiter");

    vrt_task_resume(
        &highTask);

    /*
     * Give HIGH enough time to enter its wait.
     */
    vTaskDelay(
        pdMS_TO_TICKS(20));

    Serial.println();
    Serial.println("============================================================");
    Serial.println("BOTH WAITERS SHOULD NOW BE BLOCKED");
    Serial.println("============================================================");

    Serial.print("Low completed  : ");
    Serial.println(
        lowCompleted ? "YES" : "NO");

    Serial.print("High completed : ");
    Serial.println(
        highCompleted ? "YES" : "NO");

    Serial.print("Wait queue empty : ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue)
            ? "YES"
            : "NO");

    Serial.print("Timed waiter : ");
    Serial.println(
        (uintptr_t)testSem.timedWaiter,
        HEX);

    /*
     * ------------------------------------------------------------
     * FIRST SIGNAL
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "CONTROLLER: FIRST SEMAPHORE SIGNAL");

    vrt_sem_signal(
        &testSem);

    Serial.println(
        "First signal complete");

    /*
     * Give HIGH time to acquire and exit.
     */
    vTaskDelay(
        pdMS_TO_TICKS(30));

    Serial.println();
    Serial.println("============================================================");
    Serial.println("AFTER FIRST SIGNAL");
    Serial.println("============================================================");

    Serial.print("Low completed  : ");
    Serial.println(
        lowCompleted ? "YES" : "NO");

    Serial.print("Low acquired   : ");
    Serial.println(
        lowAcquired ? "YES" : "NO");

    Serial.print("High completed : ");
    Serial.println(
        highCompleted ? "YES" : "NO");

    Serial.print("High acquired  : ");
    Serial.println(
        highAcquired ? "YES" : "NO");

    Serial.print("Acquisition order : ");
    Serial.println(
        acquisitionOrder);

    Serial.print("Wait queue empty : ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue)
            ? "YES"
            : "NO");

    /*
     * HIGH must have acquired first.
     */
    bool firstSignalPass =
        highAcquired &&
        !lowAcquired &&
        highCompleted;

    Serial.print(
        "HIGH selected first : ");

    Serial.println(
        firstSignalPass
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------
     * SECOND SIGNAL
     * ------------------------------------------------------------
     *
     * LOW should now be the only remaining waiter.
     */

    Serial.println();
    Serial.println(
        "CONTROLLER: SECOND SEMAPHORE SIGNAL");

    vrt_sem_signal(
        &testSem);

    Serial.println(
        "Second signal complete");

    /*
     * Give LOW time to acquire and exit.
     */
    vTaskDelay(
        pdMS_TO_TICKS(30));

    Serial.println();
    Serial.println("============================================================");
    Serial.println("AFTER SECOND SIGNAL");
    Serial.println("============================================================");

    Serial.print("Low completed  : ");
    Serial.println(
        lowCompleted ? "YES" : "NO");

    Serial.print("Low acquired   : ");
    Serial.println(
        lowAcquired ? "YES" : "NO");

    Serial.print("High completed : ");
    Serial.println(
        highCompleted ? "YES" : "NO");

    Serial.print("High acquired  : ");
    Serial.println(
        highAcquired ? "YES" : "NO");

    Serial.print("Semaphore count : ");
    Serial.println(
        testSem.count);

    Serial.print("Timed waiter    : ");
    Serial.println(
        (uintptr_t)testSem.timedWaiter,
        HEX);

    Serial.print("Wait queue empty: ");
    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue)
            ? "YES"
            : "NO");

    /*
     * LOW must have acquired second.
     */
    bool secondSignalPass =
        lowAcquired &&
        lowCompleted &&
        highAcquired &&
        highCompleted;

    Serial.print(
        "LOW selected second : ");

    Serial.println(
        secondSignalPass
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------
     * FINAL RESULT
     * ------------------------------------------------------------
     */

    bool finalPass =
        firstSignalPass &&
        secondSignalPass &&
        vrt_list_is_empty(&testSem.waitQueue) &&
        testSem.timedWaiter == nullptr &&
        testSem.count == 0U;

    Serial.println();
    Serial.println("============================================================");
    Serial.println("STEP 5F RESULT");
    Serial.println("============================================================");

    Serial.print(
        "HIGH priority waiter selected first : ");

    Serial.println(
        firstSignalPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "LOW waiter selected second : ");

    Serial.println(
        secondSignalPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Wait queue cleaned : ");

    Serial.println(
        vrt_list_is_empty(&testSem.waitQueue)
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Timed waiter cleared : ");

    Serial.println(
        testSem.timedWaiter == nullptr
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Semaphore count cleared : ");

    Serial.println(
        testSem.count == 0U
            ? "PASS"
            : "FAIL");

    Serial.println();

    if (finalPass)
    {
        Serial.println(
            "STEP 5F DIAGNOSTIC: PASS");
    }
    else
    {
        Serial.println(
            "STEP 5F DIAGNOSTIC: FAIL");
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
 * SETUP
 * ========================================================================
 */

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("============================================================");
    Serial.println("VertexRT v0.2 STEP 5F");
    Serial.println("SEMAPHORE PRIORITY ORDERING");
    Serial.println("============================================================");

    scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == nullptr)
    {
        Serial.println(
            "ERROR: scheduler instance is NULL");

        return;
    }

    Serial.print(
        "Using scheduler instance = ");

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
        "Semaphore initialized: EMPTY");

    Serial.print(
        "Backend semaphore = ");

    Serial.println(
        (uintptr_t)testSem.backendHandle,
        HEX);

    /*
     * ------------------------------------------------------------
     * LOW
     * ------------------------------------------------------------
     */

    vrt_task_init(
        &lowTask,
        low_task,
        nullptr,
        2U,
        lowStack,
        VRT_STACK_SIZE,
        "lowTask");

    if (lowTask.sp == nullptr)
    {
        Serial.println(
            "ERROR: lowTask SP is NULL");

        return;
    }

    /*
     * ------------------------------------------------------------
     * HIGH
     * ------------------------------------------------------------
     */

    vrt_task_init(
        &highTask,
        high_task,
        nullptr,
        3U,
        highStack,
        VRT_STACK_SIZE,
        "highTask");

    if (highTask.sp == nullptr)
    {
        Serial.println(
            "ERROR: highTask SP is NULL");

        return;
    }

    /*
     * ------------------------------------------------------------
     * CONTROLLER
     * ------------------------------------------------------------
     */

    vrt_task_init(
        &controllerTask,
        controller_task,
        nullptr,
        1U,
        controllerStack,
        VRT_STACK_SIZE,
        "controller");

    if (controllerTask.sp == nullptr)
    {
        Serial.println(
            "ERROR: controller SP is NULL");

        return;
    }

    /*
     * Add all tasks.
     */
    if (!vrt_scheduler_add_task(
            scheduler,
            &lowTask))
    {
        Serial.println(
            "ERROR: failed to add lowTask");

        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &highTask))
    {
        Serial.println(
            "ERROR: failed to add highTask");

        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &controllerTask))
    {
        Serial.println(
            "ERROR: failed to add controller");

        return;
    }

    /*
     * HIGH starts suspended so LOW is guaranteed to enter the
     * semaphore wait queue first.
     */
    vrt_task_suspend(
        &highTask);

    Serial.println(
        "HIGH task initially suspended");

    Serial.println(
        "All tasks added");

    /*
     * Start scheduler.
     */
    Serial.println();
    Serial.println(
        "Starting VertexRT scheduler...");
    Serial.println();

    vrt_scheduler_start(
        scheduler);
}

void loop()
{
}