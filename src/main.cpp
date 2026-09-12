#include <Arduino.h>

extern "C"
{
#include "vrt_config.h"
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_tick.h"
#include "vrt_interrupt.h"
}

/*
 * ============================================================================
 * STEP 13
 * ISR -> TASK NOTIFICATION
 * ============================================================================
 */

#define STEP13_GPIO 27U
#define STEP13_REQUIRED_WAKES 3U

static vrt_scheduler_t scheduler;

static vrt_task_t lowTask;
static vrt_task_t highTask;

static uint32_t lowStack[2048];
static uint32_t highStack[2048];

/*
 * ============================================================================
 * Test state
 * ============================================================================
 */

static volatile uint32_t isrCallbackCount = 0U;

static volatile uint32_t highWakeCount = 0U;
static volatile uint32_t lowIterations = 0U;

static volatile bool highStarted = false;
static volatile bool highFinished = false;

/*
 * Interrupt count observed before the test begins.
 */
static uint32_t interruptBaseline = 0U;

/*
 * ============================================================================
 * ISR callback
 * ============================================================================
 *
 * Keep this extremely small.
 */
static void IRAM_ATTR
step13_isr_callback(
    void *argument)
{
    volatile uint32_t *counter =
        (volatile uint32_t *)argument;

    if (counter != NULL)
    {
        (*counter)++;
    }
}

/*
 * ============================================================================
 * Low-priority task
 * ============================================================================
 */

static void lowTaskEntry(
    void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println(
        "[LOW TASK] started.");

    while (true)
    {
        lowIterations++;

        /*
         * The low-priority task continues executing while
         * the high-priority task is blocked.
         */
        vrt_task_yield();
    }
}

/*
 * ============================================================================
 * High-priority interrupt waiter
 * ============================================================================
 */

static void highTaskEntry(
    void *argument)
{
    (void)argument;

    highStarted = true;

    Serial.println();
    Serial.println(
        "[HIGH TASK] started.");

    Serial.println(
        "[HIGH TASK] waiting for GPIO 27 interrupts.");

    /*
     * ------------------------------------------------------------------------
     * Wait for exactly three task notifications.
     * ------------------------------------------------------------------------
     */
    while (highWakeCount <
           STEP13_REQUIRED_WAKES)
    {
        uint32_t countBefore =
            vrt_interrupt_get_count(
                STEP13_GPIO);

        uint32_t callbackBefore =
            isrCallbackCount;

        bool result =
            vrt_interrupt_wait(
                STEP13_GPIO);

        if (!result)
        {
            Serial.println();
            Serial.println(
                "[HIGH TASK] ERROR: interrupt wait failed.");

            break;
        }

        highWakeCount++;

        uint32_t countAfter =
            vrt_interrupt_get_count(
                STEP13_GPIO);

        uint32_t callbackAfter =
            isrCallbackCount;

        Serial.println();
        Serial.println(
            "----------------------------------------");

        Serial.print(
            "[HIGH TASK] WAKE #");

        Serial.println(
            (unsigned long)
                highWakeCount);

        Serial.print(
            "Interrupt count before wait : ");

        Serial.println(
            (unsigned long)
                countBefore);

        Serial.print(
            "Interrupt count after wake  : ");

        Serial.println(
            (unsigned long)
                countAfter);

        Serial.print(
            "Callback count before      : ");

        Serial.println(
            (unsigned long)
                callbackBefore);

        Serial.print(
            "Callback count after       : ");

        Serial.println(
            (unsigned long)
                callbackAfter);

        Serial.print(
            "Low task iterations        : ");

        Serial.println(
            (unsigned long)
                lowIterations);

        Serial.println(
            "----------------------------------------");
    }

    highFinished = true;

    Serial.println();
    Serial.println(
        "[HIGH TASK] Required notifications received.");

    /*
     * Do not terminate the FreeRTOS backing task.
     */
    for (;;)
    {
        vrt_task_yield();
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
    Serial.println(
        "========================================");

    Serial.println(
        "VertexRT Step 13 Test");

    Serial.println(
        "ISR -> TASK NOTIFICATION");

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
     * Kernel tick
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
     * Interrupt subsystem
     * ------------------------------------------------------------------------
     */

    bool interruptInit =
        vrt_interrupt_init();

    Serial.print(
        "Interrupt subsystem init : ");

    Serial.println(
        interruptInit
            ? "PASS"
            : "FAIL");

    if (!interruptInit)
    {
        return;
    }

    /*
     * Attach GPIO 27 as a falling-edge interrupt.
     */
    bool attachResult =
        vrt_interrupt_attach_gpio(
            STEP13_GPIO,
            VRT_INTERRUPT_FALLING,
            true,
            false,
            step13_isr_callback,
            (void *)&isrCallbackCount);

    Serial.print(
        "GPIO 27 interrupt attach : ");

    Serial.println(
        attachResult
            ? "PASS"
            : "FAIL");

    if (!attachResult)
    {
        return;
    }

    /*
     * Reset all counters before beginning the actual qualification.
     */
    vrt_interrupt_reset_count(
        STEP13_GPIO);

    isrCallbackCount = 0U;

    interruptBaseline =
        vrt_interrupt_get_count(
            STEP13_GPIO);

    /*
     * ------------------------------------------------------------------------
     * Tasks
     * ------------------------------------------------------------------------
     */

    /*
     * Low priority = 1.
     */
    vrt_task_init(
        &lowTask,
        lowTaskEntry,
        NULL,
        1U,
        lowStack,
        2048U,
        "LowTask");

    /*
     * High priority = 3.
     */
    vrt_task_init(
        &highTask,
        highTaskEntry,
        NULL,
        3U,
        highStack,
        2048U,
        "HighTask");

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
     * User instructions
     * ------------------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "========================================");

    Serial.println(
        "STEP 13 LIVE TEST");

    Serial.println(
        "========================================");

    Serial.println();
    Serial.println(
        "GPIO 27 has an internal pull-up.");

    Serial.println(
        "Leave it disconnected initially.");

    Serial.println();

    Serial.println(
        "The HIGH task will block on:");

    Serial.println(
        "    vrt_interrupt_wait(27)");

    Serial.println();

    Serial.println(
        "The LOW task will continue running.");

    Serial.println();

    Serial.println(
        "Connect GPIO 27 to GND and release it.");

    Serial.println(
        "Repeat the operation THREE times.");

    Serial.println();

    Serial.println(
        "Expected for each interrupt:");

    Serial.println(
        "    GPIO interrupt");

    Serial.println(
        "        -> ISR");

    Serial.println(
        "        -> pending notification");

    Serial.println(
        "        -> HIGH task READY");

    Serial.println(
        "        -> HIGH task wakes");

    Serial.println();

    Serial.println(
        "Starting scheduler...");

    vrt_scheduler_start(
        &scheduler);
}

/*
 * ============================================================================
 * Arduino loop
 * ============================================================================
 */

void loop()
{
    /*
     * VertexRT owns scheduling after startup.
     */
    delay(1000);
}