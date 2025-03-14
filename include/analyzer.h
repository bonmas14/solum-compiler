#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler.h"
#include "area_alloc.h"
#include "hashmap.h"
#include "scanner.h"

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

enum type_flags_t {
    TYPE_FLAG_ERROR  = 0x0, 
    TYPE_FLAG_STRUCT = 0x1, 
    TYPE_FLAG_UNION  = 0x2, 
    TYPE_FLAG_ENUM   = 0x4, 
    TYPE_FLAG_STD    = 0x8, 
};

struct types_t {
    u32 type_flags;

};

struct scope_tuple_t {
    b32 is_global;
    u64 parent_scope;
    hashmap_t<scope_entry_t> scope;
    list_t<types_t> user_types_lookup_list; 
};

struct analyzer_t {
    list_t<scope_tuple_t> scopes;
};

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

b32 analyze_code(compiler_t *compiler);

#endif // ANALYZER_H
