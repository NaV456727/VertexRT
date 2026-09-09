#include <Arduino.h>

extern "C"
{
#include "vrt_scheduler.h"
#include "vrt_task.h"
#include "vrt_memory.h"
#include "vrt_tick.h"
#include "vrt_config.h"
}

/*=========================================================
 * Test Task Stack
 *=========================================================*/

static uint32_t step9TaskStack[VRT_STACK_SIZE];

/*=========================================================
 * Test Task
 *=========================================================*/

static void step9TestTask(void *argument)
{
    (void)argument;

    Serial.println();
    Serial.println("========================================");
    Serial.println("STEP 9: MEMORY MANAGEMENT QUALIFICATION");
    Serial.println("========================================");

    /*-----------------------------------------------------
     * Initialize memory manager
     *-----------------------------------------------------*/

    bool initPass =
        vrt_memory_init();

    Serial.printf(
        "Memory init : %s\n",
        initPass ? "PASS" : "FAIL");

    if (!initPass)
    {
        Serial.println("STEP 9 RESULT: FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    /*-----------------------------------------------------
     * Initial heap state
     *-----------------------------------------------------*/

    size_t total =
        vrt_memory_total();

    size_t initialFree =
        vrt_memory_free();

    bool initialPass =
        (total == VRT_MEMORY_HEAP_SIZE) &&
        (initialFree == total);

    Serial.printf(
        "Initial heap state : %s\n",
        initialPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Basic allocation
     *-----------------------------------------------------*/

    void *a =
        vrt_malloc(128U);

    void *b =
        vrt_malloc(256U);

    bool allocationPass =
        (a != NULL) &&
        (b != NULL) &&
        (vrt_memory_free() < initialFree);

    Serial.printf(
        "Basic allocation : %s\n",
        allocationPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Write/read allocated memory
     *-----------------------------------------------------*/

    bool dataPass =
        true;

    if (a != NULL)
    {
        uint8_t *bytes =
            (uint8_t *)a;

        for (size_t i = 0; i < 128U; i++)
        {
            bytes[i] =
                (uint8_t)(i & 0xFFU);
        }

        for (size_t i = 0; i < 128U; i++)
        {
            if (bytes[i] !=
                (uint8_t)(i & 0xFFU))
            {
                dataPass = false;
                break;
            }
        }
    }
    else
    {
        dataPass = false;
    }

    Serial.printf(
        "Memory read/write : %s\n",
        dataPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Calloc
     *-----------------------------------------------------*/

    uint32_t *c =
        (uint32_t *)vrt_calloc(
            32U,
            sizeof(uint32_t));

    bool callocPass =
        (c != NULL);

    if (c != NULL)
    {
        for (size_t i = 0; i < 32U; i++)
        {
            if (c[i] != 0U)
            {
                callocPass = false;
                break;
            }
        }
    }

    Serial.printf(
        "Calloc zeroing : %s\n",
        callocPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Realloc
     *-----------------------------------------------------*/

    bool reallocPass =
        false;

    if (a != NULL)
    {
        uint8_t *resized =
            (uint8_t *)vrt_realloc(
                a,
                512U);

        if (resized != NULL)
        {
            reallocPass = true;

            for (size_t i = 0; i < 128U; i++)
            {
                if (resized[i] !=
                    (uint8_t)(i & 0xFFU))
                {
                    reallocPass = false;
                    break;
                }
            }

            a = resized;
        }
    }

    Serial.printf(
        "Realloc / data preservation : %s\n",
        reallocPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Free and coalescing
     *-----------------------------------------------------*/

    size_t beforeFree =
        vrt_memory_free();

    vrt_free(b);

    size_t afterBFree =
        vrt_memory_free();

    vrt_free(c);

    size_t afterCFree =
        vrt_memory_free();

    vrt_free(a);

    size_t finalFree =
        vrt_memory_free();

    bool freePass =
        (afterBFree >= beforeFree) &&
        (afterCFree >= afterBFree) &&
        (finalFree == initialFree);

    Serial.printf(
        "Free / coalescing : %s\n",
        freePass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Exhaustion test
     *-----------------------------------------------------*/

    void *blocks[32] = {0};

    uint32_t allocatedBlocks =
        0U;

    for (size_t i = 0; i < 32U; i++)
    {
        blocks[i] =
            vrt_malloc(512U);

        if (blocks[i] == NULL)
        {
            break;
        }

        allocatedBlocks++;
    }

    void *overflow =
        vrt_malloc(4096U);

    bool exhaustionPass =
        (allocatedBlocks > 0U) &&
        (overflow == NULL);

    Serial.printf(
        "Heap exhaustion handling : %s\n",
        exhaustionPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Free exhaustion allocations
     *-----------------------------------------------------*/

    for (size_t i = 0; i < 32U; i++)
    {
        if (blocks[i] != NULL)
        {
            vrt_free(blocks[i]);
        }
    }

    bool recoveryPass =
        (vrt_memory_free() == initialFree);

    Serial.printf(
        "Heap recovery : %s\n",
        recoveryPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Final statistics
     *-----------------------------------------------------*/

    size_t used =
        vrt_memory_used();

    size_t freeBytes =
        vrt_memory_free();

    bool statisticsPass =
        (used + freeBytes ==
         vrt_memory_total());

    Serial.printf(
        "Heap statistics : %s\n",
        statisticsPass ? "PASS" : "FAIL");

    /*-----------------------------------------------------
     * Overall result
     *-----------------------------------------------------*/

    bool overallPass =
        initPass &&
        initialPass &&
        allocationPass &&
        dataPass &&
        callocPass &&
        reallocPass &&
        freePass &&
        exhaustionPass &&
        recoveryPass &&
        statisticsPass;

    Serial.println();
    Serial.println("----------------------------------------");

    if (overallPass)
    {
        Serial.println("STEP 9 RESULT: PASS");
    }
    else
    {
        Serial.println("STEP 9 RESULT: FAIL");
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
    Serial.println("VertexRT Step 9 Test");
    Serial.println("========================================");

    /*-----------------------------------------------------
     * Scheduler
     *-----------------------------------------------------*/

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        Serial.println(
            "Scheduler instance : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    vrt_scheduler_init(
        scheduler);

    Serial.println(
        "Scheduler init : PASS");

    /*-----------------------------------------------------
     * Kernel tick
     *-----------------------------------------------------*/

    if (!vrt_tick_init())
    {
        Serial.println(
            "Kernel tick init : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    if (!vrt_tick_start())
    {
        Serial.println(
            "Kernel tick start : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Kernel tick : PASS");

    /*-----------------------------------------------------
     * Create test task
     *-----------------------------------------------------*/

    static vrt_task_t testTask;

    vrt_task_init(
        &testTask,
        step9TestTask,
        NULL,
        2,
        step9TaskStack,
        VRT_STACK_SIZE,
        "Step9");

    /*-----------------------------------------------------
     * Add test task
     *-----------------------------------------------------*/

    if (!vrt_scheduler_add_task(
            scheduler,
            &testTask))
    {
        Serial.println(
            "Test task add : FAIL");

        for (;;)
        {
            delay(1000);
        }
    }

    Serial.println(
        "Test task ready : PASS");

    /*-----------------------------------------------------
     * Start scheduler
     *-----------------------------------------------------*/

    Serial.println(
        "Starting scheduler...");

    vrt_scheduler_start(
        scheduler);

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