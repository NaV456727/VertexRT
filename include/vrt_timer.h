#ifndef VRT_TIMER_H
#define VRT_TIMER_H

#include "vrt_config.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*=========================================================
     * Software Timer
     *=========================================================*/

    typedef void (*vrt_timer_callback_t)(
        void *argument);

    typedef struct
    {
        uint32_t periodTicks;

        uint32_t expiryTick;

        bool autoReload;

        bool active;

        bool deleted;

        vrt_timer_callback_t callback;

        void *argument;

    } vrt_timer_t;

    /*=========================================================
     * Timer System
     *=========================================================*/

    /*
     * Initialize the software timer subsystem.
     *
     * The hardware/kernel tick itself is initialized separately
     * through vrt_tick_init().
     */
    void vrt_timer_system_init(void);

    /*
     * Process one VertexRT kernel tick.
     *
     * Called internally from vrt_tick_callback().
     */
    void vrt_timer_process_tick(void);

    /*=========================================================
     * Timer Object API
     *=========================================================*/

    /*
     * Create/configure a timer object.
     *
     * The timer is NOT started automatically.
     */
    bool vrt_timer_create(
        vrt_timer_t *timer,
        uint32_t periodTicks,
        bool autoReload,
        vrt_timer_callback_t callback,
        void *argument);

    /*
     * Start or restart a timer.
     */
    bool vrt_timer_start(
        vrt_timer_t *timer);

    /*
     * Stop a timer.
     */
    bool vrt_timer_stop(
        vrt_timer_t *timer);

    /*
     * Delete a timer.
     */
    bool vrt_timer_delete(
        vrt_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* VRT_TIMER_H */