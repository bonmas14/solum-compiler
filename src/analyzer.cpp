#include "analyzer.h"

#include "logger.h"
#include "compiler.h"
#include "scanner.h"
#include "parser.h"

// @todo @fix ... in parser check if all nodes in separated expression are just identifiers (AST_PRIMARY). caching the tokens or just changing them all to use temp alloc
// so in multiple definitions we getting multiple errors that are for one entry_type
// but it happens like this:
//    a, a : s32 = 123;

struct analyzer_state_t {
    scanner_t *scanner;
    hashmap_t<string_t, scope_entry_t> *scope;
};

b32 add_symbol_to_global_scope(analyzer_state_t *state, string_t key, scope_entry_t *entry) {
    assert(entry != NULL);
    assert(entry->node != NULL);

    if (hashmap_contains(state->scope, key)) {
        scope_entry_t *exists = hashmap_get(state->scope, key);

        log_push_color(INFO_COLOR);
        string_t decorated_name = string_temp_concat(string_temp_concat(STRING("The identifier '"), key), STRING("' "));
        string_t info = string_temp_concat(decorated_name, STRING("is already used before.\n\0"));

        log_error_token(info.data, state->scanner, entry->node->token, 0);

        log_info(STR("Was used here:"));
        log_pop_color();
        log_push_color(255, 255, 255);
        print_lines_of_code(state->scanner, 1, 1, exists->node->token, 0);
        log_pop_color();

        return false;
    }

    if (!hashmap_add(state->scope, key, entry)) {
        log_error(STR("Couldn't reserve symbol..."));
        assert(false);
    }

    return true;
}

b32 get_entry_if_exists(analyzer_state_t *state, string_t key, scope_entry_t **output) {
    UNUSED(output);

    if (hashmap_contains(state->scope, key)) {
        *output = hashmap_get(state->scope, key);
        return true;
    }

    return false;
}

// funcs
b32 scan_struct_def(analyzer_state_t *state, ast_node_t *node) {
    // why not add new state->current_scope and create all of the data anyways???
    // because we will do it later, we just need the SYMBOL
    //
    // WE WILL MAKE IT A COMPILE UNIT AFTER, IDIOT,
    // THEN WE WILL TYPE CHECK AND FINALIZE ALL TYPES
    assert(node->type == AST_STRUCT_DEF);

    scope_entry_t entry = {};
    string_t key = node->token.data.string;

    entry.entry_type = ENTRY_TYPE;
    entry.node = node;
    
    return add_symbol_to_global_scope(state, key, &entry);
}

b32 scan_prototype_def(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_UNARY_PROTO_DEF);

    scope_entry_t entry = {};
    string_t key = node->token.data.string;

    entry.entry_type = ENTRY_PROTOTYPE;
    entry.node = node;

    return add_symbol_to_global_scope(state, key, &entry);
}

b32 scan_unary_var_def(analyzer_state_t *state, ast_node_t *node) {
    string_t key = node->token.data.string;
    scope_entry_t entry = {};

    if (node->left->type == AST_UNKN_TYPE) {
        string_t type_key = node->left->token.data.string;

        scope_entry_t *type;

        if (get_entry_if_exists(state, type_key, &type)) {
            switch (type->entry_type) {
                case ENTRY_UNKN:
                    entry.not_resolved_type = true;
                    break;

                case ENTRY_TYPE:
                    break;

                case ENTRY_PROTOTYPE: {
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

                case ENTRY_FUNC: {
                    log_error_token(STR("Couldn't create variable with function type."), state->scanner, node->token, 0);
                    
                    log_push_color(INFO_COLOR);
                    string_t decorated_name = string_temp_concat(string_temp_concat(STRING("---- '"), type_key), STRING("' "));
                    string_t info = string_temp_concat(decorated_name, STRING("is defined as a function here:\n\0"));

                    log_print(info);
                    log_pop_color();

                    log_push_color(255, 255, 255);
                    print_lines_of_code(state->scanner, 2, 1, type->node->token, 0);
                    log_pop_color();
                } return false;

                default:
                    log_error(STR("Unexpected type..."));
                    return false;
            }
        } else {
            entry.not_resolved_type = true;
        }
    }

    entry.entry_type = ENTRY_VAR;
    entry.node = node;

    return add_symbol_to_global_scope(state, key, &entry);
}

b32 scan_bin_var_defs(analyzer_state_t *state, ast_node_t *node) {
    assert(state != NULL);
    assert(node  != NULL);
    assert(node->type == AST_BIN_MULT_DEF);

    b32 result = true;
    b32 is_not_resolved = false;

    if ((node->right->type == AST_UNKN_TYPE) || (node->right->type == AST_AUTO_TYPE)) {
        is_not_resolved = true;
    }

    if (node->right->type != AST_MUL_TYPES) {
        ast_node_t * next = node->left->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            scope_entry_t entry = {};
            string_t key = next->token.data.string;

            entry.entry_type = ENTRY_VAR;
            entry.node = node;
            entry.not_resolved_type = is_not_resolved;

            if (!add_symbol_to_global_scope(state, key, &entry)) 
                result = false;

            next = next->list_next;
        }

        return result;
    }

    if (node->left->child_count != node->right->child_count) {
        ast_node_t * next = node->left->list_start;

        while (next->list_next != NULL) {
            next = next->list_next;
        }

        log_error_token(STR("This variable didn't have it's own type:"), state->scanner, next->token, 0);
        return false;
    }

    ast_node_t * name = node->left->list_start;
    ast_node_t * type = node->right->list_start;

    for (u64 i = 0; i < node->left->child_count; i++) {
        scope_entry_t entry = {};
        string_t key = name->token.data.string;

        entry.entry_type = ENTRY_VAR;
        entry.node = node;

        if (type->type == AST_UNKN_TYPE) {
            entry.not_resolved_type = true;
        }

        if (type->type == AST_VOID_TYPE) {
            log_push_color(ERROR_COLOR);
            string_t decorated_name = string_temp_concat(string_temp_concat(STRING("The variable '"), key), STRING("' "));
            string_t info = string_temp_concat(decorated_name, STRING("is defined with void type:\n\0"));
            log_print(info);
            log_pop_color();

            log_push_color(255, 255, 255);
            print_lines_of_code(state->scanner, 0, 0, name->token, 0);
            log_write(STR("\n"));
            log_pop_color();
            result = false;
        }

        if (!add_symbol_to_global_scope(state, key, &entry)) 
            result = false;

        name = name->list_next;
        type = type->list_next;
    }

    return result;
}

b32 scan_tern_var_defs(analyzer_state_t *state, ast_node_t *node) {
    assert(state != NULL);
    assert(node  != NULL);
    assert(node->type == AST_TERN_MULT_DEF);

    b32 result = true;
    b32 is_not_resolved = false;

    if ((node->center->type == AST_UNKN_TYPE) || (node->center->type == AST_AUTO_TYPE)) {
        is_not_resolved = true;
    }

    if (node->center->type != AST_MUL_TYPES) {
        ast_node_t * next = node->left->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            scope_entry_t entry = {};
            string_t key = next->token.data.string;

            entry.entry_type = ENTRY_VAR;
            entry.node = node;
            entry.not_resolved_type = is_not_resolved;

            if (!add_symbol_to_global_scope(state, key, &entry)) 
                result = false;

            next = next->list_next;
        }

        return result;
    }

    if (node->left->child_count != node->center->child_count) {
        ast_node_t * next;
        u64 least_size = MIN(node->left->child_count, node->center->child_count);

        if (least_size >= node->left->child_count) {
            next = node->center->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }
            log_error_token(STR("Too much types in definition. Starting from this one:"), state->scanner, next->token, 0);
        } else {
            next = node->left->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }
            log_error_token(STR("Starting from this variable name, there's no enough types in definition:"), state->scanner, next->token, 0);
        }

        return false;
    }

    ast_node_t * name = node->left->list_start;
    ast_node_t * type = node->center->list_start;

    for (u64 i = 0; i < node->left->child_count; i++) {
        scope_entry_t entry = {};
        string_t key = name->token.data.string;

        entry.entry_type = ENTRY_VAR;
        entry.node = node;

        if (type->type == AST_UNKN_TYPE) {
            entry.not_resolved_type = true;
        }

        if (type->type == AST_VOID_TYPE) {
            log_push_color(ERROR_COLOR);
            string_t decorated_name = string_temp_concat(string_temp_concat(STRING("The variable '"), key), STRING("' "));
            string_t info = string_temp_concat(decorated_name, STRING("is defined with void type:\n\0"));
            log_print(info);
            log_pop_color();

            log_push_color(255, 255, 255);
            print_lines_of_code(state->scanner, 0, 0, name->token, 0);
            log_write(STR("\n"));
            log_pop_color();
            result = false;
        }

        if (!add_symbol_to_global_scope(state, key, &entry)) 
            result = false;

        name = name->list_next;
        type = type->list_next;
    }

    if (node->right->type != AST_SEPARATION) {
        return true;
    }

    if (node->left->child_count != node->right->child_count) {
        ast_node_t * next;
        u64 least_size = MIN(node->left->child_count, node->right->child_count);

        if (least_size >= node->left->child_count) {
            next = node->right->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }

            log_error_token(STR("Trailing expression without its variable:"), state->scanner, next->token, 0);
        } else {
            next = node->left->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }
            log_error_token(STR("This variable didn't have it's expression:"), state->scanner, next->token, 0);
        }


        return false;
    }

    return result;
}

b32 scan_unkn_def(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_BIN_UNKN_DEF);

    scope_entry_t entry = {};
    entry.entry_type = ENTRY_ERROR;
    entry.node = node;

    if (node->left->type == AST_UNKN_TYPE) {
        entry.not_resolved_type = true;
        return add_symbol_to_global_scope(state, node->token.data.string, &entry);
    } 

    switch (node->left->type) {
        case AST_FUNC_TYPE:
            entry.entry_type = ENTRY_FUNC;
            break;

        case AST_AUTO_TYPE:
        case AST_STD_TYPE:
        case AST_ARR_TYPE:
        case AST_PTR_TYPE:
            entry.entry_type = ENTRY_VAR;
            break;

        default:
            log_error(STR("unexpected type of ast node. in scan unkn def"));
            return false;
    } 

    if (entry.entry_type == ENTRY_VAR && node->right->type == AST_BLOCK_IMPERATIVE) {
        log_error_token(STR("Variable assignment should be expression"), state->scanner, node->right->token, 0);
        return false;
    }

    return add_symbol_to_global_scope(state, node->token.data.string, &entry);
}

b32 scan_node(analyzer_state_t *state, ast_node_t *node) {
    switch (node->type) {
        case AST_UNNAMED_MODULE:
            // here we just load and parse the file from string

            break;

        case AST_NAMED_MODULE:
            // here we parse file into local state that will be accessed through that name
            // add_and_parse_file_into_local_scope();
            break;

        case AST_STRUCT_DEF:
            return scan_struct_def(state, node);

        case AST_UNION_DEF:
            break;

        case AST_ENUM_DEF:
            break;

        case AST_UNARY_PROTO_DEF: 
            return scan_prototype_def(state, node);

        case AST_UNARY_VAR_DEF:
            return scan_unary_var_def(state, node);

        case AST_BIN_MULT_DEF:
            return scan_bin_var_defs(state, node);

        case AST_TERN_MULT_DEF:
            return scan_tern_var_defs(state, node);

        case AST_BIN_UNKN_DEF:
            return scan_unkn_def(state, node);

        default:
            log_error_token(STR("Wrong entry_type of construct in global state->current_scope."), state->scanner, node->token, 0);
            break;
    }

    return false;
}

// first thing we do is add all of symbols in table
// then we can set 'ast_node_t.analyzed' to 'true'
// and then we can already analyze the code

void delete_scanned_defs(analyzer_state_t *state, ast_node_t *node) {
    switch (node->type) {
        case AST_BIN_MULT_DEF:
        case AST_TERN_MULT_DEF:
            {
                ast_node_t * next = node->left->list_start;
                for (u64 i = 0; i < node->left->child_count; i++) {
                    hashmap_remove(state->scope, next->token.data.string);
                    next = next->list_next;
                }
            } break;

        default:
            hashmap_remove(state->scope, node->token.data.string);
            break;
    }
}

b32 analyze_file(compiler_t *compiler, string_t filename) {
    source_file_t *file = hashmap_get(&compiler->files, filename);
    assert(file != NULL);

    analyzer_state_t state = {};
    state.scanner = file->scanner;
    state.scope   = &file->scope;

    b32 result = true;

    for (u64 i = 0; i < file->parsed_roots.count; i++) {
        ast_node_t *node = *list_get(&file->parsed_roots, i);
        assert(node->type != AST_ERROR);

        if (!scan_node(&state, node)) 
            result = false;
    }

    for (u64 i = 0; i < state.scope->capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = state.scope->entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        scope_entry_t entry = pair.value;

        if (!entry.not_resolved_type)
            continue;

        delete_scanned_defs(&state, pair.value.node);

        if (!scan_node(&state, entry.node)) 
            result = false;
    }

    return result;
}
