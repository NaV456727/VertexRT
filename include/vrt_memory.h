#ifndef VRT_MEMORY_H
#define VRT_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*=========================================================
     * Kernel Heap Configuration
     *=========================================================*/

#define VRT_MEMORY_HEAP_SIZE 16384U
#define VRT_MEMORY_ALIGNMENT 8U

    /*=========================================================
     * Initialization
     *=========================================================*/

    /**
     * @brief Initialize the VertexRT kernel heap.
     */
    bool vrt_memory_init(void);

    /*=========================================================
     * Allocation
     *=========================================================*/

    /**
     * @brief Allocate memory from the VertexRT kernel heap.
     *
     * @param size Number of bytes requested.
     *
     * @return Pointer to allocated memory, or NULL on failure.
     */
    void *vrt_malloc(size_t size);

    /**
     * @brief Allocate zero-initialized memory.
     *
     * @param count Number of elements.
     * @param size Size of each element.
     *
     * @return Pointer to allocated memory, or NULL on failure.
     */
    void *vrt_calloc(size_t count, size_t size);

    /**
     * @brief Resize an existing allocation.
     *
     * @param ptr Existing allocation.
     * @param size New size in bytes.
     *
     * @return Pointer to resized memory, or NULL on failure.
     */
    void *vrt_realloc(void *ptr, size_t size);

    /**
     * @brief Free an allocated block.
     *
     * @param ptr Pointer previously returned by vrt_malloc(),
     *            vrt_calloc(), or vrt_realloc().
     */
    void vrt_free(void *ptr);

    /*=========================================================
     * Heap Information
     *=========================================================*/

    /**
     * @brief Return total kernel heap size.
     */
    size_t vrt_memory_total(void);

    /**
     * @brief Return currently free heap bytes.
     */
    size_t vrt_memory_free(void);

    /**
     * @brief Return currently allocated heap bytes.
     */
    size_t vrt_memory_used(void);

#ifdef __cplusplus
}
#endif

#endif /* VRT_MEMORY_H */