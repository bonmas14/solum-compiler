#pragma once

#include "stddefines.h"
#include "logger.h"
#include <memory.h> 

#ifndef CUSTIOM_MEM_CTRL

#define ALLOC(x)      calloc(1, x) 
#define REALLOC(x, y) realloc(x, y)
#define FREE(x)       free(x) 

#endif

typedef struct list_t {
    size_t count;
    void *data;

    size_t element_size;
    size_t raw_size;
    size_t grow_size;
} list_t;


bool list_create(list_t *container, size_t init_size, size_t element_size);
bool list_delete(list_t *arr);

bool  list_add(list_t *arr, void *data);
void* list_get(list_t *arr, size_t index);
/*
void  list_insert_at(list_t *arr, size_t index, void *data);
void  list_remove_at(list_t *arr, size_t index);
*/
