#include "hashmap.h"

// @todo if you need make hashmap not depend on string_t
u32 get_string_hash(string_t string) {
    u32 hash = 216613261u;

    for (u64 i = 0; i < string.size; i++) {
        hash ^= (u8)string.data[i];
        hash *= 0x1000193;
    }

    return hash;
}

b32 compare_strings(string_t a, string_t b) {
    if (a.size != b.size) return false;

    b32 hash_comp = get_string_hash(a) == get_string_hash(b);

    if (!hash_comp) return hash_comp;

    for (u64 i = 0; i < a.size; i++) {
        if (a.data[i] != b.data[i])
            return false;
    }

    return true;
}

b32 hashmap_create(hashmap_t *map, u64 size, u64 element_size) {
    if (size < 256) {
        log_warning(STR("Hashmap: init size was less than 256. defaulting to 256."), 0);
        size = 256;
    }

    // @todo asserts for element size!!
    //

    if (element_size == 0) {
        log_warning(STR("Hashmap: element size was zero."), 0);
        return false;
    }


    map->capacity     = size;
    map->element_size = element_size;

    map->entries = (hash_entry_t*)ALLOC(sizeof(hash_entry_t) * size);
    map->values  = ALLOC(element_size * size);

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

void hashmap_delete(hashmap_t *map) {
    FREE(map->values);
    FREE(map->entries);
}

b32 rebuild_map(hashmap_t *map) {
    hashmap_t old_map = *map;

    if (!hashmap_create(map, map->capacity * 2, map->element_size)) {
        return false;
    }

    for (u64 i = 0; i < old_map.capacity; i++) {
        if (!map->entries[i].occupied) continue;
        if (map->entries[i].deleted)   continue;

        hashmap_add(map, old_map.entries[i].name, (u8*)old_map.values + i * map->element_size);
    }

    hashmap_delete(&old_map);
    return true;
}

b32 hashmap_add(hashmap_t *map, string_t key, void *value) {
    if (map->load > (map->capacity * MAX_HASHMAP_LOAD)) {
        if (!rebuild_map(map)) {
            log_error(STR("Map is full, couldn't insert element, attempted to but we resize it and still failed."), 0);
        }
    }

    u32 hash = get_string_hash(key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (!map->entries[lookup].occupied || map->entries[lookup].deleted) { 
            
            memcpy((u8*)map->values + map->element_size * lookup, value, map->element_size);

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

b32 hashmap_contains(hashmap_t *map, string_t key) {
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

void *hashmap_get(hashmap_t *map, string_t key) {
    u32 hash = get_string_hash(key);
    u32 index = hash % map->capacity;

    for (u64 offset = 0; offset < map->capacity; offset++) {
        u32 lookup = (index + offset) % map->capacity;

        if (map->entries[lookup].deleted) continue;

        if (!map->entries[lookup].occupied) return NULL;

        if (compare_strings(key, map->entries[lookup].name)) {
            return (u8*)map->values + lookup * map->element_size;
        }
    }

    return NULL;
}

b32 hashmap_remove(hashmap_t *map, string_t key) {
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


void hashmap_tests(void) {
    hashmap_t map = {};

    b32 result = hashmap_create(&map, 256, sizeof(u32));

    assert(result, "hashmap couldnt create itself.");
    map.capacity = 3;

    u8 name1[] = { "hashmap1" };
    u8 name2[] = { "hashmap2" };
    u64 size = 8;
    void* ref;

    string_t key1 = (string_t){ .size = size , .data = name1 };

    string_t key2 = (string_t){ .size = size, .data = name2 };

    ref = hashmap_get(&map, key1);
    assert(ref == NULL, "hashmap returned valid address even without elements");
    
    assert(hashmap_add(&map, key1, &size), "adding new key failed (1)");
    ref = hashmap_get(&map, key1);
    assert(ref != NULL, "hashmap didn't return valid address of element");

    assert(hashmap_add(&map, key2, &size), "adding new key failed (2)");

    ref = hashmap_get(&map, key2);
    assert(ref != NULL, "hashmap didn't return valid address of second element");

    assert(!hashmap_add(&map, key2, &size), "adding key (2) should fail");

    assert(hashmap_contains(&map, key1), "hash map should contain key (1)");
    assert(hashmap_contains(&map, key2), "hash map should contain key (2)");

    assert(hashmap_remove(&map, key1), "hash map should remove key (1)");
    assert(!hashmap_contains(&map, key1), "hash map shouldn't contain key (1)");

    assert(hashmap_remove(&map, key2), "hash map should remove key (2)");
    assert(!hashmap_contains(&map, key2), "hash map shouldn't contain key (2)");

    assert(hashmap_add(&map, key1, &size), "adding new key failed after deleting (1)");
    assert(hashmap_contains(&map, key1), "hash map should contain key (1)");

    hashmap_delete(&map);

    log_info(STR("hashmap: OK"), 0);
}
