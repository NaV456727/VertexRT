#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_preempt_timer.h"
#include "vrt_freertos_backend.h"

/*
 * ============================================================================
 * VertexRT STEP 10
 * Task Suspend / Resume / Exit Test
 * ============================================================================
 */

static vrt_scheduler_t *scheduler;

/*
 * ============================================================================
 * Tasks
 * ============================================================================
 */

static vrt_task_t taskA;
static vrt_task_t taskB;

/*
 * ============================================================================
 * Stacks
 * ============================================================================
 */

static uint32_t stackA[VRT_STACK_SIZE];
static uint32_t stackB[VRT_STACK_SIZE];

/*
 * ============================================================================
 * Test state
 * ============================================================================
 */

static volatile bool
    suspendPassed = false;

static volatile bool
    resumePassed = false;

static volatile bool
    exitPassed = false;

static volatile bool
    taskAStarted = false;

static volatile bool
    taskAResumed = false;

/*
 * ============================================================================
 * TASK A
 * ============================================================================
 *
 * Priority 2
 *
 * A will:
 *
 *     1. Start
 *     2. Suspend itself
 *     3. Resume later when B calls resume
 *     4. Exit
 * ============================================================================
 */

static void task_a(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println(
        "========== TASK A STARTED ==========");

    taskAStarted =
        true;

    Serial.printf(
        "A: state=%d current=%s\n",
        (int)taskA.state,
        scheduler->currentTask != NULL
            ? scheduler->currentTask->name
            : "NULL");

    /*
     * ------------------------------------------------------------------------
     * Suspend ourselves.
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "A: suspending itself...");

    vrt_task_suspend(
        &taskA);

    /*
     * We must NOT continue here until B resumes us.
     */
    Serial.println(
        "A: resumed after suspend.");

    taskAResumed =
        true;

    if (taskA.state ==
        VRT_TASK_RUNNING)
    {
        suspendPassed =
            true;

        Serial.println(
            "A: suspend/resume state = PASS");
    }
    else
    {
        Serial.printf(
            "A: unexpected state after resume = %d\n",
            (int)taskA.state);
    }

    /*
     * ------------------------------------------------------------------------
     * Exit ourselves.
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "A: exiting...");

    vrt_task_exit();

    /*
     * Should never return.
     */
    Serial.println(
        "A: ERROR - returned from vrt_task_exit().");

    for (;;)
    {
    }
}

/*
 * ============================================================================
 * TASK B
 * ============================================================================
 *
 * Priority 1
 *
 * B becomes the active task after A suspends itself.
 * ============================================================================
 */

static void task_b(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println(
        "========== TASK B STARTED ==========");

    /*
     * A should already have started and suspended itself.
     */
    if (taskAStarted &&
        taskA.state ==
            VRT_TASK_SUSPENDED)
    {
        Serial.println(
            "B: A is SUSPENDED.");

        suspendPassed =
            true;

        Serial.println(
            "B: suspend state verified.");
    }
    else
    {
        Serial.printf(
            "B: ERROR - A state=%d\n",
            (int)taskA.state);
    }

    /*
     * ------------------------------------------------------------------------
     * Resume A.
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "B: resuming A...");

    vrt_task_resume(
        &taskA);

    /*
     * A has priority 2 and should become the selected
     * task after resume.
     *
     * Give A a chance to run.
     */
    vrt_task_delay(2U);

    /*
     * ------------------------------------------------------------------------
     * Verify A resumed.
     * ------------------------------------------------------------------------
     */

    if (taskAResumed)
    {
        resumePassed =
            true;

        Serial.println(
            "B: A resume verified.");
    }
    else
    {
        Serial.println(
            "B: ERROR - A did not resume.");
    }

    /*
     * A should have exited by the time it returns
     * control to B.
     */
    vrt_task_delay(2U);

    if (taskA.state ==
        VRT_TASK_TERMINATED)
    {
        exitPassed =
            true;

        Serial.println(
            "B: A termination verified.");
    }
    else
    {
        Serial.printf(
            "B: ERROR - A state after exit=%d\n",
            (int)taskA.state);
    }

    /*
     * ------------------------------------------------------------------------
     * Final result
     * ------------------------------------------------------------------------
     */

    if (suspendPassed &&
        resumePassed &&
        exitPassed)
    {
        Serial.println();
        Serial.println(
            "====================================");

        Serial.println(
            "STEP 10 PASSED");

        Serial.println(
            "Task suspend: PASS");

        Serial.println(
            "Task resume: PASS");

        Serial.println(
            "Task exit: PASS");

        Serial.println(
            "====================================");
    }
    else
    {
        Serial.println();
        Serial.println(
            "====================================");

        Serial.println(
            "STEP 10 FAILED");

        Serial.printf(
            "suspend=%s\n",
            suspendPassed
                ? "PASS"
                : "FAIL");

        Serial.printf(
            "resume=%s\n",
            resumePassed
                ? "PASS"
                : "FAIL");

        Serial.printf(
            "exit=%s\n",
            exitPassed
                ? "PASS"
                : "FAIL");

        Serial.println(
            "====================================");
    }

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
        "VertexRT STEP 10");

    Serial.println(
        "Task Suspend / Resume / Exit Test");

    Serial.println(
        "====================================");

    scheduler =
        vrt_scheduler_get_instance();

    /*
     * ------------------------------------------------------------------------
     * Scheduler
     * ------------------------------------------------------------------------
     */

    vrt_scheduler_init(
        scheduler);

    /*
     * ------------------------------------------------------------------------
     * Task A
     * ------------------------------------------------------------------------
     */

    vrt_task_init(
        &taskA,
        task_a,
        NULL,
        2,
        stackA,
        VRT_STACK_SIZE,
        "taskA");

    /*
     * ------------------------------------------------------------------------
     * Task B
     * ------------------------------------------------------------------------
     */

    vrt_task_init(
        &taskB,
        task_b,
        NULL,
        1,
        stackB,
        VRT_STACK_SIZE,
        "taskB");

    /*
     * ------------------------------------------------------------------------
     * Add tasks
     * ------------------------------------------------------------------------
     */

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskA))
    {
        Serial.println(
            "ERROR: Task A add failed.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskB))
    {
        Serial.println(
            "ERROR: Task B add failed.");
        return;
    }

    /*
     * ------------------------------------------------------------------------
     * Timer
     * ------------------------------------------------------------------------
     */

    if (!vrt_preempt_timer_init())
    {
        Serial.println(
            "ERROR: timer initialization failed.");
        return;
    }

    if (!vrt_preempt_timer_start())
    {
        Serial.println(
            "ERROR: timer start failed.");
        return;
    }

    /*
     * ------------------------------------------------------------------------
     * Start with A
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "Starting Step 10 test...");

    scheduler->currentTask =
        &taskA;

    scheduler->running =
        true;

    taskA.state =
        VRT_TASK_RUNNING;

    taskB.state =
        VRT_TASK_READY;

    vrt_freertos_backend_start(
        &taskA);
}

/*
 * ============================================================================
 * LOOP
 * ============================================================================
 */

void loop()
{
    delay(1000);
}