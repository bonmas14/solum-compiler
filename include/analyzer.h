#ifndef ANALYZER_H
#define ANALYZER_H

#include "list.h"
#include "hashmap.h"

struct compiler_t;
struct ast_node_t;

#define MAX_COUNT_OF_PARAMS 16
#define MAX_COUNT_OF_RETS 1 

// func : (a : s32) -> s32 = { return a; }
//
// -> func
//    -> a
//       s32
//    -> s32
//    -> {}
//
// var : s32 = 43;
//
// var
//     s32
//
// test : struct = {
//     a : s32;
//     b : s32;
//     c : u64;
// }
//
// v : test;
// v.a =
//

struct scope_entry_t {
    b32 configured;
    u32 type;

    ast_node_t *node;
    hashmap_t<string_t, scope_entry_t> scope;

    // ENTRY_VAR
    
    // b32 array_type;
    // u32 array_count;
    u32 pointer_depth;
};

enum types_t {
    TYPE_ERROR = 0,
    TYPE_STD, 
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_PROTO,

};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_PROTOTYPE,
    ENTRY_FUNC,
    ENTRY_NAMESPACE,
};

b32 analyze_and_compile(compiler_t *compiler);
b32 load_and_process_file(compiler_t *compiler, string_t filename);

#endif // ANALYZER_H
