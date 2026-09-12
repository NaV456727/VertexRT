#include <Arduino.h>

extern "C"
{
#include "vrt_config.h"
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_tick.h"
#include "vrt_sync.h"
}

/*
 * ============================================================================
 * STEP 14
 * MUTEX + PRIORITY INHERITANCE QUALIFICATION
 * ============================================================================
 */

static vrt_scheduler_t scheduler;

static vrt_task_t lowTask;
static vrt_task_t highTask;

static uint32_t lowStack[2048];
static uint32_t highStack[2048];

static vrt_mutex_t testMutex;

/*
 * ============================================================================
 * Test state
 * ============================================================================
 */

static volatile bool lowStarted = false;
static volatile bool lowLocked = false;

static volatile bool highStarted = false;
static volatile bool highTrying = false;
static volatile bool highAcquired = false;
static volatile bool highFinished = false;

static volatile bool highBlockedObserved = false;

/*
 * ============================================================================
 * Task state helper
 * ============================================================================
 */

static const char *
taskStateName(
    vrt_task_state_t state)
{
    switch (state)
    {
    case VRT_TASK_READY:
        return "READY";

    case VRT_TASK_RUNNING:
        return "RUNNING";

    case VRT_TASK_BLOCKED:
        return "BLOCKED";

    case VRT_TASK_SUSPENDED:
        return "SUSPENDED";

    case VRT_TASK_TERMINATED:
        return "TERMINATED";

    default:
        return "UNKNOWN";
    }
}

/*
 * ============================================================================
 * LOW PRIORITY TASK
 * ============================================================================
 */

static void lowTaskEntry(
    void *argument)
{
    (void)argument;

    lowStarted = true;

    Serial.println();
    Serial.println("[LOW] started.");

    Serial.println(
        "[LOW] locking mutex...");

    vrt_mutex_lock(
        &testMutex);

    lowLocked = true;

    Serial.println(
        "[LOW] mutex acquired.");

    Serial.print(
        "[LOW] owner correct : ");

    Serial.println(
        testMutex.owner == &lowTask
            ? "YES"
            : "NO");

    Serial.print(
        "[LOW] priority : ");

    Serial.println(
        (unsigned long)
            lowTask.priority);

    /*
     * HIGH starts suspended.
     *
     * Resume HIGH directly from LOW so there is no
     * controller task involved.
     */
    Serial.println();
    Serial.println(
        "[LOW] resuming HIGH...");

    vrt_task_resume(
        &highTask);

    /*
     * HIGH has higher priority and should now execute.
     *
     * HIGH will attempt the mutex, block, and the scheduler
     * should return execution to LOW with inherited priority.
     */

    while (!highTrying)
    {
        vrt_task_yield();
    }

    /*
     * Wait until HIGH is actually blocked.
     */
    while (highTask.state !=
           VRT_TASK_BLOCKED)
    {
        vrt_task_yield();
    }

    highBlockedObserved = true;

    Serial.println();
    Serial.println(
        "[LOW] HIGH is BLOCKED.");

    Serial.print(
        "[LOW] HIGH state : ");

    Serial.println(
        taskStateName(
            highTask.state));

    Serial.print(
        "[LOW] inherited priority : ");

    Serial.println(
        (unsigned long)
            lowTask.priority);

    Serial.print(
        "[LOW] HIGH priority : ");

    Serial.println(
        (unsigned long)
            highTask.priority);

    /*
     * LOW should now own the inherited priority.
     *
     * Release the mutex.
     */
    Serial.println();
    Serial.println(
        "[LOW] unlocking mutex...");

    vrt_mutex_unlock(
        &testMutex);

    /*
     * After unlock, HIGH should be the owner and LOW
     * should have its original priority again.
     */
    Serial.print(
        "[LOW] priority after unlock : ");

    Serial.println(
        (unsigned long)
            lowTask.priority);

    for (;;)
    {
        vrt_task_yield();
    }
}

/*
 * ============================================================================
 * HIGH PRIORITY TASK
 * ============================================================================
 */

static void highTaskEntry(
    void *argument)
{
    (void)argument;

    highStarted = true;

    Serial.println();
    Serial.println(
        "[HIGH] started.");

    Serial.println(
        "[HIGH] attempting mutex lock...");

    highTrying = true;

    /*
     * LOW already owns the mutex.
     *
     * This should block HIGH.
     */
    vrt_mutex_lock(
        &testMutex);

    /*
     * We only reach here after LOW releases it.
     */
    highAcquired = true;

    Serial.println();
    Serial.println(
        "[HIGH] mutex acquired.");

    Serial.print(
        "[HIGH] mutex owner correct : ");

    Serial.println(
        testMutex.owner == &highTask
            ? "YES"
            : "NO");

    /*
     * Release the mutex.
     */
    vrt_mutex_unlock(
        &testMutex);

    highFinished = true;

    Serial.println(
        "[HIGH] mutex released.");

    for (;;)
    {
        vrt_task_yield();
    }
}

/*
 * ============================================================================
 * SETUP
 * ============================================================================
 */

void setup()
{
    Serial.begin(
        115200);

    delay(1000);

    Serial.println();
    Serial.println(
        "========================================");

    Serial.println(
        "VertexRT Step 14 Test");

    Serial.println(
        "MUTEX + PRIORITY INHERITANCE");

    Serial.println(
        "========================================");

    /*
     * ------------------------------------------------------------------------
     * Scheduler
     * ------------------------------------------------------------------------
     */

    vrt_scheduler_init(
        &scheduler);

    Serial.println(
        "Scheduler init : PASS");

    /*
     * ------------------------------------------------------------------------
     * Tick
     * ------------------------------------------------------------------------
     */

    bool tickInit =
        vrt_tick_init();

    bool tickStart =
        vrt_tick_start();

    Serial.print(
        "Kernel tick init : ");

    Serial.println(
        tickInit
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Kernel tick start : ");

    Serial.println(
        tickStart
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------------------
     * Mutex
     * ------------------------------------------------------------------------
     */

    vrt_mutex_init(
        &testMutex);

    bool mutexInitial =
        !testMutex.locked &&
        testMutex.owner == NULL &&
        vrt_list_is_empty(
            &testMutex.waitQueue);

    Serial.print(
        "Mutex init : ");

    Serial.println(
        mutexInitial
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------------------
     * LOW TASK
     * ------------------------------------------------------------------------
     *
     * Priority 2.
     */

    vrt_task_init(
        &lowTask,
        lowTaskEntry,
        NULL,
        2U,
        lowStack,
        2048U,
        "LowTask");

    /*
     * ------------------------------------------------------------------------
     * HIGH TASK
     * ------------------------------------------------------------------------
     *
     * Priority 4.
     */

    vrt_task_init(
        &highTask,
        highTaskEntry,
        NULL,
        4U,
        highStack,
        2048U,
        "HighTask");

    /*
     * ------------------------------------------------------------------------
     * Register tasks
     * ------------------------------------------------------------------------
     */

    bool lowAdded =
        vrt_scheduler_add_task(
            &scheduler,
            &lowTask);

    bool highAdded =
        vrt_scheduler_add_task(
            &scheduler,
            &highTask);

    Serial.print(
        "Low task add : ");

    Serial.println(
        lowAdded
            ? "PASS"
            : "FAIL");

    Serial.print(
        "High task add : ");

    Serial.println(
        highAdded
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------------------
     * HIGH must not run before LOW owns the mutex.
     * ------------------------------------------------------------------------
     */

    vrt_task_suspend(
        &highTask);

    bool highSuspended =
        highTask.state ==
        VRT_TASK_SUSPENDED;

    Serial.print(
        "High task suspended : ");

    Serial.println(
        highSuspended
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------------------
     * Start scheduler.
     * ------------------------------------------------------------------------
     *
     * LOW is the only runnable task.
     */

    Serial.println();
    Serial.println(
        "Starting scheduler...");

    vrt_scheduler_start(
        &scheduler);
}

void loop()
{
    delay(1000);
}