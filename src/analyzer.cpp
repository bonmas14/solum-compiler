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

b32 add_symbol_to_global_scope(compiler_t *state, string_t key, scope_entry_t *entry) {
    scope_t * scope = get_global_scope(state);

    if (hashmap_contains(&scope->table, key)) {
        if (!memcmp(entry, hashmap_get(&scope->table, key), sizeof(scope_entry_t))) {
            return true;
        }

        log_error_token(STR("Symbol already used before."), state->scanner, entry->node->token, 0);
        return false;
    }

    if (!hashmap_add(&scope->table, key, entry)) {
        log_error(STR("Couldn't reserve symbol..."));
        assert(false);
    }

    return true;
}

b32 scan_var_def_unary(compiler_t *state, ast_node_t *node) {
    scope_t *scope = get_global_scope(state);
    
    if (node->left->type == AST_UNKN_TYPE) {
        string_t key = node->token.data.string;
        scope_entry_t entry = {};

        if (!hashmap_contains(&scope->table, key)) {
            entry.not_resolved = true;
        } else {
            scope_entry_t *recieved = hashmap_get(&scope->table, key);

            switch (recieved->type) {
                case ENTRY_FUNC:
                case ENTRY_VAR:
                case ENTRY_ERROR:
                    log_error(STR("Unexpected type..."));
                    return false;

                case ENTRY_PROTOTYPE:
                    log_error(STR("Prototype should have a body."));
                    return false;

                case ENTRY_UNKN:
                    entry.not_resolved = true;
                    break;

                default:
                    break;
            }
        }

        entry.type = ENTRY_VAR;
        entry.node = node;

        hashmap_add(&scope->table, key, &entry);
    }

    return true;
}

b32 scan_var_defs(compiler_t *state, ast_node_t *node) {
    ast_node_t * next = node->left->list_start;

    // @todo entry.not_resolved = true; here
    for (u64 i = 0; i < node->left->child_count; i++) {
        scope_entry_t entry = {};
        entry.type = ENTRY_VAR;
        entry.node = node;
        string_t key = next->token.data.string;
        add_symbol_to_global_scope(state, key, &entry);
        next = next->list_next;
    }

    return true;
}

b32 scan_unkn_def(compiler_t *state, ast_node_t *node) {
    assert(node->type == AST_BIN_UNKN_DEF);

    scope_entry_t entry = {};
    entry.type = ENTRY_ERROR;
    entry.node = node;

    if (node->left->type == AST_UNKN_TYPE) {
        entry.not_resolved = true;
        entry.type = ENTRY_UNKN;
        return add_symbol_to_global_scope(state, node->token.data.string, &entry);
    } 

    switch (node->left->type) {
        case AST_FUNC_TYPE:
            entry.type = ENTRY_FUNC;
            break;

        case AST_AUTO_TYPE:
        case AST_STD_TYPE:
        case AST_ARR_TYPE:
        case AST_PTR_TYPE:
            entry.type = ENTRY_VAR;
            break;

        default:
            log_error(STR("unexpected type of ast node. in scan unkn def"));
            return false;
    } 

    if (entry.type == ENTRY_VAR && node->right->type == AST_BLOCK_IMPERATIVE) {
        log_error_token(STR("Variable assignment should be expression"), state->scanner, node->right->token, 0);
        return false;
    }

    return add_symbol_to_global_scope(state, node->token.data.string, &entry);
}

b32 scan_struct_def(compiler_t *state, ast_node_t *node) {
    return false;
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
                log_info(STR("Analyzing ast_un_var"));
                 result = scan_var_def_unary(state, node);
                break;

            case AST_BIN_MULT_DEF:
                break;
            case AST_TERN_MULT_DEF:
                result = scan_var_defs(state, node);
                break;

            case AST_BIN_UNKN_DEF:
                // only here we can find funcitons
                result = scan_unkn_def(state, node);
                break;

            case AST_STRUCT_DEF:
                result = scan_struct_def(state, node);
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

    for (u64 i = 0; i < state->analyzer->scopes.data[0].table.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = state->analyzer->scopes.data[0].table.entries[i];
        if (!pair.occupied) continue;

        log_update_color();
        fprintf(stderr, "%llu -> %.*s\n", i, pair.key.size, pair.key.data);
    }

    return true;
}
