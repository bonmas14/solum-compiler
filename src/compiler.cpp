#include "compiler.h"

#include "arena.h"
#include "hashmap.h"
#include "parser.h"
#include "scanner.h"
#include "analyzer.h"
#include "backend.h"

analyzer_t *analyzer_create(allocator_t *allocator) {
    analyzer_t *analyzer = (analyzer_t*)mem_alloc(allocator, sizeof(analyzer_t));

    check_value(list_create(&analyzer->scopes, 100));

    u64 index = {};
    list_allocate(&analyzer->scopes, 1, &index);

    scope_t *global_scope = list_get(&analyzer->scopes, index);
    assert(index == 0);

    check_value(hashmap_create(&global_scope->table, 100, get_string_hash, compare_string_keys));
    return analyzer;
}

compiler_t create_compiler_instance(void) {
    compiler_t compiler = {};

    compiler.scanner = (scanner_t*)mem_alloc(default_allocator, sizeof(scanner_t));
    compiler.parser  =  (parser_t*)mem_alloc(default_allocator, sizeof(parser_t));

	compiler.nodes   = preserve_allocator_from_stack(create_arena_allocator(sizeof(ast_node_t) * INIT_NODES_SIZE));
    compiler.strings = preserve_allocator_from_stack(create_arena_allocator(4096));

    compiler.analyzer = analyzer_create(default_allocator);
    compiler.codegen  = codegen_create(default_allocator);

    compiler.is_valid = true;

    return compiler;
}
