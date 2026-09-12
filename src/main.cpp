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
 * Step 12 configuration
 * ============================================================================
 */

#define STEP12_GPIO 27U
#define STEP12_REQUIRED_INTERRUPTS 3U

/*
 * ============================================================================
 * Scheduler / test task
 * ============================================================================
 */

static vrt_scheduler_t scheduler;
static vrt_task_t testTask;

static uint32_t testTaskStack[2048];

/*
 * ============================================================================
 * Interrupt test state
 * ============================================================================
 */

static volatile uint32_t isrCallbackCount = 0U;

static void IRAM_ATTR
step12_isr_callback(
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
 * Helper
 * ============================================================================
 */

static void printInterruptState()
{
    Serial.println();
    Serial.println("----------------------------------------");

    Serial.print(
        "Attached : ");

    Serial.println(
        vrt_interrupt_is_attached(
            STEP12_GPIO)
            ? "YES"
            : "NO");

    Serial.print(
        "Enabled  : ");

    Serial.println(
        vrt_interrupt_is_enabled(
            STEP12_GPIO)
            ? "YES"
            : "NO");

    Serial.print(
        "ISR count: ");

    Serial.println(
        (unsigned long)
            vrt_interrupt_get_count(
                STEP12_GPIO));

    Serial.print(
        "Callback count: ");

    Serial.println(
        (unsigned long)
            isrCallbackCount);

    Serial.println(
        "----------------------------------------");
}

/*
 * ============================================================================
 * Step 12 test task
 * ============================================================================
 */

static void step12TestTask(
    void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========================================");
    Serial.println("STEP 12: INTERRUPT MANAGEMENT");
    Serial.println("========================================");

    /*
     * ------------------------------------------------------------
     * TEST 1: Initialization
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println("TEST 1: INTERRUPT SUBSYSTEM INIT");

    bool initResult =
        vrt_interrupt_init();

    Serial.print(
        "Interrupt init : ");

    Serial.println(
        initResult
            ? "PASS"
            : "FAIL");

    if (!initResult)
    {
        Serial.println(
            "STEP 12 RESULT: FAIL");
        return;
    }

    /*
     * ------------------------------------------------------------
     * TEST 2: GPIO attachment
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println("TEST 2: GPIO INTERRUPT ATTACH");

    bool attachResult =
        vrt_interrupt_attach_gpio(
            STEP12_GPIO,
            VRT_INTERRUPT_FALLING,
            true,
            false,
            step12_isr_callback,
            (void *)&isrCallbackCount);

    Serial.print(
        "GPIO 27 attach : ");

    Serial.println(
        attachResult
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Attached state : ");

    Serial.println(
        vrt_interrupt_is_attached(
            STEP12_GPIO)
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Enabled state  : ");

    Serial.println(
        vrt_interrupt_is_enabled(
            STEP12_GPIO)
            ? "PASS"
            : "FAIL");

    if (!attachResult)
    {
        Serial.println(
            "STEP 12 RESULT: FAIL");
        return;
    }

    /*
     * ------------------------------------------------------------
     * TEST 3: Manual interrupt qualification
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println("========================================");
    Serial.println("TEST 3: LIVE GPIO INTERRUPT");
    Serial.println("========================================");

    Serial.println();
    Serial.println(
        "GPIO 27 uses internal pull-up.");

    Serial.println(
        "Leave the jumper disconnected initially.");

    Serial.println();
    Serial.println(
        "Now connect GPIO 27 to GND and release it.");

    Serial.println(
        "Repeat this 3 times.");

    Serial.println(
        "Each HIGH -> LOW transition generates one interrupt.");

    Serial.println();

    uint32_t startCount =
        vrt_interrupt_get_count(
            STEP12_GPIO);

    uint32_t timeoutMs =
        millis() + 15000UL;

    while (
        vrt_interrupt_get_count(
            STEP12_GPIO) <
            STEP12_REQUIRED_INTERRUPTS &&
        (int32_t)(millis() -
                  timeoutMs) < 0)
    {
        /*
         * Give the VertexRT scheduler regular opportunities
         * while waiting for the external interrupt.
         */
        vrt_task_delay(10U);
    }

    uint32_t interruptCount =
        vrt_interrupt_get_count(
            STEP12_GPIO);

    uint32_t callbackCount =
        isrCallbackCount;

    Serial.println();

    Serial.print(
        "Interrupts observed : ");

    Serial.println(
        (unsigned long)
            interruptCount);

    Serial.print(
        "Callbacks observed  : ");

    Serial.println(
        (unsigned long)
            callbackCount);

    bool interruptPass =
        (interruptCount >=
         STEP12_REQUIRED_INTERRUPTS);

    bool callbackPass =
        (callbackCount >=
         STEP12_REQUIRED_INTERRUPTS);

    Serial.print(
        "Interrupt delivery : ");

    Serial.println(
        interruptPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "ISR callback       : ");

    Serial.println(
        callbackPass
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------
     * TEST 4: Disable
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "TEST 4: INTERRUPT DISABLE");

    bool disableResult =
        vrt_interrupt_disable(
            STEP12_GPIO);

    Serial.print(
        "Disable call : ");

    Serial.println(
        disableResult
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Enabled state: ");

    Serial.println(
        !vrt_interrupt_is_enabled(
            STEP12_GPIO)
            ? "PASS"
            : "FAIL");

    uint32_t countBeforeDisabled =
        vrt_interrupt_get_count(
            STEP12_GPIO);

    Serial.println();
    Serial.println(
        "Touch GPIO 27 to GND once.");

    Serial.println(
        "This interrupt should NOT be counted.");

    /*
     * Wait up to 3 seconds.
     */
    uint32_t disableDeadline =
        millis() + 3000UL;

    while (
        (int32_t)(millis() -
                  disableDeadline) < 0)
    {
        vrt_task_delay(20U);
    }

    uint32_t countAfterDisabled =
        vrt_interrupt_get_count(
            STEP12_GPIO);

    bool disabledPass =
        (countAfterDisabled ==
         countBeforeDisabled);

    Serial.print(
        "Disabled interrupt blocked : ");

    Serial.println(
        disabledPass
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------
     * TEST 5: Re-enable
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "TEST 5: INTERRUPT RE-ENABLE");

    bool enableResult =
        vrt_interrupt_enable(
            STEP12_GPIO);

    Serial.print(
        "Enable call : ");

    Serial.println(
        enableResult
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Enabled state: ");

    Serial.println(
        vrt_interrupt_is_enabled(
            STEP12_GPIO)
            ? "PASS"
            : "FAIL");

    uint32_t countBeforeReenable =
        vrt_interrupt_get_count(
            STEP12_GPIO);

    Serial.println();
    Serial.println(
        "Connect GPIO 27 to GND and release it once.");

    uint32_t reenableDeadline =
        millis() + 5000UL;

    while (
        vrt_interrupt_get_count(
            STEP12_GPIO) <=
            countBeforeReenable &&
        (int32_t)(millis() -
                  reenableDeadline) < 0)
    {
        vrt_task_delay(10U);
    }

    uint32_t countAfterReenable =
        vrt_interrupt_get_count(
            STEP12_GPIO);

    bool reenablePass =
        (countAfterReenable >
         countBeforeReenable);

    Serial.print(
        "Re-enabled interrupt delivered : ");

    Serial.println(
        reenablePass
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------
     * TEST 6: Counter reset
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "TEST 6: INTERRUPT COUNTER RESET");

    bool resetResult =
        vrt_interrupt_reset_count(
            STEP12_GPIO);

    uint32_t countAfterReset =
        vrt_interrupt_get_count(
            STEP12_GPIO);

    bool resetPass =
        resetResult &&
        countAfterReset == 0U;

    Serial.print(
        "Counter reset : ");

    Serial.println(
        resetPass
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------
     * Test 7: Detach
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "TEST 7: INTERRUPT DETACH");

    bool detachResult =
        vrt_interrupt_detach_gpio(
            STEP12_GPIO);

    Serial.print(
        "Detach call : ");

    Serial.println(
        detachResult
            ? "PASS"
            : "FAIL");

    bool detached =
        !vrt_interrupt_is_attached(
            STEP12_GPIO);

    bool disabledAfterDetach =
        !vrt_interrupt_is_enabled(
            STEP12_GPIO);

    Serial.print(
        "Detached state : ");

    Serial.println(
        detached
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Disabled after detach : ");

    Serial.println(
        disabledAfterDetach
            ? "PASS"
            : "FAIL");

    /*
     * ------------------------------------------------------------
     * Final state
     * ------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "========================================");

    Serial.println(
        "STEP 12 RESULT");

    Serial.println(
        "========================================");

    bool result =
        initResult &&
        attachResult &&
        interruptPass &&
        callbackPass &&
        disableResult &&
        disabledPass &&
        enableResult &&
        reenablePass &&
        resetPass &&
        detachResult &&
        detached &&
        disabledAfterDetach;

    Serial.print(
        "Interrupt subsystem init : ");

    Serial.println(
        initResult
            ? "PASS"
            : "FAIL");

    Serial.print(
        "GPIO attachment          : ");

    Serial.println(
        attachResult
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Interrupt delivery       : ");

    Serial.println(
        interruptPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "ISR callback             : ");

    Serial.println(
        callbackPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Disable control          : ");

    Serial.println(
        disabledPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Re-enable control        : ");

    Serial.println(
        reenablePass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Counter reset            : ");

    Serial.println(
        resetPass
            ? "PASS"
            : "FAIL");

    Serial.print(
        "Detach cleanup           : ");

    Serial.println(
        (detachResult &&
         detached &&
         disabledAfterDetach)
            ? "PASS"
            : "FAIL");

    Serial.println();
    Serial.println(
        "----------------------------------------");

    Serial.print(
        "STEP 12 RESULT: ");

    Serial.println(
        result
            ? "PASS"
            : "FAIL");

    Serial.println(
        "----------------------------------------");

    /*
     * Leave the test task alive.
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
        "VertexRT Step 12 Test");

    Serial.println(
        "========================================");

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
     * Test task.
     */
    vrt_task_init(
        &testTask,
        step12TestTask,
        NULL,
        3U,
        testTaskStack,
        2048U,
        "Step12");

    bool taskAdded =
        vrt_scheduler_add_task(
            &scheduler,
            &testTask);

    Serial.print(
        "Test task add : ");

    Serial.println(
        taskAdded
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