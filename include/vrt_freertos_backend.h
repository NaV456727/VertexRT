#ifndef VRT_FREERTOS_BACKEND_H
#define VRT_FREERTOS_BACKEND_H

#include "vrt_task.h"
#include "esp_attr.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool vrt_freertos_backend_init(void);

    bool vrt_freertos_backend_register_task(
        vrt_task_t *task);

    void vrt_freertos_backend_start(
        vrt_task_t *first);

    void IRAM_ATTR vrt_freertos_backend_on_preemption(
        vrt_task_t *next);

    void vrt_freertos_backend_block_current(void);

    void vrt_freertos_backend_wake_task(
        vrt_task_t *task);

    void IRAM_ATTR
    vrt_freertos_backend_wake_task_from_isr(
        vrt_task_t *task);

    void vrt_freertos_backend_switch_to(
        vrt_task_t *next);

    void vrt_freertos_backend_exit_current(
        vrt_task_t *next);

    vrt_task_t *vrt_freertos_backend_get_current_task(void);

    void vrt_freertos_backend_suspend_task(
        vrt_task_t *task);

    void vrt_freertos_backend_resume_task(
        vrt_task_t *task);

#ifdef __cplusplus
}
#endif

#endif /* VRT_FREERTOS_BACKEND_H */