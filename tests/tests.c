

#include "../evf.h"
#include <assert.h>
#include <stdio.h>

enum Test_event_types
{
    TEST_EVENT_TYPE_1 = EVF_USER_EVENT_TYPES_START,
    TEST_EVENT_TYPE_2,
    TEST_EVENT_TYPE_3,
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
            TEST_EVENT_TYPE_1,
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
            TEST_EVENT_TYPE_2,
            TEST_EVENT_TYPE_1,
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
            TEST_EVENT_TYPE_1,
            TEST_EVENT_TYPE_3,
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

    
    evf_post()

    while (true)
    {
        evf_task();
    }

    return 0;
}