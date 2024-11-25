/**************************************************************************************************
 * 
 *************************************************************************************************/

#ifndef EVF_H
#define EVF_H

#include "evf_list.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef EVF_MAX_NUM_ACTIVE_OBJECTS
#define EVF_MAX_NUM_ACTIVE_OBJECTS    32
#endif 

#ifndef EVF_MAX_NUM_USER_DEFINED_EVENT_TYPES
#define EVF_MAX_NUM_USER_DEFINED_EVENT_TYPES   32
#endif

#ifndef EVF_EVENT_QUEUE_LENGTH   
#define EVF_EVENT_QUEUE_LENGTH   16
#endif 

#ifndef EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH
#define EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH    32
#endif

#ifndef EVF_ACTIVE_OBJECT_PRIORITY_MAX
#define EVF_ACTIVE_OBJECT_PRIORITY_MAX    32
#endif

#ifndef EVF_ACTIVE_OBJECT_MAX_NUM_SUBSCRIPTIONS     
#define EVF_ACTIVE_OBJECT_MAX_NUM_SUBSCRIPTIONS    32
#endif

// EVF-defined event types.
#define EVF_EVENT_TYPE_NULL              -3
#define EVF_EVENT_TYPE_SHUTDOWN_PENDING  -2
#define EVF_EVENT_TYPE_TIMER_FINISHED    -1

// User-defined event types must be sequential, starting at this number (inclusive).
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

// Forward-declarations.
struct Evf_active_object;
struct Evf_event;

// Event queue (implemented as a circular buffer).
struct Evf_event_queue
{
    struct Evf_event * p_event_buffer[EVF_EVENT_QUEUE_LENGTH];
    uint32_t ri;
    uint32_t wi;
    uint32_t num_in_queue;
};


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

// This type of event is posted to an active object when one of its timer's (see Evf_timer) finishes.
struct Evf_event_timer_finished
{
    struct Evf_event base; // The type is EVF_EVENT_TYPE_TIMER_FINISHED.
    uint32_t timer_id;
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

// See evf_register_event_destructor for more information.
typedef void (*Evf_event_destructor)(struct Evf_event * p_event);

/* The 'base class' for active objects. When defining your own active objects you must embed an  
 * instance of this struct as the first member. For example...
 * struct Adc_reader_active_object
 * {
 *     struct Evf_active_object base; // Note: name does not matter.
 *     uint32_t adc_channel_number;
 *     uint32_t reading_period_ms; 
 * };
 * 
 * Then when the initialisation of your active object type might look like...
 * struct Adc_reader_active_object channel_1_reader_ao = {
 *     .base = {
 *          .name = "ADC Channel 1 Reader",
 *          .priority = 3,
 *          .handle_event = &adc_reader_handle_event,
 *          .event_type_subscriptions = {
 *              EVENT_TYPE_ADC_SAMPLING_COMPLETE,
 *              EVENT_TYPE_ADC_ERROR,
 *              EVF_EVENT_TYPE_NULL
 *          }
 *      },
 *     .adc_channel_number = 1,
 *     .reading_period_ms  = 50,
 * };
 */
struct Evf_active_object
{
    /*
     *
     */
    char const name[EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH+1];

    /* Priority level (0 is the maximum priority) affect scheduling. A higher priority active 
     * object will be scheduled before any lower priority active object
     */
    uint8_t const priority; 

    /* When the active object gets scheduled by the EVF, an event is taken from the front of the
     * event queue and passed to the handle_event function. This is how all event handling is done.
     */
    Evf_event_handler const handle_event;

    /* When an event of a subscribed event type is published, a reference to that event will be
     * delivered to the active object's event queue (unless it was the one that published it).
     * Note: the active object can still receive any event type via posting. 
     * Note: EVF-defined events (e.g. EVF_EVENT_TYPE_TIMER_WENT_OFF) must not be in this list.
     * Note: this list must be terminated by EVF_EVENT_TYPE_NULL, even when not in use. 
     */ 
    int32_t const event_type_subscriptions[EVF_ACTIVE_OBJECT_MAX_NUM_SUBSCRIPTIONS+1];

    // For EVF-internal use only.
    struct Evf_event_queue event_queue;
};

struct Evf_timer
{
    // The owner active object is who will receive the timer event when the timer finishes. 
    struct Evf_active_object const * p_owner;

    // 
    uint32_t const timer_id;

    uint64_t const time_ms;
    bool const is_periodic;

    // For EVF-internal usage only.
    uint64_t finish_timestamp;
    bool is_running;
    struct Evf_list_item item;
};

/**************************************************************************************************
 * Initialises the EVF. Must be done before any other EVF operations.  
 *************************************************************************************************/
enum Evf_ret evf_init();

/**************************************************************************************************
 * Once an active object is registered it can receive events that it has subscribed to/events that
 * have been posted to it. All registrations must be done prior to calling evf_task.
 *************************************************************************************************/
enum Evf_ret evf_register_active_object(struct Evf_active_object * p_ao);

/**************************************************************************************************
 * Publishes an event to all of the registered active objects that are subscribed to the event
 * type in question. The event must have been allocated using the evf_malloc function. Use NULL
 * for p_publisher if the publish is not being done by an active object.  
 *************************************************************************************************/
enum Evf_ret evf_publish(struct Evf_active_object * p_publisher, struct Evf_event * p_event);

/**************************************************************************************************
 * Posts an event directly to the specified active object. The event must have been allocated using 
 * the evf_malloc function. 
 *************************************************************************************************/
enum Evf_ret evf_post(struct Evf_active_object * p_receiver, struct Evf_event * p_event);

/**************************************************************************************************
 * Starts (or restarts if it is already started) a timer. Note: only the pointer is copied, timers
 * must have static lifetime. Note: should be called only in active object code (e.g. not from
 * an ISR).
 *************************************************************************************************/
enum Evf_ret evf_timer_start(struct Evf_timer * p_timer);

/**************************************************************************************************
 * Stops a timer. Has no effect if the timer is not running.
 *************************************************************************************************/
enum Evf_ret evf_timer_stop(struct Evf_timer * p_timer);

/**************************************************************************************************
 * This function must be called in a loop for the active objects to handle events.
 *************************************************************************************************/
enum Evf_status evf_task();

/**************************************************************************************************
 * Checks if there are any events waiting to be handled in any active object queues. If there are
 * then there is work to be done by the EVF. Only intended for use in embedded systems to 
 * determine if the system can go into low-power mode (LPM). Note: must be called within a critical
 * section.
 *************************************************************************************************/
bool evf_check_if_work_to_do();

/**************************************************************************************************
 * Registering a destructor for an event type means that everytime an event of that type is 
 * finished being handled by all of the active objects that were given the event for handling, the
 * EVF will handle the cleanup by using the registered destructor. This is useful if you have 
 * event types that require cleanup e.g. contain pointers to dynamic memory.
 *************************************************************************************************/
void evf_register_event_destructor(uint32_t event_type, Evf_event_destructor destructor);

/**************************************************************************************************
 * 
 *************************************************************************************************/
void evf_event_set_type(void * p_event, uint32_t type);

#endif // EVF_H
