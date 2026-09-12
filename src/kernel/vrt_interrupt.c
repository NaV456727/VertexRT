#include "vrt_interrupt.h"

#include "vrt_task.h"
#include "vrt_scheduler.h"
#include "vrt_freertos_backend.h"
#include "vrt_critical.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_attr.h"

#include <stddef.h>
#include <stdint.h>

#define VRT_INTERRUPT_MAX_GPIO 40U

/*
 * ============================================================================
 * Interrupt context
 * ============================================================================
 */

typedef struct
{
    bool attached;
    bool enabled;

    volatile uint32_t count;

    /*
     * Notifications that occurred while no task was waiting.
     */
    volatile uint32_t pendingNotifications;

    vrt_interrupt_handler_t handler;
    void *argument;

    /*
     * At most one VertexRT task waits on a GPIO interrupt at a time.
     */
    vrt_task_t *waitingTask;

} vrt_interrupt_context_t;

static vrt_interrupt_context_t
    vrt_interrupt_contexts[VRT_INTERRUPT_MAX_GPIO];

static bool
    vrt_interrupt_initialized = false;

/*
 * ============================================================================
 * Trigger conversion
 * ============================================================================
 */

static gpio_int_type_t
vrt_interrupt_convert_trigger(
    vrt_interrupt_trigger_t trigger)
{
    switch (trigger)
    {
    case VRT_INTERRUPT_RISING:
        return GPIO_INTR_POSEDGE;

    case VRT_INTERRUPT_FALLING:
        return GPIO_INTR_NEGEDGE;

    case VRT_INTERRUPT_CHANGE:
        return GPIO_INTR_ANYEDGE;

    case VRT_INTERRUPT_LOW_LEVEL:
        return GPIO_INTR_LOW_LEVEL;

    case VRT_INTERRUPT_HIGH_LEVEL:
        return GPIO_INTR_HIGH_LEVEL;

    default:
        return GPIO_INTR_DISABLE;
    }
}

/*
 * ============================================================================
 * ISR
 * ============================================================================
 */

static void IRAM_ATTR
vrt_interrupt_gpio_isr(
    void *argument)
{
    vrt_interrupt_context_t *context =
        (vrt_interrupt_context_t *)argument;

    if (context == NULL)
    {
        return;
    }

    /*
     * Count every hardware interrupt.
     */
    context->count++;

    /*
     * Record a pending notification.
     *
     * If a task is already waiting, one notification will
     * immediately be consumed below.
     */
    if (context->pendingNotifications <
        UINT32_MAX)
    {
        context->pendingNotifications++;
    }

    /*
     * User ISR callback.
     */
    if (context->handler != NULL)
    {
        context->handler(
            context->argument);
    }

    /*
     * ------------------------------------------------------------------------
     * Wake a VertexRT task waiting on this GPIO.
     * ------------------------------------------------------------------------
     */

    vrt_task_t *waitingTask =
        context->waitingTask;

    if (waitingTask == NULL)
    {
        return;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    bool shouldPreempt =
        false;

    vrt_task_t *current =
        scheduler->currentTask;

    /*
     * Protect the VertexRT scheduler structures while
     * modifying them from the ISR.
     */
    vrt_kernel_critical_enter_isr();

    /*
     * Re-check the waiter after entering the critical section.
     */
    if (context->waitingTask == waitingTask &&
        waitingTask->state == VRT_TASK_BLOCKED)
    {
        /*
         * Consume the notification being delivered to
         * the waiting task.
         */
        if (context->pendingNotifications > 0U)
        {
            context->pendingNotifications--;
        }

        context->waitingTask =
            NULL;

        waitingTask->interruptWaitActive =
            false;

        waitingTask->interruptWaitResult =
            true;

        waitingTask->state =
            VRT_TASK_READY;

        /*
         * Return the task to the READY queue.
         */
        if (!vrt_list_push_back(
                &scheduler->readyQueue,
                &waitingTask->node))
        {
            /*
             * Roll back if the READY queue insertion fails.
             */
            waitingTask->state =
                VRT_TASK_BLOCKED;

            waitingTask->interruptWaitActive =
                true;

            waitingTask->interruptWaitResult =
                false;

            context->waitingTask =
                waitingTask;

            if (context->pendingNotifications <
                UINT32_MAX)
            {
                context->pendingNotifications++;
            }
        }
        else
        {
            /*
             * Preempt the current task only if the
             * interrupt-woken task has higher priority.
             */
            if (current == NULL ||
                current == scheduler->idleTask ||
                waitingTask->priority >
                    current->priority)
            {
                if (current != NULL &&
                    current != waitingTask &&
                    current->state ==
                        VRT_TASK_RUNNING)
                {
                    current->state =
                        VRT_TASK_READY;
                }

                waitingTask->state =
                    VRT_TASK_RUNNING;

                scheduler->currentTask =
                    waitingTask;

                shouldPreempt =
                    true;
            }
        }
    }

    vrt_kernel_critical_exit_isr();

    /*
     * Physical context switching is deferred to the backend's
     * ISR-safe notification mechanism.
     */
    if (shouldPreempt)
    {
        vrt_freertos_backend_on_preemption(
            current,
            waitingTask);
    }
}

/*
 * ============================================================================
 * Initialization
 * ============================================================================
 */

bool vrt_interrupt_init(void)
{
    if (vrt_interrupt_initialized)
    {
        return true;
    }

    esp_err_t result =
        gpio_install_isr_service(
            ESP_INTR_FLAG_IRAM);

    if (result != ESP_OK &&
        result != ESP_ERR_INVALID_STATE)
    {
        return false;
    }

    for (uint32_t i = 0U;
         i < VRT_INTERRUPT_MAX_GPIO;
         ++i)
    {
        vrt_interrupt_contexts[i].attached =
            false;

        vrt_interrupt_contexts[i].enabled =
            false;

        vrt_interrupt_contexts[i].count =
            0U;

        vrt_interrupt_contexts[i].pendingNotifications =
            0U;

        vrt_interrupt_contexts[i].handler =
            NULL;

        vrt_interrupt_contexts[i].argument =
            NULL;

        vrt_interrupt_contexts[i].waitingTask =
            NULL;
    }

    vrt_interrupt_initialized =
        true;

    return true;
}

/*
 * ============================================================================
 * Attach
 * ============================================================================
 */

bool vrt_interrupt_attach_gpio(
    uint8_t gpio,
    vrt_interrupt_trigger_t trigger,
    bool pullup,
    bool pulldown,
    vrt_interrupt_handler_t handler,
    void *argument)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO ||
        handler == NULL)
    {
        return false;
    }

    if (vrt_interrupt_contexts[gpio].attached)
    {
        return false;
    }

    gpio_int_type_t interruptType =
        vrt_interrupt_convert_trigger(
            trigger);

    if (interruptType ==
        GPIO_INTR_DISABLE)
    {
        return false;
    }

    gpio_config_t config = {
        .pin_bit_mask =
            (1ULL << gpio),

        .mode =
            GPIO_MODE_INPUT,

        .pull_up_en =
            pullup
                ? GPIO_PULLUP_ENABLE
                : GPIO_PULLUP_DISABLE,

        .pull_down_en =
            pulldown
                ? GPIO_PULLDOWN_ENABLE
                : GPIO_PULLDOWN_DISABLE,

        .intr_type =
            interruptType};

    esp_err_t result =
        gpio_config(
            &config);

    if (result != ESP_OK)
    {
        return false;
    }

    vrt_interrupt_contexts[gpio].count =
        0U;

    vrt_interrupt_contexts[gpio].pendingNotifications =
        0U;

    vrt_interrupt_contexts[gpio].handler =
        handler;

    vrt_interrupt_contexts[gpio].argument =
        argument;

    vrt_interrupt_contexts[gpio].waitingTask =
        NULL;

    result =
        gpio_isr_handler_add(
            (gpio_num_t)gpio,
            vrt_interrupt_gpio_isr,
            &vrt_interrupt_contexts[gpio]);

    if (result != ESP_OK)
    {
        vrt_interrupt_contexts[gpio].handler =
            NULL;

        vrt_interrupt_contexts[gpio].argument =
            NULL;

        return false;
    }

    vrt_interrupt_contexts[gpio].attached =
        true;

    vrt_interrupt_contexts[gpio].enabled =
        true;

    return true;
}

/*
 * ============================================================================
 * Detach
 * ============================================================================
 */

bool vrt_interrupt_detach_gpio(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO)
    {
        return false;
    }

    if (!vrt_interrupt_contexts[gpio].attached)
    {
        return false;
    }

    /*
     * Do not detach while a task is waiting.
     */
    if (vrt_interrupt_contexts[gpio].waitingTask != NULL)
    {
        return false;
    }

    gpio_intr_disable(
        (gpio_num_t)gpio);

    esp_err_t result =
        gpio_isr_handler_remove(
            (gpio_num_t)gpio);

    if (result != ESP_OK)
    {
        return false;
    }

    vrt_interrupt_contexts[gpio].attached =
        false;

    vrt_interrupt_contexts[gpio].enabled =
        false;

    vrt_interrupt_contexts[gpio].count =
        0U;

    vrt_interrupt_contexts[gpio].pendingNotifications =
        0U;

    vrt_interrupt_contexts[gpio].handler =
        NULL;

    vrt_interrupt_contexts[gpio].argument =
        NULL;

    vrt_interrupt_contexts[gpio].waitingTask =
        NULL;

    gpio_set_intr_type(
        (gpio_num_t)gpio,
        GPIO_INTR_DISABLE);

    return true;
}

/*
 * ============================================================================
 * Enable / Disable
 * ============================================================================
 */

bool vrt_interrupt_enable(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO ||
        !vrt_interrupt_contexts[gpio].attached)
    {
        return false;
    }

    esp_err_t result =
        gpio_intr_enable(
            (gpio_num_t)gpio);

    if (result != ESP_OK)
    {
        return false;
    }

    vrt_interrupt_contexts[gpio].enabled =
        true;

    return true;
}

bool vrt_interrupt_disable(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO ||
        !vrt_interrupt_contexts[gpio].attached)
    {
        return false;
    }

    esp_err_t result =
        gpio_intr_disable(
            (gpio_num_t)gpio);

    if (result != ESP_OK)
    {
        return false;
    }

    vrt_interrupt_contexts[gpio].enabled =
        false;

    return true;
}

/*
 * ============================================================================
 * Count / state
 * ============================================================================
 */

uint32_t vrt_interrupt_get_count(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO)
    {
        return 0U;
    }

    return vrt_interrupt_contexts[gpio].count;
}

bool vrt_interrupt_reset_count(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO)
    {
        return false;
    }

    vrt_interrupt_contexts[gpio].count =
        0U;

    vrt_interrupt_contexts[gpio].pendingNotifications =
        0U;

    return true;
}

bool vrt_interrupt_is_attached(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO)
    {
        return false;
    }

    return vrt_interrupt_contexts[gpio].attached;
}

bool vrt_interrupt_is_enabled(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO)
    {
        return false;
    }

    return vrt_interrupt_contexts[gpio].enabled;
}

/*
 * ============================================================================
 * ISR → VertexRT task wait
 * ============================================================================
 */

bool vrt_interrupt_wait(
    uint8_t gpio)
{
    if (!vrt_interrupt_initialized ||
        gpio >= VRT_INTERRUPT_MAX_GPIO)
    {
        return false;
    }

    vrt_interrupt_context_t *context =
        &vrt_interrupt_contexts[gpio];

    if (!context->attached ||
        !context->enabled)
    {
        return false;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return false;
    }

    vrt_task_t *current =
        vrt_freertos_backend_get_current_task();

    if (current == NULL ||
        current == scheduler->idleTask ||
        current->isIdle)
    {
        return false;
    }

    /*
     * Protect the scheduler state.
     */
    vrt_kernel_critical_enter();

    /*
     * If an interrupt already occurred, consume the pending
     * notification immediately without blocking.
     */
    if (context->pendingNotifications > 0U)
    {
        context->pendingNotifications--;

        vrt_kernel_critical_exit();

        return true;
    }

    /*
     * Only one task can wait on a GPIO at a time.
     */
    if (context->waitingTask != NULL)
    {
        vrt_kernel_critical_exit();

        return false;
    }

    /*
     * Synchronize logical current task.
     */
    scheduler->currentTask =
        current;

    /*
     * Register this task as the interrupt waiter.
     */
    context->waitingTask =
        current;

    current->interruptWaitActive =
        true;

    current->interruptWaitGpio =
        gpio;

    current->interruptWaitResult =
        false;

    /*
     * Block current task.
     */
    current->state =
        VRT_TASK_BLOCKED;

    /*
     * Remove from READY queue.
     */
    if (!vrt_list_remove(
            &scheduler->readyQueue,
            &current->node))
    {
        context->waitingTask =
            NULL;

        current->interruptWaitActive =
            false;

        current->state =
            VRT_TASK_RUNNING;

        vrt_kernel_critical_exit();

        return false;
    }

    /*
     * Select another runnable task.
     */
    scheduler->currentTask =
        NULL;

    vrt_scheduler_schedule(
        scheduler);

    vrt_task_t *next =
        scheduler->currentTask;

    /*
     * No replacement task.
     */
    if (next == NULL ||
        next == current)
    {
        context->waitingTask =
            NULL;

        current->interruptWaitActive =
            false;

        current->state =
            VRT_TASK_RUNNING;

        (void)vrt_list_push_back(
            &scheduler->readyQueue,
            &current->node);

        scheduler->currentTask =
            current;

        vrt_kernel_critical_exit();

        return false;
    }

    vrt_kernel_critical_exit();

    /*
     * Physically switch to the selected VertexRT task.
     *
     * When this task is later woken by the ISR, execution resumes
     * here.
     */
    vrt_freertos_backend_switch_to(
        next);

    bool result =
        current->interruptWaitResult;

    current->interruptWaitResult =
        false;

    current->interruptWaitActive =
        false;

    return result;
}