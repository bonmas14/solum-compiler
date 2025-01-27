#include "hashmap.h"

static inline u32 get_hash(u64 size, void *data) {
    assert(size > 0);
    assert(data != 0);
    u32 hash = 216613261u;

    u8* arr = (u8*)data;
    for (u64 i = 0; i < size; i++) {
        hash ^= arr[i];
        hash *= 0x1000193;
    }

    return hash;
}

inline u32 get_string_hash(string_t string) {
    return get_hash(string.size, string.data);
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

void hashmap_tests(void) {
    hashmap_t<u32> map = {};

    b32 result = hashmap_create(&map, 256);

    assert(result);
    map.capacity = 3;

    u8 name1[] = { "hashmap1" };
    u8 name2[] = { "hashmap2" };
    u32 size = 8;
    void* ref;

    string_t key1 = (string_t){ .size = size , .data = name1 };

    string_t key2 = (string_t){ .size = size, .data = name2 };

    ref = hashmap_get(&map, key1);
    assert(ref == NULL);
    
    assert(hashmap_add(&map, key1, &size));
    ref = hashmap_get(&map, key1);
    assert(ref != NULL);

    assert(hashmap_add(&map, key2, &size));

    ref = hashmap_get(&map, key2);
    assert(ref != NULL);

    assert(!hashmap_add(&map, key2, &size));

    assert(hashmap_contains(&map, key1));
    assert(hashmap_contains(&map, key2));

    assert(hashmap_remove(&map, key1));
    assert(!hashmap_contains(&map, key1));

    assert(hashmap_remove(&map, key2));
    assert(!hashmap_contains(&map, key2));

    assert(hashmap_add(&map, key1, &size));
    assert(hashmap_contains(&map, key1));

    hashmap_delete(&map);

    log_info(STR("hashmap: OK"), 0);
}
