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

u32 get_symbol_hash(area_t<u8> *symbols, symbol_t symbol) {
    void *data = area_get(symbols, symbol.table_index);
    assert(data != NULL);
    return get_hash(symbol.size, data);
}

b32 compare_symbols(area_t<u8> *symbols, symbol_t a, symbol_t b) {
    if (a.size != b.size) return false;

    b32 hash_comp = get_symbol_hash(symbols, a) == get_symbol_hash(symbols, b);

    if (!hash_comp) return hash_comp;

    u8 *a_data = area_get(symbols, a.table_index);
    u8 *b_data = area_get(symbols, b.table_index);

    for (u64 i = 0; i < a.size; i++) {
        if (a_data[i] != b_data[i]) {
            return false;
        }
    }

    return true;
}


void hashmap_tests(void) {
    area_t<u8> symbols = {};

    assert(area_create(&symbols, 100));

    hashmap_t<u32> map = {};

    b32 result = hashmap_create(&map, &symbols, 256);

    assert(result);
    map.capacity = 3;
    
    u8 name1[] = { "hashmap1" };
    u8 name2[] = { "hashmap2" };

    u64 index = 0;
    area_allocate(&symbols, sizeof(name1), &index);

    area_fill(&symbols, (u8*)name1, sizeof(name1), index);
    symbol_t key1 = (symbol_t){ .size = sizeof(name1), .table_index = (u32)index };

    area_allocate(&symbols, sizeof(name2), &index);
    area_fill(&symbols, (u8*)name2, sizeof(name2), index);
    symbol_t key2 = (symbol_t){ .size = sizeof(name2), .table_index = (u32)index };

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

    log_info(STR("hashmap: OK"), 0);
}
