#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler.h"
#include "parser.h"
#include "area_alloc.h"
#include "hashmap.h"
#include "scanner.h"

struct scope_entry_t {
    b32 resolved;
    u32 type;
    ast_node_t *node;

    union {
        struct {
            u32 argc;
            u32 retc;

            u64 argv_types[MAX_COUNT_OF_PARAMS];
            u64 retv_types[MAX_COUNT_OF_RETS];
        } func;

        struct {
            b32 is_const;
            u64 type;
        } var;

        struct {
            u32 size;
            u32 alignment;
        } type;
    } data;
};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_FUNC,
};

struct scope_tuple_t {
    b32 is_global;
    u64 parent_scope;
    hashmap_t<string_t, scope_entry_t> table;
};

struct analyzer_t {
    list_t<scope_tuple_t> scopes;
    // hashmap_t<u64, types_t> type_lookup;
};

enum funciton_flags_t {
    SCOPE_FUNC_EXTERNAL = 0x00010000,
    SCOPE_FUNC_PROTO    = 0x00020000,
};

b32 analyze_code(compiler_t *compiler);

#endif // ANALYZER_H
