
#include "evf_port.h"
#include <assert.h>
#include <malloc.h>

void * evf_malloc(size_t num_bytes)
{
    return malloc(num_bytes);
}

void evf_free(void * p_memory)
{
    free(p_memory);
}

static void evf_assert(bool condition)
{
    assert(condition); 
}