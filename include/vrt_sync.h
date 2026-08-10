#ifndef VRT_SYNC_H
#define VRT_SYNC_H

#include "vrt_task.h"
#include "vrt_list.h"

#include <stdint.h>
#include <stdbool.h>

/*=========================================================
 * Binary Semaphore
 *=========================================================*/

typedef struct
{
    uint8_t count;

    vrt_list_t waitQueue;

} vrt_sem_t;

/*=========================================================
 * Binary Semaphore API
 *=========================================================*/

void vrt_sem_init(
    vrt_sem_t *sem,
    bool initialState);

void vrt_sem_wait(
    vrt_sem_t *sem);

void vrt_sem_signal(
    vrt_sem_t *sem);

/*=========================================================
 * Mutex
 *=========================================================*/

typedef struct
{
    bool locked;

    vrt_task_t *owner;

    vrt_list_t waitQueue;

} vrt_mutex_t;

/*=========================================================
 * Mutex API
 *=========================================================*/

void vrt_mutex_init(
    vrt_mutex_t *mutex);

void vrt_mutex_lock(
    vrt_mutex_t *mutex);

void vrt_mutex_unlock(
    vrt_mutex_t *mutex);

#endif