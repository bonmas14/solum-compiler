#include "compiler.h"
#include "hashmap.h"
#include "parser.h"
#include "scanner.h"
#include "analyzer.h"
#include "backend.h"

analyzer_t *analyzer_create(arena_t *allocator) {
    analyzer_t *analyzer = (analyzer_t*)arena_allocate(allocator, sizeof(analyzer_t));

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

    compiler.scanner = (scanner_t*)arena_allocate(default_allocator, sizeof(scanner_t));
    compiler.parser  =  (parser_t*)arena_allocate(default_allocator, sizeof(parser_t));

	compiler.node_allocator = arena_create(sizeof(ast_node_t) * INIT_NODES_SIZE);
    assert(compiler.node_allocator != NULL);

    compiler.string_allocator = arena_create(4096);
    assert(compiler.string_allocator != NULL);

    compiler.analyzer = analyzer_create(default_allocator);
    compiler.codegen  = codegen_create(default_allocator);

    compiler.is_valid = true;

    return compiler;
}
