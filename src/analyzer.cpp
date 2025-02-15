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


b32 analyze_function(compiler_t *compiler, ast_node_t *node, scope_tuple_t *parent_scope) {
    log_info(STR("analyzing"), 0);

    scope_entry_t entry = {};

    if (hashmap_contains(&parent_scope->scope, node->token.data.symbol)) {
        log_error_token(STR("Function was already declared"), compiler->scanner, node->token, 0);

        log_error_token(STR("Previously declared here: "),
                compiler->scanner,
                area_get(&compiler->parser->nodes, hashmap_get(&parent_scope->scope, node->token.data.symbol)->node_index)->token,
                0);

        return false;
    }

    entry.type = SCOPE_FUNC;
    // check and create unique symbol
    // create scope, add parent

    entry.node_index = node->self_index;


    ast_node_t *type = area_get(&compiler->parser->nodes, node->left_index);

    entry.func.argc = area_get(&compiler->parser->nodes, type->left_index)->child_count;
    entry.func.retc = area_get(&compiler->parser->nodes,type->right_index)->child_count;

    if (entry.func.retc > 1) {
        log_error_token(STR("multiple return values are not supported currently."), compiler->scanner, node->token, 0);
        return false;
    }

    hashmap_add(&parent_scope->scope, node->token.data.symbol, &entry);

    return true;
}

b32 analyze_root_statement(compiler_t *compiler, ast_node_t *node, scope_tuple_t *global_scope) {
    switch (node->subtype) {
        case SUBTYPE_AST_EXPR:
        case SUBTYPE_AST_PARAM_DEF:
            // not here

            log_error(STR("expressions in global scope are not supported"), 0);
            break;

        case SUBTYPE_AST_UNKN_DEF:
            // block type is allowed over here
            // check for parameter types. only allow std ones right now
            

            return analyze_function(compiler, node, global_scope);

            break;

        case SUBTYPE_AST_STRUCT_DEF:
        case SUBTYPE_AST_UNION_DEF:
        case SUBTYPE_AST_ENUM_DEF:
        default:
            break;
    }

    return true;
}


b32 analyze_code(compiler_t *compiler) {
    parser_t   *parser   = compiler->parser;
    analyzer_t *analyzer = compiler->analyzer;

    check(area_create(&analyzer->scopes, 100));

    scope_tuple_t global_scope = {};

    global_scope.is_global    = true;
    global_scope.parent_scope = 0;

    check(hashmap_create(&global_scope.scope, &compiler->scanner->symbols, 100));


    for (u64 i = 0; i < parser->root_indices.count; i++) {
        ast_node_t* root = area_get(&parser->nodes, *area_get(&parser->root_indices, i));


        (void)analyze_root_statement(compiler, root, &global_scope);

        // analyze 
        // print_node(&compiler, root, 0);
    }


    // check all global states
    //
    //

    // find main
    return false;
}
