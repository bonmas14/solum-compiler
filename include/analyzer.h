#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler.h"
#include "parser.h"
#include "list.h"
#include "hashmap.h"
#include "scanner.h"

struct scope_entry_t {
    b8 not_resolved;
    b8 can_be_used;
    u32 type;
    ast_node_t *node;
};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_PROTOTYPE,
    ENTRY_FUNC,
    ENTRY_UNKN,
};

struct scope_t {
    u64 parent_scope;
    hashmap_t<string_t, scope_entry_t> table;
};

struct analyzer_t {
    list_t<scope_t> scopes;
};

b32 analyze_code(compiler_t *compiler);

#endif // ANALYZER_H
