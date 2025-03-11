#ifndef ARENA_H
#define ARENA_H


#include "stddefines.h"
#include "logger.h"

#ifndef CUSTIOM_MEM_CTRL

#include <memory.h> 

#define ALLOC(x)      calloc(1, x) 
#define FREE(x)       free(x) 

#endif

#ifdef DEBUG
#define ZERO_CHECK(ptr, size) \
        for (u64 i = 0; i < size; i++) \
            { if ((*(((u8*)ptr) + i) != 0)) { log_error(STR("area block is not zero"), 0); assert(false); }}
#else
#define ZERO_CHECK(ptr, size) 
#endif

struct arena_t {
    u64 size;
    u64 index;
    arena_t *next;
    void    *start;
};

arena_t * arena_create(u64 init_size);
void      arena_delete(arena_t *cont);

void * arena_allocate(arena_t *cont, u64 size);

void arena_tests(void);

#endif // ARENA_H
