#include "compiler.h"

#include "arena.h"
#include "hashmap.h"
#include "parser.h"
#include "scanner.h"
#include "analyzer.h"
#include "backend.h"

void create_standart_types(analyzer_t *analyzer) {
    types_t type = {};

    type.type = TYPE_STD;

    type.std.is_void = true;
    list_add(&analyzer->types, &type);
    type.std.is_void = false;

    // U8-64
    type.std.is_unsigned = true;

    for (u64 i = 1; i < 8; i++) {
        type.size = i;
        type.alignment = i;

        list_add(&analyzer->types, &type);
    }

    // S8-64
    type.std.is_unsigned = false;

    for (u64 i = 1; i < 8; i++) {
        type.size = i;
        type.alignment = i;
        list_add(&analyzer->types, &type);
    }

    // B8 B32
    type.std.is_unsigned = true;
    
    type.size = 1;
    type.alignment = 1;
    list_add(&analyzer->types, &type);

    type.size = 4;
    type.alignment = 4;
    list_add(&analyzer->types, &type);

    // f32 f64
    type.std.is_float    = true;
    type.std.is_unsigned = false;

    type.size      = 4;
    type.alignment = 4;
    list_add(&analyzer->types, &type);

    type.size      = 8;
    type.alignment = 8;
    list_add(&analyzer->types, &type);
}

analyzer_t *analyzer_create(allocator_t *allocator) {
    analyzer_t *analyzer = (analyzer_t*)mem_alloc(allocator, sizeof(analyzer_t));

    u64 index = {};
    list_allocate(&analyzer->scopes, 1, &index);
    scope_t *global_scope = list_get(&analyzer->scopes, index);
    assert(index == 0);

    global_scope->table.hash_func    = get_string_hash;
    global_scope->table.compare_func = compare_string_keys;

    return analyzer;
}

compiler_t create_compiler_instance(void) {
    compiler_t compiler = {};

	compiler.nodes   = preserve_allocator_from_stack(create_arena_allocator(sizeof(ast_node_t) * INIT_NODES_SIZE));
    compiler.strings = preserve_allocator_from_stack(create_arena_allocator(4096));

    compiler.analyzer = analyzer_create(default_allocator);
    compiler.codegen  = codegen_create(default_allocator);

    compiler.files.hash_func    = get_string_hash;
    compiler.files.compare_func = compare_string_keys;

    compiler.is_valid = true;

    return compiler;
}
