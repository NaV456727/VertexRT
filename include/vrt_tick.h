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

#define VRT_TICK_RATE_HZ 1000U

    /*=========================================================
     * Tick Initialization
     *=========================================================*/

    /**
     * @brief Initialize the VertexRT system tick.
     *
     * Configures the hardware timer used by VertexRT.
     */
    void vrt_tick_init(void);

    /*=========================================================
     * Tick Control
     *=========================================================*/

    /**
     * @brief Start the system tick.
     *
     * Starts the hardware timer after the kernel
     * has been initialized.
     */
    void vrt_tick_start(void);

    /**
     * @brief Stop the system tick.
     *
     * Stops the hardware timer.
     */
    void vrt_tick_stop(void);

    /*=========================================================
     * Tick Information
     *=========================================================*/

    /**
     * @brief Get the current system tick count.
     *
     * @return Number of system ticks since the tick system
     *         was started.
     */
    uint32_t vrt_tick_get_count(void);

    /**
     * @brief Process one system tick.
     *
     * This function is called by the hardware timer
     * interrupt handler.
     */
    void vrt_tick_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_TICK_H */