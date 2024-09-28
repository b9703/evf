
/**************************************************************************************************
 * 
 *************************************************************************************************/

#include "evf.h"
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#if (EVF_ASSERTIONS_ENABLED == 1)
#define EVF_ASSERT(condition)   evf_assert(condition)
#else
#define EVF_ASSERT(condition)  
#endif

#define CONTAINER_OF(p_member, container_type, member_name) \
  ((p_member == NULL) ? NULL : ((container_type *)(((char *)(p_member)) - offsetof(container_type, member_name))))

// This list is always kept in order of decreasing priority (for scheduling reasons).
static struct Evf_list ao_list;





static void evf_assert(bool condition)
{
    assert(condition); 
}

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

static bool evf_list_check_is_empty(struct Evf_list const * p_list)
{
    return (p_list->p_head == NULL) && (p_list->p_tail == NULL);
}

static void evf_list_init(struct Evf_list * p_list)
{
    EVF_ASSERT(p_list != NULL);
    p_list->p_head = NULL;
    p_list->p_tail = NULL;
}

static void evf_list_insert(struct Evf_list * p_list,
                            struct Evf_list_item * p_to_insert,
                            struct Evf_list_item * p_ref_spot)
{
    EVF_ASSERT(p_list != NULL);
    EVF_ASSERT(p_to_insert != NULL);

    if (p_ref_spot == NULL)
    {
        p_ref_spot = p_list->p_tail;
    }

    evf_list_item_set_links(p_ref_spot, p_to_insert, p_ref_spot->p_next);
    if (p_to_insert->p_next == NULL)
    {
        p_list->p_tail = p_to_insert;
    }
}


static void evf_list_remove_item(struct Evf_list * p_list, struct Evf_list_item * p_item)
{
    EVF_ASSERT(p_list != NULL);
    EVF_ASSERT(p_item != NULL);

    // Update the links for the prev and next elements of the item to be removed.
    struct Evf_list_item * p_prev = p_item->p_prev;
    struct Evf_list_item * p_next = p_item->p_next;
    evf_list_item_set_links(p_prev->p_prev, p_prev, p_next);
    evf_list_item_set_links(p_prev, p_next, p_next->p_next);

}

static struct Evf_list_item * evf_list_get_by_index(struct Evf_list * p_list, uint32_t index)
{
    EVF_ASSERT(p_list != NULL);

    struct Evf_list_item * p_result = NULL;
    struct Evf_list_item * p_curr   = p_list->p_head;
    uint32_t i = 0;
    do 
    {
        if (i == index)
        {
            p_result = p_curr;
            break;
        }
        i++;
    } while ((p_curr->p_next) != NULL);
    
    return p_result;
}

static struct Evf_active_object * get_first_active_object()
{
    return CONTAINER_OF(ao_list.p_head, struct Evf_active_object, ao_list_item);
}

static struct Evf_active_object * get_next_active_object(struct Evf_active_object * p_curr)
{
    return CONTAINER_OF(p_curr->ao_list_item.p_next, struct Evf_active_object, ao_list_item);
}

static void insert_into_active_object_list(struct Evf_active_object * p_ao,
                                           struct Evf_active_object * p_ref_spot)
{
    EVF_ASSERT(p_ao != NULL);
    if (p_ref_spot == NULL)
    {
        evf_list_insert(&ao_list, &p_ao->ao_list_item, NULL);
    } 
    else 
    {
        evf_list_insert(&ao_list, &p_ao->ao_list_item, &p_ref_spot->ao_list_item);
    }
}

/**************************************************************************************************
 * EVF API function implementations
 *************************************************************************************************/

enum Evf_ret evf_init()
{
    evf_list_init(&ao_list);
}

enum Evf_ret evf_register_active_object(struct Evf_active_object * p_ao)
{
    EVF_ASSERT(p_ao != NULL);

    evf_list_init(&p_ao->event_queue);

    // Active objects are sorted in order of decreasing priority.
    struct Evf_active_object * p_curr = get_first_active_object();
    while ((p_curr != NULL) && (p_curr->priority > p_ao->priority))
    {
        p_curr = get_next_active_object(p_curr);
    }

    insert_into_active_object_list(p_ao, p_curr);
}


enum Evf_ret evf_deregister_active_object(struct Evf_active_object * p_ao)
{

};

enum Evf_ret evf_publish(struct Evf_event const * p_event)
{

};

enum Evf_ret evf_post(struct Evf_active_object * p_receiver, struct Evf_event const * p_event)
{
    EVF_ASSERT(p_receiver != NULL);
    EVF_ASSERT(p_event != NULL);

    evf_list_insert(&p_receiver->event_queue, )
};

enum Evf_status evf_task()
{

}