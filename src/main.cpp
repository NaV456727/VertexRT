#include <Arduino.h>

extern "C"
{
#include "vrt_config.h"
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_tick.h"
}

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * ============================================================================
 * Step 10 Diagnostic Test State
 * ============================================================================
 */

static vrt_scheduler_t scheduler;
static vrt_task_t testTask;

static uint32_t step10TaskStack[2048];

static volatile bool testComplete = false;

/*
 * ============================================================================
 * Utility
 * ============================================================================
 */

static void print_separator()
{
    Serial.println("----------------------------------------");
}

/*
 * ============================================================================
 * Stack Consumption Test
 * ============================================================================
 *
 * This function reports the FreeRTOS high-water mark while the local
 * buffer is actually alive.
 */

__attribute__((noinline)) static void consume_stack_once()
{
    volatile uint8_t buffer[512];

    /*
     * Force every byte to be used.
     */
    for (size_t i = 0U;
         i < sizeof(buffer);
         ++i)
    {
        buffer[i] =
            (uint8_t)(i ^ 0x5AU);
    }

    volatile uint8_t check =
        buffer[sizeof(buffer) - 1U];

    (void)check;

    /*
     * IMPORTANT:
     * This measurement happens while buffer[] still exists.
     */
    UBaseType_t insideHighWaterWords =
        uxTaskGetStackHighWaterMark(NULL);

    Serial.println();
    Serial.println("[DIAGNOSTIC] Inside consume_stack_once()");

    Serial.print(
        "FreeRTOS high-water words : ");

    Serial.println(
        (unsigned long)insideHighWaterWords);

    Serial.print(
        "FreeRTOS high-water bytes : ");

    Serial.println(
        (unsigned long)(insideHighWaterWords *
                        sizeof(StackType_t)));

    Serial.print(
        "sizeof(StackType_t)       : ");

    Serial.println(
        (unsigned long)sizeof(StackType_t));

    Serial.println(
        "[DIAGNOSTIC] Leaving consume_stack_once()");
}

/*
 * ============================================================================
 * Step 10 Test Task
 * ============================================================================
 */

static void step10TestTask(
    void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========================================");
    Serial.println("STEP 10: STACK MONITORING DIAGNOSTIC");
    Serial.println("========================================");

    /*
     * ------------------------------------------------------------------------
     * Identify the actual FreeRTOS backing task.
     * ------------------------------------------------------------------------
     */

    TaskHandle_t currentHandle =
        xTaskGetCurrentTaskHandle();

    const char *currentName =
        pcTaskGetName(currentHandle);

    Serial.println();
    Serial.println("[DIAGNOSTIC] Current FreeRTOS task");

    Serial.print(
        "Task name                 : ");

    Serial.println(
        currentName != NULL
            ? currentName
            : "<null>");

    Serial.print(
        "Task handle               : 0x");

    Serial.println(
        (unsigned long)(uintptr_t)currentHandle,
        HEX);

    Serial.print(
        "StackType_t size          : ");

    Serial.println(
        (unsigned long)sizeof(StackType_t));

    Serial.print(
        "Configured backing stack  : ");

    Serial.print(
        2048U);

    Serial.print(
        " words / ");

    Serial.print(
        (unsigned long)(2048U *
                        sizeof(StackType_t)));

    Serial.println(
        " bytes");

    /*
     * ------------------------------------------------------------------------
     * RAW FreeRTOS measurement #1
     * ------------------------------------------------------------------------
     */

    UBaseType_t initialRawWords =
        uxTaskGetStackHighWaterMark(NULL);

    size_t initialRawBytes =
        (size_t)initialRawWords *
        sizeof(StackType_t);

    Serial.println();
    Serial.println(
        "[DIAGNOSTIC] Initial FreeRTOS measurement");

    Serial.print(
        "Raw high-water words     : ");

    Serial.println(
        (unsigned long)initialRawWords);

    Serial.print(
        "Raw high-water bytes     : ");

    Serial.println(
        (unsigned long)initialRawBytes);

    /*
     * ------------------------------------------------------------------------
     * VertexRT measurement #1
     * ------------------------------------------------------------------------
     */

    size_t initialFree =
        vrt_task_stack_free(&testTask);

    size_t initialUsed =
        vrt_task_stack_used(&testTask);

    size_t initialHighWater =
        vrt_task_stack_high_water_mark(
            &testTask);

    Serial.println();
    Serial.println(
        "[DIAGNOSTIC] Initial VertexRT measurement");

    Serial.print(
        "vrt_task_stack_free()    : ");

    Serial.println(
        (unsigned long)initialFree);

    Serial.print(
        "vrt_task_stack_used()    : ");

    Serial.println(
        (unsigned long)initialUsed);

    Serial.print(
        "vrt_task_stack_high_water_mark() : ");

    Serial.println(
        (unsigned long)initialHighWater);

    /*
     * ------------------------------------------------------------------------
     * Basic initial-state qualification
     * ------------------------------------------------------------------------
     */

    bool initialState =
        (initialRawWords > 0U &&
         initialFree > 0U &&
         initialUsed > 0U);

    Serial.println();
    Serial.print(
        "Initial stack state : ");

    Serial.println(
        initialState
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------------------
     * Measure immediately before consuming stack.
     * ------------------------------------------------------------------------
     */

    UBaseType_t beforeWords =
        uxTaskGetStackHighWaterMark(NULL);

    Serial.println();
    Serial.println(
        "[DIAGNOSTIC] Before consume_stack_once()");

    Serial.print(
        "FreeRTOS high-water words : ");

    Serial.println(
        (unsigned long)beforeWords);

    /*
     * ------------------------------------------------------------------------
     * Consume stack.
     * ------------------------------------------------------------------------
     */

    consume_stack_once();

    /*
     * ------------------------------------------------------------------------
     * Measure after returning.
     * ------------------------------------------------------------------------
     */

    UBaseType_t afterWords =
        uxTaskGetStackHighWaterMark(NULL);

    Serial.println();
    Serial.println(
        "[DIAGNOSTIC] After consume_stack_once()");

    Serial.print(
        "FreeRTOS high-water words : ");

    Serial.println(
        (unsigned long)afterWords);

    Serial.print(
        "FreeRTOS high-water bytes : ");

    Serial.println(
        (unsigned long)((size_t)afterWords *
                        sizeof(StackType_t)));

    /*
     * ------------------------------------------------------------------------
     * VertexRT measurement #2
     * ------------------------------------------------------------------------
     */

    size_t finalFree =
        vrt_task_stack_free(&testTask);

    size_t finalUsed =
        vrt_task_stack_used(&testTask);

    size_t finalHighWater =
        vrt_task_stack_high_water_mark(
            &testTask);

    Serial.println();
    Serial.println(
        "[DIAGNOSTIC] Final VertexRT measurement");

    Serial.print(
        "vrt_task_stack_free()    : ");

    Serial.println(
        (unsigned long)finalFree);

    Serial.print(
        "vrt_task_stack_used()    : ");

    Serial.println(
        (unsigned long)finalUsed);

    Serial.print(
        "vrt_task_stack_high_water_mark() : ");

    Serial.println(
        (unsigned long)finalHighWater);

    /*
     * ------------------------------------------------------------------------
     * Compare raw FreeRTOS result directly.
     * ------------------------------------------------------------------------
     */

    bool rawChanged =
        (afterWords < beforeWords);

    Serial.println();
    Serial.print(
        "Raw FreeRTOS watermark changed : ");

    Serial.println(
        rawChanged
            ? "YES"
            : "NO");

    if (beforeWords >= afterWords)
    {
        Serial.print(
            "Raw word difference            : ");

        Serial.println(
            (unsigned long)(beforeWords -
                            afterWords));

        Serial.print(
            "Raw byte difference            : ");

        Serial.println(
            (unsigned long)((beforeWords -
                             afterWords) *
                            sizeof(StackType_t)));
    }

    /*
     * ------------------------------------------------------------------------
     * Compare VertexRT result.
     * ------------------------------------------------------------------------
     */

    bool vertexChanged =
        (finalHighWater < initialHighWater &&
         finalUsed > initialUsed);

    Serial.print(
        "VertexRT usage detected        : ");

    Serial.println(
        vertexChanged
            ? "YES"
            : "NO");

    /*
     * ------------------------------------------------------------------------
     * Check API consistency.
     * ------------------------------------------------------------------------
     */

    bool apiConsistent =
        (finalHighWater ==
         finalFree);

    Serial.print(
        "VertexRT API consistency       : ");

    Serial.println(
        apiConsistent
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------------------
     * Check accounting.
     * ------------------------------------------------------------------------
     */

    size_t totalStackBytes =
        2048U *
        sizeof(StackType_t);

    bool accountingValid =
        (finalUsed +
             finalHighWater ==
         totalStackBytes);

    Serial.print(
        "Stack accounting               : ");

    Serial.println(
        accountingValid
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------------------
     * Diagnostic conclusion.
     * ------------------------------------------------------------------------
     */

    print_separator();

    Serial.println(
        "[DIAGNOSTIC] CONCLUSION");

    if (!rawChanged)
    {
        Serial.println(
            "FreeRTOS itself did NOT detect additional stack usage.");

        Serial.println(
            "This means the issue is in the test workload / actual");
        Serial.println(
            "execution stack, not the VertexRT conversion.");
    }
    else if (rawChanged &&
             !vertexChanged)
    {
        Serial.println(
            "FreeRTOS detected stack usage, but VertexRT did not.");

        Serial.println(
            "This points to vrt_task_stack_* conversion/API logic.");
    }
    else
    {
        Serial.println(
            "Both FreeRTOS and VertexRT detected stack usage.");

        Serial.println(
            "The stack-monitoring mechanism is functioning.");
    }

    print_separator();

    /*
     * ------------------------------------------------------------------------
     * Existing qualification result
     * ------------------------------------------------------------------------
     */

    bool result =
        initialState &&
        vertexChanged &&
        apiConsistent &&
        accountingValid;

    Serial.print(
        "STEP 10 RESULT: ");

    Serial.println(
        result
            ? "PASS"
            : "FAIL");

    Serial.println(
        "----------------------------------------");

    testComplete = true;

    for (;;)
    {
        vrt_task_yield();
    }
}

/*
 * ============================================================================
 * Arduino Setup
 * ============================================================================
 */

void setup()
{
    Serial.begin(
        115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("VertexRT Step 10 Test");
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
    vrt_tick_init();
    vrt_tick_start();

    Serial.println(
        "Kernel tick : PASS");

    /*
     * VertexRT task.
     */
    vrt_task_init(
        &testTask,
        step10TestTask,
        NULL,
        2,
        step10TaskStack,
        2048,
        "Step10");

    bool taskReady =
        (testTask.state ==
         VRT_TASK_READY);

    Serial.print(
        "Test task ready : ");

    Serial.println(
        taskReady
            ? "PASS"
            : "FAIL");

    /*
     * Register task.
     */
    vrt_scheduler_add_task(
        &scheduler,
        &testTask);

    Serial.println(
        "Starting scheduler...");

    /*
     * Start VertexRT.
     */
    vrt_scheduler_start(
        &scheduler);
}

/*
 * ============================================================================
 * Arduino Loop
 * ============================================================================
 */

void loop()
{
    delay(1000);
}