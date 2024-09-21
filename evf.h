
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

enum Evf_status
{
    EVF_STATUS_RUNNING,
    EVF_STATUS_SHUTDOWN,
};



/* The 'base class' for events. Fields are only for EVF-internal usage (see evf_event_ functions).
 * Embed an instance of this struct as the first member of any user-defined ('derived class') events.
 * For example...
 * struct My_custom_event
 * {
 *     struct Evf_event base;
 *     int some_data;
 * };
 */
struct Evf_event
{
    int32_t type; 
    uint32_t ref_count;
};

struct Evf_active_object;
typedef enum Evf_status (*Evf_event_handler)(struct Evf_active_object * p_self,
                                             struct Evf_event * p_event);


struct Evf_active_object
{
    char name[EVF_ACTIVE_OBJECT_MAX_NAME_LENGTH+1];
    struct Evf_list event_queue;     
    uint8_t priority; 
};


#endif // EVF_H