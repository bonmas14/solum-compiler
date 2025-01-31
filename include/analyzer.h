#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler.h"
#include "area_alloc.h"
#include "hashmap.h"
#include "scanner.h"

enum funciton_flags_t {
    SCOPE_IS_GENERIC    = 0x00100000,
    SCOPE_FUNC_EXTERNAL = 0x00010000,
    SCOPE_FUNC_PROTO    = 0x00020000,
};

enum scope_entry_types_t {
    SCOPE_VAR,
    SCOPE_FUNC, // generic or not
    SCOPE_TYPE // proto, enum, struct, union
};

struct scope_entry_t {
    u32 type;
    u32 flags;
    u32 node_index;
};

struct scope_tuple_t {
    b32 is_global;
    u64 parent_scope;
    hashmap_t<scope_entry_t> scope;
};

struct analyzer_t {
    area_t<scope_tuple_t> scopes;
};


#endif // ANALYZER_H
