#include "analyzer.h"

#include "stack.h"
#include "logger.h"
#include "compiler.h"
#include "scanner.h"
#include "parser.h"

#include "talloc.h"
#include "strings.h"


enum {
    GET_NOT_FIND,
    GET_NOT_ANALYZED,
    GET_DEPS_ERROR,
    GET_SUCCESS,
};

enum {
    STMT_ERR,
    STMT_OK,
    
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
};

// @todo @fix ... in parser check if all nodes in separated expression are just identifiers (AST_PRIMARY).
// caching the tokens or just changing them all to use temp alloc
// so in multiple definitions we getting multiple errors that are for one entry_type
// but it happens like this:
//    a, a : s32 = 123;

struct analyzer_state_t {
    compiler_t *compiler;
    stack_t<hashmap_t<string_t, scope_entry_t>*> *current_scope_stack;

    // stack_t<string_t> *expr_deps;
    stack_t<string_t> *local_type_deps;
    hashmap_t<string_t, stack_t<string_t>> *type_deps;
};

b32 analyze_function(analyzer_state_t   *state, b32 is_prototype, scope_entry_t *entry, b32 *should_wait);
u32 analyze_statement(analyzer_state_t  *state, u64 expected_return_amount, b32 in_loop, ast_node_t *node, b32 *should_wait);
b32 analyze_expression(analyzer_state_t *state, u64 expected_count_of_expressions, string_t *depend_on, ast_node_t *expr, b32 *should_wait);

// ------------ helpers

hashmap_t<string_t, scope_entry_t> create_scope(void) {
    hashmap_t<string_t, scope_entry_t> scope = {};
    scope.hash_func    = get_string_hash;
    scope.compare_func = compare_string_keys;
    return scope;
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
            string_t buffer = {};

            buffer = string_temp_concat(string_temp_concat(STRING("The identifier '"), key), STRING("' "));
            buffer = string_temp_concat(buffer, STRING("is already used before."));
            log_error_token(buffer, node->token);

            buffer = string_temp_concat(string_temp_concat(STRING("'"), entry->node->token.data.string), STRING("' "));
            buffer = string_temp_concat(buffer, STRING("was used here:"));
            log_info_token(buffer, entry->node->token);

            return false;
        }

        *output = entry;
        return true;
    }

    add_blank_entry(scope, key, output);
    return true;
}

scope_entry_t get_entry_to_report(analyzer_state_t *state, string_t key) {
    for (u64 i = 0; i < state->current_scope_stack->index; i++) {
        if (!hashmap_contains(state->current_scope_stack->data[i], key))
            continue;

        scope_entry_t e = *hashmap_get(state->current_scope_stack->data[i], key);
        return e;
    }

    assert(false);
    return {};
}

b32 check_dependencies(analyzer_state_t *state, b32 report_error, scope_entry_t *entry, string_t key) {
    stack_t<string_t> *deps = hashmap_get(state->type_deps, key);

        // @cleanup @speed @todo: doesnt work for pointers...
    for (u64 i = 0; i < state->local_type_deps->index; i++) {
        if (string_compare(key, state->local_type_deps->data[i])) {
            if (!report_error)
                return false;

            log_error_token(STRING("Identifier can not be resolved because it's definition is recursive."), entry->node->token);

            allocator_t * talloc = get_temporary_allocator();
            scope_entry_t e      = get_entry_to_report(state, state->local_type_deps->data[i]);

            log_error_token(string_concat(STRING("Recursion found in type '"), string_concat(state->local_type_deps->data[i], STRING("'"), talloc), talloc), e.node->token);
            return false;
        }

        if (deps == NULL) continue;

        for (u64 j = 0; j < deps->index; j++) {
            if (string_compare(state->local_type_deps->data[i], deps->data[j])) {
                if (!report_error)
                    return false;

                log_error_token(STRING("Identifier can not be resolved because type definition is recursive."), entry->node->token);

                allocator_t * talloc = get_temporary_allocator();
                scope_entry_t e      = get_entry_to_report(state, deps->data[j]);

                log_error_token(string_concat(STRING("Recursion found in type '"), string_concat(deps->data[j], STRING("'"), talloc), talloc), e.node->token);
                return false;
            }
        }
    }

    return true;
}

u32 get_if_exists(analyzer_state_t *state, b32 report_deps_error, string_t key, scope_entry_t **output) {
    UNUSED(output);

    for (u64 i = 0; i < state->current_scope_stack->index; i++) {
        if (!hashmap_contains(state->current_scope_stack->data[i], key))
            continue;

        scope_entry_t *entry = hashmap_get(state->current_scope_stack->data[i], key);

        if (output) {
            *output = entry;
        }

        if (!check_dependencies(state, report_deps_error, entry, key)) {
            return GET_DEPS_ERROR;
        }
        
        if (!entry->node->analyzed) {
            return GET_NOT_ANALYZED;
        }

        return GET_SUCCESS;
    }

    return GET_NOT_FIND;
}

// -----------------

void set_std_info(token_t token, type_info_t *info) {
    switch (token.type) {
        case TOK_S8:  info->type = TYPE_s8;  info->size = 1; break;
        case TOK_S16: info->type = TYPE_s16; info->size = 2; break;
        case TOK_S32: info->type = TYPE_s32; info->size = 4; break;
        case TOK_S64: info->type = TYPE_s64; info->size = 8; break;

        case TOK_U8:  info->type = TYPE_u8;  info->size = 1; break;
        case TOK_U16: info->type = TYPE_u16; info->size = 2; break;
        case TOK_U32: info->type = TYPE_u32; info->size = 4; break;
        case TOK_U64: info->type = TYPE_u64; info->size = 8; break;

        case TOK_BOOL8:  info->type = TYPE_b8;  info->size = 1; break;
        case TOK_BOOL32: info->type = TYPE_b32; info->size = 4; break;

        case TOK_F32: info->type = TYPE_f32; info->size = 4; break;
        case TOK_F64: info->type = TYPE_f64; info->size = 8; break;

        case TOK_VOID: info->type = TYPE_void; break;

        default:
            assert(false);
            break;
    }
}

void set_std_info(token_t token, scope_entry_t *entry) {
    switch (token.type) {
        case TOK_S8:  entry->info.type = TYPE_s8;  entry->info.size = 1; break;
        case TOK_S16: entry->info.type = TYPE_s16; entry->info.size = 2; break;
        case TOK_S32: entry->info.type = TYPE_s32; entry->info.size = 4; break;
        case TOK_S64: entry->info.type = TYPE_s64; entry->info.size = 8; break;

        case TOK_U8:  entry->info.type = TYPE_u8;  entry->info.size = 1; break;
        case TOK_U16: entry->info.type = TYPE_u16; entry->info.size = 2; break;
        case TOK_U32: entry->info.type = TYPE_u32; entry->info.size = 4; break;
        case TOK_U64: entry->info.type = TYPE_u64; entry->info.size = 8; break;

        case TOK_BOOL8:  entry->info.type = TYPE_b8;  entry->info.size = 1; break;
        case TOK_BOOL32: entry->info.type = TYPE_b32; entry->info.size = 4; break;

        case TOK_F32: entry->info.type = TYPE_f32; entry->info.size = 4; break;
        case TOK_F64: entry->info.type = TYPE_f64; entry->info.size = 8; break;

        case TOK_VOID: entry->info.type = TYPE_void; entry->info.size = 0; break;

        default:
            assert(false);
            break;
    }
}

enum {
    CONST_TYPE_FLOAT = 0x10,
    CONST_TYPE_BOOL  = 0x20,
    CONST_TYPE_UINT  = 0x40,
    CONST_TYPE_INT   = 0x80,
};

b32 analyze_expression(analyzer_state_t *state, u64 expected_count_of_expressions, string_t *depend_on, ast_node_t *expr, b32 *should_wait) {
    b32 result = true;

    if (expr->type == AST_PRIMARY) switch (expr->token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOK_DEFAULT:
            expr->analyzed = true;
            return result;

        case TOKEN_IDENT: {
                string_t var_name = expr->token.data.string;

                scope_entry_t *output = NULL; 
                switch (get_if_exists(state, true, var_name, &output)) {
                    case GET_NOT_FIND:
                        *should_wait = true;
                        break;

                    case GET_NOT_ANALYZED:
                        *should_wait = true;
                        break;

                    case GET_DEPS_ERROR:
                        result = false;
                        break;

                    case GET_SUCCESS: switch (output->type) {
                        case ENTRY_VAR:
                        case ENTRY_FUNC:
                            break;

                        case ENTRY_PROTOTYPE:
                        case ENTRY_TYPE:
                            log_error_token(STRING("Cant use Type in expression"), expr->token);
                            result = false;
                            break;


                        default:
                            log_error(STRING("Unexpected entry..."));
                            result = false;
                            break;
                    } break;
                }
            } break;
    } else switch (expr->type) {
        case AST_UNARY_DEREF:
        case AST_UNARY_REF:
        case AST_UNARY_NEGATE:
        case AST_UNARY_NOT:
            result = analyze_expression(state, expected_count_of_expressions, depend_on, expr->left, should_wait);
            break;


        case AST_BIN_CAST:
            // left is type
            break;

        case AST_BIN_ASSIGN:
        case AST_BIN_GR:
        case AST_BIN_LS:
        case AST_BIN_GEQ:
        case AST_BIN_LEQ:
        case AST_BIN_EQ:
        case AST_BIN_NEQ:
        case AST_BIN_LOG_OR:
        case AST_BIN_LOG_AND:
        case AST_BIN_ADD:
        case AST_BIN_SUB:
        case AST_BIN_MUL:
        case AST_BIN_DIV:
        case AST_BIN_MOD:
        case AST_BIN_BIT_XOR:
        case AST_BIN_BIT_OR:
        case AST_BIN_BIT_AND:
        case AST_BIN_BIT_LSHIFT:
        case AST_BIN_BIT_RSHIFT:
        case AST_FUNC_CALL:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->left, should_wait)) {
                result = false;
            }

            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->right, should_wait)) {
                result = false;
            }
            break;

        case AST_BIN_SWAP:
            log_error_token(STRING("AST_BIN_SWAP DOESNT WORK RN"), expr->token);
            break;
        case AST_SEPARATION: {
            ast_node_t *next = expr->list_start;

            if (expr->child_count != expected_count_of_expressions) {
                log_error_token(STRING("Not expected count of elements"), expr->token);
                result = false;
                break;
            }

            // element count should match other size or amount of return argumets
            for (u64 i = 0; i < expr->child_count; i++) {
                if (!analyze_expression(state, expected_count_of_expressions, depend_on, next, should_wait)) 
                    result = false;

                if (*should_wait)
                    break;

                next = next->list_next;
            }

        } break;
    }

    if (!*should_wait) {
        expr->analyzed = true;
    }

    if (!result) {
        expr->analyzed = true;
    }

    return result;
}

b32 analyze_definition(analyzer_state_t *state, b32 can_do_func, ast_node_t *name, ast_node_t *type, ast_node_t *expr, b32 *should_wait) {
    string_t key = name->token.data.string;

    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->current_scope_stack), key, name, &entry)) {
        return false;
    }

    if (!entry->configured) {
        entry->node = name;
    }

    b32 is_pointer = false;

    while (type->type == AST_PTR_TYPE) {
        if (!entry->configured) {
            entry->info.pointer_depth++;
        }
        type = type->left;
        is_pointer = true;
    }

    if (!entry->configured) {
        entry->configured = true;
    }

    if (type->type == AST_UNKN_TYPE) {
        string_t type_key = type->token.data.string;

        if (!is_pointer && state->local_type_deps->index > 0) {
            stack_push(hashmap_get(state->type_deps, stack_peek(state->local_type_deps)), type_key);
        }

        scope_entry_t *output_type = NULL; 

        switch (get_if_exists(state, !is_pointer, type_key, &output_type)) {
            case GET_NOT_FIND:
            case GET_NOT_ANALYZED:
                *should_wait = true;
                break;

            case GET_DEPS_ERROR:
                if (!is_pointer) {
                    return false;
                }
            case GET_SUCCESS: 
            {
                switch (output_type->type) {
                    case ENTRY_PROTOTYPE:
                        if (can_do_func) {
                            entry->type = ENTRY_FUNC;
                            entry->prototype_name = type_key;

                            entry->func_params = hashmap_clone(&output_type->func_params);
                            entry->return_typenames = list_clone(&output_type->return_typenames);
                        } else {
                            entry->type      = ENTRY_VAR;
                            entry->info.type = TYPE_UNKN;
                            entry->info.type_name = type_key;
                        }
                        break;

                    case ENTRY_TYPE:
                        entry->type = ENTRY_VAR;
                        break;

                    case ENTRY_FUNC: 
                        log_error(STRING("Cant use FUNC as a type of variable [@better_message]"));
                        entry->type = ENTRY_ERROR;
                        entry->node->analyzed = true;
                        return false;

                    default:
                        log_error(STRING("Unexpected type..."));
                        entry->type = ENTRY_ERROR;
                        entry->node->analyzed = true;
                        return false;
                }

                entry->info.size      = output_type->info.size;
                entry->info.type_name = type_key;
            } break;
        }
    } else switch (type->type) {
        case AST_FUNC_TYPE: {
            if (!can_do_func) {
                log_error(STRING("Cant do functions for multiple var decl..."));
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
            }

            entry->type = ENTRY_FUNC;

            if (!analyze_function(state, false, entry, should_wait)) {
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
            }
        }
        break;

        case AST_VOID_TYPE: // fallthrough
            if (!is_pointer) {
                log_error_token(STRING("Cant make variable with void type..."), type->token);
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
            }
        case AST_STD_TYPE:
            entry->type = ENTRY_VAR;
            set_std_info(type->token, entry);
            break;

        case AST_MUL_AUTO:
        case AST_AUTO_TYPE:
            assert(expr != NULL);

            log_error_token(STRING("Cant evaluate the types right now..."), type->token);
            entry->type = ENTRY_ERROR;
            entry->node->analyzed = true;
            return false;

        case AST_ARR_TYPE:
            log_error(STRING("we dont support arrays right now"));
            entry->type = ENTRY_ERROR;
            entry->node->analyzed = true;
            return false;

        case AST_PTR_TYPE: // because we stripped the pointer in top!
            assert(false);

        default:
            log_error(STRING("unexpected type of ast node..."));
            entry->node->analyzed = true;
            return false;
    } 

    entry->expr = expr;

    if (entry->type == ENTRY_FUNC) {
        if (expr->type != AST_BLOCK_IMPERATIVE) {
            log_error_token(STRING("cant do functions with expression body."), expr->token);
            return false;
        }

        stack_push(state->current_scope_stack, &entry->func_params);
        stack_push(state->local_type_deps, key);

        ast_node_t *stmt = expr->list_start;

        for (u64 i = 0; i < expr->child_count; i++) {
            switch (analyze_statement(state, entry->return_typenames.count, false, stmt, should_wait)) {
                case STMT_BREAK:
                case STMT_CONTINUE:
                    log_error_token(STRING("Cant use break or continue outside of loop."), stmt->token);
                    return false;

                case STMT_ERR:
                    return false;

                case STMT_RETURN:
                    if (i != (expr->child_count - 1)) {
                        log_warning_token(STRING("There are statemets after return."), stmt->token);
                    }
                    break;

                case STMT_OK:
                    break;
            }

            if (*should_wait) {
                break;
            }

            stmt = stmt->list_next;
        }

        stack_pop(state->current_scope_stack);
        stack_pop(state->local_type_deps);
    } else if (entry->type == ENTRY_VAR && expr != NULL) {
        if (!analyze_expression(state, 1, &name->token.data.string, expr, should_wait)) {
            return false;
        }
    }


    if (!*should_wait) {
        entry->node->analyzed = true;
    }


    return true;
}

b32 analyze_function(analyzer_state_t *state, b32 is_prototype, scope_entry_t *entry, b32 *should_wait) {
    ast_node_t *func = entry->node;
    ast_node_t *type_node  = func->left;
    assert(type_node->type  == AST_FUNC_TYPE);

    entry->expr = func->right;

    string_t key = entry->node->token.data.string;
    stack_push(state->current_scope_stack, &entry->func_params);
    stack_push(state->local_type_deps, key);

    {
        stack_t<string_t> type_deps = {};
        hashmap_add(state->type_deps, key, &type_deps);
    }

    b32 result = true;
    ast_node_t *next_type = type_node->left->list_start;
    for (u64 i = 0; i < type_node->left->child_count; i++) {
        assert(next_type->type == AST_PARAM_DEF);

        if (!analyze_definition(state, false, next_type, next_type->left, NULL, should_wait)) {
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

        type_info_t info = {};

        b32 is_pointer = is_prototype;

        ast_node_t *current_type = next_type;

        while (current_type->type == AST_PTR_TYPE) {
            info.pointer_depth++;
            current_type = current_type->left;
            is_pointer = true;
        }

        if (current_type->type == AST_UNKN_TYPE) {
            string_t type_key = current_type->token.data.string;

            if (!is_pointer && state->local_type_deps->index > 0) {
                stack_push(hashmap_get(state->type_deps, stack_peek(state->local_type_deps)), type_key);
            }

            scope_entry_t *output_type = NULL; 

            switch (get_if_exists(state, !is_pointer, type_key, &output_type)) {
                case GET_NOT_FIND:
                case GET_NOT_ANALYZED:
                    *should_wait = true;
                    break;

                case GET_DEPS_ERROR:
                    if (!is_pointer) {
                        return false;
                    }
                case GET_SUCCESS: 
                {
                    switch (output_type->type) {
                        case ENTRY_PROTOTYPE:
                        case ENTRY_TYPE:
                            break;

                        case ENTRY_FUNC: 
                            log_error_token(STRING("Cant use FUNC as a return type [@better_message]"), current_type->token);
                            entry->type = ENTRY_ERROR;
                            result = false;
                            break;

                        default:
                            log_error_token(STRING("Unexpected type... [@better_message]"), current_type->token);
                            entry->type = ENTRY_ERROR;
                            result = false;
                            break;
                    }

                    info.type = TYPE_UNKN;
                    info.size = output_type->info.size;
                    info.type_name = type_key;
                } break;
            }
        } else switch (current_type->type) {
            case AST_VOID_TYPE: // fallthrough
                if (!is_pointer) {
                    log_error_token(STRING("Cant return void."), current_type->token);
                    entry->type = ENTRY_ERROR;
                    result = false;
                    break;
                }
            case AST_STD_TYPE:
                set_std_info(current_type->token, &info);
                break;

            case AST_ARR_TYPE:
                log_error(STRING("we dont support arrays right now"));
                entry->type = ENTRY_ERROR;
                result = false;
                break;

            case AST_MUL_AUTO:
            case AST_AUTO_TYPE:
            case AST_PTR_TYPE: 
                assert(false);

            default:
                log_error(STRING("unexpected type of ast node..."));
                result = false;
                break;
        } 

        if (!result || *should_wait)
            break;

        list_add(&entry->return_typenames, &info);
        next_type->analyzed = true;
        next_type = next_type->list_next;
    }

    stack_pop(state->local_type_deps);
    stack_pop(state->current_scope_stack);

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

    if (!analyze_definition(state, false, node, node->left, NULL, should_wait)) {
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

    if (!aquire_entry(stack_peek(state->current_scope_stack), key, node, &entry)) {
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
            if (!analyze_definition(state, false, next, node->right, NULL, should_wait)) {
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

        log_error_token(STRING("This variable didn't have it's own type:"), next->token);
        return false;
    }

    ast_node_t * name = node->left->list_start;
    ast_node_t * type = node->right->list_start;

    for (u64 i = 0; i < node->left->child_count; i++) {
        if (!analyze_definition(state, false, name, type, NULL, should_wait)) {
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


    if (!analyze_definition(state, true, node, node->left, node->right, should_wait)) {
        return false;
    }

    if (*should_wait) {
        return true;
    }

    return true;
}

b32 analyze_tern_def(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type  == AST_TERN_MULT_DEF);
    assert(node->right->type == AST_SEPARATION);
    assert(state != NULL);
    assert(node  != NULL);
    assert(should_wait != NULL);

    ast_node_t *type_node = node->center;

    b32 mult_types = type_node->type == AST_MUL_TYPES;
    b32 mult_expr  = node->right->child_count > 1;

    if (!mult_types && !mult_expr) {
        ast_node_t * next = node->left->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, next, node->center, node->right->list_start, should_wait)) {
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

    if (mult_types && !mult_expr) {
        if (node->left->child_count != node->center->child_count) {
            ast_node_t * next;
            u64 least_size = MIN(node->left->child_count, node->center->child_count);

            if (least_size >= node->left->child_count) {
                next = node->center->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }

                log_error_token(STRING("Trailing type without its identifier:"), next->token);
            } else {
                next = node->left->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }
                log_error_token(STRING("This variable didn't have it's type:"), next->token);
            }

            node->analyzed = true;
            return false;
        }

        ast_node_t * name = node->left->list_start;
        ast_node_t * type = node->center->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, name, type, node->right->list_start, should_wait)) {
                return false;
            }

            if (*should_wait) {
                return true;
            }

            name = name->list_next;
            type = type->list_next;
        }
    }

    if (!mult_types && mult_expr) {
        if (node->left->child_count != node->right->child_count) {
            ast_node_t * next;
            u64 least_size = MIN(node->left->child_count, node->right->child_count);

            if (least_size >= node->left->child_count) {
                next = node->right->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }

                log_error_token(STRING("Trailing expression without its identifier:"), next->token);
            } else {
                next = node->left->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }
                log_error_token(STRING("This variable didn't have it's expression:"), next->token);
            }

            node->analyzed = true;
            return false;
        }

        ast_node_t * name = node->left->list_start;
        ast_node_t * type = node->center;
        ast_node_t * expr = node->right->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, name, type, expr, should_wait)) {
                return false;
            }

            if (*should_wait) {
                return true;
            }

            name = name->list_next;
            expr = expr->list_next;
        }
    } 

    if (mult_types && mult_expr) {
        if (node->left->child_count != node->center->child_count) {
            ast_node_t * next = node->left->list_start;

            while (next->list_next != NULL) {
                next = next->list_next;
            }

            log_error_token(STRING("This variable didn't have it's own type:"), next->token);
            node->analyzed = true;
            return false;
        }

        if (node->left->child_count != node->right->child_count) {
            ast_node_t * next;
            u64 least_size = MIN(node->left->child_count, node->right->child_count);

            if (least_size >= node->left->child_count) {
                next = node->right->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }

                log_error_token(STRING("Trailing expression without its variable:"), next->token);
            } else {
                next = node->left->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }
                log_error_token(STRING("This variable didn't have it's expression:"), next->token);
            }

            node->analyzed = true;
            return false;
        }

        ast_node_t * name = node->left->list_start;
        ast_node_t * type = node->center->list_start;
        ast_node_t * expr = node->right->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, name, type, expr, should_wait)) {
                return false;
            }

            if (*should_wait) {
                return true;
            }

            name = name->list_next;
            type = type->list_next;
            expr = expr->list_next;
        }
    }

    node->analyzed = true;
    return true;
}

b32 analyze_and_add_type_members(analyzer_state_t *state, b32 *should_wait, scope_entry_t *entry) {
    ast_node_t * block = entry->node->left;
    ast_node_t * next  = block->list_start;

    b32 result = true;

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
                log_error_token(STRING("Cant use initialization in member declaration."), entry->node->token);
                result = false;
                break;

            default:
                log_error_token(STRING("Cant use this as member of type."), entry->node->token);
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

    if (!aquire_entry(stack_peek(state->current_scope_stack), key, node, &entry)) {
        return false;
    }

    if (!entry->configured) {
        entry->node = node;
        entry->type = ENTRY_PROTOTYPE;
        entry->configured = true;
    }

    b32 result = false;
    b32 should_wait = false;

    result = analyze_function(state, true, entry, &should_wait);

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

    if (!aquire_entry(stack_peek(state->current_scope_stack), key, node, &entry)) {
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

    stack_push(state->current_scope_stack, &entry->scope);
    stack_push(state->local_type_deps, key);
    
    {
        stack_t<string_t> type_deps = {};
        hashmap_add(state->type_deps, key, &type_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    stack_pop(state->local_type_deps);
    stack_pop(state->current_scope_stack);

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

    if (!aquire_entry(stack_peek(state->current_scope_stack), key, node, &entry)) {
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

    stack_push(state->current_scope_stack, &entry->scope);
    stack_push(state->local_type_deps, key);

    {
        stack_t<string_t> type_deps = {};
        hashmap_add(state->type_deps, key, &type_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    stack_pop(state->local_type_deps);
    stack_pop(state->current_scope_stack);

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

    if (!aquire_entry(stack_peek(state->current_scope_stack), key, node, &entry)) {
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

    stack_push(state->current_scope_stack, &entry->scope);
    stack_push(state->local_type_deps, key);

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

    stack_pop(state->local_type_deps);
    stack_pop(state->current_scope_stack);

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
        log_error(STRING("Couldn't add file to work with."));
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
    log_write(STRING("TRY: "));
    log_write(file);
#endif

    if (!is_file_exists(file)) { 
#ifdef DEBUG
        log_push_color(ERROR_COLOR);
        log_write(STRING(" FAIL\n"));
        log_pop_color();
#endif
        return false;
    } else { 
#ifdef DEBUG
        log_push_color(GREEN_COLOR);
        log_write(STRING(" OK\n"));
        log_pop_color();
#endif

        *valid_file = load_and_process_file(compiler, string_copy(file, compiler->strings));
    }

    return true;
}

b32 find_and_add_file(compiler_t *compiler, ast_node_t *node) {
    allocator_t *talloc = get_temporary_allocator();

    // ./<file>.slm
    // ./<file>/module.slm
    // /<compiler>/<file>.slm
    // /<compiler>/<file>/module.slm

    string_t from_file = node->token.from->filename;
    string_t directory, name = node->token.data.string;

    s64 slash = index_of_last_file_slash(from_file);
    if (slash == -1) {
        directory = string_copy(STRING("./"), talloc);
    } else {
        directory = string_substring(from_file, 0, slash + 1, talloc);
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

    log_error_token(STRING("Couldn't find corresponding file to this declaration."), node->token);
    node->analyzed = true;
    return false;
}

// --------------------


u32 analyze_statement(analyzer_state_t *state, u64 expect_return_amount, b32 in_loop, ast_node_t *node, b32 *should_wait) {
    temp_reset();

    assert(!node->analyzed);
    u32 result = STMT_ERR;

    switch (node->type) {
        case AST_BLOCK_IMPERATIVE:
            log_error_token(STRING("Blocks are not supported currently..."), node->token);
            result = STMT_ERR;
            break;
        case AST_UNNAMED_MODULE:
            log_error_token(STRING("cant load files from functions."), node->token);
            result = STMT_ERR;
            break;

        case AST_STRUCT_DEF: 
        case AST_UNION_DEF:
        case AST_ENUM_DEF: 
        case AST_UNARY_PROTO_DEF: 
            log_error_token(STRING("cant create types not in global scope."), node->token);
            result = STMT_ERR;
            break;


        case AST_IF_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left, should_wait)) {
                result = STMT_ERR;
                break;
            }
            result = analyze_statement(state, expect_return_amount, in_loop, node->right, should_wait);
            if (result != STMT_ERR) {
                result = STMT_OK;
            }
        } break;

        case AST_IF_ELSE_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left, should_wait)) {
                result = STMT_ERR;
                break;
            }

            b32 lr = analyze_statement(state, expect_return_amount, in_loop, node->right, should_wait);
            b32 rr = analyze_statement(state, expect_return_amount, in_loop, node->right, should_wait);

            if (lr == rr && lr == STMT_RETURN) {
                result = STMT_RETURN;
            } else if (lr == STMT_ERR || rr == STMT_ERR) {
                result = STMT_ERR;
            } else {
                result = STMT_OK;
            }
        } break;

        case AST_WHILE_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left, should_wait)) {
                result = STMT_ERR;
                break;
            }

            result = analyze_statement(state, expect_return_amount, true, node->right, should_wait);
        } break;

        case AST_RET_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left, should_wait)) {
                result = STMT_ERR;
                break;
            }

            result = STMT_RETURN;
        } break;

        case AST_BREAK_STMT:
            result = STMT_BREAK;
            break;

        case AST_CONTINUE_STMT:
            result = STMT_CONTINUE;
            break;

        case AST_UNARY_VAR_DEF:
            result = analyze_unary_var_def(state, node, should_wait) ? STMT_OK : STMT_ERR;
            break;

        case AST_BIN_UNKN_DEF: 
            result = analyze_unkn_def(state, node, should_wait)? STMT_OK : STMT_ERR;
            break;

        case AST_BIN_MULT_DEF: 
            result = analyze_bin_var_def(state, node, should_wait) ? STMT_OK : STMT_ERR;
            break;

        case AST_TERN_MULT_DEF:
            result = analyze_tern_def(state, node, should_wait) ? STMT_OK : STMT_ERR;
            break;

        default: {
            result = analyze_expression(state, expect_return_amount, NULL, node, should_wait) ? STMT_OK : STMT_ERR;
        } break;
    }

    if (!should_wait) node->analyzed = true;
    return result;
}

b32 analyze_global_statement(analyzer_state_t *state, ast_node_t *node) {
    temp_reset();

    assert(!node->analyzed);
    b32 result, should_wait = false;

    switch (node->type) {
        case AST_UNNAMED_MODULE:
            assert(false);

        case AST_STRUCT_DEF: 
            result = analyze_struct(state, node);
            break;

        case AST_UNION_DEF:
            result = analyze_union(state, node);
            break;

        case AST_ENUM_DEF: 
            result = analyze_enum(state, node);
            break;

        case AST_UNARY_PROTO_DEF: 
            result = analyze_prototype(state, node);
            break;


        case AST_UNARY_VAR_DEF:
            result = analyze_unary_var_def(state, node, &should_wait);
            break;

        case AST_BIN_UNKN_DEF: 
            result = analyze_unkn_def(state, node, &should_wait);
            break;

        case AST_BIN_MULT_DEF: 
            result = analyze_bin_var_def(state, node, &should_wait);
            break;

        case AST_TERN_MULT_DEF:
            result = analyze_tern_def(state, node, &should_wait);
            break;

        default:
            log_error_token(STRING("Wrong type of construct."), node->token);
            result = false;
            break;
    }

    return result;
}
b32 analyzer_preload_all_files(compiler_t *compiler) {
    assert(compiler != NULL);

    b32 result       = true;
    b32 not_finished = true;

    while (not_finished) {
        not_finished = false;

        for (u64 i = 0; i < compiler->files.capacity; i++) {
            kv_pair_t<string_t, source_file_t> pair = compiler->files.entries[i];

            if (!pair.occupied) continue;
            if (pair.deleted)   continue;

            for (u64 j = 0; j < pair.value.parsed_roots.count; j++) {
                temp_reset();
                ast_node_t *node = *list_get(&pair.value.parsed_roots, j);

                if (node->analyzed || node->type != AST_UNNAMED_MODULE) {
                    continue;
                } 

                not_finished = true;

                if (!find_and_add_file(compiler, node)) {
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
        log_error(STRING("Couldn't find a file."));
        return false;
    }

#ifdef DEBUG
    log_info(STRING("All files loaded!"));
#endif

    return result;
}

void print_var_info(type_info_t info) {
    if (info.pointer_depth) {
        if (info.pointer_depth == 1) {
            fprintf(stderr, " pointer");
        } else {
            fprintf(stderr, " pointer(%u)", info.pointer_depth);
        }
    }

    if (info.type != TYPE_UNKN) {
        fprintf(stderr, " (std... %d)", info.type);
    } else {
        fprintf(stderr, " %s", string_to_c_string(info.type_name, get_temporary_allocator()));
    }
}

void print_entry(compiler_t *compiler, kv_pair_t<string_t, scope_entry_t> pair, u64 depth) {
    allocator_t *talloc = get_temporary_allocator();

    assert(depth <= 8); // cause we have some hardcoded offset down there...

    add_left_pad(stderr, depth * 4);
    fprintf(stderr, " -> %-*s", (int)(32 - depth * 4), string_to_c_string(pair.key, talloc));

    if (pair.value.type == ENTRY_VAR) {
        fprintf(stderr, " VAR");
        print_var_info(pair.value.info);
    } else if (pair.value.type == ENTRY_TYPE) {
        fprintf(stderr, " TYPE");
    } else if (pair.value.type == ENTRY_FUNC) {
        fprintf(stderr, " FUNC");
    } else if (pair.value.type == ENTRY_PROTOTYPE) {
        fprintf(stderr, " PROTO");
    }

    fprintf(stderr, "\n");

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
        type_info_t info = pair.value.return_typenames.data[j];
        add_left_pad(stderr, (depth + 1) * 4);
        fprintf(stderr, " -> RET VAL %llu", j);
        print_var_info(info);
        fprintf(stderr, "\n");
    }
}

void print_all_definitions(compiler_t *compiler) {
    log_update_color();

    fprintf(stderr, "--------GLOBAL-SCOPE---------\n");

    for (u64 i = 0; i < compiler->scope.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = compiler->scope.entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        print_entry(compiler, pair, 0);
    }

    fprintf(stderr, "-----------------------------\n");
}

b32 analyze(compiler_t *compiler) {
    assert(compiler != NULL);

    b32 result       = true;
    b32 not_finished = true;

    stack_t<hashmap_t<string_t, scope_entry_t>*> current_scope_stack = {};
    stack_t<string_t> local_type_deps = {};
    stack_create(&local_type_deps, 16);

    hashmap_t<string_t, stack_t<string_t>> type_deps = {};
    hashmap_create(&type_deps, 128, get_string_hash, compare_string_keys);
    stack_push(&current_scope_stack, &compiler->scope);

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
            state.compiler = compiler;
            state.current_scope_stack   = &current_scope_stack;
            
            state.local_type_deps = &local_type_deps;
            state.type_deps = &type_deps;

            // we just need to try to compile, if we cant resolve something we add it, but just
            for (u64 j = 0; j < pair.value.parsed_roots.count; j++) {
                ast_node_t *node = *list_get(&pair.value.parsed_roots, j);

                if (node->analyzed) {
                    continue;
                } 

                not_finished = true;

                if (!analyze_global_statement(&state, node)) {
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
        log_error(STRING("Had an analyze error."));
        result = false;
    } else if (not_finished) { 
        log_error(STRING("There is undefined types..."));
        result = false;
    } else {
        log_info(STRING("Everything is okay, compiling."));
    }

    hashmap_delete(&type_deps);
    stack_delete(&current_scope_stack);

    print_all_definitions(compiler);

    return result;
}
