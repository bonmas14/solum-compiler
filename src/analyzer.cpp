#include "compiler.h"
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

b32 analyze_unkn_def(compiler_t *compiler, ast_node_t *node) {
    // get name and register it 
    return compiler->is_valid;
}

b32 analyze_root_stmt(compiler_t *compiler, ast_node_t *root) {
    assert(compiler != NULL);
    assert(root     != NULL);

    if (root->type == AST_EMPTY)
        return true;

    if (root->type == AST_ERROR) {
        log_error(STR("Got unexpected value @error"), 0);
        assert(false);
        return false;
    }

    switch (root->subtype) {
        case SUBTYPE_AST_UNKN_DEF:
            return analyze_unkn_def(compiler, root);

        case SUBTYPE_AST_STRUCT_DEF:
            break;

        case SUBTYPE_AST_UNION_DEF:
            break;

        case SUBTYPE_AST_ENUM_DEF:
            break;

        case SUBTYPE_AST_EXPR:
            log_error_token(STR("Expression is not valid construct in global scope."), compiler->scanner, root->token, 0);
            break;

        default:
            log_error_token(STR("Wrong type of construct in global scope."), compiler->scanner, root->token, 0);
            assert(false);
            break;
    }

    return false;
}

// first thing we do is add all of symbols in table
// then we can set 'ast_node_t.analyzed' to 'true'
// and then we can already analyze the code

b32 analyze_code(compiler_t *compiler) {
    for (u64 i = 0; i < compiler->parser->parsed_roots.count; i++) {
        ast_node_t *node = *list_get(&compiler->parser->parsed_roots, i);

        if (!analyze_root_stmt(compiler, node)) {
            // we got error there
            // it was already reported
            // so we just quit
            return false;
        }
    }

    return false;
}
