#include "analyzer.h"

#include "stack.h"
#include "logger.h"
#include "compiler.h"
#include "scanner.h"
#include "parser.h"

#include "talloc.h"
#include "strings.h"

// @todo @fix ... in parser check if all nodes in separated expression are just identifiers (AST_PRIMARY).
// caching the tokens or just changing them all to use temp alloc
// so in multiple definitions we getting multiple errors that are for one entry_type
// but it happens like this:
//    a, a : s32 = 123;

struct analyzer_state_t {
    scanner_t  *scanner;
    compiler_t *compiler;
    stack_t<hashmap_t<string_t, scope_entry_t>*> *scopes;

    stack_t<string_t>                      *local_deps;
    hashmap_t<string_t, stack_t<string_t>> *type_deps;
};

b32 analyze_function(analyzer_state_t *state, scope_entry_t *entry, b32 *should_wait);

// ------------ helpers

hashmap_t<string_t, scope_entry_t> create_scope(void) {
    hashmap_t<string_t, scope_entry_t> scope = {};
    scope.hash_func    = get_string_hash;
    scope.compare_func = compare_string_keys;
    return scope;
}

void report_already_used(analyzer_state_t *state, string_t key, ast_node_t *already_used_node) {
    string_t decorated_name = string_temp_concat(string_temp_concat(STRING("The identifier '"), key), STRING("' "));
    string_t info = string_temp_concat(decorated_name, STRING("is already used before.\n\0"));
    log_error_token((u8*)string_to_c_string(info, get_temporary_allocator()), state->scanner, already_used_node->token, 0);
}

b32 check_if_unique(scope_entry_t *entry, ast_node_t *new_node) {
    if (entry->node == NULL) return true;

    if (mem_compare((u8*)entry->node, (u8*)new_node, sizeof(ast_node_t)))
        return false;

    return true;
}

void add_blank_entry(hashmap_t<string_t, scope_entry_t> *scope, string_t key, scope_entry_t **output) {
    assert(output != NULL);
    assert(scope  != NULL);

    scope_entry_t entry = {};

    entry.func_params.hash_func    = get_string_hash;
    entry.func_params.compare_func = compare_string_keys;

    if (!hashmap_add(scope, key, &entry))
        assert(false);

    *output = hashmap_get(scope, key);
    assert(*output != NULL);
}

b32 aquire_entry(hashmap_t<string_t, scope_entry_t> *scope, string_t key, ast_node_t *node, scope_entry_t **output) {
    UNUSED(output);

    if (hashmap_contains(scope, key)) {
        scope_entry_t *entry = hashmap_get(scope, key);

        if (!check_if_unique(entry, node)) {
            return false;
        }

        *output = entry;

        return true;
    }

    add_blank_entry(scope, key, output);
    return true;
}

b32 check_dependencies(analyzer_state_t *state, scope_entry_t *entry, string_t key) {
    stack_t<string_t> *deps = hashmap_get(state->type_deps, key);

        // @cleanup @speed @todo: doesnt work for pointers...
    for (u64 i = 0; i < state->local_deps->index; i++) {
        if (string_compare(key, state->local_deps->data[i])) {
            log_error_token(STR("Identifier can not be resolved because it's definition is recursive."), state->scanner, entry->node->token, 0);
            return false;
        }

        for (u64 j = 0; j < deps->index; j++) {
            if (string_compare(state->local_deps->data[i], deps->data[j])) {
                log_error_token(STR("Identifier can not be resolved because type definition is recursive."), state->scanner, entry->node->token, 0);
                return false;
            }
        }
    }

    return true;
}

b32 get_if_exists(analyzer_state_t *state, b32 is_pointer, string_t key, b32 *failed, scope_entry_t **output) {
    UNUSED(output);

    // scan up to global scope

    for (u64 i = 0; i < state->scopes->index; i++) {
        if (!hashmap_contains(state->scopes->data[i], key))
            continue;

        scope_entry_t *entry = hashmap_get(state->scopes->data[i], key);

        if (!is_pointer && !check_dependencies(state, entry, key)) {
            *failed = true;
            return false;
        }

        if (is_pointer && !entry->node->analyzed) {
            *output = entry;
            return true;
        }
        
        if (!entry->node->analyzed) {
            return false;
        }

        *output = entry;
        return true;
    }

    return false;
}

// -----------------


// TODO:
/*

    1. обработчик типов в analyze_definition 
    который будет добавлять данные в scope_entry

    2. анализ выражений и типов

    3. compile time вычисление

    4. тайпчек

    5. компиляция

*/

void set_std_info(token_t token, var_type_info_t *info) {
    info->is_std = true;

    switch (token.type) {
        case TOK_S8:
            info->std.size = 1;
            break;
        case TOK_S16:
            info->std.size = 2;
            break;
        case TOK_S32:
            info->std.size = 4;
            break;
        case TOK_S64:
            info->std.size = 8;
            break;

        case TOK_U8:
            info->std.is_unsigned = true;
            info->std.size = 1;
            break;
        case TOK_U16:
            info->std.is_unsigned = true;
            info->std.size = 2;
            break;
        case TOK_U32:
            info->std.is_unsigned = true;
            info->std.size = 4;
            break;
        case TOK_U64:
            info->std.is_unsigned = true;
            info->std.size = 8;
            break;

        case TOK_BOOL8:
            info->std.is_boolean = true;
            info->std.size = 1;
            break;

        case TOK_BOOL32:
            info->std.is_boolean = true;
            info->std.size = 4;
            break;

        case TOK_VOID:
            info->std.size = 0;
            break;

        case TOK_F32:
            info->std.is_float = true;
            info->std.size = 4;
            break;

        case TOK_F64:
            info->std.is_float = true;
            info->std.size = 8;
            break;

        default:
            assert(false);
            break;
    }
}

void set_std_info(token_t token, scope_entry_t *entry) {
    entry->is_std = true;

    switch (token.type) {
        case TOK_S8:
            entry->std.size = 1;
            break;
        case TOK_S16:
            entry->std.size = 2;
            break;
        case TOK_S32:
            entry->std.size = 4;
            break;
        case TOK_S64:
            entry->std.size = 8;
            break;

        case TOK_U8:
            entry->std.is_unsigned = true;
            entry->std.size = 1;
            break;
        case TOK_U16:
            entry->std.is_unsigned = true;
            entry->std.size = 2;
            break;
        case TOK_U32:
            entry->std.is_unsigned = true;
            entry->std.size = 4;
            break;
        case TOK_U64:
            entry->std.is_unsigned = true;
            entry->std.size = 8;
            break;

        case TOK_BOOL8:
            entry->std.is_boolean = true;
            entry->std.size = 1;
            break;

        case TOK_BOOL32:
            entry->std.is_boolean = true;
            entry->std.size = 4;
            break;

        case TOK_VOID:
            entry->std.size = 0;
            break;

        case TOK_F32:
            entry->std.is_float = true;
            entry->std.size = 4;
            break;

        case TOK_F64:
            entry->std.is_float = true;
            entry->std.size = 8;
            break;

        default:
            assert(false);
            break;
    }
}

b32 analyze_definition(analyzer_state_t *state, b32 can_do_func, ast_node_t *name, ast_node_t *type, b32 *should_wait) {
    string_t key = name->token.data.string;

    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->scopes), key, name, &entry)) {
        report_already_used(state, key, name);
        return false;
    }

    if (!entry->configured) {
        entry->node = name;
        entry->configured = true;
    }

    b32 is_pointer = false;
    b32 failed     = false;

    while (type->type == AST_PTR_TYPE) {
        entry->pointer_depth++;
        type = type->left;
        is_pointer = true;
    }

    if (type->type == AST_UNKN_TYPE) {
        string_t type_key = type->token.data.string;

        if (!is_pointer && state->local_deps->index > 0) {
            stack_push(hashmap_get(state->type_deps, stack_peek(state->local_deps)), type_key);
        }

        scope_entry_t *output_type = NULL; 

        if (get_if_exists(state, is_pointer, type_key, &failed, &output_type)) {
            switch (output_type->type) {
                case ENTRY_PROTOTYPE:
                    entry->type = ENTRY_FUNC;
                    entry->from_prototype = true;
                    entry->prototype_name = type_key;
                    break;

                case ENTRY_TYPE:
                    entry->type = ENTRY_VAR;
                    break;

                case ENTRY_FUNC: 
                    log_error(STR("Cant use FUNC as a type of variable [@better_message]"));
                    entry->type = ENTRY_ERROR;
                    entry->node->analyzed = true;
                    return false;

                default:
                    log_error(STR("Unexpected type..."));
                    entry->type = ENTRY_ERROR;
                    entry->node->analyzed = true;
                    return false;
            }

            entry->type_name = type_key;
            entry->node->analyzed = true;
        } else if (failed) {
            return false;
        } else if (!is_pointer) {
            *should_wait = true;
        }
    } else switch (type->type) {
        case AST_FUNC_TYPE: 
        {
            if (!can_do_func) {
                log_error(STR("Cant do functions for multiple var decl..."));
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
            }

            entry->type = ENTRY_FUNC;

            if (!analyze_function(state, entry, should_wait)) {
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
            }
        }
        break;

        case AST_VOID_TYPE: // fallthrough
            if (!is_pointer) {
                log_error_token(STR("Cant make variable with void type..."), state->scanner, type->token, 0);
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
            }
        case AST_STD_TYPE:
            entry->type = ENTRY_VAR;
            entry->node->analyzed = true;
            set_std_info(type->token, entry);
            break;

        case AST_MUL_AUTO:
        case AST_AUTO_TYPE:
            log_error_token(STR("Cant evaluate the types right now..."), state->scanner, type->token, 0);
            entry->type = ENTRY_ERROR;
            entry->node->analyzed = true;
            return false;

        case AST_ARR_TYPE:
            log_error(STR("we dont support arrays right now"));
            entry->type = ENTRY_ERROR;
            entry->node->analyzed = true;
            return false;

        case AST_PTR_TYPE: // because we stripped the pointer in top!
            assert(false);

        default:
            log_error(STR("unexpected type of ast node..."));
            entry->node->analyzed = true;
            return false;
    } 

    return true;
}

b32 analyze_function(analyzer_state_t *state, scope_entry_t *entry, b32 *should_wait) {
    ast_node_t *func = entry->node;
    ast_node_t *type_node  = func->left;

    // name: left is type right is block/expr
    assert(type_node->type  == AST_FUNC_TYPE);

    string_t key = entry->node->token.data.string;
    stack_push(state->scopes, &entry->func_params);
    stack_push(state->local_deps, key);

    {
        stack_t<string_t> type_deps = {};
        hashmap_add(state->type_deps, key, &type_deps);
    }
    
    b32 result = true;
    ast_node_t *next_type = type_node->left->list_start;
    for (u64 i = 0; i < type_node->left->child_count; i++) {
        assert(next_type->type == AST_PARAM_DEF);

        if (!analyze_definition(state, false, next_type, next_type->left, should_wait)) {
            result = false;
        }

        if (*should_wait) {
            break;
        }

        next_type = next_type->list_next;
    }

    next_type = type_node->right->list_start;
    for (u64 i = 0; i < type_node->right->child_count; i++) {
        if (next_type->analyzed) {
            next_type = next_type->list_next;
            continue;
        }

        var_type_info_t info = {};

        b32 is_pointer = false;
        b32 failed     = false;

        ast_node_t *current_type = next_type;


        while (current_type->type == AST_PTR_TYPE) {
            info.pointer_depth++;
            current_type = current_type->left;
            is_pointer = true;
        }

        if (current_type->type == AST_UNKN_TYPE) {
            string_t type_key = current_type->token.data.string;

            if (!is_pointer && state->local_deps->index > 0) {
                stack_push(hashmap_get(state->type_deps, stack_peek(state->local_deps)), type_key);
            }

            scope_entry_t *output_type = NULL; 

            if (get_if_exists(state, is_pointer, type_key, &failed, &output_type)) {
                switch (output_type->type) {
                    case ENTRY_PROTOTYPE:
                    case ENTRY_TYPE:
                        break;

                    case ENTRY_FUNC: 
                        log_error_token(STR("Cant use FUNC as a return type [@better_message]"), state->scanner, current_type->token, 0);
                        entry->type = ENTRY_ERROR;
                        result = false;
                        break;

                    default:
                        log_error_token(STR("Unexpected type... [@better_message]"), state->scanner, current_type->token, 0);
                        entry->type = ENTRY_ERROR;
                        result = false;
                        break;
                }

                info.type_name = type_key;
            } else if (failed) {
                log_error_token(STR("Failed when accessing name of type"), state->scanner, current_type->token, 0);
                result = false;
                break;
            } else if (!is_pointer) {
                *should_wait = true;
            }
        } else switch (current_type->type) {
            case AST_VOID_TYPE: // fallthrough
                if (!is_pointer) {
                    log_error_token(STR("Cant return void."), state->scanner, current_type->token, 0);
                    entry->type = ENTRY_ERROR;
                    result = false;
                    break;
                }
            case AST_STD_TYPE:
                set_std_info(current_type->token, &info);
                break;

            case AST_ARR_TYPE:
                log_error(STR("we dont support arrays right now"));
                entry->type = ENTRY_ERROR;
                result = false;
                break;

            case AST_MUL_AUTO:
            case AST_AUTO_TYPE:
            case AST_PTR_TYPE: 
                assert(false);

            default:
                log_error(STR("unexpected type of ast node..."));
                result = false;
                break;
        } 

        if (!result || *should_wait)
            break;

        list_add(&entry->return_typenames, &info);
        next_type->analyzed = true;
        next_type = next_type->list_next;
    }


    stack_pop(state->local_deps);
    stack_pop(state->scopes);

    if (!*should_wait) {
        entry->node->analyzed = true;
    }

    return result;
}

b32 analyze_unary_var_def(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_UNARY_VAR_DEF);
    assert(state       != NULL);
    assert(node        != NULL);
    assert(should_wait != NULL);

    if (!analyze_definition(state, false, node, node->left, should_wait)) {
        return false;
    }

    if (*should_wait) {
        return true;
    }

    return true;
}

b32 analyze_enum_decl(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_ENUM_DECL);
    assert(state       != NULL);
    assert(node        != NULL);
    assert(should_wait != NULL);

    *should_wait = false;

    string_t       key   = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node = node;
        entry->type = ENTRY_VAR;
        entry->configured = true;

    }

    // node->left // expression
    
    node->analyzed = true;
    return true;
}


b32 analyze_bin_var_def(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_BIN_MULT_DEF);
    assert(state != NULL);
    assert(node  != NULL);
    assert(should_wait != NULL);

    ast_node_t *type_node = node->right;

    if (type_node->type != AST_MUL_TYPES) {
        ast_node_t * next = node->left->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, next, node->right, should_wait)) {
                return false;
            }

            if (*should_wait) {
                return true;
            }

            next = next->list_next;
        }

        node->analyzed = true;
        return true;
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
        if (!analyze_definition(state, false, name, type, should_wait)) {
            return false;
        }

        if (*should_wait) {
            return true;
        }

        name = name->list_next;
        type = type->list_next;
    }

    node->analyzed = true;

    return true;
} 

b32 analyze_unkn_def(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_BIN_UNKN_DEF);
    assert(state != NULL);
    assert(node  != NULL);
    assert(should_wait != NULL);


    if (!analyze_definition(state, true, node, node->left, should_wait)) {
        return false;
    }

    if (*should_wait) {
        return true;
    }

    return true;
}

b32 analyze_tern_def(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_TERN_MULT_DEF);
    assert(state != NULL);
    assert(node  != NULL);
    assert(should_wait != NULL);

    ast_node_t *type_node = node->center;

    if (type_node->type != AST_MUL_TYPES) {
        ast_node_t * next = node->left->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, next, node->center, should_wait)) {
                return false;
            }

            if (*should_wait) {
                return true;
            }

            next = next->list_next;
        }

        node->analyzed = true;
        return true;
    }

    if (node->left->child_count != node->center->child_count) {
        ast_node_t * next = node->left->list_start;

        while (next->list_next != NULL) {
            next = next->list_next;
        }

        log_error_token(STR("This variable didn't have it's own type:"), state->scanner, next->token, 0);
        return false;
    }

    ast_node_t * name = node->left->list_start;
    ast_node_t * type = node->center->list_start;

    for (u64 i = 0; i < node->left->child_count; i++) {
        if (!analyze_definition(state, false, name, type, should_wait)) {
            return false;
        }

        if (*should_wait) {
            return true;
        }


        name = name->list_next;
        type = type->list_next;
    }

    if (node->right->type != AST_SEPARATION) {
        // one expression for all
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

    node->analyzed = true;
    return true;
}

b32 analyze_and_add_type_members(analyzer_state_t *state, b32 *should_wait, scope_entry_t *entry) {
    ast_node_t * block = entry->node->left;
    ast_node_t * next  = block->list_start;

    b32 result      = true;

    for (u64 i = 0; i < block->child_count; i++) {
        switch (next->type) {
            case AST_UNARY_VAR_DEF:
                if (!analyze_unary_var_def(state, next, should_wait)) {
                    result = false;
                } break;
            case AST_BIN_MULT_DEF:  
                if (!analyze_bin_var_def(state, next, should_wait)) {
                    result = false;
                } break;

                // We can find them only in enum_block
            case AST_ENUM_DECL: 
                assert(block->type == AST_BLOCK_ENUM);
                if (!analyze_enum_decl(state, next, should_wait)) {
                    result = false;
                } break;
            case AST_BIN_UNKN_DEF:
            case AST_TERN_MULT_DEF:
                log_error_token(STR("Cant use initialization in member declaration."), state->scanner, entry->node->token, 0);
                result = false;
                break;

            default:
                log_error_token(STR("Cant use this as member of type."), state->scanner, entry->node->token, 0);
                result = false;
                break;

        }

        if (*should_wait) {
            return result;
        }

        next = next->list_next;
    }

    entry->node->analyzed = true;
    return result;
}

b32 analyze_prototype(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_UNARY_PROTO_DEF);
    if (node->analyzed) return true; 

    string_t key = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node = node;
        entry->type = ENTRY_PROTOTYPE;
        entry->configured = true;
    }

    b32 result = false;
    b32 should_wait = false;

    result = analyze_function(state, entry, &should_wait);

    if (result && !should_wait) {
        node->analyzed = true;
    }

    return result;
}

b32 analyze_struct(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_STRUCT_DEF);
    if (node->analyzed) return true; 

    string_t key = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node = node;
        entry->type = ENTRY_TYPE;
        entry->scope = create_scope();
        entry->configured = true;
    }

    b32 result = false;
    b32 should_wait = false;

    stack_push(state->scopes, &entry->scope);
    stack_push(state->local_deps, key);
    
    {
        stack_t<string_t> type_deps = {};
        hashmap_add(state->type_deps, key, &type_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    stack_pop(state->local_deps);
    stack_pop(state->scopes);

    if (!should_wait) {
        node->analyzed = true;
    }

    return result;
}

b32 analyze_union(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_UNION_DEF);
    if (node->analyzed) return true; 

    string_t key = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node  = node;
        entry->type  = ENTRY_TYPE;
        entry->scope = create_scope();
        entry->configured = true;
    }

    b32 result = false;
    b32 should_wait = false;

    stack_push(state->scopes, &entry->scope);
    stack_push(state->local_deps, key);

    {
        stack_t<string_t> type_deps = {};
        hashmap_add(state->type_deps, key, &type_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    stack_pop(state->local_deps);
    stack_pop(state->scopes);

    if (!should_wait) {
        node->analyzed = true;
    }

    return result;
}

b32 analyze_enum(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_ENUM_DEF);
    if (node->analyzed) return true; 

    string_t key = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node  = node;
        entry->type  = ENTRY_TYPE;
        entry->scope = create_scope();
        entry->configured = true;
    }

    b32 result = false;
    b32 should_wait = false;

    stack_push(state->scopes, &entry->scope);
    stack_push(state->local_deps, key);

    {
        stack_t<string_t> type_deps = {};
        hashmap_add(state->type_deps, key, &type_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    for (u64 i = 0; i < entry->scope.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> *pair = entry->scope.entries + i;

        if (!pair->occupied) continue;
        if (pair->deleted)   continue;

        set_std_info(node->right->token, &pair->value);
    }

    stack_pop(state->local_deps);
    stack_pop(state->scopes);

    if (!should_wait) {
        node->analyzed = true;
    }

    return result;
}


#define GREEN_COLOR 63, 255, 63

// --------------------------------------------------- FILE LOADING


string_t construct_source_name(string_t path, string_t name, allocator_t *alloc) {
    return string_swap(string_concat(path, string_concat(name, STRING(".slm"), alloc), alloc), SWAP_SLASH, (u8)HOST_SYSTEM_SLASH, alloc);
}

string_t construct_module_name(string_t path, string_t name, allocator_t *alloc) {
    return string_swap(string_concat(path, string_concat(name, STRING("/module.slm"), alloc), alloc), SWAP_SLASH, (u8)HOST_SYSTEM_SLASH, alloc);
}

// @cleanup fopen is not seems good
b32 is_file_exists(string_t name) {
    FILE *file = fopen(string_to_c_string(name, get_temporary_allocator()), "rb");

    if (file == NULL) return false;

    fclose(file);
    return true;
}

b32 load_and_process_file(compiler_t *compiler, string_t filename) {
    source_file_t file = create_source_file(compiler, NULL);

    string_t source;

    if (!read_file_into_string(filename, default_allocator, &source)) {
        return false;
    }

    if (!scanner_open(&filename, &source, file.scanner)) {
        return false;
    }

    if (!hashmap_add(&compiler->files, filename, &file)) {
        log_error(STR("Couldn't add file to work with."));
        return false;
    }

    if (!parse_file(compiler, filename)) {
        return false;
    }

    return true;
}

b32 add_file_if_exists(compiler_t *compiler, b32 *valid_file, string_t file) {

    // @cleanup
#ifdef DEBUG
    log_write(STR("TRY: "));
    log_print(file);
#endif

    if (!is_file_exists(file)) { 
#ifdef DEBUG
        log_push_color(ERROR_COLOR);
        log_write(STR(" FAIL\n"));
        log_pop_color();
#endif
        return false;
    } else { 
#ifdef DEBUG
        log_push_color(GREEN_COLOR);
        log_write(STR(" OK\n"));
        log_pop_color();
#endif

        *valid_file = load_and_process_file(compiler, string_copy(file, compiler->strings));
    }

    return true;
}

b32 find_and_add_file(compiler_t *compiler, analyzer_state_t *state, ast_node_t *node) {
    allocator_t *talloc = get_temporary_allocator();

    // ./<file>.slm
    // ./<file>/module.slm
    // /<compiler>/<file>.slm
    // /<compiler>/<file>/module.slm

    string_t directory, name = node->token.data.string;

    s64 slash = index_of_last_file_slash(state->scanner->filename);
    if (slash == -1) {
        directory = string_copy(STRING("./"), talloc);
    } else {
        directory = string_substring(state->scanner->filename, 0, slash + 1, talloc);
    }

    b32 valid_file;

    if (add_file_if_exists(compiler, &valid_file, construct_source_name(directory, name, talloc))) {
        node->analyzed = true;
        return valid_file;
    }

    if (add_file_if_exists(compiler, &valid_file, construct_module_name(directory, name, talloc))) {
        node->analyzed = true;
        return valid_file;
    }

    if (add_file_if_exists(compiler, &valid_file, construct_source_name(compiler->modules_path, name, talloc))) {
        node->analyzed = true;
        return valid_file;
    }

    if (add_file_if_exists(compiler, &valid_file, construct_module_name(compiler->modules_path, name, talloc))) {
        node->analyzed = true;
        return valid_file;
    }

    log_error_token(STR("Couldn't find corresponding file to this declaration."), state->scanner, node->token, 0);
    node->analyzed = true;
    return false;
}

// --------------------

b32 analyze_statement(compiler_t *compiler, analyzer_state_t *state, ast_node_t *node) {
    temp_reset();

    assert(!node->analyzed);
    b32 should_wait = false;

    switch (node->type) {
        case AST_UNNAMED_MODULE:
                return find_and_add_file(compiler, state, node);

        case AST_STRUCT_DEF: 
                return analyze_struct(state, node);

        case AST_UNION_DEF:
                return analyze_union(state, node);

        case AST_ENUM_DEF: 
                return analyze_enum(state, node);

        case AST_UNARY_PROTO_DEF: 
                return analyze_prototype(state, node);


        case AST_UNARY_VAR_DEF:
                return analyze_unary_var_def(state, node, &should_wait);
                
        case AST_BIN_UNKN_DEF: 
                return analyze_unkn_def(state, node, &should_wait);

        case AST_BIN_MULT_DEF: 
                return analyze_bin_var_def(state, node, &should_wait);

        case AST_TERN_MULT_DEF:
                return analyze_tern_def(state, node, &should_wait);

                             /*
        case AST_NAMED_MODULE:
            // ENTRY_NAMESPACE
            log_error_token(STR("Named modules dont work right now."), state->scanner, node->token, 0);
            node->analyzed = true;
            break;
                             */

        default:
            log_error_token(STR("Wrong type of construct."), state->scanner, node->token, 0);
            break;
    }

    return false;
}

// priorities of compiling funcitons
//
// #run 
// other code
//
void print_var_info(var_type_info_t info) {
    if (info.pointer_depth) {
        if (info.pointer_depth == 1) {
            fprintf(stderr, " pointer");
        } else {
            fprintf(stderr, " pointer(%u)", info.pointer_depth);
        }
    }

    if (info.is_std) {
        fprintf(stderr, " (%s, %s%s%u bytes)", info.std.is_unsigned ? "unsigned" : "signed", info.std.is_boolean ? "boolean, " : "", info.std.is_float ? "float, " : "", info.std.size);
    } else {
        fprintf(stderr, " %s", string_to_c_string(info.type_name, get_temporary_allocator()));
    }
}

var_type_info_t entry_to_info(scope_entry_t e) {
    var_type_info_t i = {};

    i.std.is_unsigned = e.std.is_unsigned;
    i.std.is_boolean = e.std.is_boolean;
    i.pointer_depth = e.pointer_depth;
    i.std.is_float = e.std.is_float;
    i.type_name = e.type_name;
    i.std.size = e.std.size;
    i.is_std = e.is_std;

    return i;
}

void print_entry(compiler_t *compiler, kv_pair_t<string_t, scope_entry_t> pair, u64 depth) {
    allocator_t *talloc = get_temporary_allocator();

    add_left_pad(stderr, depth * 4);
    fprintf(stderr, " -> %s", string_to_c_string(pair.key, talloc));

    if (pair.value.type == ENTRY_VAR) {
        fprintf(stderr, " VAR");
        print_var_info(entry_to_info(pair.value));
    } else if (pair.value.type == ENTRY_TYPE) {
        fprintf(stderr, " TYPE");
    } else if (pair.value.type == ENTRY_FUNC) {
        fprintf(stderr, " FUNC");
    } else if (pair.value.type == ENTRY_PROTOTYPE) {
        fprintf(stderr, " PROTO");
    }

    fprintf(stderr, "\n");

    if (pair.value.type == ENTRY_FUNC && pair.value.from_prototype) {
        add_left_pad(stderr, (depth + 1) * 4);
        fprintf(stderr, " -> from prototype: %s\n", string_to_c_string(pair.value.prototype_name, talloc));
        return;
    }

    for (u64 j = 0; j < pair.value.scope.capacity; j++) {
        kv_pair_t<string_t, scope_entry_t> member = pair.value.scope.entries[j];

        if (!member.occupied) continue;
        if (member.deleted)   continue;

        print_entry(compiler, member, depth + 1);
    }

    if (pair.value.type == ENTRY_VAR)
        return;

    if (pair.value.type == ENTRY_TYPE)
        return;

    for (u64 j = 0; j < pair.value.return_typenames.count; j++) {
        var_type_info_t info = pair.value.return_typenames.data[j];
        add_left_pad(stderr, (depth + 1) * 4);
        fprintf(stderr, " -> RET VAL %llu", j);
        print_var_info(info);
        fprintf(stderr, "\n");
    }
}

b32 analyze_and_compile(compiler_t *compiler) {
    assert(compiler != NULL);

    b32 result       = true;
    b32 not_finished = true;

    stack_t<hashmap_t<string_t, scope_entry_t>*> scopes = {};

    stack_t<string_t> local_deps = {};
    stack_create(&local_deps, 16);

    hashmap_t<string_t, stack_t<string_t>> type_deps = {};
    hashmap_create(&type_deps, 128, get_string_hash, compare_string_keys);

    stack_push(&scopes, &compiler->scope);


    // @todo @fix, change so we wont have infinite loop on undefined declarations
    u64 max_iterations = 10;

    // we analyze and typecheck and prepare to compile this
    u64 curr_index = 0;
    while (max_iterations-- > 0 && not_finished) {
        not_finished = false;

#ifdef DEBUG
        log_push_color(GREEN_COLOR);
        log_update_color();
        fprintf(stderr, "step: %zu\n", curr_index);
        log_pop_color();
#endif
        curr_index++;

        for (u64 i = 0; i < compiler->files.capacity; i++) {
            kv_pair_t<string_t, source_file_t> pair = compiler->files.entries[i];

            if (!pair.occupied) continue;
            if (pair.deleted)   continue;

            analyzer_state_t state = {};
            state.scanner  = pair.value.scanner;
            state.compiler = compiler;
            state.scopes   = &scopes;
            
            state.local_deps = &local_deps;
            state.type_deps = &type_deps;

            // we just need to try to compile, if we cant resolve something we add it, but just
            for (u64 j = 0; j < pair.value.parsed_roots.count; j++) {
                ast_node_t *node = *list_get(&pair.value.parsed_roots, j);

                if (node->analyzed) {
                    continue;
                } 

                not_finished = true;

                if (!analyze_statement(compiler, &state, node)) {
                    result = false;
                }
            }

            if (!result) {
                not_finished = false;
                break;
            }
        }
    }

    if (!result) {
        log_error(STR("Had an analyze error, wont go further."));
        return false;
    } else if (not_finished) { 
        log_error(STR("There is undefined types..."));
        return false;
    } else {
#ifdef DEBUG
        log_info(STR("Everything is okay, compiling."));
#endif
    }

    hashmap_delete(&type_deps);
    stack_delete(&scopes);

    log_update_color();
    fprintf(stderr, "--------GLOBAL-SCOPE---------\n");

    for (u64 i = 0; i < compiler->scope.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = compiler->scope.entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        print_entry(compiler, pair, 0);
    }

    fprintf(stderr, "-----------------------------\n");


    return result;
}

// first thing is to add all types into one place and resolve them all
// then we will do same to prototypes and variables
// after that main part. functions and variables
// first thing - we resolve all of type info, before starting going deep
// and then we run resolving for every func body.
//
//
// use: "core";
//
// release_build : b32 = false;
//
// entry : (args : []string) -> s32 = {
// }
//
// #run entry();
//

// then we will make dependency "graph" for compiling, but we need to start somewhere
// 
// for example we start with some definition and try to resolve it's type, 
// if we cant do that, we will go
