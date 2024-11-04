
#ifndef EVF_LIST_H
#define EVF_LIST_H

#include <stdint.h>
#include <stdbool.h>

struct Evf_list_item
{
    struct Evf_list_item * p_prev;
    struct Evf_list_item * p_next;
};

struct Evf_list
{
    struct Evf_list_item * p_head;
    struct Evf_list_item * p_tail; 
    uint32_t length;
};

void evf_list_init(struct Evf_list * p_list);

uint32_t evf_list_get_length(struct Evf_list const * p_list);

void evf_list_insert_after(struct Evf_list * p_list,
                           struct Evf_list_item * p_to_insert,
                           struct Evf_list_item * p_ref_spot);
                           
void evf_list_insert_before(struct Evf_list * p_list,
                           struct Evf_list_item * p_to_insert,
                           struct Evf_list_item * p_ref_spot);

void evf_list_append(struct Evf_list * p_list, struct Evf_list_item * p_to_append);

void evf_list_remove_item(struct Evf_list * p_list, struct Evf_list_item * p_item);


struct Evf_list_item * evf_list_get_by_index(struct Evf_list * p_list, uint32_t index);


#endif // EVF_LIST_H