#ifndef ARRAY_H
#define ARRAY_H

#include "stddefines.h"
#include "logger.h"
#include "memctl.h"
#include "allocator.h"
#include "list.h"

#define STD_ARRAY_SIZE 128

template<typename DataType>
struct array_entry_t {
    u64       size;
    DataType *data;
};

template<typename DataType>
struct array_t {
    u64         count;
    u64         grow_size;
    allocator_t alloc;

    list_t<array_entry_t<DataType>> entries;

    DataType operator[](u64 index) {
        if (index >= count) {
            return {};
        }

        u64 index_offset = 0;
        u64 ei = 0;

        while (ei < entries.count) {
            if (entries[ei].size <= (index - index_offset)) {
                index_offset += entries[ei].size;
                ei++;
                continue;
            }

            break;
        }

        return entries[ei].data[index - index_offset];
    }
};

template<typename DataType>
void array_create(array_t<DataType> *array, u64 size, allocator_t alloc);

template<typename DataType>
void array_delete(array_t<DataType> *array);

template<typename DataType>
void array_add(array_t<DataType> *array, DataType data);

template<typename DataType>
DataType *array_get(array_t<DataType> *array, u64 index);

void array_tests(void);

// ---- Impl
template<typename DataType>
void array_create(array_t<DataType> *array, u64 size, allocator_t alloc) {
    if (alloc.proc == NULL) alloc = *default_allocator;
    array->count     = 0;
    array->alloc     = alloc;
    array->grow_size = size;

    array_entry_t<DataType> entry = {};
    entry.data = (DataType*)mem_alloc(&array->alloc, array->grow_size * sizeof(DataType));
    entry.size = array->grow_size; 
    array->grow_size *= 2;
    assert(entry.data != NULL);

    list_create(&array->entries, 16, &alloc);
    list_add(&array->entries, &entry);
}

template<typename DataType>
void array_delete(array_t<DataType> *array) {
    if (array->alloc.proc == NULL) return;

    list_delete(&array->entries);
}

template<typename DataType>
void array_add(array_t<DataType> *array, DataType data) {
    if (array->entries.data == NULL) array_create(array, STD_ARRAY_SIZE, *default_allocator);
    u64 index_offset = 0;
    u64 ei = 0;

    while (ei < array->entries.count) {
        if (array->entries[ei].size <= (array->count - index_offset)) {
            index_offset += array->entries[ei].size;
            ei++;
            continue;
        }
        break;
    }

    array_entry_t<DataType> *e = list_get(&array->entries, ei);

    if (e == NULL) {
        array_entry_t<DataType> entry = {};

        entry.data = (DataType*)mem_alloc(&array->alloc, array->grow_size * sizeof(DataType));
        entry.size = array->grow_size; 
        array->grow_size *= 2;

        assert(entry.data != NULL);

        list_add(&array->entries, &entry);

        e = list_get(&array->entries, ei);
        assert(e);
    }

    mem_copy((u8*)(e->data + (array->count - index_offset)), (u8*)&data, sizeof(DataType));
    array->count++;
}


template<typename DataType>
DataType *array_get(array_t<DataType> *array, u64 index) {
    if (array->entries.data == NULL) array_create(array, STD_ARRAY_SIZE, *default_allocator);
    
    if (index >= array->count) {
        return {};
    }

    u64 index_offset = 0;
    u64 ei = 0;

    while (ei < array->entries.count) {
        if (array->entries[ei].size <= (index - index_offset)) {
            index_offset += array->entries[ei].size;
            ei++;
            continue;
        }
        break;
    }

    array_entry_t<DataType> *e = list_get(&array->entries, ei);
    assert(e);

    return e->data + (index - index_offset);
}

#endif// ARRAY_H
