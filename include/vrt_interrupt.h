#ifndef VRT_INTERRUPT_H
#define VRT_INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * ============================================================================
     * Interrupt edge configuration
     * ============================================================================
     */

    typedef enum
    {
        VRT_INTERRUPT_RISING = 0,
        VRT_INTERRUPT_FALLING,
        VRT_INTERRUPT_CHANGE,
        VRT_INTERRUPT_LOW_LEVEL,
        VRT_INTERRUPT_HIGH_LEVEL

    } vrt_interrupt_trigger_t;

    /*
     * ============================================================================
     * Interrupt callback
     * ============================================================================
     *
     * IMPORTANT:
     *
     * This callback executes inside the hardware ISR.
     *
     * It must therefore be very short and ISR-safe.
     * Do not use Serial, delay(), malloc(), or other blocking operations.
     * ============================================================================
     */

    typedef void (*vrt_interrupt_handler_t)(
        void *argument);

    /*
     * ============================================================================
     * Interrupt management
     * ============================================================================
     */

    /*
     * Initialize the interrupt subsystem.
     *
     * The ESP32 GPIO ISR service is installed on first use.
     */
    bool vrt_interrupt_init(void);

    /*
     * Attach a GPIO interrupt.
     *
     * The supplied callback executes from the ISR.
     */
    bool vrt_interrupt_attach_gpio(
        uint8_t gpio,
        vrt_interrupt_trigger_t trigger,
        bool pullup,
        bool pulldown,
        vrt_interrupt_handler_t handler,
        void *argument);

    /*
     * Detach a GPIO interrupt.
     */
    bool vrt_interrupt_detach_gpio(
        uint8_t gpio);

    /*
     * Enable an attached GPIO interrupt.
     */
    bool vrt_interrupt_enable(
        uint8_t gpio);

    /*
     * Disable an attached GPIO interrupt.
     */
    bool vrt_interrupt_disable(
        uint8_t gpio);

    /*
     * Return the number of times the interrupt has fired.
     */
    uint32_t vrt_interrupt_get_count(
        uint8_t gpio);

    /*
     * Reset the software interrupt counter.
     */
    bool vrt_interrupt_reset_count(
        uint8_t gpio);

    /*
     * Return whether an interrupt is currently attached.
     */
    bool vrt_interrupt_is_attached(
        uint8_t gpio);

    /*
     * Return whether an interrupt is currently enabled.
     */
    bool vrt_interrupt_is_enabled(
        uint8_t gpio);

#ifdef __cplusplus
}
#endif

#endif /* VRT_INTERRUPT_H */