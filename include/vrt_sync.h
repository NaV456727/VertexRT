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
 * Semaphore API
 *=========================================================*/

/*
 * Initialize a binary semaphore.
 *
 * initialState:
 *
 *     true  -> available
 *     false -> unavailable
 */
void vrt_sem_init(
    vrt_sem_t *sem,
    bool initialState);

/*
 * Wait for the semaphore.
 *
 * If the semaphore is available, the current task
 * acquires it immediately.
 *
 * Otherwise the current task becomes BLOCKED.
 */
void vrt_sem_wait(
    vrt_sem_t *sem);

/*
 * Release the semaphore.
 *
 * If another task is waiting, that task is woken.
 * Otherwise the semaphore becomes available.
 */
void vrt_sem_signal(
    vrt_sem_t *sem);

#endif