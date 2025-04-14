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


enum types_t {
    TYPE_ERROR,
    TYPE_UNKN,

    TYPE_u8,
    TYPE_u16,
    TYPE_u32,
    TYPE_u64,

    TYPE_s8,
    TYPE_s16,
    TYPE_s32,
    TYPE_s64,
    
    TYPE_f32,
    TYPE_f64,

    TYPE_b8,
    TYPE_b32,

    TYPE_void,
};

struct type_info_t {
    u32 type;
    u32 size;
    u32 pointer_depth;
    string_t type_name;
};

struct scope_entry_t {
    b32 configured;
    u32 type;

    ast_node_t *node;
    ast_node_t *expr;

    // ENTRY_TYPE
    u32 def_type; // types_t
    
    union {
        // NOTE: if it is not in union, you need to change creation in analyzer
        hashmap_t<string_t, scope_entry_t> scope; 

        // ENTRY_FUNC
        struct {
            hashmap_t<string_t, scope_entry_t> func_params;
            list_t<type_info_t> return_typenames;
            string_t prototype_name;
        };
    };

    // ENTRY_VAR
    
    // b32 array_type;
    // u32 array_count;

    type_info_t info;
};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_PROTOTYPE,
    ENTRY_FUNC,
    // ENTRY_NAMESPACE,
};

b32 analyzer_preload_all_files(compiler_t *compiler);
b32 analyze(compiler_t *compiler);

b32 load_and_process_file(compiler_t *compiler, string_t filename);

#endif // ANALYZER_H
