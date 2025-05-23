#include "allocator.h"
#include "logger.h"

#define ALLOCATOR_GLOBAL_BUFF_SIZE 128

struct {
    u64 current_index;
    allocator_t allocators[ALLOCATOR_GLOBAL_BUFF_SIZE];
} __global_allocators_buffer;

allocator_t *preserve_allocator_from_stack(allocator_t allocator) {
    if (__global_allocators_buffer.current_index >= ALLOCATOR_GLOBAL_BUFF_SIZE) {
        log_error(STRING("Allcator stack is full"));
        return NULL;
    }

    __global_allocators_buffer.allocators[__global_allocators_buffer.current_index++] = allocator;

    return __global_allocators_buffer.allocators + __global_allocators_buffer.current_index - 1;
}


ALLOCATOR_PROC(std_alloc_proc) {
    UNUSED(data);

    switch (message) {
        case ALLOC_ALLOCATE: {
            void *out = calloc(1, size);
#ifdef VERBOSE
            log_update_color();
            fprintf(stderr, "ALLOC--: %llx SIZE: %llu\n", (uintptr_t)out, size);
#endif

            return out; 
        }

        case ALLOC_REALLOCATE: {
            void *out = realloc(p, size);
#ifdef VERBOSE
            log_update_color();
            fprintf(stderr, "REALLOC: %llx -> %llx SIZE: %llu\n", (uintptr_t)p, (uintptr_t)out, size);
#endif
            return out;
        }

        case ALLOC_DEALLOCATE:
            free(p);
#ifdef VERBOSE
            log_update_color();
            fprintf(stderr, "FREE---: %llx\n", (uintptr_t)p);
#endif
            break;

        default:
            log_error(STRING("unexpected allocator message."));
            assert(false);
            break;
    }

    return NULL;
}

void alloc_init(void) {
    allocator_t alloc;
    alloc.proc = std_alloc_proc;
    default_allocator = preserve_allocator_from_stack(alloc);
}
