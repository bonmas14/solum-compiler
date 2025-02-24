#include "analyzer.h"
#include "parser.h"
#include "scanner.h"
#include "logger.h"
#include "area_alloc.h"
#include "hashmap.h"

// plan @todo:
    // error at every global scope expression
    // scan tree and create all scopes + add all symbols
    // create compile units and after that type check all of them
    // create final IR of every scope
    //
    // codegen
    //
    //
    // count elements on left and rignt in a, b = b, a;
    // typecheck
    //
    // check functions to not contain assingment expresions! because it will break ','
    //
    // block in general := structure is not allowed if it is not an function def

void analyzer_create(analyzer_t *analyzer) {
    assert(area_create(&analyzer->scopes, 100));

    u64 index = {};
    area_allocate(&analyzer->scopes, 1, &index);

    scope_tuple_t *global_scope = area_get(&analyzer->scopes, index);

    global_scope->is_global    = true;
    global_scope->parent_scope = 0;

    assert(area_create(&analyzer->symbols, 256));
    assert(hashmap_create(&global_scope->scope, &analyzer->symbols, 100));
}

b32 analyze_code(compiler_t *compiler) {
    parser_t   *parser   = compiler->parser;
    analyzer_t *analyzer = compiler->analyzer;


    return false;
}
