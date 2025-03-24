#ifndef HASHMAP_H
#define HASHMAP_H

#include "stddefines.h"
#include "logger.h"

#include <memory.h> 

#ifndef CUSTOM_MEM_CTRL
#define ALLOC(x)      calloc(1, x) 
#define FREE(x)       free(x) 
#endif

#define MAX_HASHMAP_LOAD 0.75

#define COMPUTE_HASH(name) b32 name(u64 size, void * key)
typedef COMPUTE_HASH(hash_func_t);

#define COMPARE_KEYS(name) b32 name(u64 a_size, void * a, u64 b_size, void * b)
typedef COMPARE_KEYS(compare_func_t);

COMPUTE_HASH(get_string_hash);
COMPARE_KEYS(compare_string_keys);

COMPUTE_HASH(get_hash_std);
COMPARE_KEYS(compare_keys_std);

template<typename KeyType, typename DataType>
struct kv_pair_t {
    b32 occupied;
    b32 deleted;

    KeyType  key; 
    DataType value;
};

template<typename KeyType, typename DataType>
struct hashmap_t {
    u64 load;
    u64 capacity;
    hash_func_t *hash_func;
    compare_func_t *compare_func;
    kv_pair_t<KeyType, DataType> *entries;
};

// ----------- Initialization 

template<typename KeyType, typename DataType>
b32 hashmap_create(hashmap_t<KeyType, DataType> *map, u64 init_size, hash_func_t *hash_func, compare_func_t *compare_func);

template<typename KeyType, typename DataType>
b32 hashmap_delete(hashmap_t<KeyType, DataType> *map);

// ----------- Control 

template<typename KeyType, typename DataType>
b32 hashmap_add(hashmap_t<KeyType, DataType> *map, KeyType key, DataType *value);

template<typename KeyType, typename DataType>
DataType *hashmap_get(hashmap_t<KeyType, DataType> *map, KeyType key);

template<typename KeyType, typename DataType>
b32 hashmap_remove(hashmap_t<KeyType, DataType>   *map, KeyType key);

template<typename KeyType, typename DataType>
b32 hashmap_contains(hashmap_t<KeyType, DataType> *map, KeyType key);

// ----------- Helpers

u32  get_hash(u64 size, void *data);
void hashmap_tests(void);

template<typename KeyType, typename DataType>
b32 rebuild_map(hashmap_t<KeyType, DataType> *map);

// ----------- Implementation

template<typename KeyType, typename DataType>
b32 hashmap_create(hashmap_t<KeyType, DataType> *map, u64 init_size, hash_func_t *hash_func, compare_func_t *compare_func) {
    assert(init_size > 0);

    if (hash_func)
        map->hash_func = hash_func;
    else
        map->hash_func = get_hash_std;

    if (compare_func)
        map->compare_func = compare_func;
    else
        map->compare_func = compare_keys_std;

    map->capacity  = init_size;
    map->entries   = (kv_pair_t<KeyType, DataType>*)ALLOC(sizeof(kv_pair_t<KeyType, DataType>) * init_size);

    if (map->entries == NULL) {
        log_warning(STR("Hashmap: Couldn't initialize map"), 0);
        return false;
    }

    return true;
}

template<typename KeyType, typename DataType>
b32 hashmap_delete(hashmap_t<KeyType, DataType> *map) {
    FREE(map->entries);
    return true;
}

template<typename KeyType, typename DataType>
b32 hashmap_contains(hashmap_t<KeyType, DataType> *map, KeyType key) {
    u32 hash = map->hash_func(sizeof(KeyType), (void*)&key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return false;

        if (map->compare_func(sizeof(KeyType), (void*)&key, sizeof(KeyType), (void*)&map->entries[lookup].key)) {
            return true;
        }
    }

    return false;
}

template<typename KeyType, typename DataType>
DataType *hashmap_get(hashmap_t<KeyType, DataType> *map, KeyType key) {
    u32 hash = map->hash_func(sizeof(KeyType), (void*)&key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return NULL;

        if (map->compare_func(sizeof(KeyType), (void*)&key, sizeof(KeyType), (void*)&map->entries[lookup].key)) {
            return &(map->entries + lookup)->value;
        }
    }

    return NULL;
}
// note about implication
template<typename KeyType, typename DataType>
b32 hashmap_add(hashmap_t<KeyType, DataType> *map, KeyType key, DataType *value) {
    if (map->load > (map->capacity * MAX_HASHMAP_LOAD)) {
        if (!rebuild_map(map)) {
            log_error(STR("Map is full, couldn't insert element, attempted to but we resize it and still failed."), 0);
        }
    }

    u32 hash  = map->hash_func(sizeof(KeyType), (void*)&key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (!map->entries[lookup].occupied || map->entries[lookup].deleted) { 
            memcpy(&map->entries[lookup].value, value, sizeof(DataType));

            map->entries[lookup].key      = key;
            map->entries[lookup].deleted  = false;
            map->entries[lookup].occupied = true;
            map->load++;

            return true;
        } else if (map->compare_func(sizeof(KeyType), (void*)&key, sizeof(KeyType), (void*)&map->entries[lookup].key)) {
            return false;
        }
    }

    return false;
}

template<typename KeyType, typename DataType>
b32 hashmap_remove(hashmap_t<KeyType, DataType> *map, KeyType key) {
    u32 hash = map->hash_func(sizeof(KeyType), (void*)&key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return false;

        if (map->compare_func(sizeof(KeyType), (void*)&key, sizeof(KeyType), (void*)&map->entries[lookup].key)) {
            map->entries[lookup].deleted = true;
            map->load--;
            return true;
        }
    }

    return false;
}


template<typename KeyType, typename DataType>
b32 rebuild_map(hashmap_t<KeyType, DataType> *map) {
    hashmap_t<KeyType, DataType> old_map = *map;

    if (!hashmap_create(map, map->capacity * 2, old_map.hash_func, old_map.compare_func)) {
        *map = old_map;
        return false;
    }

    for (u64 i = 0; i < old_map.capacity; i++) {
        if (!map->entries[i].occupied) continue;
        if (map->entries[i].deleted)   continue;

        hashmap_add(map, old_map.entries[i].key, &old_map.entries[i].value);
    }

    hashmap_delete(&old_map);
    return true;
}

#endif // HASHMAP_H
