#ifndef HASHMAP_H
#define HASHMAP_H

#ifndef CUSTIOM_MEM_CTRL

#include "stddefines.h"
#include "logger.h"
#include <memory.h> 

#define ALLOC(x)      calloc(1, x) 
#define REALLOC(x, y) realloc(x, y)
#define FREE(x)       free(x) 

#endif

#define MAX_HASHMAP_LOAD 0.75

typedef struct hash_entry_t {
    string_t name;
    b32 occupied;
    b32 deleted;
} hash_entry_t;

typedef struct hashmap_t {
    u64 load;
    u64 element_size;
    u64 capacity;

    hash_entry_t *entries;
    void         *values;
} hashmap_t;

u32 get_string_hash(string_t string);

b32  hashmap_create(hashmap_t *container, u64 size, u64 element_size);
void hashmap_delete(hashmap_t *map);

b32   hashmap_add(hashmap_t *map, string_t key, void *value);
void *hashmap_get(hashmap_t *map, string_t key);

b32   hashmap_remove(hashmap_t   *map, string_t key);
b32   hashmap_contains(hashmap_t *map, string_t key);

void hashmap_tests(void);

#endif // HASHMAP_H
