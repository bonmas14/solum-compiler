#ifndef AREA_ALLOC_H
#define AREA_ALLOC_H

#include "stddefines.h"
#include "logger.h"
#include <memory.h> 

#ifndef CUSTIOM_MEM_CTRL

#define ALLOC(x)      calloc(1, x) 
#define REALLOC(x, y) realloc(x, y)
#define FREE(x)       free(x) 

#endif

struct area_t {
    u64 count;
    void *data;

    u64 element_size;
    u64 raw_size;    // In elements
    u64 grow_size;   // In elements
};


b32 area_create(area_t *container, u64 init_size, u64 element_size);
b32 area_delete(area_t *area);

b32   area_add(area_t *area, void *data);
void* area_get(area_t *area, u64 index);

b32 area_allocate(area_t *area, u64 elements_amount, u64 *start_index);

void area_tests(void);

#endif // AREA_ALLOC_H
