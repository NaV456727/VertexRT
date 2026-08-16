#ifndef VRT_PREEMPT_TIMER_H
#define VRT_PREEMPT_TIMER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool vrt_preempt_timer_init(void);

    bool vrt_preempt_timer_start(void);

    uint32_t vrt_preempt_timer_get_count(void);

#ifdef __cplusplus
}
#endif

#endif