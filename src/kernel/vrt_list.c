#include "vrt_list.h"

void vrt_list_init(vrt_list_t *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

bool vrt_list_is_empty(const vrt_list_t *list)
{
    return (list->size == 0);
}

uint32_t vrt_list_size(const vrt_list_t *list)
{
    return list->size;
}

bool vrt_list_push_back(vrt_list_t *list, vrt_list_node_t *node)
{
    if (node == NULL || list == NULL)
    {

        return false;
    }

    node->next = NULL;
    node->prev = list->tail;

    if (list->tail != NULL)
        list->tail->next = node;
    else
        list->head = node;

    list->tail = node;
    list->size++;

    return true;
}

bool vrt_list_push_front(vrt_list_t *list, vrt_list_node_t *node)
{
    if (node == NULL || list == NULL)
    {

        return false;
    }

    node->prev = NULL;
    node->next = list->head;

    if (list->head != NULL)
        list->head->prev = node;
    else
        list->tail = node;

    list->head = node;
    list->size++;

    return true;
}

bool vrt_list_remove(vrt_list_t *list, vrt_list_node_t *node)
{
    if (node == NULL || list == NULL || vrt_list_is_empty(list))
    {
        return false;
    }

    if (node->prev != NULL)
        node->prev->next = node->next;
    else
        list->head = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    node->next = NULL;
    node->prev = NULL;
    node->owner = NULL;

    if (list->size > 0)
    {

        list->size--;
    }

    return true;
}

vrt_list_node_t *vrt_list_pop_front(vrt_list_t *list)
{
    if (vrt_list_is_empty(list))
    {
        return NULL;
    }

    vrt_list_node_t *node = list->head;

    vrt_list_remove(list, node);

    return node;
}

vrt_list_node_t *vrt_list_pop_back(vrt_list_t *list)
{
    if (vrt_list_is_empty(list))
    {
        return NULL;
    }

    vrt_list_node_t *node = list->tail;

    vrt_list_remove(list, node);

    return node;
}

bool vrt_list_insert_before(
    vrt_list_t *list,
    vrt_list_node_t *position,
    vrt_list_node_t *node)
{
    if (list == NULL ||
        position == NULL ||
        node == NULL)
    {
        return false;
    }

    if (node->next != NULL ||
        node->prev != NULL)
    {
        return false;
    }

    node->next = position;
    node->prev = position->prev;

    if (position->prev != NULL)
    {
        position->prev->next = node;
    }
    else
    {
        list->head = node;
    }

    position->prev = node;

    list->size++;

    return true;
}