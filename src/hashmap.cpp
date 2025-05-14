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

struct MyKey { int a; char b; };

COMPUTE_HASH(mykey_hash) {
    UNUSED(size);
    MyKey* k = (MyKey*)key;
    return k->a * 31 + k->b;
}

COMPARE_KEYS(mykey_compare) {
    UNUSED(a_size);
    UNUSED(b_size);
    MyKey* k1 = (MyKey*)a;
    MyKey* k2 = (MyKey*)b;
    return k1->a == k2->a && k1->b == k2->b;
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

    {
        hashmap_t<u32, u32> map;
        b32 created = hashmap_create(&map, 5, NULL, NULL);
        assert(created);
        assert(map.capacity == 5);
        assert(map.load == 0);
        hashmap_delete(&map);
    }

    {
        hashmap_t<string_t, u32> original;
        hashmap_create(&original, 3, get_string_hash, compare_string_keys);

        string_t key1 = STRING("key1");
        u32 value1 = 100;
        hashmap_add(&original, key1, &value1);

        auto clone = hashmap_clone(&original);
        assert(hashmap_contains(&clone, key1));
        assert(*hashmap_get(&clone, key1) == value1);

        // Modify original and verify clone is unaffected
        string_t key2 = STRING("key2");
        u32 value2 = 200;
        hashmap_add(&original, key2, &value2);
        assert(!hashmap_contains(&clone, key2));

        hashmap_delete(&original);
        hashmap_delete(&clone);
    }

    {
        hashmap_t<u32, u32> map;
        hashmap_create(&map, 10, NULL, NULL);

        u32 key = 42;
        u32 value = 100;
        hashmap_add(&map, key, &value);
        hashmap_clear(&map);
        assert(map.load == 0);
        assert(hashmap_get(&map, key) == NULL);
        hashmap_delete(&map);
    }

    // Test collision handling
    {
        hashmap_t<u32, u32> map;
        hashmap_create(&map, 3, NULL, NULL);

        u32 key1 = 1, key2 = 4; // Colliding keys if capacity is 3
        u32 val1 = 100, val2 = 200;

        hashmap_add(&map, key1, &val1);
        hashmap_add(&map, key2, &val2);
        assert(*hashmap_get(&map, key1) == val1);
        assert(*hashmap_get(&map, key2) == val2);

        hashmap_remove(&map, key1);
        assert(hashmap_get(&map, key1) == NULL);
        assert(*hashmap_get(&map, key2) == val2);

        hashmap_delete(&map);
    }

    // Test edge cases
    {
        // Remove non-existent key
        hashmap_t<u32, u32> map;
        hashmap_create(&map, 5, NULL, NULL);
        assert(!hashmap_remove(&map, (u32)123));
        hashmap_delete(&map);

        // Add after removal
        hashmap_t<u32, u32> map2;
        hashmap_create(&map2, 5, NULL, NULL);
        u32 key = 5, value = 10;
        hashmap_add(&map2, key, &value);
        hashmap_remove(&map2, key);
        hashmap_add(&map2, key, &value);
        assert(*hashmap_get(&map2, key) == value);
        hashmap_delete(&map2);
    }

    // Test custom key type
    {
        hashmap_t<MyKey, u32> map;
        hashmap_create(&map, 10, mykey_hash, mykey_compare);

        MyKey key1 = {5, 'x'}, key2 = {5, 'y'};
        u32 v1 = 100, v2 = 200;
        hashmap_add(&map, key1, &v1);
        hashmap_add(&map, key2, &v2);
        assert(*hashmap_get(&map, key1) == v1);
        assert(*hashmap_get(&map, key2) == v2);
        hashmap_delete(&map);
    }

    // Test create_map_if_needed (implicit initialization)
    {
        hashmap_t<u32, u32> map = {};
        u32 key = 1, value = 100;
        assert(hashmap_add(&map, key, &value)); // Should auto-initialize
        assert(hashmap_contains(&map, key));
        hashmap_delete(&map);
    }
}
