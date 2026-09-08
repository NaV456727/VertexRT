#include <Arduino.h>

extern "C"
{
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_timer.h"
#include "vrt_tick.h"
#include "vrt_config.h"
}

/*=========================================================
 * Software Timers
 *=========================================================*/

static vrt_timer_t oneShotTimer;
static vrt_timer_t periodicTimer;
static vrt_timer_t restartTimer;

/*=========================================================
 * Test Task Stack
 *=========================================================*/

static uint32_t step8TaskStack[VRT_STACK_SIZE];

/*=========================================================
 * Test Counters
 *=========================================================*/

static volatile uint32_t oneShotCount = 0;
static volatile uint32_t periodicCount = 0;
static volatile uint32_t restartCount = 0;

/*=========================================================
 * Timer Callbacks
 *=========================================================*/

static void oneShotCallback(void *argument)
{
    (void)argument;

    oneShotCount++;

    Serial.printf(
        "One-shot timer callback: %lu\n",
        (unsigned long)oneShotCount);
}

static void periodicCallback(void *argument)
{
    (void)argument;

    periodicCount++;

    Serial.printf(
        "Periodic timer callback: %lu\n",
        (unsigned long)periodicCount);
}

static void restartCallback(void *argument)
{
    (void)argument;

    restartCount++;

    Serial.printf(
        "Restart timer callback: %lu\n",
        (unsigned long)restartCount);
}

/*=========================================================
 * Step 8 Test Task
 *=========================================================*/

static void step8TestTask(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========================================");
    Serial.println("STEP 8: SOFTWARE TIMER QUALIFICATION");
    Serial.println("========================================");

    /*-----------------------------------------------------
     * Initialize software timer subsystem
     *-----------------------------------------------------*/

    vrt_timer_system_init();

    /*-----------------------------------------------------
     * Create timers
     *-----------------------------------------------------*/

    bool oneShotCreated =
        vrt_timer_create(
            &oneShotTimer,
            10U,
            false,
            oneShotCallback,
            NULL);

    bool periodicCreated =
        vrt_timer_create(
            &periodicTimer,
            5U,
            true,
            periodicCallback,
            NULL);

    bool restartCreated =
        vrt_timer_create(
            &restartTimer,
            20U,
            false,
            restartCallback,
            NULL);

    if (!oneShotCreated ||
        !periodicCreated ||
        !restartCreated)
    {
        Serial.println("Timer creation : FAIL");
        Serial.println("STEP 8 RESULT: FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println("Timer creation : PASS");

    /*-----------------------------------------------------
     * Start one-shot and periodic timers
     *-----------------------------------------------------*/

    bool oneShotStarted =
        vrt_timer_start(&oneShotTimer);

    bool periodicStarted =
        vrt_timer_start(&periodicTimer);

    if (!oneShotStarted || !periodicStarted)
    {
        Serial.println("Timer start : FAIL");
        Serial.println("STEP 8 RESULT: FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println("Timer start : PASS");

    /*-----------------------------------------------------
     * Allow timers to run
     *-----------------------------------------------------*/

    delay(250);

    uint32_t oneShotAfterFirstRun =
        oneShotCount;

    uint32_t periodicAfterFirstRun =
        periodicCount;

    bool oneShotPass =
        (oneShotAfterFirstRun == 1U);

    bool periodicPass =
        (periodicAfterFirstRun >= 3U);

    Serial.printf(
        "One-shot execution : %s (%lu)\n",
        oneShotPass ? "PASS" : "FAIL",
        (unsigned long)oneShotAfterFirstRun);

    Serial.printf(
        "Periodic execution : %s (%lu)\n",
        periodicPass ? "PASS" : "FAIL",
        (unsigned long)periodicAfterFirstRun);

    /*-----------------------------------------------------
     * Stop periodic timer
     *-----------------------------------------------------*/

    bool periodicStopped =
        vrt_timer_stop(&periodicTimer);

    uint32_t periodicBeforeStopWait =
        periodicCount;

    delay(150);

    uint32_t periodicAfterStopWait =
        periodicCount;

    bool periodicStopPass =
        periodicStopped &&
        (periodicAfterStopWait == periodicBeforeStopWait);

    Serial.printf(
        "Periodic stop : %s\n",
        periodicStopPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Start restart timer
     *-----------------------------------------------------*/

    bool restartStarted =
        vrt_timer_start(&restartTimer);

    delay(250);

    uint32_t restartAfterFirstStart =
        restartCount;

    bool restartFirstPass =
        restartStarted &&
        (restartAfterFirstStart == 1U);

    Serial.printf(
        "One-shot restart timer : %s (%lu)\n",
        restartFirstPass ? "PASS" : "FAIL",
        (unsigned long)restartAfterFirstStart);

    /*-----------------------------------------------------
     * Start the same timer again
     *-----------------------------------------------------*/

    bool restartStartedAgain =
        vrt_timer_start(&restartTimer);

    delay(250);

    uint32_t restartAfterSecondStart =
        restartCount;

    bool restartSecondPass =
        restartStartedAgain &&
        (restartAfterSecondStart == 2U);

    Serial.printf(
        "Timer restart : %s (%lu)\n",
        restartSecondPass ? "PASS" : "FAIL",
        (unsigned long)restartAfterSecondStart);

    /*-----------------------------------------------------
     * Delete timers
     *-----------------------------------------------------*/

    bool oneShotDeleted =
        vrt_timer_delete(&oneShotTimer);

    bool periodicDeleted =
        vrt_timer_delete(&periodicTimer);

    bool restartDeleted =
        vrt_timer_delete(&restartTimer);

    bool deletePass =
        oneShotDeleted &&
        periodicDeleted &&
        restartDeleted;

    Serial.printf(
        "Timer deletion : %s\n",
        deletePass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Deleted timer must not restart
     *-----------------------------------------------------*/

    bool deletedRestart =
        vrt_timer_start(&oneShotTimer);

    bool deletedRestartPass =
        (deletedRestart == false);

    Serial.printf(
        "Deleted timer cannot restart : %s\n",
        deletedRestartPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Final result
     *-----------------------------------------------------*/

    bool overallPass =
        oneShotPass &&
        periodicPass &&
        periodicStopPass &&
        restartFirstPass &&
        restartSecondPass &&
        deletePass &&
        deletedRestartPass;

    Serial.println();
    Serial.println("----------------------------------------");

    if (overallPass)
    {
        Serial.println("STEP 8 RESULT: PASS");
    }
    else
    {
        Serial.println("STEP 8 RESULT: FAIL");
    }

    Serial.println("----------------------------------------");

    for (;;)
    {
        delay(1000);
    }
}

/*=========================================================
 * Arduino Setup
 *=========================================================*/

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("VertexRT Step 8 Test");
    Serial.println("========================================");

    /*-----------------------------------------------------
     * Get scheduler
     *-----------------------------------------------------*/

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        Serial.println("Scheduler instance : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    /*-----------------------------------------------------
     * Initialize scheduler
     *-----------------------------------------------------*/

    vrt_scheduler_init(scheduler);

    Serial.println("Scheduler init : PASS");

    /*-----------------------------------------------------
     * Initialize software timer subsystem
     *-----------------------------------------------------*/

    vrt_timer_system_init();

    Serial.println("Software timer system init : PASS");

    /*-----------------------------------------------------
     * Initialize and start kernel tick
     *-----------------------------------------------------*/

    if (!vrt_tick_init())
    {
        Serial.println("Kernel tick init : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println("Kernel tick init : PASS");

    if (!vrt_tick_start())
    {
        Serial.println("Kernel tick start : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println("Kernel tick start : PASS");

    /*-----------------------------------------------------
     * Create Step 8 test task
     *-----------------------------------------------------*/

    static vrt_task_t testTask;

    vrt_task_init(
        &testTask,
        step8TestTask,
        NULL,
        2,
        step8TaskStack,
        VRT_STACK_SIZE,
        "Step8");

    /*-----------------------------------------------------
     * Add task to scheduler
     *-----------------------------------------------------*/

    bool taskAdded =
        vrt_scheduler_add_task(
            scheduler,
            &testTask);

    if (!taskAdded)
    {
        Serial.println("Test task add : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println("Test task ready : PASS");

    /*-----------------------------------------------------
     * Start scheduler
     *-----------------------------------------------------*/

    Serial.println("Starting scheduler...");

    vrt_scheduler_start(scheduler);

    /* Should never return */
    for (;;)
    {
        delay(1000);
    }
}

/*=========================================================
 * Arduino Loop
 *=========================================================*/

void loop()
{
    delay(1000);
}