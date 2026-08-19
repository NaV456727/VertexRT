#include "vrt_list.h"

/*
 * ============================================================================
 * Initialization
 * ============================================================================
 */

void vrt_list_init(
    vrt_list_t *list)
{
    if (list == NULL)
    {
        return;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0U;
}

/*
 * ============================================================================
 * Push Back
 * ============================================================================
 */

bool vrt_list_push_back(
    vrt_list_t *list,
    vrt_list_node_t *node)
{
    if (list == NULL ||
        node == NULL)
    {
        return false;
    }

    /*
     * A node already belonging to a list cannot be inserted again.
     */
    if (node->list != NULL)
    {
        return false;
    }

    node->next = NULL;
    node->prev = list->tail;
    node->list = list;

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

/*
 * ============================================================================
 * Push Front
 * ============================================================================
 */

bool vrt_list_push_front(
    vrt_list_t *list,
    vrt_list_node_t *node)
{
    if (list == NULL ||
        node == NULL)
    {
        return false;
    }

    /*
     * A node already belonging to a list cannot be inserted again.
     */
    if (node->list != NULL)
    {
        return false;
    }

    node->prev = NULL;
    node->next = list->head;
    node->list = list;

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

/*
 * ============================================================================
 * Remove
 * ============================================================================
 */

bool vrt_list_remove(
    vrt_list_t *list,
    vrt_list_node_t *node)
{
    if (list == NULL ||
        node == NULL)
    {
        return false;
    }

    /*
     * The node must belong to this exact list.
     */
    if (node->list != list)
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
    node->list = NULL;

    if (list->size > 0U)
    {
        list->size--;
    }

    return true;
}

/*
 * ============================================================================
 * Pop Front
 * ============================================================================
 */

vrt_list_node_t *
vrt_list_pop_front(
    vrt_list_t *list)
{
    if (list == NULL ||
        list->head == NULL)
    {
        return NULL;
    }

    vrt_list_node_t *node =
        list->head;

    /*
     * Remove using the normal membership-aware path.
     */
    if (!vrt_list_remove(
            list,
            node))
    {
        return NULL;
    }

    return node;
}

/*
 * ============================================================================
 * Pop Back
 * ============================================================================
 */

vrt_list_node_t *
vrt_list_pop_back(
    vrt_list_t *list)
{
    if (list == NULL ||
        list->tail == NULL)
    {
        return NULL;
    }

    vrt_list_node_t *node =
        list->tail;

    /*
     * Remove using the normal membership-aware path.
     */
    if (!vrt_list_remove(
            list,
            node))
    {
        return NULL;
    }

    return node;
}

/*
 * ============================================================================
 * Is Empty
 * ============================================================================
 */

bool vrt_list_is_empty(
    const vrt_list_t *list)
{
    if (list == NULL)
    {
        return true;
    }

    return list->size == 0U;
}

/*
 * ============================================================================
 * Size
 * ============================================================================
 */

uint32_t vrt_list_size(
    const vrt_list_t *list)
{
    if (list == NULL)
    {
        return 0U;
    }

    return list->size;
}

/*
 * ============================================================================
 * Insert Before
 * ============================================================================
 */

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
     * New node must be detached.
     */
    if (node->list != NULL)
    {
        return false;
    }

    /*
     * Position must belong to this exact list.
     */
    if (position->list != list)
    {
        return false;
    }

    /*
     * Insert before position.
     */
    node->next = position;
    node->prev = position->prev;
    node->list = list;

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