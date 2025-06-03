#ifndef HASHMAP_H
#define HASHMAP_H

#include <typeinfo>
#include "stddefines.h"
#include "logger.h"
#include "memctl.h"

#ifndef CUSTOM_MEM_CTRL
#define ALLOC(x)      calloc(1, x) 
#define FREE(x)       free(x) 
#endif

#define STANDART_MAP_SIZE 16
#define MAX_HASHMAP_LOAD 0.75

#define COMPUTE_HASH(name) b32 name(u64 size, void * key)
typedef COMPUTE_HASH(hash_func_t);

#define COMPARE_KEYS(name) b32 name(u64 size, void * a, void * b)
typedef COMPARE_KEYS(key_compare_func_t);

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
    key_compare_func_t *compare_func;
    kv_pair_t<KeyType, DataType> *entries;

    DataType * operator[](KeyType &key) {
        return hashmap_get(this, key);
    }
};

// ----------- Initialization 

template<typename KeyType, typename DataType>
b32 hashmap_create(hashmap_t<KeyType, DataType> *map, u64 init_size, hash_func_t *hash_func, key_compare_func_t *compare_func);

template<typename KeyType, typename DataType>
hashmap_t<KeyType, DataType> hashmap_clone(hashmap_t<KeyType, DataType> *map);

template<typename KeyType, typename DataType>
b32 hashmap_delete(hashmap_t<KeyType, DataType> *map);

template<typename KeyType, typename DataType>
void hashmap_clear(hashmap_t<KeyType, DataType> *map);

// ----------- Control 

template<typename KeyType, typename DataType>
b32 hashmap_add(hashmap_t<KeyType, DataType> *map, KeyType key, DataType *value);

template<typename KeyType, typename DataType>
DataType *hashmap_get(hashmap_t<KeyType, DataType> *map, KeyType key);

template<typename KeyType, typename DataType>
b32 hashmap_remove(hashmap_t<KeyType, DataType>   *map, KeyType key);

template<typename KeyType, typename DataType>
b32 hashmap_contains(hashmap_t<KeyType, DataType> *map, KeyType key) {
    return hashmap_get(map, key) != NULL ? true : false;
}


// ----------- Helpers

u32  get_hash(u64 size, void *data);
void hashmap_tests(void);

template<typename KeyType, typename DataType>
void create_map_if_needed(hashmap_t<KeyType, DataType> *map);

template<typename KeyType, typename DataType>
b32 rebuild_map(hashmap_t<KeyType, DataType> *map);

// ----------- Implementation

template<typename KeyType, typename DataType>
b32 hashmap_create(hashmap_t<KeyType, DataType> *map, u64 init_size, hash_func_t *hash_func, key_compare_func_t *compare_func) {
    *map = {};
    assert(init_size > 0);

    if (hash_func) {
        map->hash_func = hash_func;
    } else if (typeid(KeyType) == typeid(string_t)) {
        map->hash_func = get_string_hash;
    } else {
        map->hash_func = get_hash_std;
    }

    if (compare_func) {
        map->compare_func = compare_func;
    } else if (typeid(KeyType) == typeid(string_t)) {
        map->compare_func = compare_string_keys;
    } else {
        map->compare_func = compare_keys_std;
    }

    map->capacity  = init_size;
    map->entries   = (kv_pair_t<KeyType, DataType>*)ALLOC(sizeof(kv_pair_t<KeyType, DataType>) * init_size);

    if (map->entries == NULL) {
        log_warning(STRING("Hashmap: Couldn't initialize map"));
        return false;
    }

    return true;
}

template<typename KeyType, typename DataType>
hashmap_t<KeyType, DataType> hashmap_clone(hashmap_t<KeyType, DataType> *map) {
    if (map->capacity == 0) {
        map->hash_func    = map->hash_func;
        map->compare_func = map->compare_func;
        return {};
    }

    hashmap_t<KeyType, DataType> clone = {};
    if (!hashmap_create(&clone, map->capacity, map->hash_func, map->compare_func)) {
        return {};
    }

    check_value(clone.entries != NULL);

    clone.load = map->load;
    mem_copy((u8*)clone.entries, (u8*)map->entries, clone.capacity * sizeof(kv_pair_t<KeyType, DataType>));
    return clone;
}

template<typename KeyType, typename DataType>
b32 hashmap_delete(hashmap_t<KeyType, DataType> *map) {
    FREE(map->entries);
    *map = {};
    return true;
}

template<typename KeyType, typename DataType>
void hashmap_clear(hashmap_t<KeyType, DataType> *map) {
    if (map->entries == 0) return;

    map->load = 0;
    mem_set((u8*)map->entries, 0, map->capacity * sizeof(kv_pair_t<KeyType, DataType>));
}


template<typename KeyType, typename DataType>
DataType *hashmap_get(hashmap_t<KeyType, DataType> *map, KeyType key) {
    create_map_if_needed(map);

    u32 hash = map->hash_func(sizeof(KeyType), (void*)&key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return NULL;

        if (map->compare_func(sizeof(KeyType), (void*)&key, (void*)&map->entries[lookup].key)) {
            return &(map->entries + lookup)->value;
        }
    }

    return NULL;
}
// note about implication
template<typename KeyType, typename DataType>
b32 hashmap_add(hashmap_t<KeyType, DataType> *map, KeyType key, DataType *value) {
    create_map_if_needed(map);

    if (map->load > (map->capacity * MAX_HASHMAP_LOAD)) {
        if (!rebuild_map(map)) {
            log_error(STRING("Map is full't insert element to but we resize it and still failed."));
        }
    }

    u32 hash  = map->hash_func(sizeof(KeyType), (void*)&key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (!map->entries[lookup].occupied || map->entries[lookup].deleted) { 
            mem_copy((u8*)&map->entries[lookup].value, (u8*)value, sizeof(DataType));

            map->entries[lookup].key      = key;
            map->entries[lookup].deleted  = false;
            map->entries[lookup].occupied = true;
            map->load++;

            return true;
        } else if (map->compare_func(sizeof(KeyType), (void*)&key, (void*)&map->entries[lookup].key)) {
            return false;
        }
    }

    return false;
}

template<typename KeyType, typename DataType>
b32 hashmap_remove(hashmap_t<KeyType, DataType> *map, KeyType key) {
    create_map_if_needed(map);

    u32 hash = map->hash_func(sizeof(KeyType), (void*)&key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return false;

        if (map->compare_func(sizeof(KeyType), (void*)&key, (void*)&map->entries[lookup].key)) {
            map->entries[lookup].deleted = true;
            map->load--;
            return true;
        }
    }

    return false;
}

template<typename KeyType, typename DataType>
void create_map_if_needed(hashmap_t<KeyType, DataType> *map) {
    if (map->entries == NULL && !hashmap_create(map, STANDART_MAP_SIZE, map->hash_func, map->compare_func)) {
        log_error(STRING("tried to create hashmap and failed."));
    }
}

template<typename KeyType, typename DataType>
b32 rebuild_map(hashmap_t<KeyType, DataType> *map) {
    hashmap_t<KeyType, DataType> old_map = *map;

    if (!hashmap_create(map, old_map.capacity * 2, old_map.hash_func, old_map.compare_func)) {
        *map = old_map;
        return false;
    }

    for (u64 i = 0; i < old_map.capacity; i++) {
        if (!old_map.entries[i].occupied) continue;
        if (old_map.entries[i].deleted)   continue;

        hashmap_add(map, old_map.entries[i].key, &old_map.entries[i].value);
    }

    hashmap_delete(&old_map);
    return true;
}

#endif // HASHMAP_H
