#include "vrt_memory.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

/*=========================================================
 * Internal Heap Block
 *=========================================================*/

typedef struct vrt_memory_block
{
    size_t size;
    bool free;

    struct vrt_memory_block *next;
    struct vrt_memory_block *prev;

} vrt_memory_block_t;

/*=========================================================
 * Static Kernel Heap
 *=========================================================*/

static uint8_t vrt_memory_heap[VRT_MEMORY_HEAP_SIZE]
    __attribute__((aligned(VRT_MEMORY_ALIGNMENT)));

/*=========================================================
 * Heap State
 *=========================================================*/

static vrt_memory_block_t *vrt_memory_first_block = NULL;

static bool vrt_memory_initialized = false;

/*
 * ESP32 FreeRTOS critical-section mutex.
 */
static portMUX_TYPE vrt_memory_lock =
    portMUX_INITIALIZER_UNLOCKED;

/*=========================================================
 * Alignment
 *=========================================================*/

static size_t vrt_memory_align(
    size_t size)
{
    size_t remainder =
        size % VRT_MEMORY_ALIGNMENT;

    if (remainder == 0U)
    {
        return size;
    }

    return size +
           (VRT_MEMORY_ALIGNMENT - remainder);
}

/*=========================================================
 * Block Splitting
 *=========================================================*/

static void vrt_memory_split_block(
    vrt_memory_block_t *block,
    size_t requestedSize)
{
    if (block == NULL)
    {
        return;
    }

    /*
     * Do not create unusably small blocks.
     */
    if (block->size <=
        requestedSize +
            sizeof(vrt_memory_block_t) +
            VRT_MEMORY_ALIGNMENT)
    {
        return;
    }

    uint8_t *payload =
        (uint8_t *)(block + 1);

    vrt_memory_block_t *newBlock =
        (vrt_memory_block_t *)(payload + requestedSize);

    newBlock->size =
        block->size -
        requestedSize -
        sizeof(vrt_memory_block_t);

    newBlock->free =
        true;

    newBlock->next =
        block->next;

    newBlock->prev =
        block;

    if (newBlock->next != NULL)
    {
        newBlock->next->prev =
            newBlock;
    }

    block->next =
        newBlock;

    block->size =
        requestedSize;
}

/*=========================================================
 * Block Coalescing
 *=========================================================*/

static void vrt_memory_coalesce(
    vrt_memory_block_t *block)
{
    if (block == NULL)
    {
        return;
    }

    /*
     * Merge with next block.
     */
    if (block->next != NULL &&
        block->next->free)
    {
        vrt_memory_block_t *next =
            block->next;

        block->size +=
            sizeof(vrt_memory_block_t) +
            next->size;

        block->next =
            next->next;

        if (block->next != NULL)
        {
            block->next->prev =
                block;
        }
    }

    /*
     * Merge with previous block.
     */
    if (block->prev != NULL &&
        block->prev->free)
    {
        vrt_memory_block_t *prev =
            block->prev;

        prev->size +=
            sizeof(vrt_memory_block_t) +
            block->size;

        prev->next =
            block->next;

        if (prev->next != NULL)
        {
            prev->next->prev =
                prev;
        }
    }
}

/*=========================================================
 * Initialization
 *=========================================================*/

bool vrt_memory_init(void)
{
    taskENTER_CRITICAL(
        &vrt_memory_lock);

    /*
     * Reset the heap.
     */
    memset(
        vrt_memory_heap,
        0,
        sizeof(vrt_memory_heap));

    vrt_memory_first_block =
        (vrt_memory_block_t *)vrt_memory_heap;

    vrt_memory_first_block->size =
        VRT_MEMORY_HEAP_SIZE -
        sizeof(vrt_memory_block_t);

    vrt_memory_first_block->free =
        true;

    vrt_memory_first_block->next =
        NULL;

    vrt_memory_first_block->prev =
        NULL;

    vrt_memory_initialized =
        true;

    taskEXIT_CRITICAL(
        &vrt_memory_lock);

    return true;
}

/*=========================================================
 * Allocation
 *=========================================================*/

void *vrt_malloc(
    size_t size)
{
    if (!vrt_memory_initialized ||
        size == 0U)
    {
        return NULL;
    }

    size =
        vrt_memory_align(size);

    taskENTER_CRITICAL(
        &vrt_memory_lock);

    vrt_memory_block_t *block =
        vrt_memory_first_block;

    while (block != NULL)
    {
        if (block->free &&
            block->size >= size)
        {
            vrt_memory_split_block(
                block,
                size);

            block->free =
                false;

            void *result =
                (void *)(block + 1);

            taskEXIT_CRITICAL(
                &vrt_memory_lock);

            return result;
        }

        block =
            block->next;
    }

    taskEXIT_CRITICAL(
        &vrt_memory_lock);

    return NULL;
}

/*=========================================================
 * Calloc
 *=========================================================*/

void *vrt_calloc(
    size_t count,
    size_t size)
{
    if (count == 0U ||
        size == 0U)
    {
        return NULL;
    }

    if (count >
        (SIZE_MAX / size))
    {
        return NULL;
    }

    size_t total =
        count * size;

    void *ptr =
        vrt_malloc(total);

    if (ptr == NULL)
    {
        return NULL;
    }

    memset(
        ptr,
        0,
        total);

    return ptr;
}

/*=========================================================
 * Reallocation
 *=========================================================*/

void *vrt_realloc(
    void *ptr,
    size_t size)
{
    if (ptr == NULL)
    {
        return vrt_malloc(size);
    }

    if (size == 0U)
    {
        vrt_free(ptr);
        return NULL;
    }

    size =
        vrt_memory_align(size);

    taskENTER_CRITICAL(
        &vrt_memory_lock);

    vrt_memory_block_t *block =
        ((vrt_memory_block_t *)ptr) - 1;

    /*
     * Existing block is already large enough.
     */
    if (block->size >= size)
    {
        vrt_memory_split_block(
            block,
            size);

        taskEXIT_CRITICAL(
            &vrt_memory_lock);

        return ptr;
    }

    /*
     * Expand into the immediately following
     * free block if possible.
     */
    if (block->next != NULL &&
        block->next->free &&
        block->size +
                sizeof(vrt_memory_block_t) +
                block->next->size >=
            size)
    {
        vrt_memory_block_t *next =
            block->next;

        block->size +=
            sizeof(vrt_memory_block_t) +
            next->size;

        block->next =
            next->next;

        if (block->next != NULL)
        {
            block->next->prev =
                block;
        }

        vrt_memory_split_block(
            block,
            size);

        taskEXIT_CRITICAL(
            &vrt_memory_lock);

        return ptr;
    }

    size_t oldSize =
        block->size;

    taskEXIT_CRITICAL(
        &vrt_memory_lock);

    /*
     * Allocate a new block.
     */
    void *newPtr =
        vrt_malloc(size);

    if (newPtr == NULL)
    {
        return NULL;
    }

    size_t copySize =
        oldSize;

    if (copySize > size)
    {
        copySize = size;
    }

    memcpy(
        newPtr,
        ptr,
        copySize);

    vrt_free(ptr);

    return newPtr;
}

/*=========================================================
 * Free
 *=========================================================*/

void vrt_free(
    void *ptr)
{
    if (!vrt_memory_initialized ||
        ptr == NULL)
    {
        return;
    }

    taskENTER_CRITICAL(
        &vrt_memory_lock);

    vrt_memory_block_t *block =
        ((vrt_memory_block_t *)ptr) - 1;

    block->free =
        true;

    vrt_memory_coalesce(
        block);

    taskEXIT_CRITICAL(
        &vrt_memory_lock);
}

/*=========================================================
 * Heap Information
 *=========================================================*/

size_t vrt_memory_total(void)
{
    return VRT_MEMORY_HEAP_SIZE;
}

size_t vrt_memory_free(void)
{
    if (!vrt_memory_initialized)
    {
        return 0U;
    }

    size_t totalFree = 0U;

    taskENTER_CRITICAL(
        &vrt_memory_lock);

    vrt_memory_block_t *block =
        vrt_memory_first_block;

    while (block != NULL)
    {
        if (block->free)
        {
            /*
             * Include the block header in free space.
             *
             * This makes the initial free heap equal
             * to the complete VRT_MEMORY_HEAP_SIZE.
             */
            totalFree +=
                block->size +
                sizeof(vrt_memory_block_t);
        }

        block = block->next;
    }

    taskEXIT_CRITICAL(
        &vrt_memory_lock);

    return totalFree;
}

size_t vrt_memory_used(void)
{
    if (!vrt_memory_initialized)
    {
        return 0U;
    }

    return VRT_MEMORY_HEAP_SIZE -
           vrt_memory_free();
}