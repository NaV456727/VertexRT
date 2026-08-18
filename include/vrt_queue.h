#ifndef VRT_QUEUE_H
#define VRT_QUEUE_H

#include "vrt_task.h"
#include "vrt_list.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint8_t *buffer;

        size_t itemSize;
        size_t capacity;

        size_t head;
        size_t tail;
        size_t count;

        vrt_list_t sendWaitQueue;
        vrt_list_t receiveWaitQueue;

    } vrt_queue_t;

    void vrt_queue_init(
        vrt_queue_t *queue,
        void *buffer,
        size_t itemSize,
        size_t capacity);

    bool vrt_queue_send(
        vrt_queue_t *queue,
        const void *item);

    bool vrt_queue_receive(
        vrt_queue_t *queue,
        void *item);

    bool vrt_queue_is_empty(
        const vrt_queue_t *queue);

    bool vrt_queue_is_full(
        const vrt_queue_t *queue);

    size_t vrt_queue_count(
        const vrt_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif