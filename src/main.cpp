#include <Arduino.h>

extern "C"
{
#include "vrt_config.h"
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_tick.h"
}

static vrt_scheduler_t scheduler;

static vrt_task_t taskA;
static vrt_task_t taskB;
static vrt_task_t controller;

static uint32_t stackA[2048];
static uint32_t stackB[2048];
static uint32_t controllerStack[2048];

static volatile bool taskAStarted = false;
static volatile bool taskBStarted = false;

static volatile uint32_t taskAYields = 0U;
static volatile uint32_t taskBYields = 0U;

/*
 * ============================================================================
 * Task A
 * ============================================================================
 */

static void taskAEntry(
    void *argument)
{
    (void)argument;

    taskAStarted = true;

    while (true)
    {
        taskAYields++;

        /*
         * Equal-priority cooperative scheduling.
         */
        vrt_task_yield();
    }
}

/*
 * ============================================================================
 * Task B
 * ============================================================================
 */

static void taskBEntry(
    void *argument)
{
    (void)argument;

    taskBStarted = true;

    while (true)
    {
        taskBYields++;

        /*
         * Equal-priority cooperative scheduling.
         */
        vrt_task_yield();
    }
}

/*
 * ============================================================================
 * Controller
 * ============================================================================
 */

static void controllerEntry(
    void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========================================");
    Serial.println("STEP 11: CPU / RUNTIME STATISTICS");
    Serial.println("========================================");

    Serial.println();
    Serial.println("Controller started.");
    Serial.println("Blocking for 2 seconds...");

    /*
     * At VRT_TICK_HZ = 1000 Hz, 2000 ticks = approximately 2 seconds.
     */
    vrt_task_delay(2000U);

    /*
     * If execution reaches here, the controller was successfully
     * blocked and later woken by the kernel tick.
     */
    Serial.println();
    Serial.println("Controller woke.");

    /*
     * IMPORTANT:
     *
     * Do NOT call vrt_task_yield() here.
     *
     * The controller has higher priority than A/B, and the current
     * scheduler's cooperative schedule() can select another READY
     * task when yield() is called.
     *
     * We want to collect the statistics immediately.
     */

    uint64_t runtimeA =
        vrt_task_runtime_us(
            &taskA);

    uint64_t runtimeB =
        vrt_task_runtime_us(
            &taskB);

    uint64_t controllerRuntime =
        vrt_task_runtime_us(
            &controller);

    uint64_t totalRuntime =
        runtimeA +
        runtimeB;

    uint32_t cpuA =
        vrt_task_cpu_percent(
            &taskA);

    uint32_t cpuB =
        vrt_task_cpu_percent(
            &taskB);

    /*
     * ------------------------------------------------------------------------
     * Statistics
     * ------------------------------------------------------------------------
     */

    Serial.println();
    Serial.println("----------------------------------------");
    Serial.println("RUNTIME STATISTICS");
    Serial.println("----------------------------------------");

    Serial.print(
        "Task A started    : ");

    Serial.println(
        taskAStarted
            ? "YES"
            : "NO");

    Serial.print(
        "Task B started    : ");

    Serial.println(
        taskBStarted
            ? "YES"
            : "NO");

    Serial.print(
        "Task A runtime us : ");

    Serial.println(
        (unsigned long long)runtimeA);

    Serial.print(
        "Task B runtime us : ");

    Serial.println(
        (unsigned long long)runtimeB);

    Serial.print(
        "Controller us     : ");

    Serial.println(
        (unsigned long long)controllerRuntime);

    Serial.print(
        "Total A+B us      : ");

    Serial.println(
        (unsigned long long)totalRuntime);

    Serial.print(
        "Task A CPU %      : ");

    Serial.println(
        cpuA);

    Serial.print(
        "Task B CPU %      : ");

    Serial.println(
        cpuB);

    Serial.print(
        "Task A yields     : ");

    Serial.println(
        (unsigned long)taskAYields);

    Serial.print(
        "Task B yields     : ");

    Serial.println(
        (unsigned long)taskBYields);

    /*
     * ------------------------------------------------------------------------
     * Runtime reset qualification
     * ------------------------------------------------------------------------
     */

    vrt_task_runtime_reset(
        &taskA);

    uint64_t resetRuntime =
        vrt_task_runtime_us(
            &taskA);

    /*
     * ------------------------------------------------------------------------
     * Qualification
     * ------------------------------------------------------------------------
     */

    bool tasksRan =
        taskAStarted &&
        taskBStarted;

    bool runtimePass =
        runtimeA > 0U &&
        runtimeB > 0U;

    bool switchingPass =
        taskAYields > 0U &&
        taskBYields > 0U;

    bool cpuPass =
        cpuA > 0U &&
        cpuB > 0U;

    bool totalPass =
        totalRuntime >= runtimeA &&
        totalRuntime >= runtimeB;

    bool resetPass =
        resetRuntime == 0U;

    Serial.println();
    Serial.println("----------------------------------------");
    Serial.println("QUALIFICATION");
    Serial.println("----------------------------------------");

    Serial.print(
        "Both tasks executed : ");

    Serial.println(
        tasksRan
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Runtime accumulation: ");

    Serial.println(
        runtimePass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Context switching   : ");

    Serial.println(
        switchingPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "CPU statistics      : ");

    Serial.println(
        cpuPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Runtime total       : ");

    Serial.println(
        totalPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Runtime reset       : ");

    Serial.println(
        resetPass
            ? "PASS"
            : "FAIL");

    bool result =
        tasksRan &&
        runtimePass &&
        switchingPass &&
        cpuPass &&
        totalPass &&
        resetPass;

    Serial.println();
    Serial.println("----------------------------------------");

    Serial.print(
        "STEP 11 RESULT: ");

    Serial.println(
        result
            ? "PASS"
            : "FAIL");

    Serial.println("----------------------------------------");

    /*
     * Stop the test only after all measurements have been printed.
     */
    for (;;)
    {
        /*
         * Do not yield. Just remain here.
         */
    }
}

/*
 * ============================================================================
 * Setup
 * ============================================================================
 */

void setup()
{
    Serial.begin(
        115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("VertexRT Step 11 Test");
    Serial.println("========================================");

    /*
     * Scheduler.
     */
    vrt_scheduler_init(
        &scheduler);

    Serial.println(
        "Scheduler init : PASS");

    /*
     * Kernel tick.
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
     * Task A.
     */
    vrt_task_init(
        &taskA,
        taskAEntry,
        NULL,
        2U,
        stackA,
        2048U,
        "TaskA");

    /*
     * Task B.
     */
    vrt_task_init(
        &taskB,
        taskBEntry,
        NULL,
        2U,
        stackB,
        2048U,
        "TaskB");

    /*
     * Controller has higher priority and starts first.
     */
    vrt_task_init(
        &controller,
        controllerEntry,
        NULL,
        3U,
        controllerStack,
        2048U,
        "Controller");

    /*
     * Register tasks.
     */
    bool addA =
        vrt_scheduler_add_task(
            &scheduler,
            &taskA);

    bool addB =
        vrt_scheduler_add_task(
            &scheduler,
            &taskB);

    bool addController =
        vrt_scheduler_add_task(
            &scheduler,
            &controller);

    Serial.print(
        "Task A add : ");

    Serial.println(
        addA
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Task B add : ");

    Serial.println(
        addB
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Controller add : ");

    Serial.println(
        addController
            ? "PASS"
            : "FAIL");

    Serial.println(
        "Starting scheduler...");

    vrt_scheduler_start(
        &scheduler);
}

void loop()
{
    delay(1000);
}