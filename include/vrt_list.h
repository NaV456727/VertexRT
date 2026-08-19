#ifndef VRT_LIST_H
#define VRT_LIST_H

#include "vrt_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct vrt_list;

    typedef struct vrt_list_node
    {
        /* Adjacent nodes */
        struct vrt_list_node *next;
        struct vrt_list_node *prev;

        /* Object that owns this node */
        void *owner;

        /*
         * List that currently owns this node.
         *
         * NULL means the node is detached.
         */
        struct vrt_list *list;

    } vrt_list_node_t;

    typedef struct vrt_list
    {
        /* First node */
        vrt_list_node_t *head;

        /* Last node */
        vrt_list_node_t *tail;

        /* Number of nodes in the list */
        uint32_t size;

    } vrt_list_t;

    /* Initialize */
    void vrt_list_init(
        vrt_list_t *list);

    /* Insertion */
    bool vrt_list_push_back(
        vrt_list_t *list,
        vrt_list_node_t *node);

    bool vrt_list_push_front(
        vrt_list_t *list,
        vrt_list_node_t *node);

    /* Removal */
    bool vrt_list_remove(
        vrt_list_t *list,
        vrt_list_node_t *node);

    vrt_list_node_t *
    vrt_list_pop_front(
        vrt_list_t *list);

    vrt_list_node_t *
    vrt_list_pop_back(
        vrt_list_t *list);

    /* Utilities */
    bool vrt_list_is_empty(
        const vrt_list_t *list);

    uint32_t vrt_list_size(
        const vrt_list_t *list);

    /**
     * @brief Insert a node before another node.
     */
    bool vrt_list_insert_before(
        vrt_list_t *list,
        vrt_list_node_t *position,
        vrt_list_node_t *node);

#ifdef __cplusplus
}
#endif

#endif /* VRT_LIST_H */