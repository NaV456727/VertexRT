#ifndef VRT_FREERTOS_BACKEND_H
#define VRT_FREERTOS_BACKEND_H

#include "vrt_task.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool vrt_freertos_backend_init(void);

    bool vrt_freertos_backend_register_task(
        vrt_task_t *task);

    void vrt_freertos_backend_start(void);

    void vrt_freertos_backend_on_preemption(
        vrt_task_t *next);

#ifdef __cplusplus
}
#endif

#endif /* VRT_FREERTOS_BACKEND_H */