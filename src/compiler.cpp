#include "compiler.h"
#include "parser.h"
#include "scanner.h"
#include "analyzer.h"

void analyzer_create(analyzer_t *analyzer) {
    check(list_create(&analyzer->scopes, 100));

    u64 index = {};
    list_allocate(&analyzer->scopes, 1, &index);

    scope_tuple_t *global_scope = list_get(&analyzer->scopes, index);

    global_scope->is_global    = true;
    global_scope->parent_scope = 0;

    check(hashmap_create(&global_scope->scope, 100));
}

compiler_t create_compiler_instance(void) {
    compiler_t compiler = {};

    compiler.scanner  =  (scanner_t*)arena_allocate(default_allocator, sizeof(scanner_t));
    compiler.parser   =   (parser_t*)arena_allocate(default_allocator, sizeof(parser_t));
    compiler.analyzer = (analyzer_t*)arena_allocate(default_allocator, sizeof(analyzer_t));

    assert(compiler.scanner  != NULL);
    assert(compiler.parser   != NULL);
    assert(compiler.analyzer != NULL);

	compiler.node_allocator = arena_create(sizeof(ast_node_t) * INIT_NODES_SIZE);

    if (!compiler.node_allocator) {
        log_error(STR("Parser: Couldn't create node_allocator arena."), 0);
        return {};
    }

    compiler.is_valid = true;
    compiler.string_allocator = arena_create(4096);

    assert(compiler.string_allocator != NULL);

    analyzer_create(compiler.analyzer);

    return compiler;
}
