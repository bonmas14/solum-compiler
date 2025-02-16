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

b32 node_should_be_changed_or_added(scope_tuple_t *scope, ast_node_t *node) {
    if (!hashmap_contains(&scope->scope, node->token.data.symbol)) {
        scope_entry_t empty = {};
        hashmap_add(&scope->scope, node->token.data.symbol, &empty);
        return true;
    } 

    if (!hashmap_get(&scope->scope, node->token.data.symbol)->resolved) {
        return true;
    }

    return false;
}

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

void print_previous_declaration(compiler_t *compiler, ast_node_t *node, scope_tuple_t *parent_scope) {
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
    if (!node_should_be_changed_or_added(in_scope, node)) {
        if (!is_already_resolved_symbol(in_scope, node)) {
            return true;
        }

        log_error_token(STR("Function was already declared"), compiler->scanner, node->token, 0);
        print_previous_declaration(compiler, node, in_scope);

        return false;
    }

    scope_entry_t *entry = hashmap_get(&in_scope->scope, node->token.data.symbol);

    entry->resolved = true;
    entry->type     = SCOPE_FUNC;

    entry->node_index = node->self_index;

    ast_node_t *type = area_get(&compiler->parser->nodes, node->left_index);

    entry->func.argc = area_get(&compiler->parser->nodes, type->left_index)->child_count;
    entry->func.retc = area_get(&compiler->parser->nodes,type->right_index)->child_count;

    if (entry->func.retc > MAX_COUNT_OF_RETS) {
        log_error_token(STR("multiple return values are not supported currently."), compiler->scanner, node->token, 0);
        return false;
    }

    if (entry->func.argc > MAX_COUNT_OF_PARAMS) {
        log_error_token(STR("too much of parameters in function."), compiler->scanner, node->token, 0);
        return false;
    }

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

            break;
        case SUBTYPE_AST_FUNC_TYPE:
            return analyze_function(compiler, node, global_scope);
            break;
        case SUBTYPE_AST_UNKN_TYPE:
            // proto struct enum union etc
            break;
        default:
            return false;
    }
    
	return false;
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


b32 analyze_code(compiler_t *compiler) {
    parser_t   *parser   = compiler->parser;
    analyzer_t *analyzer = compiler->analyzer;

    check(area_create(&analyzer->scopes, 100));

    scope_tuple_t global_scope = {};

    global_scope.is_global    = true;
    global_scope.parent_scope = 0;

    check(hashmap_create(&global_scope.scope, &compiler->scanner->symbols, 100));


    b32 valid = true;
    for (u64 i = 0; i < parser->root_indices.count; i++) {
        ast_node_t* root = area_get(&parser->nodes, *area_get(&parser->root_indices, i));

        if (!analyze_root_statement(compiler, root, &global_scope)) {
            valid = false;
        }
    }

    if (!valid) return false;
    log_info(STR("PASS2"), 0);

    for (u64 i = 0; i < parser->root_indices.count; i++) {
        ast_node_t* root = area_get(&parser->nodes, *area_get(&parser->root_indices, i));

        (void)analyze_root_statement(compiler, root, &global_scope);
    }

    return false;
}
