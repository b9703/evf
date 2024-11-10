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

#define ARRAY_LENGTH(arr)    (sizeof(arr)/sizeof(arr[0]))

#if (EVF_ASSERTIONS_ENABLED == 1)
#define EVF_ASSERT(condition)   evf_assert(condition)
#else
#define EVF_ASSERT(condition)  
#endif

#define CONTAINER_OF(p_member, container_type, member_name) \
  ((p_member == NULL) ? NULL : ((container_type *)(((char *)(p_member)) - offsetof(container_type, member_name))))

enum Evf_state 
{
    EVF_STATE_UNINIT
    EVF_STATE_INIT_NOT_STARTED,
    EVF_STATE_STARTED,
    EVF_STATE_SHUTDOWN,
};

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

struct Subscription_table_item
{
    struct Evf_active_object * p_subscribers[EVF_MAX_NUM_ACTIVE_OBJECTS];
    uint32_t num_subscribers;
};

/**************************************************************************************************
 * 
 *************************************************************************************************/
static struct Evf_active_object * registered_aos[EVF_MAX_NUM_ACTIVE_OBJECTS];
static uint32_t num_registered_aos;


static struct Subscription_table_item subscription_table[EVF_MAX_NUM_EVENT_TYPES];

static struct Evf_list priority_scheduling_queue;
static int32_t total_event_references_count;
static uint32_t num_user_defined_event_types;
static enum Evf_state state = EVF_STATE_UNINIT;

static struct Evf_list running_timers_list; // Sorted in order of nearest deadline.


/**************************************************************************************************
 * 
 *************************************************************************************************/

static void evf_event_queue_init(struct Evf_event_queue * p_queue)
{
    p_queue->num_in_queue = 0;
    p_queue->wi = 0;
    p_queue->ri = 0;
}

static bool evf_event_queue_push_back(struct Evf_event_queue * p_queue, 
                                      struct Evf_event * p_event)
{
    if (p_queue->num_in_queue >= ARRAY_LENGTH(p_queue->p_event_buffer)) { return false; }

    p_queue->p_event_buffer[p_queue->wi] = p_event;
    p_queue->wi = (p_queue->wi == ARRAY_LENGTH(p_queue->p_event_buffer)-1) ? 0 : (p_queue->wi + 1);
    p_queue->num_in_queue++;

    return true;
}

static struct Evf_event * evf_event_queue_pop_front(struct Evf_event_queue * p_queue)
{
    if (p_queue->num_in_queue == 0) { return NULL; }

    struct Evf_event * p_event = p_queue->p_event_buffer[p_queue->ri]; 
    p_queue->ri = (p_queue->ri == 0) ? (ARRAY_LENGTH(p_queue->p_event_buffer)-1) : (p_queue->ri - 1);
    p_queue->num_in_queue--;

    return p_event;
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

static void subscription_table_init()
{
    for (uint32_t event_type = 0; event_type < EVF_MAX_NUM_EVENT_TYPES; event_type++)
    {
        subscription_table[event_type].num_subscribers = 0;
    }
}

static void add_event_subscriber(int32_t event_type, struct Evf_active_object * p_ao)
{
    struct Subscription_table_item * p_item = &subscription_table[event_type];
    EVF_ASSERT(p_item->num_subscribers < EVF_MAX_NUM_ACTIVE_OBJECTS);
    p_item->p_subscribers[p_item->num_subscribers++] = p_ao;
}

static void event_ref_count_init(struct Evf_event * p_event)
{
    p_event->ref_count = 0;
}

static bool post_event_to_active_object(struct Evf_active_object * p_ao, struct Evf_event * p_event)
{
    if (!evf_event_queue_push_back(&p_receiver->event_queue, p_event))
    {
        p_event->ref_count++;
        total_event_references_count++;
        return true;
    }
    return false;
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

static void schedule_callback_for_next_to_finish_timer()
{
    struct Evf_timer * p_next_to_finish = CONTAINER_OF(running_timers_list.p_head, struct Evf_timer, item);
    if (p_next_to_finish != NULL)
    {
        evf_schedule_callback(p_next_to_finish->finish_time, &timer_handler_callback);
    }
}

static void running_timers_list_register_timer(struct Evf_timer * p_timer)
{
    if (running_timers_list.length == 0)
    {
        evf_list_append(&running_timers_list, &p_timer->item);
    }
    else
    {
        // Running timers list is sorted by order of increasing finish time.
        struct Evf_list_item * p_curr = running_timers_list.p_head;
        struct Evf_timer * p_curr_timer = NULL;
        do 
        {
            p_curr_timer = CONTAINER_OF(p_curr, struct Evf_timer, item);
        } while((p_timer->finish_time >= p_curr_timer->finish_time)
            && ((p_curr = p_curr->p_next) != NULL));

        evf_list_insert_after(&running_timers_list, p_curr, &p_timer->item);
    }
    
    // The head of the running timers list is the next to finish...
    if (evf_list_check_item_is_head(&running_timers_list, &p_timer->item))
    {
        schedule_callback_for_next_to_finish_timer();
    }
}

static bool running_timers_list_remove(struct Evf_timer * p_timer)
{
    // If timer being removed is was the next to finish, then we now have a new 'next to finish'...
    bool was_next_to_finish = evf_list_check_item_is_head(&running_timers_list, &p_timer->item);
    evf_list_remove_item(&running_timers_list, &p_timer->item);
    if (was_next_to_finish)
    {
        schedule_callback_for_next_to_finish_timer();
    }    
}

static void timer_handler_callback()
{
    /* TODO: consider making this a bit more sophisticated. If there are many timers to go off 
     * at a time then 'now' may be outdated and we may miss the deadline of some timers.
     */
    uint64_t now = evf_get_timestamp_ms();

    evf_critical_section_enter();
    struct Evf_list_item * p_curr_item = running_timers_list.p_head;
    struct Evf_timer * p_curr_timer = NULL;
    do 
    {
        p_curr_timer = CONTAINER_OF(p_curr_item, struct Evf_timer, item);
        if (p_curr_timer->finish_time >= now)
        {
            struct Evf_event_timer_finished * p_event = EVF_EVENT_ALLOC(struct Evf_event_timer_finished);
            evf_event_set_type(p_event, EVF_EVENT_TYPE_TIMER_FINISHED);
            p_event->timer_id = p_curr_timer->timer_id;
            event_ref_count_init(p_event);
            bool okay = post_event_to_active_object(p_curr_timer->p_owner, p_event);
            evf_list_remove_item(&running_timers_list, p_curr_item); //NOTE: cant do this before setting the next.
        }
        else
        {
            break;
        }

    } while((p_curr_item = p_curr_item->p_next) != NULL);
    evf_critical_section_exit();

    schedule_callback_for_next_to_finish_timer();
}

/**************************************************************************************************
 * EVF API function implementations
 *************************************************************************************************/

enum Evf_ret evf_init(uint32_t num_event_types)
{
    EVF_ASSERT(state == EVF_STATE_UNINIT);
    num_user_defined_event_types = num_event_types;
    num_registered_aos = 0;
    total_event_references_count = 0;
    subscription_table_init();
    evf_list_init(&running_timers_list);
    state = EVF_STATE_INIT_NOT_STARTED;
}

enum Evf_ret evf_register_active_object(struct Evf_active_object * p_ao)
{
    EVF_ASSERT(state == EVF_STATE_INIT_NOT_STARTED);
    EVF_ASSERT(p_ao != NULL);
    registered_aos[num_registered_aos++] = p_ao;
    for (uint32_t i = 0; p_ao->event_type_subscriptions[i] != EVF_EVENT_TYPE_NULL; i++)
    {
        add_event_subscriber(p_ao->event_type_subscriptions[i], p_ao);    
    }
}

enum Evf_ret evf_publish(struct Evf_event * p_event)
{
    EVF_ASSERT(state == EVF_STATE_STARTED);
    EVF_ASSERT(p_event != NULL);

    struct Subscription_table_item const * p_item = &subscription_table[p_event->type];
    for (uint32_t i = 0; i < p_item->num_subscribers; i++)
    {
        struct Evf_active_object * p_receiver = p_item->p_subscribers[i];

        /* Events may be posted from (other) ISRs/threads so we need to protect each post 
         * from pre-emption.
         */
        evf_critical_section_enter();
        post_event_to_active_object(p_receiver, p_event);
        evf_critical_section_exit();
    }
};

enum Evf_ret evf_post(struct Evf_active_object * p_receiver, struct Evf_event * p_event)
{
    EVF_ASSERT(state == EVF_STATE_STARTED);
    EVF_ASSERT(p_receiver != NULL);
    EVF_ASSERT(p_event != NULL);

    event_ref_count_init(p_event);

    evf_critical_section_enter();
    bool okay = post_event_to_active_object(p_receiver, p_event);
    evf_critical_section_exit();

    return okay;
};

enum Evf_ret evf_timer_start(struct Evf_timer * p_timer)
{
    EVF_ASSERT(p_timer != NULL);
    EVF_ASSERT(p_timer->p_owner != NULL);

    evf_critical_section_enter();

    if (p_timer->is_running)
    {
        running_timers_list_remove(p_timer);
    }

    p_timer->finish_time = evf_get_timestamp_ms() + p_timer->time_ms;

    running_timers_list_register_timer(p_timer);
    p_timer->is_running = true;

    evf_critical_section_exit();
}

enum Evf_ret evf_timer_stop(struct Evf_timer * p_timer)
{

}

enum Evf_status evf_task()
{
    struct Evf_active_object * p_ao = get_next_scheduled_active_object();
    if (p_ao != NULL)
    {
        evf_critical_section_enter();
        struct Evf_event * p_event = evf_event_queue_pop_front(&p_ao->event_queue);
        evf_critical_section_exit();

        /*
         *
         */
        enum Evf_status status = p_ao->handle_event(p_ao, p_event);
        destroy_event_reference(p_event);
    }
}

void evf_event_set_type(void * p_event, uint32_t type)
{
    struct Evf_event * p_evf_event = (struct Evf_event *)p_event;
    p_evf_event->type = type;
}