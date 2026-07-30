#ifndef VRT_LIST_H
#define VRT_LIST_H

#include "vrt_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct vrt_list_node
    {
        struct vrt_list_node *next;
        struct vrt_list_node *prev;
        void *data;
    } vrt_list_node_t;

    typedef struct
    {
        vrt_list_node_t *head;
        vrt_list_node_t *tail;
        uint32_t size;
    } vrt_list_t;

    void vrt_list_init(vrt_list_t *list);

    bool vrt_list_push_back(vrt_list_t *list, vrt_list_node_t *node);
    bool vrt_list_push_front(vrt_list_t *list, vrt_list_node_t *node);
    bool vrt_list_remove(vrt_list_t *list, vrt_list_node_t *node);

    vrt_list_node_t *vrt_list_pop_front(vrt_list_t *list);
    vrt_list_node_t *vrt_list_pop_back(vrt_list_t *list);

    bool vrt_list_is_empty(const vrt_list_t *list);
    uint32_t vrt_list_size(const vrt_list_t *list);

#ifdef __cplusplus
}
#endif

#endif