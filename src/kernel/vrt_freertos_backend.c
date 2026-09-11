#include "vrt_freertos_backend.h"

#include "vrt_scheduler.h"
#include "esp_attr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/semphr.h"
#include "vrt_config.h"
#include "esp_timer.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * ============================================================================
 * Backend binding
 * ============================================================================
 */

typedef struct
{
    vrt_task_t *vrtTask;
    TaskHandle_t handle;
    SemaphoreHandle_t timedWaitSemaphore;
} vrt_freertos_binding_t;

static vrt_freertos_binding_t
    bindings[VRT_MAX_TASKS + 1U];

static uint32_t
    binding_count = 0U;

static bool
    backend_initialized = false;

/*
 * ============================================================================
 * Dispatcher state
 * ============================================================================
 *
 * The dispatcher runs at a higher FreeRTOS priority than VertexRT backing
 * tasks.
 *
 * Timer ISR:
 *
 *     select next VertexRT task
 *             ↓
 *     notify dispatcher
 *             ↓
 *     FreeRTOS switches to dispatcher
 *
 * Dispatcher:
 *
 *     suspend previous backing task
 *     resume next backing task
 * ============================================================================
 */

static TaskHandle_t
    dispatcher_handle = NULL;

static volatile vrt_task_t *
    pending_next_task = NULL;

static TaskHandle_t
    active_freertos_task = NULL;

/*
 * ============================================================================
 * Find binding
 * ============================================================================
 */

static vrt_freertos_binding_t *
find_binding(
    vrt_task_t *task)
{
    if (task == NULL)
    {
        return NULL;
    }

    for (uint32_t i = 0U;
         i < binding_count;
         ++i)
    {
        if (bindings[i].vrtTask == task)
        {
            return &bindings[i];
        }
    }

    return NULL;
}

/*
 * ============================================================================
 * Find VertexRT task from the currently executing FreeRTOS task
 * ============================================================================
 */

static vrt_task_t *
find_task_from_handle(
    TaskHandle_t handle)
{
    if (handle == NULL)
    {
        return NULL;
    }

    for (uint32_t i = 0U;
         i < binding_count;
         ++i)
    {
        if (bindings[i].handle == handle)
        {
            return bindings[i].vrtTask;
        }
    }

    return NULL;
}

/*
 * ============================================================================
 * Runtime accounting helpers
 * ============================================================================
 */

static void vrt_freertos_runtime_start(
    vrt_task_t *task)
{
    if (task == NULL)
    {
        return;
    }

    task->runtimeStartUs =
        (uint64_t)esp_timer_get_time();
}

static void vrt_freertos_runtime_stop(
    vrt_task_t *task)
{
    if (task == NULL)
    {
        return;
    }

    if (task->runtimeStartUs == 0U)
    {
        return;
    }

    uint64_t now =
        (uint64_t)esp_timer_get_time();

    if (now >=
        task->runtimeStartUs)
    {
        task->runtimeUs +=
            now -
            task->runtimeStartUs;
    }

    task->runtimeStartUs =
        0U;
}

/*
 * ============================================================================
 * Dispatcher task
 * ============================================================================
 */

static void
vrt_freertos_dispatcher(
    void *argument)
{
    (void)argument;

    for (;;)
    {
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);

        vrt_task_t *next =
            (vrt_task_t *)pending_next_task;

        pending_next_task =
            NULL;

        if (next == NULL)
        {
            continue;
        }

        vrt_scheduler_t *scheduler =
            vrt_scheduler_get_instance();

        if (scheduler == NULL)
        {
            continue;
        }

        vrt_task_t *previous =
            scheduler->currentTask;

        vrt_freertos_binding_t *nextBinding =
            find_binding(next);

        if (nextBinding == NULL)
        {
            continue;
        }

        /*
         * Suspend every VertexRT backing task except
         * the selected next task.
         */
        for (uint32_t i = 0U;
             i < binding_count;
             ++i)
        {
            TaskHandle_t handle =
                bindings[i].handle;

            if (handle == NULL ||
                handle == dispatcher_handle ||
                handle == nextBinding->handle)
            {
                continue;
            }

            vTaskSuspend(
                handle);
        }

        /*
         * Complete the logical transition.
         */
        if (previous != NULL &&
            previous != next &&
            previous->state ==
                VRT_TASK_RUNNING)
        {
            previous->state =
                VRT_TASK_READY;
        }

        next->state =
            VRT_TASK_RUNNING;

        scheduler->currentTask =
            next;

        active_freertos_task =
            nextBinding->handle;

        /*
         * Make the selected backing task runnable.
         */
        vTaskResume(
            nextBinding->handle);

        /*
         * Dispatcher must stop being runnable.
         *
         * Once suspended, FreeRTOS will run the selected
         * VertexRT backing task.
         */
        taskYIELD();
    }
}

/*
 * ============================================================================
 * VertexRT backing task
 * ============================================================================
 */

static void
vrt_freertos_task_entry(
    void *argument)
{
    vrt_task_t *task =
        (vrt_task_t *)argument;

    if (task == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    task->state =
        VRT_TASK_RUNNING;

    if (task->entry != NULL)
    {
        task->entry(
            task->argument);
    }

    vrt_task_exit();

    for (;;)
    {
    }
}

/*
 * ============================================================================
 * Backend initialization
 * ============================================================================
 */

bool vrt_freertos_backend_init(void)
{
    binding_count = 0U;

    dispatcher_handle =
        NULL;

    pending_next_task =
        NULL;

    active_freertos_task =
        NULL;

    backend_initialized =
        false;

    BaseType_t result =
        xTaskCreate(
            vrt_freertos_dispatcher,
            "vrt_dispatch",
            2048,
            NULL,
            5,
            &dispatcher_handle);

    if (result != pdPASS)
    {
        return false;
    }

    backend_initialized =
        true;

    /*
     * Dispatcher should sleep until a VertexRT preemption occurs.
     */
    vTaskSuspend(
        dispatcher_handle);

    /*
     * Re-enable dispatcher.
     *
     * The task remains blocked on ulTaskNotifyTake().
     */
    vTaskResume(
        dispatcher_handle);

    return true;
}

/*
 * ============================================================================
 * Register VertexRT task
 * ============================================================================
 */

bool vrt_freertos_backend_register_task(
    vrt_task_t *task)
{
    if (!backend_initialized ||
        task == NULL)
    {
        return false;
    }

    if (binding_count >=
        (VRT_MAX_TASKS + 1U))
    {
        return false;
    }

    if (find_binding(task) != NULL)
    {
        return true;
    }

    TaskHandle_t handle =
        NULL;

    BaseType_t result =
        xTaskCreate(
            vrt_freertos_task_entry,
            task->name,
            2048,
            task,
            0,
            &handle);

    if (result != pdPASS)
    {
        return false;
    }

    /*
     * Each VertexRT task gets its own native semaphore.
     *
     * This semaphore is used only when this particular task
     * performs a timed semaphore wait.
     */
    SemaphoreHandle_t timedWaitSemaphore =
        xSemaphoreCreateBinary();

    if (timedWaitSemaphore == NULL)
    {
        vTaskDelete(handle);
        return false;
    }

    /*
     * Backing task must not execute until VertexRT selects it.
     */
    vTaskSuspend(
        handle);

    bindings[binding_count].vrtTask =
        task;

    bindings[binding_count].handle =
        handle;

    bindings[binding_count].timedWaitSemaphore =
        timedWaitSemaphore;

    binding_count++;

    return true;
}

bool vrt_freertos_backend_register_idle(
    vrt_task_t *task)
{
    if (!backend_initialized ||
        task == NULL)
    {
        return false;
    }

    if (find_binding(task) != NULL)
    {
        return true;
    }

    if (binding_count >=
        (VRT_MAX_TASKS + 1U))
    {
        return false;
    }

    TaskHandle_t handle =
        NULL;

    BaseType_t result =
        xTaskCreate(
            vrt_freertos_task_entry,
            "vrt_idle",
            2048,
            task,
            0,
            &handle);

    if (result != pdPASS)
    {
        return false;
    }

    /*
     * Idle also gets a private timed-wait semaphore so that every
     * binding has the same structure.
     *
     * The idle task itself must never perform a timed wait.
     */
    SemaphoreHandle_t timedWaitSemaphore =
        xSemaphoreCreateBinary();

    if (timedWaitSemaphore == NULL)
    {
        vTaskDelete(handle);
        return false;
    }

    /*
     * Idle backing task starts suspended.
     *
     * VertexRT will explicitly select it when there
     * are no runnable user tasks.
     */
    vTaskSuspend(
        handle);

    bindings[binding_count].vrtTask =
        task;

    bindings[binding_count].handle =
        handle;

    bindings[binding_count].timedWaitSemaphore =
        timedWaitSemaphore;

    binding_count++;

    return true;
}

/*
 * ============================================================================
 * Start first VertexRT task
 * ============================================================================
 */

void vrt_freertos_backend_start(
    vrt_task_t *first)
{
    if (!backend_initialized ||
        first == NULL)
    {
        return;
    }

    vrt_freertos_binding_t *binding =
        find_binding(first);

    if (binding == NULL)
    {
        return;
    }

    active_freertos_task =
        binding->handle;

    first->state =
        VRT_TASK_RUNNING;

    vrt_freertos_runtime_start(
        first);

    /*
     * Start the first backing task.
     */
    vTaskResume(
        binding->handle);

    /*
     * The calling Arduino task can yield.
     */
    taskYIELD();
}

/*
 * ============================================================================
 * ISR preemption notification
 * ============================================================================
 */

void IRAM_ATTR
vrt_freertos_backend_on_preemption(
    vrt_task_t *previous,
    vrt_task_t *next)
{
    if (!backend_initialized ||
        next == NULL ||
        dispatcher_handle == NULL)
    {
        return;
    }

    (void)previous;

    pending_next_task =
        next;

    BaseType_t higherPriorityTaskWoken =
        pdFALSE;

    vTaskNotifyGiveFromISR(
        dispatcher_handle,
        &higherPriorityTaskWoken);

    if (higherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void vrt_freertos_backend_block_current(void)
{
    TaskHandle_t currentHandle =
        xTaskGetCurrentTaskHandle();

    if (currentHandle == NULL)
    {
        return;
    }

    /*
     * The current FreeRTOS task is the backing task
     * for the currently running VertexRT task.
     *
     * Block indefinitely. It will be resumed when
     * VertexRT wakes and selects the task again.
     */
    vTaskSuspend(
        currentHandle);
}

void vrt_freertos_backend_wake_task(
    vrt_task_t *task)
{
    /*
     * Waking a VertexRT task only makes it READY
     * from the VertexRT scheduler's point of view.
     *
     * Do not resume the FreeRTOS backing task here.
     *
     * VertexRT must first decide whether this task
     * should actually run.
     */
    (void)task;
}

void IRAM_ATTR
vrt_freertos_backend_wake_task_from_isr(
    vrt_task_t *task)
{
    /*
     * Do not directly resume the backing FreeRTOS task here.
     *
     * The timer ISR only updates VertexRT's scheduler state.
     * The VertexRT scheduler must decide which READY task should
     * actually execute.
     */
    (void)task;
}

void vrt_freertos_backend_switch_to(
    vrt_task_t *next)
{
    if (!backend_initialized ||
        next == NULL)
    {
        return;
    }

    vrt_freertos_binding_t *nextBinding =
        find_binding(next);

    if (nextBinding == NULL ||
        nextBinding->handle == NULL)
    {
        return;
    }

    /*
     * ------------------------------------------------------------------------
     * VertexRT keeps track of the backing task that is logically running.
     *
     * This is important because switch_to() can be called from:
     *
     *   1. the currently executing VertexRT backing task
     *   2. the ESP timer task
     *   3. another kernel context
     *
     * Therefore xTaskGetCurrentTaskHandle() alone cannot determine the
     * VertexRT task being replaced.
     * ------------------------------------------------------------------------
     */

    TaskHandle_t previousHandle =
        active_freertos_task;

    if (previousHandle == NULL)
    {
        return;
    }

    /*
     * Already running the requested task.
     */
    if (previousHandle ==
        nextBinding->handle)
    {
        return;
    }

    /*
     * Map the previous backing task back to VertexRT.
     */
    vrt_task_t *previousTask =
        find_task_from_handle(
            previousHandle);

    /*
     * ------------------------------------------------------------------------
     * Stop runtime accounting for the task being replaced.
     * ------------------------------------------------------------------------
     */

    vrt_freertos_runtime_stop(
        previousTask);

    /*
     * ------------------------------------------------------------------------
     * Update backend ownership.
     * ------------------------------------------------------------------------
     */

    active_freertos_task =
        nextBinding->handle;

    next->state =
        VRT_TASK_RUNNING;

    /*
     * Start runtime accounting for the selected task.
     */
    vrt_freertos_runtime_start(
        next);

    /*
     * ------------------------------------------------------------------------
     * Make the selected backing task runnable.
     * ------------------------------------------------------------------------
     */

    vTaskResume(
        nextBinding->handle);

    /*
     * ------------------------------------------------------------------------
     * Determine which FreeRTOS task is physically executing this function.
     * ------------------------------------------------------------------------
     */

    TaskHandle_t callerHandle =
        xTaskGetCurrentTaskHandle();

    /*
     * ------------------------------------------------------------------------
     * Case 1:
     *
     * switch_to() was called by the VertexRT task being replaced.
     *
     * Example:
     *
     *     Controller
     *         ↓
     *     vrt_task_delay()
     *         ↓
     *     switch_to(TaskA)
     *         ↓
     *     suspend Controller
     *
     * When Controller is eventually resumed, vTaskSuspend() returns and
     * execution continues normally through this function and back into
     * vrt_task_delay().
     * ------------------------------------------------------------------------
     */

    if (previousHandle ==
        callerHandle)
    {
        vTaskSuspend(
            callerHandle);

        /*
         * IMPORTANT:
         *
         * Do NOT loop here.
         *
         * When this task is later resumed, vTaskSuspend() returns and
         * this function must return to its caller.
         */
        return;
    }

    /*
     * ------------------------------------------------------------------------
     * Case 2:
     *
     * switch_to() was called from another FreeRTOS task, such as the ESP
     * timer task.
     *
     * In this case we must suspend the VertexRT backing task that was
     * previously running, NOT the caller.
     * ------------------------------------------------------------------------
     */

    vTaskSuspend(
        previousHandle);

    /*
     * Let FreeRTOS schedule the selected backing task.
     */
    taskYIELD();
}

void vrt_freertos_backend_exit_current(
    vrt_task_t *next)
{
    if (!backend_initialized ||
        next == NULL)
    {
        return;
    }

    vrt_freertos_binding_t *nextBinding =
        find_binding(next);

    if (nextBinding == NULL)
    {
        return;
    }

    vrt_task_t *currentTask =
        find_task_from_handle(
            xTaskGetCurrentTaskHandle());

    vrt_freertos_runtime_stop(
        currentTask);

    /*
     * The current VertexRT task has already been marked
     * TERMINATED by vrt_task_exit().
     *
     * Select the replacement task first.
     */
    active_freertos_task =
        nextBinding->handle;

    next->state =
        VRT_TASK_RUNNING;

    vrt_freertos_runtime_start(
        next);

    /*
     * Make the replacement backing task runnable.
     */
    vTaskResume(
        nextBinding->handle);

    /*
     * Suspend the CURRENT backing FreeRTOS task.
     *
     * Do not delete it here.
     *
     * The current task is the one executing this function.
     * Suspending it safely transfers execution to the
     * replacement task without destroying its TCB/stack
     * while the transition is still in progress.
     */
    vTaskSuspend(NULL);

    /*
     * We should never execute here again until some code
     * explicitly resumes this terminated backing task.
     *
     * It must remain suspended permanently for now.
     */
    for (;;)
    {
    }
}

vrt_task_t *
vrt_freertos_backend_get_current_task(void)
{
    TaskHandle_t handle =
        xTaskGetCurrentTaskHandle();

    if (handle == NULL)
    {
        return NULL;
    }

    /*
     * Map the actual FreeRTOS task currently executing
     * back to its VertexRT task control block.
     */
    return find_task_from_handle(
        handle);
}

void vrt_freertos_backend_suspend_task(
    vrt_task_t *task)
{
    if (task == NULL)
    {
        return;
    }

    vrt_freertos_binding_t *binding =
        find_binding(task);

    if (binding == NULL)
    {
        return;
    }

    vTaskSuspend(
        binding->handle);
}

void vrt_freertos_backend_resume_task(
    vrt_task_t *task)
{
    if (task == NULL)
    {
        return;
    }

    vrt_freertos_binding_t *binding =
        find_binding(task);

    if (binding == NULL)
    {
        return;
    }

    vTaskResume(
        binding->handle);
}

bool vrt_freertos_backend_sem_init(
    void **handle,
    bool initialState)
{
    if (handle == NULL)
    {
        return false;
    }

    SemaphoreHandle_t semaphore =
        xSemaphoreCreateBinary();

    if (semaphore == NULL)
    {
        *handle = NULL;
        return false;
    }

    if (initialState)
    {
        if (xSemaphoreGive(
                semaphore) != pdTRUE)
        {
            vSemaphoreDelete(
                semaphore);

            *handle = NULL;

            return false;
        }
    }

    *handle =
        (void *)semaphore;

    return true;
}

bool vrt_freertos_backend_sem_take(
    void *handle)
{
    if (handle == NULL)
    {
        return false;
    }

    return xSemaphoreTake(
               (SemaphoreHandle_t)handle,
               0) == pdTRUE;
}

bool vrt_freertos_backend_sem_give(
    void *handle)
{
    if (handle == NULL)
    {
        return false;
    }

    return xSemaphoreGive(
               (SemaphoreHandle_t)handle) == pdTRUE;
}

bool vrt_freertos_backend_block_current_on_sem(
    void *handle,
    vrt_task_t *next,
    uint32_t timeoutTicks)
{
    if (!backend_initialized ||
        handle == NULL ||
        next == NULL ||
        timeoutTicks == 0U)
    {
        return false;
    }

    vrt_freertos_binding_t *nextBinding =
        find_binding(next);

    if (nextBinding == NULL)
    {
        return false;
    }

    TaskHandle_t currentHandle =
        xTaskGetCurrentTaskHandle();

    if (currentHandle == NULL ||
        currentHandle == dispatcher_handle ||
        currentHandle == nextBinding->handle)
    {
        return false;
    }

    /*
     * FreeRTOS tick timeout.
     */
    uint64_t freertosTicks64 =
        ((uint64_t)timeoutTicks *
             (uint64_t)configTICK_RATE_HZ +
         (uint64_t)VRT_TICK_HZ -
         1ULL) /
        (uint64_t)VRT_TICK_HZ;

    if (freertosTicks64 == 0ULL)
    {
        freertosTicks64 = 1ULL;
    }

    TickType_t freertosTicks =
        (TickType_t)freertosTicks64;

    /*
     * Make the selected VertexRT backing task runnable.
     */
    vTaskResume(
        nextBinding->handle);

    /*
     * The current FreeRTOS task is still physically executing
     * at this exact moment.
     *
     * Do NOT change active_freertos_task yet.
     *
     * xSemaphoreTake() below will block the current task.
     */
    BaseType_t result =
        xSemaphoreTake(
            (SemaphoreHandle_t)handle,
            freertosTicks);

    /*
     * We are executing here again after:
     *
     *   1. semaphore was given
     *   2. timeout occurred
     *
     * The current task is physically running again.
     */
    active_freertos_task =
        currentHandle;

    return result == pdTRUE;
}

bool vrt_freertos_backend_block_current_timed(
    vrt_task_t *current,
    vrt_task_t *next,
    uint32_t timeoutTicks)
{
    if (!backend_initialized ||
        current == NULL ||
        next == NULL ||
        timeoutTicks == 0U)
    {
        return false;
    }

    vrt_freertos_binding_t *currentBinding =
        find_binding(current);

    vrt_freertos_binding_t *nextBinding =
        find_binding(next);

    if (currentBinding == NULL ||
        nextBinding == NULL ||
        currentBinding->timedWaitSemaphore == NULL)
    {
        return false;
    }

    TaskHandle_t currentHandle =
        xTaskGetCurrentTaskHandle();

    if (currentHandle == NULL ||
        currentHandle == dispatcher_handle)
    {
        return false;
    }

    /*
     * Convert VertexRT ticks to FreeRTOS ticks.
     */
    uint64_t freertosTicks64 =
        ((uint64_t)timeoutTicks *
             (uint64_t)configTICK_RATE_HZ +
         (uint64_t)VRT_TICK_HZ -
         1ULL) /
        (uint64_t)VRT_TICK_HZ;

    if (freertosTicks64 == 0ULL)
    {
        freertosTicks64 = 1ULL;
    }

    TickType_t freertosTicks =
        (TickType_t)freertosTicks64;

    /*
     * Make sure this private semaphore starts empty.
     */
    (void)xSemaphoreTake(
        currentBinding->timedWaitSemaphore,
        0);

    /*
     * Make the next VertexRT task physically runnable.
     */
    vTaskResume(
        nextBinding->handle);

    /*
     * Block THIS task on ITS OWN semaphore.
     */
    BaseType_t result =
        xSemaphoreTake(
            currentBinding->timedWaitSemaphore,
            freertosTicks);

    /*
     * We are physically executing again.
     */
    active_freertos_task =
        currentHandle;

    return result == pdTRUE;
}

bool vrt_freertos_backend_wake_timed_task(
    vrt_task_t *task)
{
    if (!backend_initialized ||
        task == NULL)
    {
        return false;
    }

    vrt_freertos_binding_t *binding =
        find_binding(task);

    if (binding == NULL ||
        binding->timedWaitSemaphore == NULL)
    {
        return false;
    }

    return xSemaphoreGive(
               binding->timedWaitSemaphore) == pdTRUE;
}

size_t vrt_freertos_backend_stack_free(
    const vrt_task_t *task)
{
    if (task == NULL)
    {
        return 0U;
    }

    vrt_freertos_binding_t *binding =
        find_binding((vrt_task_t *)task);

    if (binding == NULL ||
        binding->handle == NULL)
    {
        return 0U;
    }

    return (size_t)uxTaskGetStackHighWaterMark(
               binding->handle) *
           sizeof(StackType_t);
}