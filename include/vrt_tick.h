#ifndef VRT_TICK_H
#define VRT_TICK_H

#include "vrt_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*=========================================================
 * Tick Configuration
 *=========================================================*/

/*
 * Default system tick frequency.
 *
 * 1000 Hz = 1 ms per tick.
 */
#define VRT_TICK_RATE_HZ 1000U

    /*=========================================================
     * Tick Control
     *=========================================================*/

    /**
     * @brief Initialize the system tick.
     *
     * Configures and initializes the hardware timer used
     * by VertexRT for periodic scheduler ticks.
     */
    void vrt_tick_init(void);

    /**
     * @brief Start the system tick.
     *
     * Enables the hardware timer after the kernel has
     * been initialized.
     */
    void vrt_tick_start(void);

    /**
     * @brief Stop the system tick.
     *
     * Disables the hardware timer.
     */
    void vrt_tick_stop(void);

    /**
     * @brief Get the current system tick count.
     *
     * @return Number of ticks since the scheduler started.
     */
    uint32_t vrt_tick_get_count(void);

    /**
     * @brief Process one system tick.
     *
     * Called by the architecture-specific timer interrupt.
     */
    void vrt_tick_increment(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_TICK_H */