
#ifndef EVF_H
#define EVF_H

#include <stdint.h>
#include <stdbool.h>

#ifndef EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH
#define EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH    32
#endif

#ifndef EVF_ACTIVE_OBJECT_PRIORITY_MAX
#define EVF_ACTIVE_OBJECT_PRIORITY_MAX    32
#endif


struct Evf_list_item
{
    struct Evf_list_item * p_prev;
    struct Evf_list_item * p_next;
};

struct Evf_list
{
    struct Evf_list_item * p_head;
    struct Evf_list_item * p_tail; 
};

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

struct Evf_active_object;

/* The 'base class' for events. Fields are only for EVF-internal usage (see evf_event_ functions).
 * Embed an instance of this struct as the first member of any user-defined ('derived class') events.
 * For example...
 * struct My_custom_event
 * {
 *     struct Evf_event base;
 *     int some_data;
 * };
 * 
 * Be careful with user-defined events that use dynamic memory in some way. Destructors for event 
 * types can be registered using the evf_register_event_destructor function.
 */
struct Evf_event
{
    int32_t type; 
    uint32_t ref_count;
    struct Evf_list_item event_queue_list_item;
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
    char name[EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH+1];
    uint8_t priority; 
    Evf_event_handler handle_event;
 
    /* Only for EVF-internal use. This list is used as a queue for events that are to be passed to  
     * the active object's handler function when it gets scheduled by the EVF. 
     */
    struct Evf_list event_queue;     

    /* Only for EVF-internal use. This list item allows an active object instance to be added to
     * the EVF-internal list when registered. 
     */
    struct Evf_list_item ao_list_item;
};


enum Evf_ret evf_init();

/* 
 * 
 */
enum Evf_ret evf_register_active_object(struct Evf_active_object * p_ao);

/* 
 * 
 */
enum Evf_ret evf_deregister_active_object(struct Evf_active_object * p_ao);

enum Evf_ret evf_publish(struct Evf_event const * p_event);

enum Evf_ret evf_post(struct Evf_active_object * p_receiver, struct Evf_event const * p_event);

/* 
 * 
 */
enum Evf_status evf_task();

#endif // EVF_H