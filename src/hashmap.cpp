#include "hashmap.h"
#include "memctl.h"
#include "strings.h"

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

    b32 hash_comp = get_hash_std(size, a) == get_hash_std(size, b);

    if (!hash_comp) return hash_comp;

    return mem_compare((u8*)a, (u8*)b, size) == 0;
}

COMPUTE_HASH(get_string_hash) {
    UNUSED(size);
    assert(key != NULL);
    assert(size == sizeof(string_t));

    string_t *symbol = (string_t*)key;
    if (symbol->data == NULL) {
        return 0;
    } else {
        return get_hash(symbol->size, (void*)symbol->data);
    }
}

COMPARE_KEYS(compare_string_keys) {
    assert(a != NULL);
    assert(b != NULL);

    string_t a_str = *(string_t*)a;
    string_t b_str = *(string_t*)b;

    if (a_str.size != b_str.size) return false;

    b32 hash_comp = get_string_hash(size, a) == get_string_hash(size, b);

    if (!hash_comp) return hash_comp;

    return string_compare(a_str, b_str) == 0;
}

struct MyKey { int a; char b; };

COMPUTE_HASH(mykey_hash) {
    UNUSED(size);
    MyKey* k = (MyKey*)key;
    return k->a * 31 + k->b;
}

COMPARE_KEYS(mykey_compare) {
    UNUSED(size);
    MyKey* k1 = (MyKey*)a;
    MyKey* k2 = (MyKey*)b;
    return k1->a == k2->a && k1->b == k2->b;
}

void hashmap_tests(void) {
#ifdef DEBUG
    {
        hashmap_t<string_t, u32> map = {};
        map.hash_func    = get_string_hash;
        map.compare_func = compare_string_keys;

        map.capacity = 3;

        string_t key1 = STRING("hashmap1");
        string_t key2 = STRING("hashmap2");

        u32 size = 8;
        void* ref;

        ref = map[key1];
        assert(ref == NULL);

        assert(hashmap_add(&map, key1, &size));
        ref = map[key1];
        assert(ref != NULL);

        assert(hashmap_add(&map, key2, &size));

        ref = map[key2];
        assert(ref != NULL);

        assert(!hashmap_add(&map, key2, &size));

        assert(map[key1]);
        assert(map[key2]);

        assert(hashmap_remove(&map, key1));
        assert(!map[key1]);

        assert(hashmap_remove(&map, key2));
        assert(!map[key2]);

        assert(hashmap_add(&map, key1, &size));
        assert(map[key1]);

        hashmap_delete(&map);
    }

    {
        hashmap_t<u32, u32> map = {};

        u32 a = 0x08;
        u32 b = 0x80;

        u32 data = 100;

        void* ref;

        ref = map[a];
        assert(ref == NULL);

        assert(hashmap_add(&map, a, &data));
        ref = map[a];
    
        assert(ref != NULL);
        assert(*(u32*)ref == data);

        ref = map[b];
        assert(ref == NULL);

        data = 12312;
        assert(hashmap_add(&map, b, &data));
        ref = map[b];
    
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
        assert(clone[key1]);
        assert(*clone[key1] == value1);

        // Modify original and verify clone is unaffected
        string_t key2 = STRING("key2");
        u32 value2 = 200;
        hashmap_add(&original, key2, &value2);
        assert(!clone[key2]);

        hashmap_delete(&original);
        hashmap_delete(&clone);
    }

    { // Test hashmap clearing
        hashmap_t<u32, u32> map;
        hashmap_create(&map, 10, NULL, NULL);
        u32 key = 42;
        u32 value = 100;
        hashmap_add(&map, key, &value);
        hashmap_clear(&map);
        assert(map.load == 0);
        assert(map[key] == NULL);
        hashmap_delete(&map);
    }

    { // Test collisions
        const u64 iterations = 100;
        hashmap_t<u64, u32> map;
        hashmap_create(&map, 20, NULL, NULL);

        u64 caps = map.capacity;
        for (u64 i = 0; i < iterations; i++) {
            u32 v = (u32)i * (u32)32;
            hashmap_add(&map, i, &v);

            if (caps != map.capacity) {
                caps = map.capacity;

                for (u64 j = 0; j < map.load; j++) {
                    u32 *p = map[j];
                    assert(p != NULL);
                    assert(*p == j * 32);
                }
            }
        }

        assert(map.load == iterations);
        for (u64 j = 0; j < map.load; j++) {
            u32 *p = map[j];
            assert(*p == j * 32);
        }

        hashmap_delete(&map);
    }

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
        assert(*map[key1] == v1);
        assert(*map[key2] == v2);
        hashmap_delete(&map);
    }

    // Test create_map_if_needed (implicit initialization)
    {
        hashmap_t<u32, u32> map = {};
        u32 key = 1, value = 100;
        assert(hashmap_add(&map, key, &value)); // Should auto-initialize
        assert(map[key]);
        hashmap_delete(&map);
    }
#endif
}
