#include "vrt_tick.h"

#include "vrt_scheduler.h"
#include "vrt_config.h"

#include <stdint.h>
#include <stdbool.h>

#include "esp_timer.h"

/*
 * ============================================================================
 * VertexRT hardware tick
 * ============================================================================
 *
 * Step 12:
 *
 *     ESP timer
 *         ↓
 *     vrt_tick_callback()
 *         ↓
 *     vrt_scheduler_tick()
 *
 * No context switching is performed from the timer yet.
 * ============================================================================
 */

static esp_timer_handle_t vrt_tick_timer = NULL;

static uint32_t vrt_tick_frequency_hz = 0U;

static bool vrt_tick_initialized = false;

/*
 * ============================================================================
 * Timer callback
 * ============================================================================
 */

static void vrt_tick_callback(void *argument)
{
    (void)argument;

    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler == NULL)
    {
        return;
    }

    /*
     * Advance exactly one VertexRT kernel tick.
     */
    vrt_scheduler_tick(
        scheduler);
}

/*
 * ============================================================================
 * Initialize
 * ============================================================================
 */

bool vrt_tick_init(void)
{
    if (vrt_tick_initialized)
    {
        return true;
    }

    /*
     * Use the project's configured kernel tick frequency.
     */
    if (VRT_TICK_HZ == 0U)
    {
        return false;
    }

    uint64_t period_us =
        1000000ULL /
        (uint64_t)VRT_TICK_HZ;

    /*
     * esp_timer has microsecond resolution.
     */
    if (period_us == 0ULL)
    {
        return false;
    }

    esp_timer_create_args_t timer_args = {
        .callback = &vrt_tick_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "vrt_tick"};

    esp_err_t err =
        esp_timer_create(
            &timer_args,
            &vrt_tick_timer);

    if (err != ESP_OK)
    {
        vrt_tick_timer = NULL;
        return false;
    }

    vrt_tick_frequency_hz =
        VRT_TICK_HZ;

    vrt_tick_initialized =
        true;

    return true;
}

/*
 * ============================================================================
 * Start
 * ============================================================================
 */

bool vrt_tick_start(void)
{
    if (!vrt_tick_initialized ||
        vrt_tick_timer == NULL)
    {
        return false;
    }

    uint64_t period_us =
        1000000ULL /
        (uint64_t)vrt_tick_frequency_hz;

    esp_err_t err =
        esp_timer_start_periodic(
            vrt_tick_timer,
            period_us);

    return err == ESP_OK;
}

/*
 * ============================================================================
 * Stop
 * ============================================================================
 */

bool vrt_tick_stop(void)
{
    if (!vrt_tick_initialized ||
        vrt_tick_timer == NULL)
    {
        return false;
    }

    esp_err_t err =
        esp_timer_stop(
            vrt_tick_timer);

    return err == ESP_OK;
}

/*
 * ============================================================================
 * Frequency
 * ============================================================================
 */

uint32_t vrt_tick_get_frequency(void)
{
    return vrt_tick_frequency_hz;
}