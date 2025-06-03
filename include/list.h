#ifndef LIST_H
#define LIST_H

#include "stddefines.h"
#include "logger.h"
#include "memctl.h"
#include "allocator.h"

#define STANDARD_LIST_SIZE 10

template<typename DataType>
struct list_t {
    allocator_t alloc;
    u64 count;
    DataType *data;

    u64 current_size;
    u64 grow_size;

    DataType& operator[](u64 index) {
        assert(index < count);

        return data[index];
    }

    void operator+=(DataType &type) {
        list_add(this, &type);
    }
};

// ----------- Initialization 

template<typename DataType>
b32 list_create(list_t<DataType> *list, u64 init_size, allocator_t alloc);
template<typename DataType>
list_t<DataType> list_clone(list_t<DataType> *list);
template<typename DataType>
b32 list_delete(list_t<DataType> *list);

// --------- Control

// reserve some amount of memory in list
// return index of a first element
template<typename DataType>
void list_allocate(list_t<DataType> *list, u64 elements_amount, u64 *start_index);

template<typename DataType> 
void list_fill(list_t<DataType> *list, DataType *data, u64 elements_amount, u64 start_index);

// add element to an list, and advance
template<typename DataType>
void list_add(list_t<DataType> *list, DataType *data);

// get element by index
template<typename DataType>
DataType *list_get(list_t<DataType> *list, u64 index);


// ----------- Helpers

void list_tests(void);

template<typename DataType>
b32 list_grow(list_t<DataType> *list);

template<typename DataType>
b32 list_grow_fit(list_t<DataType> *list);

template<typename DataType>
void list_create_if_needed(list_t<DataType> *list);


// ----------- Implementation

template<typename DataType>
b32 list_create(list_t<DataType> *list, u64 init_size, allocator_t alloc) {
    list->count        = 0;
    list->current_size = init_size;

    list->alloc     = alloc;
    list->grow_size = init_size * 2;
    list->data      = (DataType*)mem_alloc(&alloc, init_size * sizeof(DataType));

    if (list->data == NULL) {
        log_error(STRING("List: Couldn't create list."));
        return false; 
    }

    return true;
}

template<typename DataType>
list_t<DataType> list_clone(list_t<DataType> *list) {
    list_t<DataType> clone = {};

    if (list->current_size == 0)
        return {};

    if (!list_create(&clone, list->current_size, list->alloc))
        return {};

    mem_copy((u8*)clone.data, (u8*)list->data, sizeof(DataType) * list->count);
    clone.count = list->count;

    return clone;
}

template<typename DataType>
b32 list_delete(list_t<DataType> *list) {
    if (list == NULL) {
        log_error(STRING("List: Reference to list wasn't valid."));
        return false;
    }

    if (list->data == NULL) {
        log_error(STRING("List: List was already deleted."));
        return false;
    }

    mem_free(&list->alloc, list->data); 
    list->data = NULL;
    list->count = 0;
    list->current_size = 0;
    list->grow_size = 0;

    return true;
}

// reserve some amount of memory in list
// return index of a first element
template<typename DataType>
void list_allocate(list_t<DataType> *list, u64 elements_amount, u64 *start_index) {
    list_create_if_needed(list);
    assert(elements_amount > 0);

    if (!list_grow_fit(list, elements_amount)) {
        assert(false);
    }

    u64 new_count = list->count + elements_amount;
    
    *start_index = list->count;
    list->count = new_count;
}

template<typename DataType> 
void list_fill(list_t<DataType> *list, DataType *data, u64 elements_amount, u64 start_index) {
    list_create_if_needed(list);

    DataType *start_address = list_get(list, start_index);
    
    if (start_address == NULL) {
        assert(false);
    }

    mem_copy((u8*)start_address, (u8*)data, elements_amount * sizeof(DataType));
}

// add element to an list, and advance
template<typename DataType>
void list_add(list_t<DataType> *list, DataType *data) {
    list_create_if_needed(list);
    assert(data != 0);

    u64 index = 0;
    list_allocate(list, 1, &index);

    mem_copy((u8*)(list->data + index), (u8*)data, sizeof(DataType));
}

// get element by index
template<typename DataType>
DataType *list_get(list_t<DataType> *list, u64 index) {
    list_create_if_needed(list);

    if (index >= list->count) {
        return NULL;
    }

    return list->data + index; 
}

// -------------- local functions

template<typename DataType>
void list_create_if_needed(list_t<DataType> *list) {
    assert(default_allocator);

    allocator_t alloc = list->alloc.proc ? list->alloc : *default_allocator;
    if (list->data == NULL && !list_create(list, STANDARD_LIST_SIZE, alloc)) {
        log_error(STRING("tried to create list but failed."));
    }
}

template<typename DataType>
b32 list_grow(list_t<DataType> *list) {
    assert(list != 0);
    DataType *data = (DataType*)mem_alloc(&list->alloc, list->grow_size * sizeof(DataType));

    if (data == NULL) {
        log_error(STRING("List: Couldn't grow list."));
        return false;
    }

    (void)mem_copy((u8*)data, (u8*)list->data, list->current_size * sizeof(DataType));

    mem_free(&list->alloc, list->data); 
    list->data = data;
    list->current_size  = list->grow_size;
    list->grow_size = list->current_size * 2;

    return true;
}

template<typename DataType>
b32 list_grow_fit(list_t<DataType> *list, u64 fit_elements) {
    assert(fit_elements > 0);

    if ((list->count + fit_elements - 1) < list->current_size) {
        return true;
    } else {
        while ((list->count + fit_elements) >= list->current_size) {
            if (!list_grow(list)) return false;
        }
    }

    return true;
}

#endif // LIST_H
