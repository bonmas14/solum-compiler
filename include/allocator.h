#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "stddefines.h"

enum alloc_message_t {
    ALLOC_ALLOCATE,
    ALLOC_REALLOCATE,
    ALLOC_DEALLOCATE,
};

#define ALLOCATOR_PROC(name) void * name(u64 size, void *p, alloc_message_t message, void *data)

typedef ALLOCATOR_PROC(allocator_proc);

struct allocator_t {
    allocator_proc *proc;
    void *data;
};

#define mem_alloc(alloc, size)        (alloc)->proc(size, NULL, ALLOC_ALLOCATE,   (alloc)->data)
#define mem_realloc(alloc, ptr, size) (alloc)->proc(size, ptr,  ALLOC_REALLOCATE, (alloc)->data)
#define mem_free(alloc, ptr)          (alloc)->proc(0,    ptr,  ALLOC_DEALLOCATE, (alloc)->data)

allocator_t * preserve_allocator_from_stack(allocator_t allocator);

#endif // ALLOCATOR_H

