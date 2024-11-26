
#include "evf_list.h"
#include <stddef.h>

/* Note: p_prev and p_next can be NULL e.g. if p_item is to be the new head or new tail of the
 * list. 
 */
static void evf_list_item_set_links(struct Evf_list_item * p_prev, 
                                    struct Evf_list_item * p_item,
                                    struct Evf_list_item * p_next)
{
    if (p_prev != NULL) { p_prev->p_next = p_item; }
    if (p_next != NULL) { p_next->p_prev = p_item; }
    if (p_item != NULL) 
    {
        p_item->p_prev = p_prev;
        p_item->p_next = p_next; 
    }
}

void evf_list_init(struct Evf_list * p_list)
{
    EVF_ASSERT(p_list != NULL);
    p_list->p_head = NULL;
    p_list->p_tail = NULL;
    p_list->length = 0;
}

void evf_list_item_init(struct Evf_list_item * p_item)
{
    p_item->p_next = NULL;
    p_item->p_prev = NULL;
}

uint32_t evf_list_get_length(struct Evf_list const * p_list)
{
    return p_list->length;
}

void evf_list_insert_after(struct Evf_list * p_list,
                           struct Evf_list_item * p_to_insert,
                           struct Evf_list_item * p_ref_spot)
{
    EVF_ASSERT(p_list != NULL);
    EVF_ASSERT(p_to_insert != NULL);
    EVF_ASSERT(p_ref_spot != NULL);

    evf_list_item_set_links(p_ref_spot, p_to_insert, p_ref_spot->p_next);
    if (p_to_insert->p_next == NULL)
    {
        p_list->p_tail = p_to_insert;
    }
    p_list->length++;
}

void evf_list_insert_before(struct Evf_list * p_list,
                           struct Evf_list_item * p_to_insert,
                           struct Evf_list_item * p_ref_spot)
{
    EVF_ASSERT(p_list != NULL);
    EVF_ASSERT(p_to_insert != NULL);
    EVF_ASSERT(p_ref_spot != NULL);

    evf_list_item_set_links(p_ref_spot->p_prev, p_to_insert, p_ref_spot);
    if (p_to_insert->p_prev == NULL)
    {
        p_list->p_head = p_to_insert;
    }
    p_list->length++;
}

void evf_list_append(struct Evf_list * p_list, struct Evf_list_item * p_to_append)
{
    EVF_ASSERT(p_list != NULL);
    EVF_ASSERT(p_to_append != NULL);

    evf_list_item_set_links(p_list->p_tail, p_to_append, NULL);
    p_list->p_tail = p_to_append;
    if (p_to_append->p_prev == NULL)
    {
        p_list->p_head = p_to_append;
    }
    p_list->length++;
}

void evf_list_remove_item(struct Evf_list * p_list, struct Evf_list_item * p_item)
{
    EVF_ASSERT(p_list != NULL);
    EVF_ASSERT(p_item != NULL);

    // Update the links for the prev and next elements of the item to be removed.
    struct Evf_list_item * p_prev = p_item->p_prev;
    struct Evf_list_item * p_next = p_item->p_next;
    evf_list_item_set_links(p_prev->p_prev, p_prev, p_next);
    evf_list_item_set_links(p_prev, p_next, p_next->p_next);
    p_list->length--;
}
