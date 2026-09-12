#include "vrt_interrupt.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <stddef.h>

/*
 * ============================================================================
 * Configuration
 * ============================================================================
 */

#define VRT_INTERRUPT_MAX_GPIO 40U

/*
 * ============================================================================
 * GPIO interrupt context
 * ============================================================================
 */

typedef struct
{
    bool attached;
    bool enabled;

    volatile uint32_t count;

    vrt_interrupt_handler_t handler;
    void *argument;

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
 * ISR wrapper
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
     * Count the hardware interrupt first.
     */
    context->count++;

    /*
     * Execute user callback if registered.
     *
     * The callback itself must be ISR-safe.
     */
    if (context->handler != NULL)
    {
        context->handler(
            context->argument);
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

    /*
     * Install the ESP32 GPIO ISR service.
     *
     * ESP_INTR_FLAG_IRAM allows ISR handlers registered with
     * IRAM-safe code to execute from IRAM.
     */
    esp_err_t result =
        gpio_install_isr_service(
            ESP_INTR_FLAG_IRAM);

    if (result != ESP_OK &&
        result != ESP_ERR_INVALID_STATE)
    {
        return false;
    }

    /*
     * Clear software state.
     */
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

        vrt_interrupt_contexts[i].handler =
            NULL;

        vrt_interrupt_contexts[i].argument =
            NULL;
    }

    vrt_interrupt_initialized =
        true;

    return true;
}

/*
 * ============================================================================
 * Attach GPIO interrupt
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
    if (!vrt_interrupt_initialized)
    {
        return false;
    }

    if (gpio >=
        VRT_INTERRUPT_MAX_GPIO)
    {
        return false;
    }

    if (handler == NULL)
    {
        return false;
    }

    /*
     * Do not attach twice.
     */
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

    /*
     * Configure GPIO.
     */
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

    /*
     * Prepare software context BEFORE registering
     * the ISR so the context is valid when the ISR fires.
     */
    vrt_interrupt_contexts[gpio].count =
        0U;

    vrt_interrupt_contexts[gpio].handler =
        handler;

    vrt_interrupt_contexts[gpio].argument =
        argument;

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

    /*
     * Start enabled.
     */
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

    vrt_interrupt_contexts[gpio].handler =
        NULL;

    vrt_interrupt_contexts[gpio].argument =
        NULL;

    vrt_interrupt_contexts[gpio].count =
        0U;

    /*
     * Disable GPIO interrupt generation.
     */
    gpio_set_intr_type(
        (gpio_num_t)gpio,
        GPIO_INTR_DISABLE);

    return true;
}

/*
 * ============================================================================
 * Enable
 * ============================================================================
 */

bool vrt_interrupt_enable(
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

/*
 * ============================================================================
 * Disable
 * ============================================================================
 */

bool vrt_interrupt_disable(
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
 * Interrupt count
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

/*
 * ============================================================================
 * Reset count
 * ============================================================================
 */

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

    return true;
}

/*
 * ============================================================================
 * State
 * ============================================================================
 */

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