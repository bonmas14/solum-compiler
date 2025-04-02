#ifndef LIST_H
#define LIST_H

#include "stddefines.h"
#include "logger.h"
#include "memctl.h"

#ifndef CUSTIOM_MEM_CTRL
#define ALLOC(x)      calloc(1, x) 
#define FREE(x)       free(x) 
#endif

#define STANDART_LIST_SIZE 64

template<typename DataType>
struct list_t {
    u64 count;
    DataType *data;

    u64 current_size;
    u64 grow_size;
};

// ----------- Initialization 

template<typename DataType>
b32 list_create(list_t<DataType> *list, u64 init_size);
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
void create_if_needed(list_t<DataType> *list);


// ----------- Implementation

template<typename DataType>
b32 list_create(list_t<DataType> *list, u64 init_size) {
    list->count        = 0;
    list->current_size = init_size;

    list->grow_size = init_size * 2;
    list->data      = (DataType*)ALLOC(init_size * sizeof(DataType));

    if (list->data == NULL) {
        log_error(STR("Area: Couldn't create list."));
        return false; 
    }

    return true;
}

template<typename DataType>
b32 list_delete(list_t<DataType> *list) {
    if (list == NULL) {
        log_error(STR("Area: Reference to list wasn't valid."));
        return false;
    }

    if (list->data == NULL) {
        log_error(STR("Area: Area was already deleted."));
        return false;
    }

    FREE(list->data); 
    list->data = NULL;

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
        log_error(STR("Area: Index is greater than count of elements."));
        return NULL;
    }

    return list->data + index; 
}

// -------------- local functions

template<typename DataType>
void list_create_if_needed(list_t<DataType> *list) {
    if (list->data == NULL && !list_create(list, STANDART_LIST_SIZE)) {
        log_error(STR("tried to create list but failed."));
    }
}

template<typename DataType>
b32 list_grow(list_t<DataType> *list) {
    assert(list != 0);
    DataType *data = (DataType*)ALLOC(list->grow_size * sizeof(DataType));

    if (data == NULL) {
        log_error(STR("Area: Couldn't grow list."));
        return false;
    }

    (void)mem_set((u8*)data, 0, list->grow_size * sizeof(DataType));

    (void)mem_copy((u8*)data, (u8*)list->data, list->current_size * sizeof(DataType));

    FREE(list->data);
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
            if (!list_grow(list)) {
                return false;
            }
        }
    }

    return true;
}

#endif // LIST_H
