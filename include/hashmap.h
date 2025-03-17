#ifndef HASHMAP_H
#define HASHMAP_H

#include "stddefines.h"
#include "logger.h"

#include <memory.h> 

#ifndef CUSTIOM_MEM_CTRL

#define ALLOC(x)      calloc(1, x) 
#define FREE(x)       free(x) 

#endif

#define MAX_HASHMAP_LOAD 0.75

struct hash_entry_t {
    string_t name;

    b32 occupied;
    b32 deleted;
};

template<typename DataType>
struct hashmap_t {
    u64 load;
    u64 capacity;

    hash_entry_t *entries;
    DataType     *values;
};

// ----------- Initialization 

template<typename DataType>
b32 hashmap_create(hashmap_t<DataType> *container, u64 init_size);

template<typename DataType>
b32 hashmap_delete(hashmap_t<DataType> *map);

// ----------- Control 

template<typename DataType>
b32       hashmap_add(hashmap_t<DataType> *map, string_t key, DataType *value);

template<typename DataType>
DataType *hashmap_get(hashmap_t<DataType> *map, string_t key);

template<typename DataType>
b32 hashmap_remove(hashmap_t<DataType>   *map, string_t key);

template<typename DataType>
b32 hashmap_contains(hashmap_t<DataType> *map, string_t key);

// ----------- Helpers

void hashmap_tests(void);
u32 get_hash(u64 size, void *data);

u32 get_string_hash(string_t symbol);
b32 compare_strings(string_t a, string_t b);

template<typename DataType>
b32 rebuild_map(hashmap_t<DataType> *map);

// ----------- Implementation

template<typename DataType>
b32 hashmap_create(hashmap_t<DataType> *map, u64 init_size) {
    assert(init_size > 0);

    map->capacity = init_size;
    map->entries  = (hash_entry_t*)ALLOC(sizeof(hash_entry_t) * init_size);
    map->values   =     (DataType*)ALLOC(sizeof(DataType)     * init_size);

    if (map->entries == NULL) {
        log_warning(STR("Hashmap: Couldn't initialize map"), 0);
        if (map->values) FREE(map->values);
        return false;
    }

    if (map->values == NULL) {
        log_warning(STR("Hashmap: Couldn't initialize map"), 0);
        if (map->entries) FREE(map->entries);
        return false;
    }

    return true;
}

template<typename DataType>
b32 hashmap_delete(hashmap_t<DataType> *map) {
    FREE(map->values);
    FREE(map->entries);
    return true;
}

template<typename DataType>
b32 hashmap_contains(hashmap_t<DataType> *map, string_t key) {
    u32 hash = get_string_hash(key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return false;

        if (compare_strings(key, map->entries[lookup].name)) {
            return true;
        }
    }

    return false;
}

template<typename DataType>
DataType *hashmap_get(hashmap_t<DataType> *map, string_t key) {
    u32 hash = get_string_hash(key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return NULL;

        if (compare_strings(key, map->entries[lookup].name)) {
            return map->values + lookup;
        }
    }

    return NULL;
}

template<typename DataType>
b32 hashmap_add(hashmap_t<DataType> *map, string_t key, DataType *value) {
    if (map->load > (map->capacity * MAX_HASHMAP_LOAD)) {
        if (!rebuild_map(map)) {
            log_error(STR("Map is full, couldn't insert element, attempted to but we resize it and still failed."), 0);
        }
    }

    u32 hash  = get_string_hash(key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (!map->entries[lookup].occupied || map->entries[lookup].deleted) { 
            memcpy(map->values + lookup, value, sizeof(DataType));

            map->entries[lookup].name     = key;
            map->entries[lookup].deleted  = false;
            map->entries[lookup].occupied = true;
            map->load++;

            return true;
        } else if (compare_strings(key, map->entries[lookup].name)) {
            return false;
        }
    }

    return false;
}

template<typename DataType>
b32 hashmap_remove(hashmap_t<DataType> *map, string_t key) {
    u32 hash = get_string_hash(key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return false;

        if (compare_strings(key, map->entries[lookup].name)) {
            map->entries[lookup].deleted = true;
            map->load--;
            return true;
        }
    }

    return false;
}


template<typename DataType>
b32 rebuild_map(hashmap_t<DataType> *map) {
    hashmap_t<DataType> old_map = *map;

    if (!hashmap_create(map, map->capacity * 2)) {
        *map = old_map;
        return false;
    }

    for (u64 i = 0; i < old_map.capacity; i++) {
        if (!map->entries[i].occupied) continue;
        if (map->entries[i].deleted)   continue;

        hashmap_add(map, old_map.entries[i].name, old_map.values + i);
    }

    hashmap_delete(&old_map);
    return true;
}

#endif // HASHMAP_H
