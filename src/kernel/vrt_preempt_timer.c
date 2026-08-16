#include "vrt_preempt_timer.h"
#include "vrt_scheduler.h"
#include "vrt_freertos_backend.h"

#include "driver/timer.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * ============================================================================
 * Timer ISR state
 * ============================================================================
 */

static timer_isr_handle_t vrt_preempt_timer_handle = NULL;

static volatile uint32_t vrt_isr_count = 0U;

/*
 * ============================================================================
 * VertexRT hardware timer ISR
 * ============================================================================
 *
 * TIMERG1 / TIMER0
 *
 * Hardware timer
 *      ↓
 * VertexRT tick
 *      ↓
 * VertexRT scheduler selects next task
 *      ↓
 * notify corresponding FreeRTOS backing task
 *
 * The ISR does NOT perform a raw Xtensa context switch.
 * FreeRTOS remains responsible for the actual CPU context switch.
 * ============================================================================
 */

static void IRAM_ATTR
vrt_preempt_timer_isr(void *arg)
{
    (void)arg;

    /*
     * Count every hardware timer interrupt.
     */
    vrt_isr_count++;

    /*
     * Advance VertexRT kernel time and detect whether
     * another VertexRT task should run.
     */
    vrt_scheduler_tick_from_isr();

    /*
     * Ask the VertexRT scheduler to select the next task.
     *
     * This only changes VertexRT scheduler state.
     * It does not manipulate Xtensa CPU context.
     */
    vrt_scheduler_t *scheduler =
        vrt_scheduler_get_instance();

    if (scheduler != NULL &&
        scheduler->preemptionPending)
    {
        vrt_task_t *next =
            vrt_scheduler_select_preemption_from_isr();

        /*
         * Tell the FreeRTOS backing layer which VertexRT
         * task has become the logical current task.
         */
        if (next != NULL)
        {
            vrt_freertos_backend_on_preemption(
                next);
        }
    }

    /*
     * Clear the timer interrupt.
     */
    timer_group_clr_intr_status_in_isr(
        TIMER_GROUP_1,
        TIMER_0);

    /*
     * Re-arm the periodic alarm.
     *
     * Timer clock:
     *
     *     80 MHz / 80 = 1 MHz
     *
     * Alarm:
     *
     *     10000 us = 10 ms
     *
     * Therefore:
     *
     *     100 Hz
     */
    timer_group_set_alarm_value_in_isr(
        TIMER_GROUP_1,
        TIMER_0,
        10000);

    timer_group_enable_alarm_in_isr(
        TIMER_GROUP_1,
        TIMER_0);
}

/*
 * ============================================================================
 * Timer initialization
 * ============================================================================
 */

bool vrt_preempt_timer_init(void)
{
    timer_config_t config = {
        .alarm_en = TIMER_ALARM_EN,
        .counter_en = TIMER_PAUSE,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = TIMER_AUTORELOAD_EN,
        .divider = 80};

    /*
     * Configure TIMERG1 / TIMER0.
     */
    if (timer_init(
            TIMER_GROUP_1,
            TIMER_0,
            &config) != ESP_OK)
    {
        return false;
    }

    /*
     * Register the ISR.
     */
    if (timer_isr_register(
            TIMER_GROUP_1,
            TIMER_0,
            vrt_preempt_timer_isr,
            NULL,
            ESP_INTR_FLAG_IRAM,
            &vrt_preempt_timer_handle) != ESP_OK)
    {
        return false;
    }

    /*
     * Start counting from zero.
     */
    if (timer_set_counter_value(
            TIMER_GROUP_1,
            TIMER_0,
            0) != ESP_OK)
    {
        return false;
    }

    /*
     * Configure 10 ms alarm.
     */
    if (timer_set_alarm_value(
            TIMER_GROUP_1,
            TIMER_0,
            10000) != ESP_OK)
    {
        return false;
    }

    /*
     * Enable timer interrupt.
     */
    if (timer_enable_intr(
            TIMER_GROUP_1,
            TIMER_0) != ESP_OK)
    {
        return false;
    }

    return true;
}

/*
 * ============================================================================
 * Timer start
 * ============================================================================
 */

bool vrt_preempt_timer_start(void)
{
    return timer_start(
               TIMER_GROUP_1,
               TIMER_0) == ESP_OK;
}

/*
 * ============================================================================
 * ISR counter
 * ============================================================================
 */

uint32_t vrt_preempt_timer_get_count(void)
{
    return vrt_isr_count;
}