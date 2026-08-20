#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_sync.h"
#include "vrt_queue.h"
#include "vrt_preempt_timer.h"
#include "vrt_freertos_backend.h"

static vrt_scheduler_t *scheduler;

static vrt_task_t taskA;
static vrt_task_t taskB;

static uint32_t stackA[VRT_STACK_SIZE];
static uint32_t stackB[VRT_STACK_SIZE];

static vrt_sem_t sem;
static vrt_mutex_t mutex;
static vrt_event_group_t events;

static vrt_queue_t queue;
static uint32_t queueStorage[3];

static volatile bool semPass = false;
static volatile bool mutexPass = false;
static volatile bool eventPass = false;
static volatile bool queuePass = false;

static volatile bool semReady = false;
static volatile bool eventReady = false;

static const uint32_t values[5] =
    {
        100, 200, 300, 400, 500};

static uint32_t received[5];
static uint32_t receivedCount = 0;

static void task_a(void *arg)
{
    (void)arg;

    Serial.println();
    Serial.println("========== TASK A ==========");

    /*
     * STEP 18A
     */
    Serial.println("18A: waiting semaphore...");

    vrt_sem_wait(&sem);

    Serial.println("18A: semaphore received.");
    semPass = true;

    /*
     * STEP 18B
     */
    Serial.println("18B: locking mutex...");

    vrt_mutex_lock(&mutex);

    Serial.println("18B: mutex acquired.");

    vrt_task_delay(4U);

    vrt_mutex_unlock(&mutex);

    Serial.println("18B: mutex released.");
    mutexPass = true;

    /*
     * STEP 19
     */
    Serial.println();
    Serial.println("========== STEP 19 ==========");

    eventReady = true;

    uint32_t result =
        vrt_event_group_wait_bits(
            &events,
            0x01U,
            false,
            true);

    Serial.printf(
        "19: event result=0x%02lX current=%s\n",
        (unsigned long)result,
        scheduler->currentTask != NULL
            ? scheduler->currentTask->name
            : "NULL");

    if (result == 0x01U)
    {
        eventPass = true;
        Serial.println("19: event PASS.");
    }

    /*
     * STEP 20
     */
    Serial.println();
    Serial.println("========== STEP 20 ==========");

    uint32_t value = 0;

    if (!vrt_queue_receive(
            &queue,
            &value))
    {
        Serial.println("20: initial receive failed.");
        return;
    }

    if (value != 100U)
    {
        Serial.println("20: FIFO failed at 100.");
        return;
    }

    received[receivedCount++] = value;

    Serial.println("20: received 100.");

    while (receivedCount < 5U)
    {
        if (!vrt_queue_receive(
                &queue,
                &value))
        {
            Serial.println("20: receive failed.");
            return;
        }

        Serial.printf(
            "20: received %lu\n",
            (unsigned long)value);

        if (value != values[receivedCount])
        {
            Serial.println("20: FIFO mismatch.");
            return;
        }

        received[receivedCount++] = value;

        vrt_task_yield();
    }

    queuePass = true;

    Serial.println("20: FIFO PASS.");

    /*
     * FINAL
     */
    Serial.println();
    Serial.println("====================================");
    Serial.println("FINAL REGRESSION TEST");
    Serial.println("====================================");

    if (semPass &&
        mutexPass &&
        eventPass &&
        queuePass)
    {
        Serial.println("STEP 18: PASS");
        Serial.println("STEP 19: PASS");
        Serial.println("STEP 20: PASS");
        Serial.println("STEP 21: PASS");
        Serial.println("------------------------------------");
        Serial.println("ALL FINAL TESTS PASSED");
    }
    else
    {
        Serial.println("FINAL TEST FAILED");

        Serial.printf(
            "Semaphore = %s\n",
            semPass ? "PASS" : "FAIL");

        Serial.printf(
            "Mutex = %s\n",
            mutexPass ? "PASS" : "FAIL");

        Serial.printf(
            "Event = %s\n",
            eventPass ? "PASS" : "FAIL");

        Serial.printf(
            "Queue = %s\n",
            queuePass ? "PASS" : "FAIL");
    }

    Serial.println(
        "====================================");

    for (;;)
    {
        vrt_task_delay(100U);
    }
}

static void task_b(void *arg)
{
    (void)arg;

    /*
     * STEP 18A signal
     */
    vrt_task_delay(3U);

    Serial.println(
        "18A: signaling semaphore.");

    vrt_sem_signal(&sem);

    /*
     * Wait until A has completed the mutex stage.
     */
    while (!mutexPass)
    {
        vrt_task_yield();
    }

    /*
     * STEP 19 event
     */
    while (!eventReady)
    {
        vrt_task_yield();
    }

    vrt_task_delay(2U);

    Serial.println(
        "19: setting BIT0.");

    vrt_event_group_set_bits(
        &events,
        0x01U);

    /*
     * STEP 20 queue producer
     */
    while (!eventPass)
    {
        vrt_task_yield();
    }

    /*
     * Send 100.
     */
    vrt_queue_send(
        &queue,
        &values[0]);

    /*
     * Fill queue.
     */
    for (uint32_t i = 1U; i < 4U; ++i)
    {
        vrt_queue_send(
            &queue,
            &values[i]);
    }

    Serial.println(
        "20: queue full, sending 500.");

    /*
     * Must block until A receives.
     */
    vrt_queue_send(
        &queue,
        &values[4]);

    Serial.println(
        "20: 500 sent after wake.");

    for (;;)
    {
        vrt_task_delay(100U);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(
        "====================================");
    Serial.println(
        "VertexRT FINAL REGRESSION TEST");
    Serial.println(
        "Steps 18 -> 21");
    Serial.println(
        "====================================");

    scheduler =
        vrt_scheduler_get_instance();

    vrt_scheduler_init(
        scheduler);

    vrt_sem_init(
        &sem,
        false);

    vrt_mutex_init(
        &mutex);

    vrt_event_group_init(
        &events);

    vrt_queue_init(
        &queue,
        queueStorage,
        sizeof(uint32_t),
        3U);

    vrt_task_init(
        &taskA,
        task_a,
        NULL,
        2,
        stackA,
        VRT_STACK_SIZE,
        "taskA");

    vrt_task_init(
        &taskB,
        task_b,
        NULL,
        3,
        stackB,
        VRT_STACK_SIZE,
        "taskB");

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskA))
    {
        Serial.println("A add failed.");
        return;
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &taskB))
    {
        Serial.println("B add failed.");
        return;
    }

    if (!vrt_preempt_timer_init())
    {
        Serial.println("Timer init failed.");
        return;
    }

    if (!vrt_preempt_timer_start())
    {
        Serial.println("Timer start failed.");
        return;
    }

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

void loop()
{
    delay(1000);
}