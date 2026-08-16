#ifndef VRT_TICK_H
#define VRT_TICK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Initialize the VertexRT hardware tick using VRT_TICK_HZ.
     *
     * Does not start the timer.
     */
    bool vrt_tick_init(void);

    /*
     * Start periodic hardware ticks.
     */
    bool vrt_tick_start(void);

    /*
     * Stop hardware ticks.
     */
    bool vrt_tick_stop(void);

    /*
     * Return configured tick frequency.
     */
    uint32_t vrt_tick_get_frequency(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_TICK_H */