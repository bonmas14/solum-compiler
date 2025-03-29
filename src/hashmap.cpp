#include "hashmap.h"

u32 get_hash(u64 size, void *data) {
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

COMPUTE_HASH(get_hash_std) {
    assert(key != NULL);
    assert(size > 0);

    return get_hash(size, key);
}

COMPARE_KEYS(compare_keys_std) {
    assert(a != NULL);
    assert(b != NULL);

    if (a_size != b_size) return false;

    b32 hash_comp = get_hash_std(a_size, a) == get_hash_std(b_size, b);

    if (!hash_comp) return hash_comp;

    for (u64 i = 0; i < a_size; i++) {
        if (((u8*)a)[i] != ((u8*)b)[i]) {
            return false;
        }
    }

    return true;
}

COMPUTE_HASH(get_string_hash) {
    assert(key != NULL);
    assert(size > 0);

    string_t *symbol = (string_t*)key;
    assert(symbol->data != NULL);
    return get_hash(symbol->size, (void*)symbol->data);
}

COMPARE_KEYS(compare_string_keys) {
    assert(a != NULL);
    assert(b != NULL);

    string_t a_str = *(string_t*)a;
    string_t b_str = *(string_t*)b;

    if (a_str.size != b_str.size) return false;

    b32 hash_comp = get_string_hash(a_size, a) == get_string_hash(b_size, b);

    if (!hash_comp) return hash_comp;

    for (u64 i = 0; i < a_str.size; i++) {
        if (a_str.data[i] != b_str.data[i]) {
            return false;
        }
    }

    return true;
}


void hashmap_tests(void) {
    {
        hashmap_t<string_t, u32> map = {};
        map.hash_func    = get_string_hash;
        map.compare_func = compare_string_keys;

        map.capacity = 3;

        string_t key1 = STRING("hashmap1");
        string_t key2 = STRING("hashmap2");

        u32 size = 8;
        void* ref;

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
    }

    {
        hashmap_t<u32, u32> map = {};

        u32 a = 0x08;
        u32 b = 0x80;

        u32 data = 100;

        void* ref;

        ref = hashmap_get(&map, a);
        assert(ref == NULL);

        assert(hashmap_add(&map, a, &data));
        ref = hashmap_get(&map, a);
    
        assert(ref != NULL);
        assert(*(u32*)ref == data);

        ref = hashmap_get(&map, b);
        assert(ref == NULL);

        data = 12312;
        assert(hashmap_add(&map, b, &data));
        ref = hashmap_get(&map, b);
    
        assert(ref != NULL);
        assert(*(u32*)ref == data);
    }

    log_info(STR("hashmap: OK"));
}
