#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_sync.h"
#include "vrt_preempt_timer.h"
#include "vrt_freertos_backend.h"

/*
 * ============================================================================
 * VertexRT STEP 8
 * Event Group Multiple-Waiter Test
 * ============================================================================
 *
 * Tests:
 *
 *     Waiter A -> BIT 0
 *     Waiter B -> BIT 1
 *
 * Setter performs:
 *
 *     set_bits(0x03)
 *
 * Both waiters must become READY from the same event update.
 *
 * clearOnExit is FALSE in this test so the two conditions can be
 * evaluated independently.
 * ============================================================================
 */

static vrt_scheduler_t *scheduler;

/*
 * Tasks
 */
static vrt_task_t waiterA;
static vrt_task_t waiterB;
static vrt_task_t setterTask;

/*
 * Stacks
 */
static uint32_t stackA[VRT_STACK_SIZE];
static uint32_t stackB[VRT_STACK_SIZE];
static uint32_t stackSetter[VRT_STACK_SIZE];

/*
 * Event group
 */
static vrt_event_group_t testEvents;

/*
 * Completion flags
 */
static volatile bool
    waiterAPassed = false;

static volatile bool
    waiterBPassed = false;

static volatile bool
    setterCompleted = false;

/*
 * ============================================================================
 * WAITER A
 * ============================================================================
 *
 * Priority 2
 * ============================================================================
 */

static void waiter_a_task(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println(
        "========== WAITER A STARTED ==========");

    Serial.println(
        "WAITER A: waiting for BIT 0...");

    uint32_t result =
        vrt_event_group_wait_bits(
            &testEvents,
            0x01U,
            false,
            false);

    Serial.printf(
        "WAITER A: woke bits=0x%02lX tick=%lu current=%s\n",
        (unsigned long)result,
        (unsigned long)scheduler->tickCount,
        scheduler->currentTask != NULL
            ? scheduler->currentTask->name
            : "NULL");

    if ((result & 0x01U) != 0U)
    {
        waiterAPassed =
            true;

        Serial.println(
            "WAITER A: BIT 0 received.");
    }

    for (;;)
    {
        vrt_task_delay(100U);
    }
}

/*
 * ============================================================================
 * WAITER B
 * ============================================================================
 *
 * Priority 3
 * ============================================================================
 */

static void waiter_b_task(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println(
        "========== WAITER B STARTED ==========");

    Serial.println(
        "WAITER B: waiting for BIT 1...");

    uint32_t result =
        vrt_event_group_wait_bits(
            &testEvents,
            0x02U,
            false,
            false);

    Serial.printf(
        "WAITER B: woke bits=0x%02lX tick=%lu current=%s\n",
        (unsigned long)result,
        (unsigned long)scheduler->tickCount,
        scheduler->currentTask != NULL
            ? scheduler->currentTask->name
            : "NULL");

    if ((result & 0x02U) != 0U)
    {
        waiterBPassed =
            true;

        Serial.println(
            "WAITER B: BIT 1 received.");
    }

    for (;;)
    {
        vrt_task_delay(100U);
    }
}

/*
 * ============================================================================
 * SETTER
 * ============================================================================
 *
 * Priority 1
 * ============================================================================
 */

static void setter_task(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println(
        "========== SETTER STARTED ==========");

    /*
     * Allow both waiters to enter their wait queues.
     */
    vrt_task_delay(5U);

    Serial.printf(
        "SETTER: setting BIT 0 | BIT 1 tick=%lu\n",
        (unsigned long)scheduler->tickCount);

    /*
     * This single operation should satisfy BOTH waiters.
     */
    vrt_event_group_set_bits(
        &testEvents,
        0x03U);

    setterCompleted =
        true;

    Serial.printf(
        "SETTER: event bits now=0x%02lX\n",
        (unsigned long)testEvents.bits);

    for (;;)
    {
        vrt_task_delay(100U);
    }
}

/*
 * ============================================================================
 * SETUP
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
        "VertexRT STEP 8");

    Serial.println(
        "Event Group Multiple-Waiter Test");

    Serial.println(
        "====================================");

    scheduler =
        vrt_scheduler_get_instance();

    /*
     * Scheduler
     */
    vrt_scheduler_init(
        scheduler);

    /*
     * Event group
     */
    vrt_event_group_init(
        &testEvents);

    /*
     * Waiter A
     */
    vrt_task_init(
        &waiterA,
        waiter_a_task,
        NULL,
        2,
        stackA,
        VRT_STACK_SIZE,
        "waiter_a");

    /*
     * Waiter B
     */
    vrt_task_init(
        &waiterB,
        waiter_b_task,
        NULL,
        3,
        stackB,
        VRT_STACK_SIZE,
        "waiter_b");

    /*
     * Setter
     */
    vrt_task_init(
        &setterTask,
        setter_task,
        NULL,
        1,
        stackSetter,
        VRT_STACK_SIZE,
        "setter");

    /*
     * Add tasks.
     *
     * Order is intentionally different from priority so that
     * scheduler selection must use priority rather than list order.
     */
    if (!vrt_scheduler_add_task(
            scheduler,
            &waiterA))
    {
        Serial.println(
            "ERROR: waiter A add failed.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &waiterB))
    {
        Serial.println(
            "ERROR: waiter B add failed.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &setterTask))
    {
        Serial.println(
            "ERROR: setter add failed.");
        return;
    }

    /*
     * Timer
     */
    if (!vrt_preempt_timer_init())
    {
        Serial.println(
            "ERROR: timer init failed.");
        return;
    }

    if (!vrt_preempt_timer_start())
    {
        Serial.println(
            "ERROR: timer start failed.");
        return;
    }

    Serial.println(
        "Starting Step 8 test...");

    /*
     * Start with the highest-priority waiter.
     */
    scheduler->currentTask =
        &waiterB;

    scheduler->running =
        true;

    waiterA.state =
        VRT_TASK_READY;

    waiterB.state =
        VRT_TASK_RUNNING;

    setterTask.state =
        VRT_TASK_READY;

    vrt_freertos_backend_start(
        &waiterB);
}

/*
 * ============================================================================
 * LOOP
 * ============================================================================
 */

void loop()
{
    /*
     * Let the tasks perform the actual test.
     */
    static bool reported = false;

    if (!reported &&
        waiterAPassed &&
        waiterBPassed &&
        setterCompleted)
    {
        reported = true;

        Serial.println();
        Serial.println(
            "====================================");

        Serial.println(
            "STEP 8 PASSED");

        Serial.println(
            "One event update woke multiple waiters.");

        Serial.println(
            "BIT 0 waiter: PASS");

        Serial.println(
            "BIT 1 waiter: PASS");

        Serial.println(
            "====================================");
    }

    delay(100);
}