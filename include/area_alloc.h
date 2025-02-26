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

// -------------------- 
//
//    So this is fancy version of area allocator
//  the fancy part is fact that it was made as generics
//  and doesnt use direct pointers.
// 
//  It was started as an list of sequential data, so this
//  is why it has 'area_get' and 'area_add' functions. 
//
//  It is specific to an compiler and works in a way as a list.
//  but can be used as non-direct an area allocator.
//
//                                   - bonmas14 (25.01.2025)
//
//  Full interface:
//
//   b32 area_create(area_t *container, u64 size)
//   b32 area_delete(area_t *container, u64 size)
//
//   void area_add(area_t *container, T element)
//   T * area_get(area_t *container, u64 index)
//
//   void area_allocate(area_t *container, u64 elements, u64 *start_index)
//   void area_fill(area_t *container, u64 elements_amount, u64 start_index)
//
// -------------------- 

template<typename DataType>
struct area_t {
    u64 count;
    DataType *data;

    u64 raw_size;
    u64 grow_size;
};

// ----------- Initialization 

template<typename DataType>
b32 area_create(area_t<DataType> *container, u64 init_size);
template<typename DataType>
b32 area_delete(area_t<DataType> *area);

// --------- Control

// reserve some amount of memory in area
// return index of a first element
template<typename DataType>
void area_allocate(area_t<DataType> *area, u64 elements_amount, u64 *start_index);

template<typename DataType> 
void area_fill(area_t<DataType> *area, DataType *data, u64 elements_amount, u64 start_index);

// add element to an area, and advance
template<typename DataType>
void area_add(area_t<DataType> *area, DataType *data);

// get element by index
template<typename DataType>
DataType *area_get(area_t<DataType> *area, u64 index);

// ----------- Helpers

void area_tests(void);

template<typename DataType>
b32 area_grow(area_t<DataType> *area);

template<typename DataType>
b32 area_grow_fit(area_t<DataType> *area);

// ----------- Implementation

template<typename DataType>
b32 area_create(area_t<DataType> *container, u64 init_size) {

    container->count     = 0;
    container->raw_size  = init_size;
    container->grow_size = init_size * 2;
    container->data      = (DataType*)ALLOC(init_size * sizeof(DataType));

    if (container->data == NULL) {
        log_error(STR("Area: Couldn't create area."), 0);
        return false; 
    }

    return true;
}

template<typename DataType>
b32 area_delete(area_t<DataType> *area) {
    if (area == NULL) {
        log_error(STR("Area: Reference to area wasn't valid."), 0);
        return false;
    }

    if (area->data == NULL) {
        log_error(STR("Area: Area was already deleted."), 0);
        return false;
    }

    FREE(area->data); 
    area->data = NULL;

    return true;
}


// reserve some amount of memory in area
// return index of a first element
template<typename DataType>
void area_allocate(area_t<DataType> *area, u64 elements_amount, u64 *start_index) {
    assert(area != 0);
    assert(elements_amount > 0);

    if (!area_grow_fit(area, elements_amount)) {
        assert(false);
    }

    u64 new_count = area->count + elements_amount;
    
    *start_index = area->count;
    area->count = new_count;
}

template<typename DataType> 
void area_fill(area_t<DataType> *area, DataType *data, u64 elements_amount, u64 start_index) {

    DataType *start_address = area_get(area, start_index);
    
    if (start_address == NULL) {
        assert(false);
    }

    memcpy(start_address, data, elements_amount * sizeof(DataType));
}

// add element to an area, and advance
template<typename DataType>
void area_add(area_t<DataType> *area, DataType *data) {
    assert(area != 0);
    assert(data != 0);

    u64 index = 0;
    area_allocate(area, 1, &index);

    memcpy(area->data + index, data, sizeof(DataType));
}

// get element by index
template<typename DataType>
DataType *area_get(area_t<DataType> *area, u64 index) {
    assert(area != 0);

    assert(index < area->count);

    if (index >= area->count) {
        log_error(STR("Area: Index is greater than count of elements."), 0);
        return NULL;
    }

    return area->data + index; 
}

// -------------- local functions to a area_alloc

template<typename DataType>
b32 area_grow(area_t<DataType> *area) {
    assert(area != 0);
    DataType *data = (DataType*)ALLOC(area->grow_size * sizeof(DataType));

    if (data == NULL) {
        log_error(STR("Area: Couldn't grow area."), 0);
        return false;
    }

    (void)memset(data, 0, area->grow_size * sizeof(DataType));

    (void)memcpy(data, area->data, area->raw_size);

    FREE(area->data);
    area->data = data;
    area->raw_size  = area->grow_size;
    area->grow_size = area->raw_size * 2;

    return true;
}

template<typename DataType>
b32 area_grow_fit(area_t<DataType> *area, u64 fit_elements) {
    assert(fit_elements > 0);

    if ((area->count + fit_elements - 1) < area->raw_size) {
        return true;
    } else {
        while ((area->count + fit_elements) >= area->raw_size) {
            if (!area_grow(area)) {
                return false;
            }
        }
    }

    return true;
}

#endif // AREA_ALLOC_H
