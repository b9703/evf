
#ifndef EVF_H
#define EVF_H

#include "evf_list.h"
#include <stdint.h>
#include <stdbool.h>

/*
 *
 */
#ifndef EVF_MAX_NUM_ACTIVE_OBJECTS
#define EVF_MAX_NUM_ACTIVE_OBJECTS    32
#endif 

/*
 *
 */
#ifndef EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH
#define EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH    32
#endif

/*
 *
 */
#ifndef EVF_ACTIVE_OBJECT_PRIORITY_MAX
#define EVF_ACTIVE_OBJECT_PRIORITY_MAX    32
#endif

/*
 *
 */
#ifndef EVF_ACTIVE_OBJECT_MAX_NUM_SUBSCRIPTIONS     
#define EVF_ACTIVE_OBJECT_MAX_NUM_SUBSCRIPTIONS    32
#endif

#define EVF_EVENT_TYPE_NULL   -3
#define EVF_EVENT_TYPE_TIMER_WENT_OFF   -1

/*
 *
 */
#define EVF_USER_EVENT_TYPES_START  0

/* A convenient macro to use when allocating events. For example...
 * struct My_custom_event * p_event = EVF_EVENT_ALLOC(struct My_custom_event);
 */
#define EVF_EVENT_ALLOC(type)   evf_malloc(sizeof(type));

enum Evf_ret
{
    EVF_RET_SUCCESS,
    EVF_RET_FAILED,
};

enum Evf_status
{
    EVF_STATUS_RUNNING,
    EVF_STATUS_SHUTDOWN,
};

enum Evf_active_object_status
{
    EVF_ACTIVE_OBJECT_STATUS_RUNNING,
    EVF_ACTIVE_OBJECT_STATUS_SHUTDOWN,
};

struct Evf_active_object;

/* The 'base class' for events. The type field is set by the user, whereas the ref_count field
 * is strictly for EVF-internal usage. Embed an instance of this struct as the first member of
 * any user-defined ('derived class') events. For example...
 * struct My_custom_event
 * {
 *     struct Evf_event base;
 *     int some_data;
 * };
 * 
 * The type field is what you will ultimately use in your active object event handlers in order to
 * cast the Evf_events back to the custom 'derived' class type. Therefore before post/publishing an
 * event you must set the type. Note: all defined types must be sequential starting from 
 * EVF_USER_EVENT_TYPES_START.
 * 
 * struct My_custom_event * p_event = evf_malloc(sizeof(struct My_custom_event));
 * p_event->base.type = EVENT_TYPE_MY_CUSTOM_EVENT;
 * p_event->some_data = 99;
 * 
 * Be careful with user-defined events that use dynamic memory in some way. Destructors for event 
 * types can be registered using the evf_register_event_destructor function.
 */
struct Evf_event
{
    int32_t type; 
    uint32_t ref_count;
};

/* p_self is a pointer to the active object that the handler belongs to. To access instance specific
 * data you should create a 'derived' class by embedding an instance of struct Evf_active_object at
 * the beginning of the derived class' struct. For example...
 * struct My_custom_active_object
 * {
 *     struct Evf_active_object base;
 *     int some_data_1;
 *     char some_string[8];
 * };
 * 
 * The p_event field the event that the active object has been given to handle by the EVF. It may
 * only be accessed for the duration of the function call. If you need to further access to the
 * event data then you must copy it. TODO: consider adding a function that allows active objects
 * to request extended access to an event e.g. evf_event_hold and evf_event_release.
 * 
 * The return value of the event handler specifies whether the active object is still running or
 * not. If it has stopped running then the EVF will de-register it. TODO: how do we handle this?
 * active object memory may need to be freed if it is dynamic. 
 */
typedef enum Evf_status (*Evf_event_handler)(struct Evf_active_object * p_self,
                                             struct Evf_event const * p_event);

/* The 'base class' for active objects. 
 * 
 */
struct Evf_active_object
{
    /*
     *
     */
    char name[EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH+1];

    /* Priority level (0 is the maximum priority) affect scheduling. A higher priority active 
     * object will be scheduled before any lower priority active object
     */
    uint8_t priority; 

    /* When the active object gets scheduled by the EVF, an event is taken from the front of the
     * event queue and passed to the handle_event function. This is how all event handling is done.
     */
    Evf_event_handler handle_event;

    /* When an event of a subscribed event type is published, a reference to that event will be
     * delivered to the active object's event queue (unless it was the one that published it).
     * Note: the active object can still receive any event type via posting. 
     * Note: EVF-defined events (e.g. EVF_EVENT_TYPE_TIMER_WENT_OFF) must not be in this list.
     * Note: this list must be terminated by EVF_EVENT_TYPE_NULL, even when not in use. 
     */ 
    int32_t event_type_subscriptions[EVF_ACTIVE_OBJECT_MAX_NUM_SUBSCRIPTIONS+1];
 
    /* Only for EVF-internal use. This list is used as a queue for events that are to be passed to  
     * the active object's handler function when it gets scheduled by the EVF. 
     */
    struct Evf_list event_queue;     
};

/**************************************************************************************************
 * Initialises the EVF. Must be done before any other EVF operations.  
 *************************************************************************************************/
enum Evf_ret evf_init(uint32_t num_event_types);

/**************************************************************************************************
 * Once an active object is registered it can receive events that it has subscribed to/events that
 * have been posted to it. All registrations must be done prior to calling evf_task.
 *************************************************************************************************/
enum Evf_ret evf_register_active_object(struct Evf_active_object * p_ao);

/**************************************************************************************************
 * Publishes an event to all of the registered active objects that are subscribed to the event
 * type in question. Note: the event must have been allocated using the evf_malloc function.  
 *************************************************************************************************/
enum Evf_ret evf_publish(struct Evf_event * p_event);

/**************************************************************************************************
 * Posts an event directly to the specified active object. Note: the event must have been allocated
 * using the evf_malloc function. 
 *************************************************************************************************/
enum Evf_ret evf_post(struct Evf_active_object * p_receiver, struct Evf_event * p_event);

/**************************************************************************************************
 * 
 *************************************************************************************************/
enum Evf_status evf_task();

#endif // EVF_H