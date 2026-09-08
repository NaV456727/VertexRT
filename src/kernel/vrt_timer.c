#include "vrt_timer.h"

#include "vrt_scheduler.h"

#include <stddef.h>

/*
 * ========================================================================
 * Configuration
 * ========================================================================
 */

#define VRT_MAX_SOFTWARE_TIMERS 8U

/*
 * ========================================================================
 * Timer registry
 * ========================================================================
 */

static vrt_timer_t *
    timerRegistry[VRT_MAX_SOFTWARE_TIMERS];

static bool
    vrt_timer_system_initialized = false;

/*
 * ========================================================================
 * Register timer
 * ========================================================================
 */

static bool vrt_timer_register(
    vrt_timer_t *timer)
{
    if (timer == NULL)
    {
        return false;
    }

    for (uint32_t i = 0U;
         i < VRT_MAX_SOFTWARE_TIMERS;
         ++i)
    {
        if (timerRegistry[i] == timer)
        {
            return true;
        }
    }

    for (uint32_t i = 0U;
         i < VRT_MAX_SOFTWARE_TIMERS;
         ++i)
    {
        if (timerRegistry[i] == NULL)
        {
            timerRegistry[i] =
                timer;

            return true;
        }
    }

    return false;
}

/*
 * ========================================================================
 * Unregister timer
 * ========================================================================
 */

static void vrt_timer_unregister(
    vrt_timer_t *timer)
{
    if (timer == NULL)
    {
        return;
    }

    for (uint32_t i = 0U;
         i < VRT_MAX_SOFTWARE_TIMERS;
         ++i)
    {
        if (timerRegistry[i] == timer)
        {
            timerRegistry[i] =
                NULL;

            return;
        }
    }
}

/*
 * ========================================================================
 * Initialize
 * ========================================================================
 */

void vrt_timer_system_init(void)
{
    for (uint32_t i = 0U;
         i < VRT_MAX_SOFTWARE_TIMERS;
         ++i)
    {
        timerRegistry[i] =
            NULL;
    }

    vrt_timer_system_initialized =
        true;
}

/*
 * ========================================================================
 * Create
 * ========================================================================
 */

bool vrt_timer_create(
    vrt_timer_t *timer,
    uint32_t periodTicks,
    bool autoReload,
    vrt_timer_callback_t callback,
    void *argument)
{
    if (!timer ||
        periodTicks == 0U ||
        callback == NULL)
    {
        return false;
    }

    if (!vrt_timer_system_initialized)
    {
        return false;
    }

    /*
     * Do not allow the same object to be registered twice.
     */
    for (uint32_t i = 0U;
         i < VRT_MAX_SOFTWARE_TIMERS;
         ++i)
    {
        if (timerRegistry[i] == timer)
        {
            return false;
        }
    }

    if (!vrt_timer_register(timer))
    {
        return false;
    }

    timer->periodTicks =
        periodTicks;

    timer->expiryTick =
        0U;

    timer->autoReload =
        autoReload;

    timer->active =
        false;

    timer->deleted =
        false;

    timer->callback =
        callback;

    timer->argument =
        argument;

    return true;
}

/*
 * ========================================================================
 * Start / Restart
 * ========================================================================
 */

bool vrt_timer_start(
    vrt_timer_t *timer)
{
    if (!vrt_timer_system_initialized ||
        timer == NULL ||
        timer->deleted ||
        timer->callback == NULL ||
        timer->periodTicks == 0U)
    {
        return false;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return false;
    }

    /*
     * Start also acts as restart.
     */
    timer->expiryTick =
        scheduler->tickCount +
        timer->periodTicks;

    timer->active =
        true;

    return true;
}

/*
 * ========================================================================
 * Stop
 * ========================================================================
 */

bool vrt_timer_stop(
    vrt_timer_t *timer)
{
    if (timer == NULL ||
        timer->deleted)
    {
        return false;
    }

    timer->active =
        false;

    return true;
}

/*
 * ========================================================================
 * Delete
 * ========================================================================
 */

bool vrt_timer_delete(
    vrt_timer_t *timer)
{
    if (timer == NULL)
    {
        return false;
    }

    vrt_timer_unregister(
        timer);

    timer->active =
        false;

    timer->deleted =
        true;

    timer->callback =
        NULL;

    timer->argument =
        NULL;

    return true;
}

/*
 * ========================================================================
 * Check whether timer has expired
 * ========================================================================
 */

static bool vrt_timer_expired(
    uint32_t currentTick,
    uint32_t expiryTick)
{
    /*
     * Signed subtraction gives wrap-safe tick comparison.
     */
    return ((int32_t)(currentTick -
                      expiryTick) >= 0);
}

/*
 * ========================================================================
 * Process one kernel tick
 * ========================================================================
 */

void vrt_timer_process_tick(void)
{
    if (!vrt_timer_system_initialized)
    {
        return;
    }

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    uint32_t currentTick =
        scheduler->tickCount;

    for (uint32_t i = 0U;
         i < VRT_MAX_SOFTWARE_TIMERS;
         ++i)
    {
        vrt_timer_t *timer =
            timerRegistry[i];

        if (timer == NULL ||
            !timer->active ||
            timer->deleted ||
            timer->callback == NULL)
        {
            continue;
        }

        if (!vrt_timer_expired(
                currentTick,
                timer->expiryTick))
        {
            continue;
        }

        /*
         * Capture callback information before changing timer state.
         */
        vrt_timer_callback_t callback =
            timer->callback;

        void *argument =
            timer->argument;

        if (timer->autoReload)
        {
            /*
             * Preserve periodic timing rather than scheduling the
             * next period from callback completion.
             */
            timer->expiryTick +=
                timer->periodTicks;
        }
        else
        {
            timer->active =
                false;
        }

        /*
         * Execute callback outside of timer state mutation.
         */
        callback(argument);
    }
}