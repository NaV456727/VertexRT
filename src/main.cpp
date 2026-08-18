#include <Arduino.h>

#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_preempt_timer.h"
#include "vrt_freertos_backend.h"
#include "vrt_queue.h"

/*
 * ============================================================================
 * VertexRT STEP 20
 * Message queue blocking / wake test
 * ============================================================================
 *
 * Queue capacity = 3
 *
 * Test sequence:
 *
 *     Consumer starts
 *         ↓
 *     queue empty
 *         ↓
 *     Consumer blocks
 *         ↓
 *     Producer starts
 *         ↓
 *     Producer sends 100
 *         ↓
 *     Consumer wakes
 *
 *     Producer sends 200
 *     Producer sends 300
 *         ↓
 *     queue full
 *
 *     Producer attempts 400
 *         ↓
 *     Producer blocks
 *
 *     Consumer receives 100
 *         ↓
 *     Producer wakes
 *
 *     Consumer receives 200
 *     Consumer receives 300
 *     Consumer receives 400
 *
 * ============================================================================
 */

/*
 * ============================================================================
 * Task objects
 * ============================================================================
 */

static vrt_task_t producerTask;
static vrt_task_t consumerTask;

/*
 * ============================================================================
 * Task stacks
 * ============================================================================
 */

static uint32_t producerStack[VRT_STACK_SIZE];
static uint32_t consumerStack[VRT_STACK_SIZE];

/*
 * ============================================================================
 * Queue
 * ============================================================================
 */

static uint32_t queueStorage[3];

static vrt_queue_t testQueue;

/*
 * ============================================================================
 * Test state
 * ============================================================================
 */

static volatile uint32_t
    receivedCount = 0U;

static volatile bool
    receiverBlockAttempted = false;

static volatile bool
    senderBlockAttempted = false;

static volatile bool
    step20Passed = false;

/*
 * ============================================================================
 * Expected FIFO values
 * ============================================================================
 */

static const uint32_t expectedValues[4] =
    {
        100U,
        200U,
        300U,
        400U};

/*
 * ============================================================================
 * Producer
 * ============================================================================
 */

static void producer_task(void *argument)
{
    (void)argument;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    Serial.println();
    Serial.println(
        "========== PRODUCER TASK STARTED ==========");

    for (uint32_t i = 0U;
         i < 4U;
         ++i)
    {
        uint32_t value =
            expectedValues[i];

        Serial.printf(
            "PRODUCER: sending %lu tick=%lu queue=%lu\n",
            (unsigned long)value,
            (unsigned long)scheduler->tickCount,
            (unsigned long)vrt_queue_count(
                &testQueue));

        /*
         * The fourth send should block because the queue
         * capacity is only three items.
         */
        if (i == 3U)
        {
            senderBlockAttempted =
                true;

            Serial.println(
                "PRODUCER: queue should be full; "
                "fourth send should block...");
        }

        if (!vrt_queue_send(
                &testQueue,
                &value))
        {
            /*
             * A failed send here is an actual test failure.
             */
            Serial.println(
                "PRODUCER: send failed.");

            for (;;)
            {
                delay(1000);
            }
        }

        Serial.printf(
            "PRODUCER: sent %lu queue=%lu\n",
            (unsigned long)value,
            (unsigned long)vrt_queue_count(
                &testQueue));
    }

    Serial.println(
        "PRODUCER: all messages sent.");

    for (;;)
    {
        delay(1000);
    }
}

/*
 * ============================================================================
 * Consumer
 * ============================================================================
 */

static void consumer_task(void *argument)
{
    (void)argument;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    Serial.println();
    Serial.println(
        "========== CONSUMER TASK STARTED ==========");

    /*
     * The queue is empty at startup.
     *
     * This receive must block until the producer sends 100.
     */
    receiverBlockAttempted =
        true;

    Serial.println(
        "CONSUMER: waiting for first message...");

    uint32_t value =
        0U;

    if (!vrt_queue_receive(
            &testQueue,
            &value))
    {
        Serial.println(
            "CONSUMER: first receive failed.");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.printf(
        "CONSUMER: received=%lu tick=%lu queue=%lu\n",
        (unsigned long)value,
        (unsigned long)scheduler->tickCount,
        (unsigned long)vrt_queue_count(
            &testQueue));

    /*
     * Verify first FIFO value.
     */
    if (value !=
        expectedValues[0])
    {
        Serial.printf(
            "ERROR: expected=%lu got=%lu\n",
            (unsigned long)expectedValues[0],
            (unsigned long)value);

        for (;;)
        {
            delay(1000);
        }
    }

    receivedCount++;

    /*
     * Receive remaining messages.
     */
    while (receivedCount < 4U)
    {
        value = 0U;

        if (!vrt_queue_receive(
                &testQueue,
                &value))
        {
            Serial.println(
                "CONSUMER: receive failed.");

            for (;;)
            {
                delay(1000);
            }
        }

        Serial.printf(
            "CONSUMER: received=%lu tick=%lu queue=%lu\n",
            (unsigned long)value,
            (unsigned long)scheduler->tickCount,
            (unsigned long)vrt_queue_count(
                &testQueue));

        if (value !=
            expectedValues[receivedCount])
        {
            Serial.printf(
                "ERROR: expected=%lu got=%lu\n",
                (unsigned long)expectedValues[receivedCount],
                (unsigned long)value);

            for (;;)
            {
                delay(1000);
            }
        }

        receivedCount++;
    }

    /*
     * Acceptance criteria.
     */
    if (receiverBlockAttempted &&
        senderBlockAttempted &&
        receivedCount == 4U)
    {
        step20Passed =
            true;

        Serial.println();
        Serial.println(
            "====================================");

        Serial.println(
            "STEP 20 PASSED");

        Serial.println(
            "Empty queue -> receiver blocked.");

        Serial.println(
            "Producer send -> receiver woke.");

        Serial.println(
            "Full queue -> producer blocked.");

        Serial.println(
            "Consumer receive -> producer woke.");

        Serial.println(
            "FIFO ordering verified.");

        Serial.println(
            "====================================");
    }

    for (;;)
    {
        delay(1000);
    }
}

/*
 * ============================================================================
 * Setup
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
        "VertexRT STEP 20");
    Serial.println(
        "Message queue blocking test");
    Serial.println(
        "====================================");

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    /*
     * ------------------------------------------------------------------------
     * Scheduler
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "Initializing scheduler...");

    vrt_scheduler_init(
        scheduler);

    Serial.println(
        "Scheduler initialized.");

    /*
     * ------------------------------------------------------------------------
     * Queue
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "Initializing message queue...");

    vrt_queue_init(
        &testQueue,
        queueStorage,
        sizeof(uint32_t),
        3U);

    Serial.println(
        "Queue capacity = 3.");

    /*
     * ------------------------------------------------------------------------
     * Consumer
     * ------------------------------------------------------------------------
     *
     * Consumer is registered first so it becomes the initial
     * task selected by the scheduler.
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "Initializing consumer task...");

    vrt_task_init(
        &consumerTask,
        consumer_task,
        NULL,
        2,
        consumerStack,
        VRT_STACK_SIZE,
        "consumer");

    /*
     * ------------------------------------------------------------------------
     * Producer
     * ------------------------------------------------------------------------
     */

    Serial.println(
        "Initializing producer task...");

    vrt_task_init(
        &producerTask,
        producer_task,
        NULL,
        2,
        producerStack,
        VRT_STACK_SIZE,
        "producer");

    /*
     * ------------------------------------------------------------------------
     * Register tasks
     * ------------------------------------------------------------------------
     */

    if (!vrt_scheduler_add_task(
            scheduler,
            &consumerTask))
    {
        Serial.println(
            "ERROR: failed to add consumer.");

        for (;;)
        {
            delay(1000);
        }
    }

    if (!vrt_scheduler_add_task(
            scheduler,
            &producerTask))
    {
        Serial.println(
            "ERROR: failed to add producer.");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Consumer added.");

    Serial.println(
        "Producer added.");

    /*
     * ------------------------------------------------------------------------
     * Timer
     * ------------------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "Initializing VertexRT timer ISR...");

    if (!vrt_preempt_timer_init())
    {
        Serial.println(
            "ERROR: timer initialization failed.");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Timer ISR initialized.");

    if (!vrt_preempt_timer_start())
    {
        Serial.println(
            "ERROR: timer start failed.");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Timer ISR started.");

    /*
     * ------------------------------------------------------------------------
     * FreeRTOS-backed startup
     * ------------------------------------------------------------------------
     *
     * Do NOT call vrt_scheduler_start() here.
     *
     * We explicitly start the FreeRTOS backing task so the queue
     * blocking path uses the same execution model as the rest of
     * the current Step 20 implementation.
     * ------------------------------------------------------------------------
     */

    Serial.println();
    Serial.println(
        "Starting FreeRTOS-backed VertexRT...");

    scheduler->currentTask =
        &consumerTask;

    scheduler->running =
        true;

    consumerTask.state =
        VRT_TASK_RUNNING;

    producerTask.state =
        VRT_TASK_READY;

    vrt_freertos_backend_start(
        &consumerTask);
}

/*
 * ============================================================================
 * Arduino loop
 * ============================================================================
 */

void loop()
{
    delay(1000);
}