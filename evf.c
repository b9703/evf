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

#define CHECK_EVENT_TYPE_IS_IN_USER_DEFINED_RANGE(type) \
    ((type >= EVF_USER_EVENT_TYPES_START) && (type < EVF_MAX_NUM_USER_DEFINED_EVENT_TYPES))

enum Evf_state 
{
    EVF_STATE_UNINIT,
    EVF_STATE_INIT_NOT_RUNNING,
    EVF_STATE_RUNNING,
    EVF_STATE_SHUTDOWN,
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
 * Static Variables 
 *************************************************************************************************/

static enum Evf_state evf_state = EVF_STATE_UNINIT;

static struct Evf_active_object * registered_aos[EVF_MAX_NUM_ACTIVE_OBJECTS];
static uint32_t num_registered_aos;

static struct Subscription_table_item subscription_table[EVF_MAX_NUM_USER_DEFINED_EVENT_TYPES];

// Determines the order that active objects are scheduled to handle events.
static struct Evf_list priority_scheduling_queue;

// Sorted in order of nearest deadline.
static struct Evf_list running_timers_list; 

static Evf_event_destructor event_destructors[EVF_MAX_NUM_USER_DEFINED_EVENT_TYPES];

/**************************************************************************************************
 * Static Functions 
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

static bool check_evf_state_allows_active_objects_to_receive_events()
{
    /* As soon as the EVF is initialised, active objects can receive events, however they will not
     * be handled until evf_task starts being called. Also, as soon as the EVF goes into shutdown 
     * mode, no more events can be received TODO: is this right?
     */
    return (evf_state == EVF_STATE_INIT_NOT_RUNNING)
        || (evf_state == EVF_STATE_RUNNING);
}

static struct Evf_active_object * get_next_scheduled_active_object()
{
    struct Active_object_list_item * p_next = CONTAINER_OF(priority_scheduling_queue.p_head,
                                                           struct Active_object_list_item,
                                                           item);
    if (p_next == NULL)
    {
        return NULL;
    } 

    struct Evf_active_object * p_next_scheduled = p_next->p_ao;
    evf_list_remove_item(&priority_scheduling_queue, &p_next->item);
    evf_free(p_next);

    return p_next_scheduled;
}

static void schedule_active_object_rtc_step(struct Evf_active_object * p_ao)
{
    // TODO: consider having an internal allocator for quicker allocation here.
    struct Active_object_list_item * p_to_insert = evf_malloc(sizeof(struct Active_object_list_item));
    EVF_ASSERT(p_to_insert != NULL);
    p_to_insert->p_ao = p_ao;

    /* The AO's RTC (run-to-completion) step is scheduled to occur after any same or higher
     * priority AO RTC steps (i.e. before any of lower priority).        
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
    for (uint32_t event_type = 0; event_type < EVF_MAX_NUM_USER_DEFINED_EVENT_TYPES; event_type++)
    {
        subscription_table[event_type].num_subscribers = 0;
    }
}

static void event_type_destructors_init()
{
    for (uint32_t event_type = 0; event_type < EVF_MAX_NUM_USER_DEFINED_EVENT_TYPES; event_type++)
    {
        event_destructors[event_type] = NULL;
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
    bool was_posted = false;
    if (evf_event_queue_push_back(&p_receiver->event_queue, p_event))
    {
        p_event->ref_count++;
        schedule_active_object_rtc_step(p_ao);
        was_posted = true;
    }

    return was_posted;
}

static void destroy_event_reference(struct Evf_event * p_event)
{
    p_event->ref_count--;
    if (p_event->ref_count == 0)
    {
        Evf_event_destructor dtor = event_destructors[p_event->type];
        if (dtor != NULL) { dtor(p_event); }
        evf_free(p_event);
    }
}

static void handle_timer_handler_callback_scheduling()
{
    struct Evf_timer * p_next_to_finish = CONTAINER_OF(running_timers_list.p_head, struct Evf_timer, item);
    if (p_next_to_finish != NULL)
    {
        evf_schedule_callback(p_next_to_finish->finish_time, &timer_handler_callback);
    }
    else // There is no next to finish so no reason to have a callback scheduled...
    {
        evf_cancel_scheduled_callback();
    }
}

static void running_timers_list_add_timer(struct Evf_timer * p_timer)
{
    // Running timers list is sorted in order of increasing finish time.
    struct Evf_list_item * p_curr = running_timers_list.p_head;
    struct Evf_timer * p_curr_timer = NULL;
    do 
    {
        p_curr_timer = CONTAINER_OF(p_curr, struct Evf_timer, item);
    } while((p_timer->finish_time >= p_curr_timer->finish_time)
        && ((p_curr = p_curr->p_next) != NULL));

    if (p_curr == NULL) { evf_list_append(&running_timers_list, &p_timer->item); }
    else { evf_list_insert_after(&running_timers_list, &p_timer->item, p_curr); }

    // Re-schedule the timer handling callback if new timer is next to finish.
    if (evf_list_check_item_is_head(&running_timers_list, &p_timer->item))
    {
        handle_timer_handler_callback_scheduling();
    }
}

static void running_timers_list_remove_timer(struct Evf_timer * p_timer)
{
    // If timer being removed is the next to finish, then we now have a new 'next to finish'...
    bool was_next_to_finish = evf_list_check_item_is_head(&running_timers_list, &p_timer->item);
    evf_list_remove_item(&running_timers_list, &p_timer->item);
    if (was_next_to_finish)
    {
        handle_timer_handler_callback_scheduling();
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
            /* TODO: consider moving the event allocation and posting out of the critical section.
             * If we just remove the finished timers from the running timers list and then exit 
             * the critical section then higher priority interrupts, or any interrupt if timers are
             * handled in the application superloop, can execute. This is not a priority at the
             * moment, do some tests later to check execution speed.  
             */
            struct Evf_event_timer_finished * p_event = EVF_EVENT_ALLOC(struct Evf_event_timer_finished);
            evf_event_set_type(p_event, EVF_EVENT_TYPE_TIMER_FINISHED);
            p_event->timer_id = p_curr_timer->timer_id;
            event_ref_count_init(p_event);
            bool okay = post_event_to_active_object(p_curr_timer->p_owner, p_event);

            evf_list_remove_item(&running_timers_list, p_curr_item); //NOTE: cant do this before setting the next.
        }
        else // Since running timers list is sorted by increasing finish time...
        {
            break;
        }

    } while((p_curr_item = p_curr_item->p_next) != NULL);
    evf_critical_section_exit();

    handle_timer_handler_callback_scheduling();
}

static void add_active_object_to_registered_array(struct Evf_active_object * p_ao)
{
    EVF_ASSERT(num_registered_aos < EVF_MAX_NUM_ACTIVE_OBJECTS);
    registered_aos[num_registered_aos++] = p_ao;
}

static void register_active_object_event_type_subscriptions(struct Evf_active_object * p_ao)
{
    int32_t curr_type;
    for (uint32_t i = 0; (curr_type = p_ao->event_type_subscriptions[i]) != EVF_EVENT_TYPE_NULL; i++)
    {
        EVF_ASSERT(i < EVF_ACTIVE_OBJECT_MAX_NUM_SUBSCRIPTIONS);
        EVF_ASSERT(CHECK_EVENT_TYPE_IS_VALID(curr_type));
        add_event_subscriber(p_ao->event_type_subscriptions[i], p_ao);    
    }
}

/**************************************************************************************************
 * EVF API function implementations
 *************************************************************************************************/

enum Evf_ret evf_init()
{
    EVF_ASSERT(evf_state == EVF_STATE_UNINIT);
    num_registered_aos = 0;
    subscription_table_init();
    event_type_destructors_init();
    evf_list_init(&running_timers_list);
    evf_state = EVF_STATE_INIT_NOT_RUNNING;
}

enum Evf_ret evf_register_active_object(struct Evf_active_object * p_ao)
{
    EVF_ASSERT(evf_state == EVF_STATE_INIT_NOT_RUNNING);
    EVF_ASSERT(p_ao != NULL);

    add_active_object_to_registered_array(p_ao);
    register_active_object_event_type_subscriptions(p_ao);
}

enum Evf_ret evf_publish(struct Active_object * p_publisher, struct Evf_event * p_event)
{
    EVF_ASSERT(p_event != NULL);
    EVF_ASSERT(check_evf_state_allows_active_objects_to_receive_events());

    struct Subscription_table_item const * p_item = &subscription_table[p_event->type];
    for (uint32_t i = 0; i < p_item->num_subscribers; i++)
    {
        struct Evf_active_object * p_receiver = p_item->p_subscribers[i];

        // Published events don't go to the active object that is doing the publishing.
        if (p_receiver == p_publisher)
        {
            continue;
        }

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
    EVF_ASSERT(check_evf_state_allows_active_objects_to_receive_events());
    EVF_ASSERT(p_receiver != NULL);
    EVF_ASSERT(p_event != NULL);
    EVF_ASSERT(CHECK_EVENT_TYPE_IS_IN_USER_DEFINED_RANGE(event_type));

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
        running_timers_list_remove_timer(p_timer);
    }

    p_timer->finish_time = evf_get_timestamp_ms() + p_timer->time_ms;

    running_timers_list_add_timer(p_timer);
    p_timer->is_running = true;

    evf_critical_section_exit();
}

enum Evf_ret evf_timer_stop(struct Evf_timer * p_timer)
{
    // 

}

enum Evf_status evf_task()
{
    struct Evf_active_object * p_ao = get_next_scheduled_active_object();
    if (p_ao != NULL)
    {
        evf_critical_section_enter();
        struct Evf_event * p_event = evf_event_queue_pop_front(&p_ao->event_queue);
        evf_critical_section_exit();

        enum Evf_status status = p_ao->handle_event(p_ao, p_event);
        destroy_event_reference(p_event);
    }

    evf_state = EVF_STATE_RUNNING;
}

bool evf_check_if_work_to_do()
{
    return (evf_list_get_length(&priority_scheduling_queue) != 0);
}

void evf_register_event_destructor(uint32_t event_type, Evf_event_destructor destructor)
{
    EVF_ASSERT(CHECK_EVENT_TYPE_IS_IN_USER_DEFINED_RANGE(event_type));
    event_destructors[event_type] = destructor;
}

void evf_event_set_type(void * p_event, uint32_t type)
{
    struct Evf_event * p_evf_event = (struct Evf_event *)p_event;
    p_evf_event->type = type;
}



