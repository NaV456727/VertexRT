#ifndef VRT_TIMER_H
#define VRT_TIMER_H

#include "vrt_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*=========================================================
 * Timer Configuration
 *=========================================================*/

/*
 * Kernel tick frequency in Hz.
 *
 * 1000 Hz = one kernel tick every 1 ms.
 */
#define VRT_TICK_RATE_HZ 1000U

    /*=========================================================
     * Timer Control
     *=========================================================*/

    /**
     * @brief Initialize the kernel timer.
     *
     * Configures the ESP32 hardware timer used to
     * generate periodic kernel ticks.
     */
    void vrt_timer_init(void);

    /**
     * @brief Start the kernel timer.
     *
     * Starts periodic kernel tick generation.
     */
    void vrt_timer_start(void);

    /**
     * @brief Stop the kernel timer.
     *
     * Stops periodic kernel tick generation.
     */
    void vrt_timer_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_TIMER_H */