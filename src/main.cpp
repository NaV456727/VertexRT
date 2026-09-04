#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_sync.h"
#include "vrt_freertos_backend.h"

static vrt_scheduler_t *scheduler;

static vrt_task_t taskL;
static vrt_task_t taskM;
static vrt_task_t taskH;

static vrt_mutex_t mutex;

static uint32_t stackL[VRT_STACK_SIZE];
static uint32_t stackM[VRT_STACK_SIZE];
static uint32_t stackH[VRT_STACK_SIZE];

static volatile bool testReady = false;

static volatile bool mAcquired = false;
static volatile bool hAcquired = false;

static volatile bool mSawLRestored = false;
static volatile bool hSawLRestored = false;

static const char *state_name(vrt_task_state_t state)
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
        return "?";
    }
}

static void dump_state(const char *tag)
{
    vrt_task_t *current =
        scheduler->currentTask;

    Serial.printf(
        "[%s] current=%s L=%s(p%u/base%u) M=%s(p%u/base%u) H=%s(p%u/base%u) ready=%lu wait=%lu owner=%s\n",
        tag,
        current != NULL ? current->name : "NULL",
        state_name(taskL.state),
        taskL.priority,
        taskL.basePriority,
        state_name(taskM.state),
        taskM.priority,
        taskM.basePriority,
        state_name(taskH.state),
        taskH.priority,
        taskH.basePriority,
        (unsigned long)vrt_list_size(
            &scheduler->readyQueue),
        (unsigned long)vrt_list_size(
            &mutex.waitQueue),
        mutex.owner != NULL
            ? mutex.owner->name
            : "NULL");
}

static void task_l(void *arg)
{
    (void)arg;

    Serial.println();
    Serial.println(
        "========== LOW TASK STARTED ==========");

    vrt_mutex_lock(
        &mutex);

    Serial.println(
        "L: mutex locked");

    vrt_task_resume(
        &taskM);

    testReady = true;

    Serial.println(
        "L: yielding for M...");

    vrt_task_yield();

    Serial.println();
    Serial.println(
        "========== L AFTER M BLOCKED ==========");

    dump_state(
        "L AFTER M");

    bool mBlocked =
        taskM.state ==
        VRT_TASK_BLOCKED;

    bool mInheritance2 =
        taskL.priority ==
        taskM.priority;

    Serial.printf(
        "M is BLOCKED: %s\n",
        mBlocked
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "L inherited M priority: %s (L=%u M=%u)\n",
        mInheritance2
            ? "PASS"
            : "FAIL",
        taskL.priority,
        taskM.priority);

    Serial.println();
    Serial.println(
        "L: resuming H...");

    vrt_task_resume(
        &taskH);

    Serial.println(
        "L: yielding for H...");

    vrt_task_yield();

    Serial.println();
    Serial.println(
        "========== L AFTER H BLOCKED ==========");

    dump_state(
        "L AFTER H");

    bool hBlocked =
        taskH.state ==
        VRT_TASK_BLOCKED;

    bool mStillBlocked =
        taskM.state ==
        VRT_TASK_BLOCKED;

    bool inheritance3 =
        taskL.priority ==
        taskH.priority;

    Serial.printf(
        "H is BLOCKED: %s\n",
        hBlocked
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "M still BLOCKED: %s\n",
        mStillBlocked
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "L inherited H priority: %s (L=%u H=%u)\n",
        inheritance3
            ? "PASS"
            : "FAIL",
        taskL.priority,
        taskH.priority);

    Serial.println();
    Serial.println(
        "L: unlocking mutex...");

    vrt_mutex_unlock(
        &mutex);

    Serial.println(
        "L: mutex unlocked");

    while (!mAcquired)
    {
        vrt_task_yield();
    }

    Serial.println();
    Serial.println(
        "========== L OBSERVED M ==========");

    dump_state(
        "L AFTER M ACQUIRED");

    bool mOwner =
        mAcquired;

    Serial.printf(
        "M acquired mutex: %s\n",
        mOwner
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "M owns mutex now: %s\n",
        mutex.owner == &taskM
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "L priority restored: %s (L=%u base=%u)\n",
        mSawLRestored
            ? "PASS"
            : "FAIL",
        taskL.priority,
        taskL.basePriority);

    while (!hAcquired)
    {
        vrt_task_yield();
    }

    Serial.println();
    Serial.println(
        "========== L OBSERVED H ==========");

    dump_state(
        "L AFTER H ACQUIRED");

    Serial.printf(
        "H acquired mutex: %s\n",
        hAcquired
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "H sees L restored: %s\n",
        hSawLRestored
            ? "PASS"
            : "FAIL");

    Serial.println();
    Serial.println(
        "========== STEP 4C RESULT ==========");

    Serial.printf(
        "M blocked           : %s\n",
        mBlocked
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "M inheritance       : %s\n",
        mInheritance2
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "H blocked           : %s\n",
        hBlocked
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "H inheritance       : %s\n",
        inheritance3
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "M acquired first    : %s\n",
        mAcquired
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "L restored          : %s\n",
        mSawLRestored
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "H acquired second   : %s\n",
        hAcquired
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "L restored final    : %s\n",
        hSawLRestored
            ? "PASS"
            : "FAIL");

    if (mBlocked &&
        mInheritance2 &&
        hBlocked &&
        inheritance3 &&
        mAcquired &&
        mSawLRestored &&
        hAcquired &&
        hSawLRestored)
    {
        Serial.println();
        Serial.println(
            "STEP 4C DIAGNOSTIC: PASS");
    }
    else
    {
        Serial.println();
        Serial.println(
            "STEP 4C DIAGNOSTIC: FAIL");
    }

    Serial.println(
        "====================================");

    for (;;)
    {
        vrt_task_yield();
    }
}

static void task_m(void *arg)
{
    (void)arg;

    while (!testReady)
    {
        vrt_task_yield();
    }

    Serial.println();
    Serial.println(
        "========== MEDIUM TASK STARTED ==========");

    Serial.println(
        "M: attempting mutex...");

    vrt_mutex_lock(
        &mutex);

    mAcquired =
        mutex.owner ==
        &taskM;

    mSawLRestored =
        taskL.priority ==
        taskL.basePriority;

    Serial.printf(
        "M owns mutex: %s\n",
        mAcquired
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "M sees L restored: %s (L=%u base=%u)\n",
        mSawLRestored
            ? "PASS"
            : "FAIL",
        taskL.priority,
        taskL.basePriority);

    vrt_mutex_unlock(
        &mutex);

    vrt_task_exit();
}

static void task_h(void *arg)
{
    (void)arg;

    while (!testReady)
    {
        vrt_task_yield();
    }

    Serial.println();
    Serial.println(
        "========== HIGH TASK STARTED ==========");

    Serial.println(
        "H: attempting mutex...");

    vrt_mutex_lock(
        &mutex);

    hAcquired =
        mutex.owner ==
        &taskH;

    hSawLRestored =
        taskL.priority ==
        taskL.basePriority;

    Serial.printf(
        "H owns mutex: %s\n",
        hAcquired
            ? "PASS"
            : "FAIL");

    Serial.printf(
        "H sees L restored: %s (L=%u base=%u)\n",
        hSawLRestored
            ? "PASS"
            : "FAIL",
        taskL.priority,
        taskL.basePriority);

    vrt_mutex_unlock(
        &mutex);

    vrt_task_exit();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(
        "============================================================");
    Serial.println(
        "VertexRT v0.2 STEP 4C");
    Serial.println(
        "MULTIPLE MUTEX WAITERS DIAGNOSTIC");
    Serial.println(
        "============================================================");

    scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        Serial.println(
            "ERROR: scheduler is NULL.");
        return;
    }

    vrt_scheduler_init(
        scheduler);

    vrt_mutex_init(
        &mutex);

    vrt_task_init(
        &taskL,
        task_l,
        NULL,
        1,
        stackL,
        VRT_STACK_SIZE,
        "taskL");

    vrt_task_init(
        &taskM,
        task_m,
        NULL,
        2,
        stackM,
        VRT_STACK_SIZE,
        "taskM");

    vrt_task_init(
        &taskH,
        task_h,
        NULL,
        3,
        stackH,
        VRT_STACK_SIZE,
        "taskH");

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskL))
    {
        Serial.println(
            "ERROR: taskL add failed.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskM))
    {
        Serial.println(
            "ERROR: taskM add failed.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskH))
    {
        Serial.println(
            "ERROR: taskH add failed.");
        return;
    }

    vrt_task_suspend(
        &taskM);

    vrt_task_suspend(
        &taskH);

    scheduler->currentTask =
        &taskL;

    taskL.state =
        VRT_TASK_RUNNING;

    scheduler->running =
        true;

    dump_state(
        "INITIAL");

    vrt_freertos_backend_start(
        &taskL);
}

void loop()
{
    delay(10);
}