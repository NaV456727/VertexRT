#include "vrt_tick.h"
#include "vrt_scheduler.h"

#include <Arduino.h>

/*=========================================================
 * Tick State
 *=========================================================*/

static volatile uint32_t vrt_tick_count = 0;

static hw_timer_t *vrt_timer = NULL;

/*=========================================================
 * Tick Interrupt Handler
 *=========================================================*/

static void IRAM_ATTR vrt_tick_isr(void)
{
    vrt_tick_count++;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler != NULL)
    {
        vrt_scheduler_tick(scheduler);
    }
}

/*=========================================================
 * Initialization
 *=========================================================*/

void vrt_tick_init(void)
{
    vrt_tick_count = 0;

    /*
     * ESP32 timer clock:
     *
     * APB clock = 80 MHz
     *
     * Divider = 80
     *
     * Timer frequency = 1 MHz
     *
     * Therefore:
     *
     *     1 timer tick = 1 microsecond
     */
    vrt_timer = timerBegin(
        0,
        80,
        true);

    if (vrt_timer == NULL)
    {
        return;
    }

    /*
     * Attach the tick interrupt.
     *
     * false = level-triggered interrupt.
     */
    timerAttachInterrupt(
        vrt_timer,
        &vrt_tick_isr,
        false);

    /*
     * 1 MHz timer frequency means:
     *
     *     1000 ticks = 1 ms
     *
     * Therefore a 1000 Hz system tick
     * requires a 1000 us alarm period.
     */
    timerAlarmWrite(
        vrt_timer,
        1000,
        true);
}

/*=========================================================
 * Start
 *=========================================================*/

void vrt_tick_start(void)
{
    if (vrt_timer == NULL)
    {
        return;
    }

    timerAlarmEnable(vrt_timer);
}

/*=========================================================
 * Stop
 *=========================================================*/

void vrt_tick_stop(void)
{
    if (vrt_timer == NULL)
    {
        return;
    }

    timerAlarmDisable(vrt_timer);
}

/*=========================================================
 * Tick Count
 *=========================================================*/

uint32_t vrt_tick_get_count(void)
{
    return vrt_tick_count;
}

/*=========================================================
 * Tick Handler
 *=========================================================*/

void vrt_tick_handler(void)
{
    vrt_tick_count++;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler != NULL)
    {
        vrt_scheduler_tick(scheduler);
    }
}