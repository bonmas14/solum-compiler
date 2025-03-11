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


enum expr_analyze_result_t {
    EXPR_GENERAL_ERROR,
    EXPR_VALID,
};


b32 analyze_unkn_stmt(compiler_t *compiler) {
    return compiler->is_valid;
}

b32 analyze_root_stmt(compiler_t *compiler) {
    return compiler->is_valid;
}

b32 analyze_code(compiler_t *compiler) {
    for (u64 i = 0; i < compiler->parser->parsed_roots.count; i++) {
        analyze_root_stmt(compiler);
    }

    return false;
}
