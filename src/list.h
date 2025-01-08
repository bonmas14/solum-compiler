#ifndef LIST_H
#define LIST_H

#include "stddefines.h"
#include "logger.h"
#include <memory.h> 

#ifndef CUSTIOM_MEM_CTRL

#define ALLOC(x)      calloc(1, x) 
#define REALLOC(x, y) realloc(x, y)
#define FREE(x)       free(x) 

#endif

struct list_t {
    u64 count;
    void *data;

    u64 element_size;
    u64 raw_size;
    u64 grow_size;
};


b32 list_create(list_t *container, u64 init_size, u64 element_size);
b32 list_delete(list_t *arr);

b32  list_add(list_t *arr, void *data);
void* list_get(list_t *arr, u64 index);

/*
void  list_insert_at(list_t *arr, u64 index, void *data);
void  list_remove_at(list_t *arr, u64 index);
*/

#endif // LIST_H
