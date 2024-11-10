/**************************************************************************************************
 * The functions in this module must be implemented in order for the EVF to compile. The functions
 * are for EVF-internal use only, unless specified otherwise.
 *************************************************************************************************/

#ifndef EVF_PORT_H
#define EVF_PORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef void (*Evf_timer_callback)();

/**************************************************************************************************
 * To be used by application code to allocate events. 
 *************************************************************************************************/
void * evf_malloc(size_t num_bytes);

/**************************************************************************************************
 *  
 *************************************************************************************************/
void evf_free(void * p_memory);

/**************************************************************************************************
 * 
 *************************************************************************************************/
void evf_assert(bool condition);

/**************************************************************************************************
 * Consider the execution contexts that EVF functions are to be called in your application. For
 * example...
 * - If everything is done in a big single-threaded superloop with no interrupts then critical
 *   sections are not required so the enter and exit functions can be empty.
 * - If you are publishing/posting events from an ISR then your enter and exit functions should 
 *   probably disable and restore interrupts respectively.
 * - If you are on a multi-threaded system and are publishing/posting events from different threads
 *   to where the evf_task function is being called then you could use a mutex (TODO: this may not
 *   be appropriate?).
 *
 * Note: must be nestable.
 *************************************************************************************************/
void evf_critical_section_enter();
void evf_critical_section_exit();

/**************************************************************************************************
 * Gets the number of elapsed milliseconds since program start. The real wall-clock time should 
 * have no bearing on this timer. For example, if the wall-clock time is changed, say when a sync
 * with an NTP server is done, this timer should be completely unaffected. 
 * Note: it does not strictly have to be since program start. Whats important is that it accurately
 * counts milliseconds elapsed and that it is always increasing.
 *************************************************************************************************/
uint64_t evf_get_timestamp_ms(); 

/**************************************************************************************************
 * Schedules a callback at a particular timestamp. This must be the same counter that is used for
 * evf_get_timestamp_ms. Only one callback can be scheduled at a time. Scheduling a new callback
 * will replace the existing one if there is one. This function must be able to be called from the
 * same context as where the callback will be called from e.g. if the callback will be called from
 * an ISR then this function must also be callable from an ISR. 
 *************************************************************************************************/
void evf_schedule_callback(uint64_t timestamp_ms, Evf_timer_callback callback); 

/**************************************************************************************************
 *
 *************************************************************************************************/
void evf_cancel_scheduled_callback(); 

#endif // EVF_PORT_H