/**************************************************************************************************
 * 
 * 
 * 
 * 
 * How should low-power mode be accomodated? if we have a function like evf_check_if_idle() which 
 * would just check if there are any events in the queue/any aos scheduled. this could be used by
 * the application, along with any other modules that they have included in, to determine if low
 * power mode can be entered.
 * 
 * 
 * 
 * what does evf critical section need to handle nested use? e.g. if in a critical section and you
 * and you want to publish which also uses a crit section? they should nest.
 *************************************************************************************************/


#include "evf.h"
#include "port/evf_port.h"
#include <stddef.h>
#include <stdbool.h>

#if (EVF_ASSERTIONS_ENABLED == 1)
#define EVF_ASSERT(condition)   evf_assert(condition)
#else
#define EVF_ASSERT(condition)  
#endif

#define CONTAINER_OF(p_member, container_type, member_name) \
  ((p_member == NULL) ? NULL : ((container_type *)(((char *)(p_member)) - offsetof(container_type, member_name))))

struct Event_list_item
{
    struct Evf_event * p_event;
    struct Evf_list_item item;
};
 
struct Active_object_list_item
{
    struct Evf_active_object * p_ao;
    struct Evf_list_item item;
};

static struct Evf_active_object * registered_aos[EVF_MAX_NUM_ACTIVE_OBJECTS];
static uint32_t num_registered_aos;

static struct Evf_list priority_scheduling_queue;
static struct Evf_list * subscriber_lists_table; // Lookup-table (event type is the key).
static int32_t total_event_references_count;
static uint32_t num_user_defined_event_types;


static void insert_into_active_object_list(struct Evf_active_object * p_ao,
                                           struct Evf_active_object * p_ref_spot)
{
    EVF_ASSERT(p_ao != NULL);
    if (p_ref_spot == NULL)
    {
        evf_list_insert(&registered_active_objects, &p_ao->ao_list_item, NULL);
    } 
    else 
    {
        evf_list_insert(&registered_active_objects, &p_ao->ao_list_item, &p_ref_spot->ao_list_item);
    }
}

static struct Evf_active_object * get_next_scheduled_active_object()
{
    struct Active_object_list_item * p_next_scheduling_item = CONTAINER_OF(priority_scheduling_queue.p_head,
                                                                           struct Active_object_list_item,
                                                                           item);
    if (p_next_scheduling_item == NULL)
    {
        return NULL;
    } 

    struct Evf_active_object * p_next_scheduled_ao = p_next_scheduling_item->p_ao;
    evf_list_remove_item(&priority_scheduling_queue, &p_next_scheduling_item->item);
    evf_free(p_next_scheduling_item);

    return p_next_scheduled_ao;
}

static void schedule_active_object_rtc_step(struct Evf_active_object * p_ao)
{
    struct Active_object_list_item * p_to_insert = evf_malloc(sizeof(struct Active_object_list_item));
    EVF_ASSERT(p_to_insert != NULL);
    p_to_insert->p_ao = p_ao;

    /* The AO's RTC (run-to-completion) step is scheduled to occur after any same or higher
     * priority AO RTC steps, but before any of lower priority.         
     */
    struct Active_object_list_item * p_curr = CONTAINER_OF(priority_scheduling_queue.p_head,
                                                           struct Active_object_list_item,
                                                           item);
    struct Active_object_list_item * p_first_of_lower_priority = NULL;
    while (p_curr != NULL)
    {
        if (p_curr->p_ao->priority < p_ao->priority)
        {
            p_first_of_lower_priority = p_curr;
            break;
        }
    }

    if (p_first_of_lower_priority == NULL)
    {
        evf_list_append(&priority_scheduling_queue, &p_to_insert->item);
    }
    else
    {
        evf_list_insert_before(&priority_scheduling_queue,
                               &p_to_insert->item,
                               &p_first_of_lower_priority->item);
    }
}

static void subscriber_lists_lookup_table_init()
{
    subscriber_lists_table = evf_malloc(num_user_defined_event_types * sizeof(struct Evf_list));
    EVF_ASSERT(subscriber_lists_table != NULL);
    for (uint32_t i = 0; i < num_user_defined_event_types; i++)
    {
        evf_list_init(&subscriber_lists_table[i]);
    }
}

static void add_event_subscriber(struct Evf_active_object * p_ao, int32_t event_type)
{
    // Event types must be defined sequentially starting from 0.
    EVF_ASSERT(event_type < num_user_defined_event_types); 

    struct Active_object_list_item * p_ao_item = evf_malloc(sizeof(struct Active_object_list_item));
    EVF_ASSERT(p_ao_item != NULL);
    p_ao_item->p_ao = p_ao;
    
    evf_list_append(&subscriber_lists_table[event_type], &p_ao_item->item);
}

static void event_ref_count_init(struct Evf_event * p_event)
{
    p_event->ref_count = 0;
}


/**************************************************************************************************
 * EVF API function implementations
 *************************************************************************************************/

enum Evf_ret evf_init(uint32_t num_event_types)
{
    num_user_defined_event_types = num_event_types;
    evf_list_init(&registered_active_objects);
    subscriber_lists_lookup_table_init();
    total_event_references_count = 0;
}

enum Evf_ret evf_register_active_object(struct Evf_active_object * p_ao)
{
    EVF_ASSERT(p_ao != NULL);
    evf_list_init(&p_ao->event_queue);
    evf_list_append(&registered_active_objects, &p_ao->ao_list_item);

    for (uint32_t i = 0; p_ao->event_type_subscriptions[i] != EVF_EVENT_TYPE_NULL; i++)
    {
        add_event_subscriber(p_ao, p_ao->event_type_subscriptions[i]);    
    }
}

static void post_event_to_active_object(struct Evf_active_object * p_ao,
                                        struct Event_list_item * p_event_item)
{
    evf_list_append(&p_ao->event_queue, &p_event_item->item);
    p_event_item->p_event->ref_count++;
    total_event_references_count++;
}

enum Evf_ret evf_publish(struct Evf_event * p_event)
{
    EVF_ASSERT(p_event != NULL);

    // TODO: i want to allocate all the list items before go to a crit section to post them. 
    // that got me thinking. Do we want to require malloc to be usable in isr ccontext. I guess
    // this mandatory if we want to publish/post events from an isr.

    
    for (uint32_t i = 0; i < subscriber_lists_table[i].length; i++)
    {
        struct Event_list_item * p_queue_item = evf_malloc(sizeof(struct Event_list_item));
        p_queue_item->p_event = p_event;
    }
    
    post_event_to_active_object(p_receiver, p_event);
};

enum Evf_ret evf_post(struct Evf_active_object * p_receiver, struct Evf_event * p_event)
{
    EVF_ASSERT(p_receiver != NULL);
    EVF_ASSERT(p_event != NULL);

    event_ref_count_init(p_event);
    struct Event_list_item * p_item = evf_malloc(sizeof(struct Event_list_item));
    p_item->p_event = p_event;
    
    evf_critical_section_enter();
    post_event_to_active_object(p_receiver, p_event);
    evf_critical_section_exit();
};

static struct Evf_event * pop_event_from_active_object_event_queue(struct Evf_active_object * p_ao)
{
    struct Event_list_item * p_queue_item = CONTAINER_OF(p_ao->event_queue.p_head, struct Event_list_item, item);
    if (p_queue_item == NULL)
    {
        return NULL;
    }

    struct Evf_event * p_event = p_queue_item->p_event;
    evf_list_remove_item(&p_ao->event_queue, &p_queue_item->item);
    evf_free(p_queue_item);

    return p_event;
}

static void destroy_event_reference(struct Evf_event * p_event)
{
    p_event->ref_count--;
    if (p_event->ref_count == 0)
    {
        // TODO: call event destructor if there is one.
        evf_free(p_event);
    }
}

enum Evf_status evf_task()
{
    struct Evf_active_object * p_ao = get_next_scheduled_active_object();
    if (p_ao != NULL)
    {
        struct Evf_event * p_event = pop_event_from_active_object_event_queue(p_ao);
        enum Evf_status status = p_ao->handle_event(p_ao, p_event);
        destroy_event_reference(p_event);
    }
}