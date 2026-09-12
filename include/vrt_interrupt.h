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
     * Interrupt trigger
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
     * ISR callback
     * ============================================================================
     *
     * This callback executes inside the hardware ISR.
     *
     * It must be short and ISR-safe.
     * ============================================================================
     */

    typedef void (*vrt_interrupt_handler_t)(
        void *argument);

    /*
     * ============================================================================
     * Interrupt subsystem
     * ============================================================================
     */

    bool vrt_interrupt_init(void);

    bool vrt_interrupt_attach_gpio(
        uint8_t gpio,
        vrt_interrupt_trigger_t trigger,
        bool pullup,
        bool pulldown,
        vrt_interrupt_handler_t handler,
        void *argument);

    bool vrt_interrupt_detach_gpio(
        uint8_t gpio);

    bool vrt_interrupt_enable(
        uint8_t gpio);

    bool vrt_interrupt_disable(
        uint8_t gpio);

    uint32_t vrt_interrupt_get_count(
        uint8_t gpio);

    bool vrt_interrupt_reset_count(
        uint8_t gpio);

    bool vrt_interrupt_is_attached(
        uint8_t gpio);

    bool vrt_interrupt_is_enabled(
        uint8_t gpio);

    /*
     * ============================================================================
     * ISR → VertexRT task notification
     * ============================================================================
     *
     * Blocks the currently executing VertexRT task until the specified GPIO
     * interrupt occurs.
     *
     * Returns:
     *
     *     true  = interrupt notification received
     *     false = wait could not be established
     *
     * If an interrupt occurred before the task called this function, the
     * pending notification is consumed immediately.
     */
    bool vrt_interrupt_wait(
        uint8_t gpio);

#ifdef __cplusplus
}
#endif

#endif /* VRT_INTERRUPT_H */