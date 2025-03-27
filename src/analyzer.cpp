#include "compiler.h"
#include "analyzer.h"
#include "parser.h"
#include "scanner.h"
#include "logger.h"
#include "list.h"
#include "hashmap.h"

// plan @todo:
//
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

// helpers
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

b32 get_entry_if_exists(compiler_t *state, string_t key, scope_entry_t **output) {
    UNUSED(output);

    scope_t *scope = get_global_scope(state);

    if (hashmap_contains(&scope->table, key)) {
        *output = hashmap_get(&scope->table, key);
        return true;
    }

    return false;
}

// funcs
b32 scan_struct_def(compiler_t *state, ast_node_t *node) {
    // why not add new scope and create all of the data anyways???
    assert(node->type == AST_STRUCT_DEF);

    scope_entry_t entry = {};
    string_t key = node->token.data.string;

    entry.type = ENTRY_TYPE;
    entry.node = node;

    return add_symbol_to_global_scope(state, key, &entry);
}

b32 scan_prototype_def(compiler_t *state, ast_node_t *node) {
    assert(node->type == AST_UNARY_PROTO_DEF);

    scope_entry_t entry = {};
    string_t key = node->token.data.string;

    entry.type = ENTRY_PROTOTYPE;
    entry.node = node;

    return add_symbol_to_global_scope(state, key, &entry);
}

b32 scan_unary_var_def(compiler_t *state, ast_node_t *node) {
    scope_t *scope = get_global_scope(state);
    
    string_t key = node->token.data.string;
    scope_entry_t entry = {};

    if (node->left->type == AST_UNKN_TYPE) {
        string_t type_key = node->left->token.data.string;

        scope_entry_t *type;

        if (get_entry_if_exists(state, type_key, &type)) {
            switch (type->type) {
                case ENTRY_UNKN:
                    entry.not_resolved_type = true;
                    break;

                case ENTRY_TYPE:
                    break;

                case ENTRY_PROTOTYPE:
                    {
                    log_error_token(STR("Couldn't create prototype realization without body."), state->scanner, node->token, 0);
                    
                    log_push_color(INFO_COLOR);
                    string_t decorated_name = string_temp_concat(string_temp_concat(STRING("---- '"), key), STRING("' "));
                    string_t info = string_temp_concat(decorated_name, STRING("is defined as a prototype of:\n\0"));

                    log_print(info);
                    log_pop_color();

                    log_push_color(255, 255, 255);
                    print_lines_of_code(state->scanner, 2, 1, type->node->token, 0);
                    log_pop_color();
                    } return false;

                case ENTRY_FUNC:
                    {
                    log_error_token(STR("Couldn't create variable that uses function name."), state->scanner, node->token, 0);
                    
                    log_push_color(INFO_COLOR);
                    string_t decorated_name = string_temp_concat(string_temp_concat(STRING("'"), type_key), STRING("' "));
                    string_t info = string_temp_concat(decorated_name, STRING("is defined as a function here:\n\0"));

                    log_print(info);
                    log_pop_color();
                    log_push_color(255, 255, 255);
                    print_lines_of_code(state->scanner, 2, 1, type->node->token, 0);
                    log_pop_color();
                    } return false;

                    break;

                default:
                    log_error(STR("Unexpected type..."));
                    return false;
            }
        } else {
            entry.not_resolved_type = true;
        }

    } else {
        // AST_STD_TYPE

        // here we will set std type or will try
    }

    entry.type = ENTRY_VAR;
    entry.node = node;

    hashmap_add(&scope->table, key, &entry);

    return true;
}

b32 scan_var_defs(compiler_t *state, ast_node_t *node) {
    ast_node_t * next = node->left->list_start;

    // @todo entry.not_resolved_type = true; here
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
        entry.not_resolved_type = true;
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

b32 scan_node(compiler_t *state, ast_node_t *node) {
    switch (node->type) {
        case AST_STRUCT_DEF:
            return scan_struct_def(state, node);
            break;

        case AST_UNION_DEF:
            break;

        case AST_ENUM_DEF:
            break;

        case AST_UNARY_PROTO_DEF: 
            return scan_prototype_def(state, node);
            break;

        case AST_UNARY_VAR_DEF:
            return scan_unary_var_def(state, node);
            break;

        case AST_BIN_MULT_DEF:
            break;
        case AST_TERN_MULT_DEF:
            return scan_var_defs(state, node);
            break;

        case AST_BIN_UNKN_DEF:
            return scan_unkn_def(state, node);
            break;

        default:
            log_error_token(STR("Wrong type of construct in global scope."), state->scanner, node->token, 0);
            break;
    }

    return false;
}

// first thing we do is add all of symbols in table
// then we can set 'ast_node_t.analyzed' to 'true'
// and then we can already analyze the code

b32 analyze_code(compiler_t *state) {
    b32 result = true;

    for (u64 i = 0; i < state->parser->parsed_roots.count; i++) {
        ast_node_t *node = *list_get(&state->parser->parsed_roots, i);
        assert(node->type != AST_ERROR);

        if (!scan_node(state, node)) 
            result = false;
    }

    for (u64 i = 0; i < state->analyzer->scopes.data[0].table.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = state->analyzer->scopes.data[0].table.entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted) continue;

        scope_entry_t entry = pair.value;

        if (!entry.not_resolved_type)
            continue;

        if (!hashmap_remove(&state->analyzer->scopes.data[0].table, pair.key)) 
            assert(false);

        if (!scan_node(state, entry.node)) 
            result = false;
    }

    /*
    for (u64 i = 0; i < state->parser->parsed_roots.count; i++) {
        ast_node_t *node = *list_get(&state->parser->parsed_roots, i);
    }
    */

    for (u64 i = 0; i < state->analyzer->scopes.data[0].table.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = state->analyzer->scopes.data[0].table.entries[i];
        if (!pair.occupied) continue;
        if (pair.deleted) continue;

        log_update_color();
        fprintf(stderr, "%llu -> %.*s\n", i, (int)pair.key.size, pair.key.data);
    }

    return result;
}
