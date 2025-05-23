#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "stddefines.h"

enum alloc_message_t {
    ALLOC_ALLOCATE,
    ALLOC_REALLOCATE,
    ALLOC_DEALLOCATE,
    ALLOC_DELETE,
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
#define mem_delete(alloc)             (alloc)->proc(0,    NULL, ALLOC_DELETE,     (alloc)->data)

allocator_t * preserve_allocator_from_stack(allocator_t allocator);

void alloc_init(void);

#endif // ALLOCATOR_H

