#include "allocator.h"

#define ALLOCATOR_GLOBAL_BUFF_SIZE 128

struct {
    u64 current_index;
    allocator_t allocators[ALLOCATOR_GLOBAL_BUFF_SIZE];
} __global_allocators_buffer;


allocator_t *preserve_allocator_from_stack(allocator_t allocator) {
    if (__global_allocators_buffer.current_index >= ALLOCATOR_GLOBAL_BUFF_SIZE) {
        log_error(STR("Allcator stack is full"), 0);
        return NULL;
    }

    __global_allocators_buffer.allocators[__global_allocators_buffer.current_index++] = allocator;

    return __global_allocators_buffer.allocators + __global_allocators_buffer.current_index - 1;
}
