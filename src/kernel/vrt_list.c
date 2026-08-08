#include "vrt_list.h"

/*=========================================================
 * Initialization
 *=========================================================*/

void vrt_list_init(vrt_list_t *list)
{
    if (list == NULL)
    {
        return;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

/*=========================================================
 * Push Back
 *=========================================================*/

bool vrt_list_push_back(
    vrt_list_t *list,
    vrt_list_node_t *node)
{
    if (list == NULL || node == NULL)
    {
        return false;
    }

    /*
     * A node must not already belong to another list.
     */
    if (node->next != NULL || node->prev != NULL)
    {
        return false;
    }

    node->next = NULL;
    node->prev = list->tail;

    /*
     * Empty list.
     */
    if (list->tail == NULL)
    {
        list->head = node;
    }
    else
    {
        list->tail->next = node;
    }

    list->tail = node;
    list->size++;

    return true;
}

/*=========================================================
 * Push Front
 *=========================================================*/

bool vrt_list_push_front(
    vrt_list_t *list,
    vrt_list_node_t *node)
{
    if (list == NULL || node == NULL)
    {
        return false;
    }

    /*
     * A node must not already belong to another list.
     */
    if (node->next != NULL || node->prev != NULL)
    {
        return false;
    }

    node->prev = NULL;
    node->next = list->head;

    /*
     * Empty list.
     */
    if (list->head == NULL)
    {
        list->tail = node;
    }
    else
    {
        list->head->prev = node;
    }

    list->head = node;
    list->size++;

    return true;
}

/*=========================================================
 * Remove
 *=========================================================*/

bool vrt_list_remove(
    vrt_list_t *list,
    vrt_list_node_t *node)
{
    if (list == NULL || node == NULL)
    {
        return false;
    }

    /*
     * Empty list.
     */
    if (list->head == NULL)
    {
        return false;
    }

    /*
     * Update previous node.
     */
    if (node->prev != NULL)
    {
        node->prev->next = node->next;
    }
    else
    {
        /*
         * Node is the head.
         */
        list->head = node->next;
    }

    /*
     * Update next node.
     */
    if (node->next != NULL)
    {
        node->next->prev = node->prev;
    }
    else
    {
        /*
         * Node is the tail.
         */
        list->tail = node->prev;
    }

    /*
     * Detach node completely.
     */
    node->next = NULL;
    node->prev = NULL;

    /*
     * Prevent underflow if the list was already
     * internally inconsistent.
     */
    if (list->size > 0)
    {
        list->size--;
    }

    return true;
}

/*=========================================================
 * Pop Front
 *=========================================================*/

vrt_list_node_t *vrt_list_pop_front(
    vrt_list_t *list)
{
    if (list == NULL || list->head == NULL)
    {
        return NULL;
    }

    vrt_list_node_t *node = list->head;

    if (node->next != NULL)
    {
        list->head = node->next;
        list->head->prev = NULL;
    }
    else
    {
        /*
         * List becomes empty.
         */
        list->head = NULL;
        list->tail = NULL;
    }

    node->next = NULL;
    node->prev = NULL;

    if (list->size > 0)
    {
        list->size--;
    }

    return node;
}

/*=========================================================
 * Pop Back
 *=========================================================*/

vrt_list_node_t *vrt_list_pop_back(
    vrt_list_t *list)
{
    if (list == NULL || list->tail == NULL)
    {
        return NULL;
    }

    vrt_list_node_t *node = list->tail;

    if (node->prev != NULL)
    {
        list->tail = node->prev;
        list->tail->next = NULL;
    }
    else
    {
        /*
         * List becomes empty.
         */
        list->head = NULL;
        list->tail = NULL;
    }

    node->next = NULL;
    node->prev = NULL;

    if (list->size > 0)
    {
        list->size--;
    }

    return node;
}

/*=========================================================
 * Is Empty
 *=========================================================*/

bool vrt_list_is_empty(
    const vrt_list_t *list)
{
    if (list == NULL)
    {
        return true;
    }

    return (list->size == 0);
}

/*=========================================================
 * Size
 *=========================================================*/

uint32_t vrt_list_size(
    const vrt_list_t *list)
{
    if (list == NULL)
    {
        return 0;
    }

    return list->size;
}

/*=========================================================
 * Insert Before
 *=========================================================*/

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

    /*
     * A node must not already belong to another list.
     */
    if (node->next != NULL || node->prev != NULL)
    {
        return false;
    }

    /*
     * Position must belong to this list.
     *
     * Search the list instead of relying solely on
     * prev/next pointers.
     */
    vrt_list_node_t *current = list->head;

    while (current != NULL)
    {
        if (current == position)
        {
            break;
        }

        current = current->next;
    }

    if (current != position)
    {
        return false;
    }

    /*
     * Insert before position.
     */
    node->next = position;
    node->prev = position->prev;

    if (position->prev != NULL)
    {
        position->prev->next = node;
    }
    else
    {
        /*
         * Position was the head.
         */
        list->head = node;
    }

    position->prev = node;

    list->size++;

    return true;
}