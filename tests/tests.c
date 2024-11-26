

#include "../evf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum Test_event_types
{
    EVENT_TYPE_MSG_RECEIVED = EVF_USER_EVENT_TYPES_START,
    EVENT_TYPE_BUTTON_PRESSED,
    EVENT_TYPE_ADC_READING,
};

struct Event_msg_received
{
    struct Evf_event base;
    char msg[256];
};

struct Event_button_pressed
{
    struct Evf_event base;
    int button_id;
};

struct Event_adc_reading
{
    struct Evf_event base;
    int sample;
};

struct Test_active_object_a
{
    struct Evf_active_object base;
    int test_data_x;
};

struct Test_active_object_b
{
    struct Evf_active_object base;
    char const * test_data_string;
};


enum Evf_status test_active_object_a_handler(struct Evf_active_object * p_self, 
                                            struct Evf_event const * p_event)
{
    printf("Test_active_object_a (%s) handling event %d\n", p_self->name, p_event->type);
    return EVF_STATUS_RUNNING;
}


enum Evf_status test_active_object_b_handler(struct Evf_active_object * p_self, 
                                            struct Evf_event const * p_event)
{
    printf("Test_active_object_b (%s) handling event %d\n", p_self->name, p_event->type);
    return EVF_STATUS_RUNNING;
}

struct Test_active_object_a test_active_object_a1 = {
    .base = {
        .name         = "A1", 
        .priority     = 10,
        .handle_event = &test_active_object_a_handler,
        .event_type_subscriptions = {
            EVENT_TYPE_ADC_READING,
            EVENT_TYPE_BUTTON_PRESSED,
            EVF_EVENT_TYPE_NULL,
        }
    },
    .test_data_x = 99
};

struct Test_active_object_a test_active_object_a2 = {
    .base = {
        .name         = "A2", 
        .priority     = 11,
        .handle_event = &test_active_object_a_handler,
        .event_type_subscriptions = {
            EVENT_TYPE_MSG_RECEIVED,
            EVENT_TYPE_ADC_READING,
            EVF_EVENT_TYPE_NULL,
        }
    },
    .test_data_x = 42
};

struct Test_active_object_b test_active_object_b1 = {
    .base = {
        .name         = "B1", 
        .priority     = 9,
        .handle_event = &test_active_object_b_handler,
        .event_type_subscriptions = {
            EVENT_TYPE_BUTTON_PRESSED,
            EVENT_TYPE_MSG_RECEIVED,
            EVF_EVENT_TYPE_NULL,
        }
    },
    .test_data_string = "Hello World"
};

int main()
{

    evf_init();

    evf_register_active_object(&test_active_object_a1.base);
    evf_register_active_object(&test_active_object_a2.base);
    evf_register_active_object(&test_active_object_b1.base);

    struct Event_adc_reading * p_event_1 = EVF_EVENT_ALLOC(struct Event_adc_reading);     
    evf_event_set_type(p_event_1, EVENT_TYPE_ADC_READING);
    p_event_1->sample = 512;

    struct Event_button_pressed * p_event_2 = EVF_EVENT_ALLOC(struct Event_button_pressed);     
    evf_event_set_type(p_event_2, EVENT_TYPE_BUTTON_PRESSED);
    p_event_2->button_id = 1;

    struct Event_msg_received * p_event_3 = EVF_EVENT_ALLOC(struct Event_msg_received);     
    evf_event_set_type(p_event_3, EVENT_TYPE_MSG_RECEIVED);
    sprintf(p_event_3->msg, "Hello World"); 

    for (uint32_t i = 0; i < 3; i++)
    {
        evf_task();
    }
    
    return 0;
}