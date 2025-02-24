#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler.h"
#include "area_alloc.h"
#include "hashmap.h"
#include "scanner.h"

#define MAX_COUNT_OF_PARAMS 16
#define MAX_COUNT_OF_RETS 1 

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
    b32 resolved;
    u32 type;
    u32 flags;
    u32 node_index;

    union {
        struct {
            u32 argc;
            u32 retc;

            u32 argv_types[MAX_COUNT_OF_PARAMS];
            u32 retv_types[MAX_COUNT_OF_RETS];
        } func;

        struct {
            b32 is_const;
            u32 type;
        } var;
    };
};

struct scope_tuple_t {
    b32 is_global;
    u64 parent_scope;
    hashmap_t<scope_entry_t> scope;
    area_t<symbol_t> user_types_lookup_list; 
};

struct analyzer_t {
    area_t<scope_tuple_t> scopes;

    area_t<u8> symbols;
};

void analyzer_create(analyzer_t *analyzer);

b32 analyze_code(compiler_t *compiler);

#endif // ANALYZER_H
