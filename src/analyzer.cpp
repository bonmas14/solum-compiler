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

b32 is_already_resolved_symbol(scope_tuple_t *scope, ast_node_t *node) {
    if (!hashmap_contains(&scope->scope, node->token.data.symbol)) {
        return false;
    }

    scope_entry_t *entry = hashmap_get(&scope->scope, node->token.data.symbol);

    if (!entry->resolved) {
        return false;
    }

    if (entry->node_index == node->self_index) {
        return false;
    }

    return true;
}

void print_redeclaration_error(compiler_t *compiler, ast_node_t *node, scope_tuple_t *parent_scope) {
    log_error_token(STR("Symbol was already declared"), compiler->scanner, node->token, 0);

    scope_entry_t *orig_entry = hashmap_get(&parent_scope->scope, node->token.data.symbol);
    ast_node_t    *orig_node  = area_get(&compiler->parser->nodes, orig_entry->node_index);

    log_error_token(STR("Previously declared here: "), compiler->scanner, orig_node->token, 0);
}

b32 analyze_variable(compiler_t *compiler, ast_node_t *node, scope_tuple_t *parent_scope) {
    log_info(STR("VAR"), 0);

    return false;
}

b32 analyze_function(compiler_t *compiler, ast_node_t *node, scope_tuple_t *in_scope) {
    log_info(STR("FUNC"), 0);

    if (hashmap_contains(&in_scope->scope, node->token.data.symbol)) {
        print_redeclaration_error(compiler, node, in_scope);
        return false;
    }

    scope_entry_t entry = {};

    entry.resolved = true;
    entry.type     = SCOPE_FUNC;

    entry.node_index = node->self_index;

    ast_node_t *type = area_get(&compiler->parser->nodes, node->left_index);

    check(type->type == AST_BIN);

    entry.func.argc = area_get(&compiler->parser->nodes, type->left_index)->child_count;
    entry.func.retc = area_get(&compiler->parser->nodes,type->right_index)->child_count;

    ast_node_t *body = area_get(&compiler->parser->nodes, node->right_index);

    if (body->type == AST_EMPTY) {
        return false;
    }

    scope_tuple_t body_scope;



    if (body->subtype == AST_BLOCK_IMPERATIVE) {
        // parse_

        // new_scope
        

        // analyze_funciton_body(compiler, body, body_scope);

    } else if (body->subtype == SUBTYPE_AST_EXPR) {
    } else {
    //    return false;
    }
    // if it is an expression. eval it and typecheck with return values
    // it is basically transforms into 
    //
    // from:
    //  a : (x : s32, y : s32) -> s32 = x + y;
    //  b : (x : s32, y : s32) -> s32, s32 = x + y, x * y;
    //
    // to:
    //  a : (x : s32, y : s32) -> s32 = { ret x + y; }
    //  b : (x : s32, y : s32) -> s32, s32 = { ret x + y, x * y; }
    // 


    // parse_argnames and create new scope 
    if (entry.func.retc > MAX_COUNT_OF_RETS) {
        log_error_token(STR("multiple return values are not supported currently."), compiler->scanner, node->token, 0);
        return false;
    }

    if (entry.func.argc > MAX_COUNT_OF_PARAMS) {
        log_error_token(STR("too much of parameters in function."), compiler->scanner, node->token, 0);
        return false;
    }

    assert(hashmap_add(&in_scope->scope, node->token.data.symbol, &entry));

    return true;
}

b32 analyze_unknown_definition(compiler_t *compiler, ast_node_t *node, scope_tuple_t *global_scope) {
    ast_node_t *type = area_get(&compiler->parser->nodes, node->left_index);

            // @todo this will allow for this case: [] ^ [] ^ [] s32
            // this doesnt make any sence, so we need to delete it in analyzer step
    switch (type->subtype) {
        case SUBTYPE_AST_ARR_TYPE:
            break;

        case SUBTYPE_AST_PTR_TYPE:
            break;

        case SUBTYPE_AST_AUTO_TYPE:
            // analyze_and_resolve_type();
            break;

        case SUBTYPE_AST_STD_TYPE:
            // return analyze_known_type_variable();
            break;

        case SUBTYPE_AST_FUNC_TYPE:
            return analyze_function(compiler, node, global_scope);
            break;

        case SUBTYPE_AST_UNKN_TYPE:
            // is_already_resolved_symbol
            // we cant know what is it so we delay everything
            // return false;
            break;
        default:
            return false;
    }
    
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
            
            // it could be an variable instead of an function

            // check for type

            return analyze_unknown_definition(compiler, node, global_scope);
            break;

        case SUBTYPE_AST_STRUCT_DEF:
        case SUBTYPE_AST_UNION_DEF:
        case SUBTYPE_AST_ENUM_DEF:
        default:
            return false;
            break;
    }

    return true;
}

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

    b32 state = true;
    for (u64 i = 0; i < parser->root_indices.count; i++) {
        ast_node_t* root = area_get(&parser->nodes, *area_get(&parser->root_indices, i));

        if (!analyze_root_statement(compiler, root, &compiler->analyzer->scopes.data[0])) {
            state = false;
        }
    }

    return state;
}
