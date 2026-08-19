#include "vrt_freertos_backend.h"

#include "vrt_scheduler.h"
#include "esp_attr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

        vrt_freertos_binding_t *nextBinding =
            find_binding(next);

        if (nextBinding == NULL)
        {
            continue;
        }

        /*
         * Enforce the VertexRT invariant:
         *
         *     selected task      -> RUNNABLE
         *     every other task   -> SUSPENDED
         *
         * This prevents FreeRTOS round-robin execution between
         * VertexRT backing tasks.
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
         * The selected VertexRT task is the only backing task
         * that should remain runnable.
         */
        active_freertos_task =
            nextBinding->handle;

        next->state =
            VRT_TASK_RUNNING;

        vTaskResume(
            nextBinding->handle);
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

    /*
     * Backing tasks are created suspended.
     *
     * Once resumed by the dispatcher, execution starts here.
     *
     * The FreeRTOS task stack now becomes the actual execution context
     * for this VertexRT task.
     */
    task->state =
        VRT_TASK_RUNNING;

    if (task->entry != NULL)
    {
        task->entry(
            task->argument);
    }

    /*
     * Returning from a VertexRT task terminates its backing task.
     */
    task->state =
        VRT_TASK_TERMINATED;

    vTaskDelete(NULL);
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
        VRT_MAX_TASKS)
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
     * Backing task must not execute until VertexRT selects it.
     */
    vTaskSuspend(
        handle);

    bindings[binding_count].vrtTask =
        task;

    bindings[binding_count].handle =
        handle;

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

    if (nextBinding == NULL)
    {
        return;
    }

    TaskHandle_t previousHandle =
        active_freertos_task;

    /*
     * Nothing to switch.
     */
    if (previousHandle ==
        nextBinding->handle)
    {
        return;
    }

    /*
     * The selected task must be runnable.
     */
    vTaskResume(
        nextBinding->handle);

    /*
     * Update VertexRT/FreeRTOS ownership BEFORE
     * suspending the previous task.
     */
    active_freertos_task =
        nextBinding->handle;

    next->state =
        VRT_TASK_RUNNING;

    /*
     * Suspend the backing task that VertexRT believed
     * was currently executing.
     */
    if (previousHandle != NULL &&
        previousHandle != dispatcher_handle &&
        previousHandle != nextBinding->handle)
    {
        vTaskSuspend(
            previousHandle);
    }

    /*
     * Let FreeRTOS perform the runnable-task selection.
     */
    taskYIELD();
}

void vrt_freertos_backend_exit_current(
    vrt_task_t *next)
{
    if (next == NULL)
    {
        /*
         * No replacement task.
         *
         * This case will be handled by the idle-task
         * implementation in the final cleanup pass.
         */
        vTaskDelete(NULL);

        for (;;)
        {
        }
    }

    vrt_freertos_binding_t *binding =
        find_binding(next);

    if (binding == NULL)
    {
        vTaskDelete(NULL);

        for (;;)
        {
        }
    }

    active_freertos_task =
        binding->handle;

    next->state =
        VRT_TASK_RUNNING;

    vTaskResume(
        binding->handle);

    /*
     * Delete the currently executing FreeRTOS backing task.
     *
     * This call does not return to the terminated task.
     */
    vTaskDelete(NULL);

    /*
     * Defensive fallback.
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