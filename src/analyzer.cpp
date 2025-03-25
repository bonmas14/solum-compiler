#include "compiler.h"
#include "analyzer.h"
#include "parser.h"
#include "scanner.h"
#include "logger.h"
#include "list.h"
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



// It is just check of all top-level symbols only after 
// it resolved we start to generate IR and when we generate IR
// we typecheck. Easy!
//
// So we just need to create global hashtable and after that 
// when we will be generating IR we will generate all of the other things
// like lambdas?

scope_t *get_global_scope(compiler_t *state) {
    assert(state != NULL);
    scope_t *scope = list_get(&state->analyzer->scopes, 0);
    assert(scope->parent_scope == 0);
    return scope;
}

b32 scan_var_defs(compiler_t *state, ast_node_t *node) {

}

b32 scan_unkn_def(compiler_t *state, ast_node_t *node) {
    assert(node->type == AST_BIN_UNKN_DEF);

    scope_t *scope = get_global_scope(state);
    string_t key   = node->token.data.string;

    if (hashmap_contains(&scope->table, key)) {
        log_error_token(STR("Symbol was already used before"), state->scanner, node->token, 0);
        return false;
    } 

    scope_entry_t entry = {};
    entry.node = node;

    if (node->left->type == AST_UNKN_TYPE) {
        entry.type = ENTRY_UNKN;
    } else switch (node->left->type) {
        case AST_FUNC_TYPE:
            entry.type = ENTRY_FUNC;

            if (node->right->type != AST_BLOCK_IMPERATIVE) {
                log_error_token(STR("Function body should be in '{' '}' block"), state->scanner, node->right->token, 0);
                return false;
            }
            break;

        case AST_AUTO_TYPE:
        case AST_STD_TYPE:
        case AST_ARR_TYPE:
        case AST_PTR_TYPE:
            entry.type = ENTRY_VAR;

            if (node->right->type == AST_BLOCK_IMPERATIVE) {
                log_error_token(STR("Variable assignment should be expression"), state->scanner, node->right->token, 0);
                return false;
            }
            break;

        default:
            entry.type = ENTRY_ERROR;
            break;
    } 

    if (!hashmap_add(&scope->table, key, &entry)) {
        log_error(STR("something"));
        assert(false);
    }

    return true;
}

// first thing we do is add all of symbols in table
// then we can set 'ast_node_t.analyzed' to 'true'
// and then we can already analyze the code

b32 analyze_code(compiler_t *state) {
    for (u64 i = 0; i < state->parser->parsed_roots.count; i++) {
        ast_node_t *node = *list_get(&state->parser->parsed_roots, i);

        if (node->analyzed) {
            continue;
        }

        assert(node->type != AST_ERROR);

        b32 result = false;

        switch (node->type) {
            case AST_UNARY_VAR_DEF:
                

            case AST_BIN_MULT_DEF:


            case AST_TERN_MULT_DEF:
                result = scan_var_defs(state, node);
                break;

            case AST_BIN_UNKN_DEF:
                // only here we can find funcitons
                result = scan_unkn_def(state, node);

                break;

            case AST_STRUCT_DEF:
                break;

            case AST_UNION_DEF:
                break;

            case AST_ENUM_DEF:
                break;

            default:
                log_error_token(STR("Wrong type of construct in global scope."), state->scanner, node->token, 0);
                break;
        }
    }

    /*
    for (u64 i = 0; i < state->parser->parsed_roots.count; i++) {
        ast_node_t *node = *list_get(&state->parser->parsed_roots, i);

        if (node->analyzed) continue;
    }
    */

    // process_all_functions here, one pass, doing everything

    return true;
}
