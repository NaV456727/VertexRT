#include "vrt_tick.h"
#include "vrt_scheduler.h"

/*=========================================================
 * Private Variables
 *=========================================================*/

/*
 * Global system tick counter.
 *
 * This is maintained by the kernel tick subsystem.
 */
static volatile uint32_t g_tick_count = 0;

/*=========================================================
 * Tick Initialization
 *=========================================================*/

void vrt_tick_init(void)
{
    /*
     * Reset the kernel tick counter.
     *
     * Hardware timer configuration is handled by
     * the architecture-specific tick port.
     */
    g_tick_count = 0;
}

/*=========================================================
 * Tick Start
 *=========================================================*/

void vrt_tick_start(void)
{
    /*
     * The actual hardware timer will be started by
     * the architecture-specific implementation.
     *
     * The kernel itself does not directly access
     * ESP32 timer registers or APIs.
     */
}

/*=========================================================
 * Tick Stop
 *=========================================================*/

void vrt_tick_stop(void)
{
    /*
     * The actual hardware timer will be stopped by
     * the architecture-specific implementation.
     */
}

/*=========================================================
 * Tick Handler
 *=========================================================*/

/**
 * @brief Process one kernel tick.
 *
 * This function is called by the architecture-specific
 * timer interrupt handler.
 */
void vrt_tick_increment(void)
{
    vrt_scheduler_t *scheduler;

    /*
     * Increment the global kernel tick count.
     */
    g_tick_count++;

    /*
     * Get the active scheduler.
     */
    scheduler = vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Give the scheduler an opportunity to perform
     * its tick processing.
     */
    vrt_scheduler_tick(scheduler);
}

/*=========================================================
 * Tick Counter
 *=========================================================*/

uint32_t vrt_tick_get_count(void)
{
    return g_tick_count;
}