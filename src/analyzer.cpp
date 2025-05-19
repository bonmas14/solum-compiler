#include "analyzer.h"

#include "stack.h"
#include "logger.h"
#include "compiler.h"
#include "scanner.h"
#include "parser.h"

#include "talloc.h"
#include "profiler.h"
#include "platform.h"

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

enum {
    STATE_GLOBAL_ANALYSIS,
    STATE_CODE_ANALYSIS,
};

struct analyzer_state_t {
    u64 state;
    compiler_t *compiler;
    list_t<hashmap_t<string_t, scope_entry_t>*> scopes;
    stack_t<hashmap_t<string_t, scope_entry_t>*> current_search_stack;

    stack_t<string_t> internal_deps;
    hashmap_t<string_t, stack_t<string_t>> symbol_deps;
};

b32 analyze_function(analyzer_state_t   *state, scope_entry_t *entry, b32 *should_wait);
u32 analyze_statement(analyzer_state_t *state, u64 expect_return_amount, u32 scope_index, b32 in_loop, ast_node_t *node);
// ------------ helpers

hashmap_t<string_t, scope_entry_t> create_scope(void) {
    hashmap_t<string_t, scope_entry_t> scope = {};
    return scope;
}

b32 check_if_unique(scope_entry_t *entry, ast_node_t *new_node) {
    assert(entry != NULL);
    assert(new_node != NULL);

    if (entry->node == NULL) return true;

    if (mem_compare((u8*)entry->node, (u8*)new_node, sizeof(ast_node_t)))
        return false;

    return true;
}


void add_blank_entry(hashmap_t<string_t, scope_entry_t> *scope, string_t key, scope_entry_t **output) {
    assert(output != NULL);
    assert(scope  != NULL);

    scope_entry_t entry = {};

    if (!hashmap_add(scope, key, &entry)) {
        assert(false);
    }

    *output = hashmap_get(scope, key);
    assert(*output != NULL);
}

b32 aquire_entry(hashmap_t<string_t, scope_entry_t> *scope, string_t key, ast_node_t *node, scope_entry_t **output) {
    assert(scope != NULL);
    assert(node != NULL);
    assert(output != NULL);

    UNUSED(output); // so compiler dont yell

    scope_entry_t *entry = (*scope)[key];
    if (entry) {
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
    assert(state != NULL);

    for (u64 i = 0; i < state->current_search_stack.index; i++) {
        scope_entry_t *entry = (*state->current_search_stack.data[i])[key];

        if (!entry)
            continue;

        return *entry;
    }

    assert(false);
    return {};
}

b32 check_dependencies(analyzer_state_t *state, b32 report_error, scope_entry_t *entry, string_t key) {
    assert(state != NULL);
    assert(entry != NULL);

    stack_t<string_t> *deps = hashmap_get(&state->symbol_deps, key);

        // @cleanup @speed @todo: doesnt work for pointers...
    for (u64 i = 0; i < state->internal_deps.index; i++) {
        if (string_compare(key, state->internal_deps.data[i])) {
            if (!report_error)
                return false;

            log_error_token("Identifier can not be resolved because it's definition is recursive.", entry->node->token);

            allocator_t * talloc = get_temporary_allocator();
            scope_entry_t e      = get_entry_to_report(state, state->internal_deps.data[i]);

            log_error_token(string_concat(STRING("Recursion found in type '"), string_concat(state->internal_deps.data[i], STRING("'"), talloc), talloc), e.node->token);
            return false;
        }

        if (deps == NULL) continue;

        for (u64 j = 0; j < deps->index; j++) {
            if (string_compare(state->internal_deps.data[i], deps->data[j])) {
                if (!report_error)
                    return false;

                log_error_token("Identifier can not be resolved because type definition is recursive.", entry->node->token);

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
    assert(state != NULL);
    assert(output != NULL);

    UNUSED(output);

    bool was_uninit = false;

    for (s64 i = (state->current_search_stack.index - 1); i >= 0; i--) {
        hashmap_t<string_t, scope_entry_t> *search_scope = state->current_search_stack.data[i];
        scope_entry_t *entry = (*search_scope)[key];

        if (!entry)
            continue;

        *output = entry;

        if (entry->type == ENTRY_VAR && state->state == STATE_CODE_ANALYSIS) {
            if (!check_dependencies(state, report_deps_error, entry, key)) {
                return GET_DEPS_ERROR;
            }
        } else if (entry->type != ENTRY_VAR) {
            if (!check_dependencies(state, report_deps_error, entry, key)) {
                return GET_DEPS_ERROR;
            }
        }
        
        if (entry->uninit) {
            was_uninit = true;
            continue;
        }

        if (!entry->node->analyzed) {
            return GET_NOT_ANALYZED;
        }

        return GET_SUCCESS;
    }

    if (was_uninit) {
        return GET_SUCCESS;
    } else {
        return GET_NOT_FIND;
    }
}

b32 add_var_type_into_search(analyzer_state_t *state, scope_entry_t *output) {
    if (output->info.type != TYPE_UNKN) return true;

    string_t type_name = output->info.type_name;

    scope_entry_t *type = NULL; 

    switch (get_if_exists(state, true, type_name, &type)) {
        case GET_NOT_FIND:
        case GET_NOT_ANALYZED:
            log_error_token("Couldn't find type name.", output->node->token);
            return false;

        case GET_DEPS_ERROR: assert(false); return false;

        case GET_SUCCESS: switch (type->type) {
            case ENTRY_VAR:
                log_error_token("Type was a variable name.", output->node->token);
                return false;

            case ENTRY_FUNC:
                stack_push(&state->current_search_stack, &type->func_params);
                break;

            case ENTRY_TYPE:
                stack_push(&state->current_search_stack, &type->scope);
                break;


            default:
                assert(false);
                return false;
        } break;
    }

    return true;
}


void set_std_info(token_t token, type_info_t *info) {
    assert(info != NULL);

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

// -----------------

enum {
    CONST_TYPE_FLOAT = 0x10,
    CONST_TYPE_BOOL  = 0x20,
    CONST_TYPE_UINT  = 0x40,
    CONST_TYPE_INT   = 0x80,
};

struct output_info_t {
    b32 valid; 
    type_info_t info;
};

b32 analyze_expression(analyzer_state_t *state, s64 expected_count_of_expressions, string_t *depend_on, ast_node_t *expr) {
    assert(state != NULL);
    assert(expr != NULL);

    b32 result = true;

    if (expr->type == AST_PRIMARY) switch (expr->token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOK_TRUE:
        case TOK_FALSE:
            expr->analyzed = true;
            return result;

        case TOKEN_IDENT: {
                string_t var_name = expr->token.data.string;

                stack_push(hashmap_get(&state->symbol_deps, stack_peek(&state->internal_deps)), var_name);
                scope_entry_t *output = NULL; 
                switch (get_if_exists(state, true, var_name, &output)) {
                    case GET_NOT_FIND:
                        log_error_token("Couldn't find identifier", expr->token);
                        result = false;
                        // @todo, break at all
                        break;

                    case GET_NOT_ANALYZED:
                        assert(false);
                        result = false;
                        break;

                    case GET_DEPS_ERROR:
                        result = false;
                        break;

                    case GET_SUCCESS: switch (output->type) {
                        case ENTRY_VAR:
                            if (!add_var_type_into_search(state, output)) {
                                result = false;
                            }

                            if (!output->uninit) {
                                break;
                            }

                            log_error_token("Usage of not created variable", expr->token);
                            result = false;
                            break;

                        case ENTRY_FUNC:
                            break;

                        case ENTRY_TYPE:
                            // @todo, @fix: who knows if we can...
                            log_error_token("Cant use Type in expression", expr->token);
                            result = false;
                            break;


                        case ENTRY_ERROR:
                            result = false;
                            break;

                        default:
                            log_error("Unexpected entry...");
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
        case AST_UNARY_INVERT:
            result = analyze_expression(state, expected_count_of_expressions, depend_on, expr->left);
            break;


        case AST_BIN_CAST:
            // left is type
            break;

        case AST_MEMBER_ACCESS:
        {
            u64 index = state->current_search_stack.index;

            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->left)) {
                result = false;
            }

            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->right)) {
                result = false;
            }

            state->current_search_stack.index = index;
        } break;

        case AST_ARRAY_ACCESS:
            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->left)) {
                result = false;
            }

            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->right)) {
                result = false;
            }
            break;
        case AST_FUNC_CALL:
            {
            u64 index = state->current_search_stack.index;

            // @todo, analyze amount of arguments and parameters
            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->left)) {
                result = false;
            }

            // analyze arguments
            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->right)) {
                result = false;
            }

            state->current_search_stack.index = index;
            } break;

        case AST_BIN_ASSIGN:

            // @todo... we should analyze left one, so we sure that we dont do
            // a + 12231 = 12;
            
            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->left)) {
                result = false;
            }

            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->right)) {
                result = false;
            }

            break;
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
            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->left)) {
                result = false;
            }

            if (!analyze_expression(state, expected_count_of_expressions, depend_on, expr->right)) {
                result = false;
            }
            break;

        case AST_BIN_SWAP: {
                s64 swap_count = expr->left->child_count;

                if (expr->left->child_count != expr->right->child_count) {
                    log_error_token("Different amount of parts on different sides of swap expression:", expr->token);
                    result = false;
                }

                if (!analyze_expression(state, swap_count, depend_on, expr->left)) {
                    result = false;
                }

                if (!analyze_expression(state, swap_count, depend_on, expr->right)) {
                    result = false;
                }
            } break;
        case AST_SEPARATION: {
            ast_node_t *next = expr->list_start;

            if (expected_count_of_expressions >= 0) {
                if (expr->child_count != (u64)expected_count_of_expressions) {
                    log_error_token("Not expected count of elements", expr->token);
                    result = false;
                    break;
                }
            }

            // element count should match other size or amount of return argumets
            for (u64 i = 0; i < expr->child_count; i++) {
                if (!analyze_expression(state, expected_count_of_expressions, depend_on, next)) 
                    result = false;

                next = next->list_next;
            }

        } break;
    }

    if (!result) {
        expr->analyzed = true;
    }

    return result;
}

b32 analyze_definition_expr(analyzer_state_t *state, scope_entry_t *entry) {
    assert(state       != NULL);
    assert(entry       != NULL);

    ast_node_t *expr = entry->expr;

    if (expr == NULL) {
        return entry->type != ENTRY_FUNC;
    }

    if (expr->analyzed) {
        return true;
    }

    if (entry->type == ENTRY_FUNC) {
        switch (expr->type) {
            case AST_NAMED_EXT_FUNC_INFO:
                entry->ext_name = string_copy(expr->right->token.data.string, state->compiler->strings);
            case AST_EXT_FUNC_INFO:
                entry->is_external = true;
                entry->ext_from = string_copy(expr->left->token.data.string,  state->compiler->strings);
                return true;

            case AST_BLOCK_IMPERATIVE:
                break;

            default:
                log_error_token("Wrong function body.", expr->token);
                return false;
        }


        stack_push(&state->current_search_stack, &entry->func_params);
        u64 curr_load = entry->func_params.load;
        b32 result = analyze_statement(state, entry->return_typenames.count, 0, false, expr);
        assert(curr_load == entry->func_params.load);
        stack_pop(&state->current_search_stack);
        expr->analyzed = true;

        return result;
    } else if (entry->type == ENTRY_VAR) {
        entry->uninit = true;

        // is hardcoding to 1, good?
        if (!analyze_expression(state, 1, &entry->node->token.data.string, expr)) {
            return false;
        }

        // @todo scopes in functions
        // here we add our variable to current scope...
        entry->uninit = false;
    }

    expr->analyzed = true;
    return true;
}

b32 analyze_definition(analyzer_state_t *state, b32 can_do_func, ast_node_t *node, ast_node_t *name, ast_node_t *type, ast_node_t *expr, b32 *should_wait) {
    assert(state != NULL);
    assert(name != NULL);
    assert(type != NULL);
    assert(should_wait != NULL);

    string_t key = name->token.data.string;

    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(&state->current_search_stack), key, name, &entry)) {
        return false;
    }

    entry->node = name;
    entry->stmt = node;
    entry->expr = expr;
    entry->info.pointer_depth = 0;

    b32 is_indirect = false;

    // @todo:
    // the problem is that if we have arrays, we should be able to create array of pointers or pointer to array of pointers...
    //

    if (type->type == AST_ARR_TYPE) {
        b32 result = analyze_expression(state, -1, NULL, type->left);

        if (!result) {
            log_error_token("Bad array initializer", type->left->token);
            return false;
        }

        // we need to set size... here @todo
        entry->info.is_array = true;
        type = type->right;
    }

    while (type->type == AST_PTR_TYPE || type->type == AST_ARR_TYPE) {
        if (type->type != AST_PTR_TYPE) {
            log_error_token("Cant create pointer to an array type.", type->token);
            return false;
        } 

        entry->info.pointer_depth++;
        type = type->left;
        is_indirect = true;
    }

    if (type->type != AST_UNKN_TYPE) {
        switch (type->type) {
            case AST_FUNC_TYPE: {
                if (!can_do_func) {
                    log_error("Cant do functions for multiple var decl...");
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
            } break;

            case AST_VOID_TYPE:
                if (!is_indirect) {
                    log_error_token("Cant make variable with void type...", type->token);
                    entry->type = ENTRY_ERROR;
                    entry->node->analyzed = true;
                    return false;
                }
            case AST_STD_TYPE:
                entry->type = ENTRY_VAR;
                set_std_info(type->token, &entry->info);
                break;

            case AST_MUL_AUTO:
            case AST_AUTO_TYPE:
                assert(expr != NULL);

                log_error_token("Cant evaluate the types right now...", type->token);
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
            case AST_ARR_TYPE:
            case AST_PTR_TYPE:
                assert(false);
                return false;

            default:
                log_error_token("unexpected type of ast node...", entry->node->token);
                entry->type = ENTRY_ERROR;
                entry->node->analyzed = true;
                return false;
        }
    } else {
        string_t type_name = type->token.data.string;

        if (!is_indirect && state->internal_deps.index > 0) {
            stack_push(hashmap_get(&state->symbol_deps, stack_peek(&state->internal_deps)), type_name);
        }

        scope_entry_t *output_type = NULL; 

        switch (get_if_exists(state, !is_indirect, type_name, &output_type)) {
            case GET_NOT_FIND:
            case GET_NOT_ANALYZED:
                *should_wait = true;
                break;

            case GET_DEPS_ERROR:
                if (!is_indirect) {
                    return false;
                }
            case GET_SUCCESS: 
            {
                switch (output_type->type) {
                    case ENTRY_TYPE:
                        entry->type = ENTRY_VAR;
                        break;

                    case ENTRY_FUNC: 
                        log_error("Cant use FUNC as a type of variable [@better_message]");
                        entry->type = ENTRY_ERROR;
                        entry->node->analyzed = true;
                        return false;

                    default:
                        log_error("Unexpected type...");
                        entry->type = ENTRY_ERROR;
                        entry->node->analyzed = true;
                        return false;
                }

                entry->info.size      = output_type->info.size;
                entry->info.type_name = type_name;
            } break;
        }
    }

    b32 result = true;

    // @cleanup: ugly
    if (state->state == STATE_CODE_ANALYSIS) {
        result = analyze_definition_expr(state, entry);
    }

    if (!*should_wait) {
        entry->node->analyzed = true;
    }

    return result;
}

b32 analyze_function(analyzer_state_t *state, scope_entry_t *entry, b32 *should_wait) {
    assert(state != NULL);
    assert(entry != NULL);
    assert(should_wait != NULL);

    ast_node_t *func = entry->node;
    ast_node_t *type_node  = func->left;
    assert(type_node->type  == AST_FUNC_TYPE);

    entry->expr = func->right;

    string_t key = entry->node->token.data.string;
    stack_push(&state->current_search_stack, &entry->func_params);
    stack_push(&state->internal_deps, key);

    {
        stack_t<string_t> symbol_deps = {};
        hashmap_add(&state->symbol_deps, key, &symbol_deps);
    }

    b32 result = true;
    ast_node_t *next_type = type_node->left->list_start;
    for (u64 i = 0; i < type_node->left->child_count; i++) {
        assert(next_type->type == AST_PARAM_DEF);

        if (!analyze_definition(state, false, entry->stmt, next_type, next_type->left, NULL, should_wait)) {
            result = false;
        }

        if (*should_wait) {
            break;
        }

        next_type = next_type->list_next;
    }

    next_type = type_node->right->list_start;
    for (u64 i = 0; i < type_node->right->child_count; i++) {
        ast_node_t *curr = next_type;

        if (next_type->analyzed) {
            next_type = next_type->list_next;
            continue;
        }

        type_info_t info = {};
        b32 is_indirect = false;

        if (curr->type == AST_ARR_TYPE) {
            b32 result = analyze_expression(state, -1, NULL, curr->left);

            if (!result) {
                log_error_token("Bad array initializer", curr->left->token);
                return false;
            }

            // we need to set size... here @todo
            info.is_array = true;
            curr = curr->right;
        }

        while (curr->type == AST_PTR_TYPE || curr->type == AST_ARR_TYPE) {
            if (curr->type != AST_PTR_TYPE) {
                log_error_token("Cant create pointer to an array type.", curr->token);
                result = false;
                break;
            } 

            info.pointer_depth++;
            curr = curr->left;
            is_indirect = true;
        }

        if (curr->type != AST_UNKN_TYPE) {
            switch (curr->type) {
                case AST_VOID_TYPE: // fallthrough
                    if (!is_indirect) {
                        log_error_token("Cant return void.", curr->token);
                        entry->type = ENTRY_ERROR;
                        result = false;
                        break;
                    }
                case AST_STD_TYPE:
                    set_std_info(curr->token, &info);
                    break;


                case AST_MUL_AUTO:
                case AST_AUTO_TYPE:
                case AST_PTR_TYPE: 
                case AST_ARR_TYPE:
                    assert(false);

                default:
                    log_error("unexpected type of ast node...");
                    result = false;
                    break;
            } 
        } else {
            string_t type_name = curr->token.data.string;

            if (!is_indirect && state->internal_deps.index > 0) {
                stack_push(hashmap_get(&state->symbol_deps, stack_peek(&state->internal_deps)), type_name);
            }

            scope_entry_t *output_type = NULL; 

            switch (get_if_exists(state, !is_indirect, type_name, &output_type)) {
                case GET_NOT_FIND:
                case GET_NOT_ANALYZED:
                    *should_wait = true;
                    break;

                case GET_DEPS_ERROR:
                    if (!is_indirect) {
                        return false;
                    }
                case GET_SUCCESS: 
                {
                    switch (output_type->type) {
                        case ENTRY_TYPE:
                            break;

                        case ENTRY_FUNC: 
                            log_error_token("Cant use FUNC as a return type [@better_message]", curr->token);
                            entry->type = ENTRY_ERROR;
                            result = false;
                            break;

                        default:
                            log_error_token("Unexpected type... [@better_message]", curr->token);
                            entry->type = ENTRY_ERROR;
                            result = false;
                            break;
                    }

                    info.type = TYPE_UNKN;
                    info.size = output_type->info.size;
                    info.type_name = type_name;
                } break;
            }
        } 

        if (!result || *should_wait)
            break;

        list_add(&entry->return_typenames, &info);
        next_type->analyzed = true;
        next_type = next_type->list_next;
    }

    stack_pop(&state->internal_deps);
    stack_pop(&state->current_search_stack);

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

    if (!analyze_definition(state, false, node, node, node->left, NULL, should_wait)) {
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

    if (!aquire_entry(stack_peek(&state->current_search_stack), key, node, &entry)) {
        return false;
    }

    entry->node = node;
    entry->type = ENTRY_VAR;

    log_error_token("TODO: enums", node->token);
    check_value(false);
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
            if (!analyze_definition(state, false, node, next, node->right, NULL, should_wait)) {
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

        log_error_token("This variable didn't have it's own type:", next->token);
        return false;
    }

    ast_node_t * name = node->left->list_start;
    ast_node_t * type = node->right->list_start;

    for (u64 i = 0; i < node->left->child_count; i++) {
        if (!analyze_definition(state, false, node, name, type, NULL, should_wait)) {
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

    if (!analyze_definition(state, true, node, node, node->left, node->right, should_wait)) {
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
            if (!analyze_definition(state, false, node, next, node->center, node->right->list_start, should_wait)) {
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

                log_error_token("Trailing type without its identifier:", next->token);
            } else {
                next = node->left->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }
                log_error_token("This variable didn't have it's type:", next->token);
            }

            node->analyzed = true;
            return false;
        }

        ast_node_t * name = node->left->list_start;
        ast_node_t * type = node->center->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, node, name, type, node->right->list_start, should_wait)) {
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

                log_error_token("Trailing expression without its identifier:", next->token);
            } else {
                next = node->left->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }
                log_error_token("This variable didn't have it's expression:", next->token);
            }

            node->analyzed = true;
            return false;
        }

        ast_node_t * name = node->left->list_start;
        ast_node_t * type = node->center;
        ast_node_t * expr = node->right->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, node, name, type, expr, should_wait)) {
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

            log_error_token("This variable didn't have it's own type:", next->token);
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

                log_error_token("Trailing expression without its variable:", next->token);
            } else {
                next = node->left->list_start;
                for (u64 i = 0; i < least_size; i++) {
                    next = next->list_next;
                }
                log_error_token("This variable didn't have it's expression:", next->token);
            }

            node->analyzed = true;
            return false;
        }

        ast_node_t * name = node->left->list_start;
        ast_node_t * type = node->center->list_start;
        ast_node_t * expr = node->right->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            if (!analyze_definition(state, false, node, name, type, expr, should_wait)) {
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
    assert(state != NULL);
    assert(should_wait != NULL);
    assert(entry != NULL);

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
                log_error_token("Cant use initialization in member declaration.", entry->node->token);
                result = false;
                break;

            default:
                log_error_token("Cant use this as member of type.", entry->node->token);
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

b32 analyze_struct(analyzer_state_t *state, ast_node_t *node) {
    assert(state != NULL);
    assert(node != NULL);
    assert(node->type == AST_STRUCT_DEF);

    if (node->analyzed) return true; 

    string_t key = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(&state->current_search_stack), key, node, &entry)) {
        return false;
    }

    entry->node = node;
    entry->type = ENTRY_TYPE;
    entry->def_type = DEF_TYPE_STRUCT;
    if (!entry->scope.entries) {
        entry->scope = create_scope();
    }

    b32 result = false;
    b32 should_wait = false;

    stack_push(&state->current_search_stack, &entry->scope);
    stack_push(&state->internal_deps, key);
    
    {
        stack_t<string_t> symbol_deps = {};
        hashmap_add(&state->symbol_deps, key, &symbol_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    stack_pop(&state->internal_deps);
    stack_pop(&state->current_search_stack);


    entry->info.type = TYPE_UNKN;

    log_warning("Struct sizes todo! analyzer.cpp l:1266");

    if (!should_wait) {
        node->analyzed = true;
    }

    return result;
}

b32 analyze_union(analyzer_state_t *state, ast_node_t *node) {
    assert(state != NULL);
    assert(node != NULL);
    assert(node->type == AST_UNION_DEF);

    if (node->analyzed) return true; 

    string_t key = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(&state->current_search_stack), key, node, &entry)) {
        return false;
    }

    entry->node  = node;
    entry->type  = ENTRY_TYPE;
    entry->def_type = DEF_TYPE_UNION;
    if (!entry->scope.entries) {
        entry->scope = create_scope();
    }

    b32 result = false;
    b32 should_wait = false;

    stack_push(&state->current_search_stack, &entry->scope);
    stack_push(&state->internal_deps, key);

    {
        stack_t<string_t> symbol_deps = {};
        hashmap_add(&state->symbol_deps, key, &symbol_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    stack_pop(&state->internal_deps);
    stack_pop(&state->current_search_stack);

    if (!should_wait) {
        node->analyzed = true;
    }

    return result;
}

b32 analyze_enum(analyzer_state_t *state, ast_node_t *node) {
    assert(state != NULL);
    assert(node != NULL);
    assert(node->type == AST_ENUM_DEF);
    if (node->analyzed) return true; 

    string_t key = node->token.data.string;
    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(&state->current_search_stack), key, node, &entry)) {
        return false;
    }

    entry->node     = node;
    entry->type     = ENTRY_TYPE;
    entry->def_type = DEF_TYPE_ENUM;

    if (!entry->scope.entries) {
        entry->scope = create_scope();
    }

    b32 result = false;
    b32 should_wait = false;

    stack_push(&state->current_search_stack, &entry->scope);
    stack_push(&state->internal_deps, key);

    {
        stack_t<string_t> symbol_deps = {};
        hashmap_add(&state->symbol_deps, key, &symbol_deps);
    }

    result = analyze_and_add_type_members(state, &should_wait, entry); 

    for (u64 i = 0; i < entry->scope.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> *pair = entry->scope.entries + i;

        if (!pair->occupied) continue;
        if (pair->deleted)   continue;

        set_std_info(node->right->token, &pair->value.info);
    }

    stack_pop(&state->internal_deps);
    stack_pop(&state->current_search_stack);

    if (!should_wait) {
        node->analyzed = true;
    }

    return result;
}


// --------------------------------------------------- FILE LOADING

#define GREEN_COLOR 63, 255, 63

string_t construct_source_name(string_t path, string_t name, allocator_t *alloc) {
    return string_swap(string_concat(path, string_concat(name, STRING(".slm"), alloc), alloc), SWAP_SLASH, (u8)HOST_SYSTEM_SLASH, alloc);
}

string_t construct_module_name(string_t path, string_t name, allocator_t *alloc) {
    return string_swap(string_concat(path, string_concat(name, STRING("/module.slm"), alloc), alloc), SWAP_SLASH, (u8)HOST_SYSTEM_SLASH, alloc);
}

b32 load_and_process_file(compiler_t *compiler, string_t filename) {
    assert(compiler != NULL);

    source_file_t file = create_source_file(compiler, NULL);

    string_t source;

    if (!platform_read_file_into_string(filename, default_allocator, &source)) {
        return false;
    }

    if (!scanner_open(&filename, &source, file.scanner)) {
        return false;
    }

    if (!hashmap_add(&compiler->files, filename, &file)) {
        log_error("Couldn't add file to work with.");
        return false;
    }

    if (!parse_file(compiler, filename)) {
        return false;
    }

    return true;
}

b32 add_file_if_exists(compiler_t *compiler, b32 *valid_file, string_t file) {
    assert(compiler != NULL);
    assert(valid_file != NULL);

    profiler_func_start();

    if (!platform_file_exists(file)) { 
        profiler_func_end();
        return false;
    } else { 
        *valid_file = load_and_process_file(compiler, string_copy(file, compiler->strings));
    }

    profiler_func_end();
    return true;
}

b32 find_and_add_file(compiler_t *compiler, ast_node_t *node) {
    assert(compiler != NULL);
    assert(node != NULL);
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

    log_error_token("Couldn't find corresponding file to this declaration.", node->token);
    node->analyzed = true;
    return false;
}

// --------------------

u32 analyze_statement(analyzer_state_t *state, u64 expect_return_amount, u32 scope_index, b32 in_loop, ast_node_t *node) {
    assert(state != NULL);
    assert(node != NULL);

    temp_reset();

    assert(!node->analyzed);

    u64 index = state->current_search_stack.index;

    u32 result = STMT_ERR;
    b32 should_wait = false;

    switch (node->type) {
        case AST_BLOCK_IMPERATIVE: 
            {
                result = STMT_OK;
                // @todo: here are all the trailing scopes go...
                // and here we will resolve our vairables
                ast_node_t *stmt = node->list_start;

                u32 new_index = 0;
                {
                    hashmap_t<string_t, scope_entry_t> block = {};
                    list_add(&state->compiler->scopes, &block);
                    new_index = state->compiler->scopes.count - 1;
                }

                node->scope_index = new_index;

                hashmap_t<string_t, scope_entry_t> *block = list_get(&state->compiler->scopes, new_index);
                stack_push(&state->current_search_stack, block);

                for (u64 i = 0; i < node->child_count; i++) {
                    switch (analyze_statement(state, expect_return_amount, new_index, false, stmt)) {
                        case STMT_BREAK:
                        case STMT_CONTINUE:
                            if (!in_loop) {
                                break;
                            }

                            log_error_token("Cant use break or continue outside of loop.", stmt->token);
                            result = STMT_ERR;
                            break;

                        case STMT_ERR:
                            result = STMT_ERR;
                            break;

                        case STMT_RETURN:
                            // @todo: more sophisticated logic of warnings
                            if (i != (node->child_count - 1)) {
                                log_warning_token("There are statemets after return.", stmt->token);
                            }
                            break;

                        case STMT_OK:
                            break;
                    }

                    stmt = stmt->list_next;
                }

                stack_pop(&state->current_search_stack);
            }
            break;
        case AST_UNNAMED_MODULE:
            log_error_token("cant load files from functions.", node->token);
            result = STMT_ERR;
            break;

        case AST_STRUCT_DEF: 
        case AST_UNION_DEF:
        case AST_ENUM_DEF: 
            log_error_token("cant create types not in global scope.", node->token);
            result = STMT_ERR;
            break;


        case AST_IF_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left)) {
                result = STMT_ERR;
                break;
            }

            result = analyze_statement(state, expect_return_amount, scope_index, in_loop, node->right);
            if (result != STMT_ERR) {
                result = STMT_OK;
            }
        } break;

        case AST_IF_ELSE_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left)) {
                result = STMT_ERR;
                break;
            }

            b32 lr = analyze_statement(state, expect_return_amount, scope_index, in_loop, node->center);
            b32 rr = analyze_statement(state, expect_return_amount, scope_index, in_loop, node->right);

            if (lr == rr && lr == STMT_RETURN) {
                result = STMT_RETURN;
            } else if (lr == STMT_ERR || rr == STMT_ERR) {
                result = STMT_ERR;
            } else {
                result = STMT_OK;
            }
        } break;

        case AST_WHILE_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left)) {
                result = STMT_ERR;
                break;
            }

            result = analyze_statement(state, expect_return_amount, scope_index, true, node->right);
        } break;

        case AST_RET_STMT: {
            if (!analyze_expression(state, expect_return_amount, NULL, node->left)) {
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
            // @todo: allow the analysis of variables
            result = analyze_unary_var_def(state, node, &should_wait) ? STMT_OK : STMT_ERR;
            break;

        case AST_BIN_UNKN_DEF: 
            result = analyze_unkn_def(state, node, &should_wait)? STMT_OK : STMT_ERR;
            break;

        case AST_BIN_MULT_DEF: 
            result = analyze_bin_var_def(state, node, &should_wait) ? STMT_OK : STMT_ERR;
            break;

        case AST_TERN_MULT_DEF:
            result = analyze_tern_def(state, node, &should_wait) ? STMT_OK : STMT_ERR;
            break;

        default: {
            result = analyze_expression(state, -1, NULL, node) ? STMT_OK : STMT_ERR;
        } break;
    }

    state->current_search_stack.index = index;
    node->analyzed = true;

    if (should_wait) {
        log_error_token("Unordered instructions in imperative block.", node->token);
        return false;
    }

    return result;
}

b32 analyze_global_statement(analyzer_state_t *state, ast_node_t *node) {
    temp_reset();

    assert(!node->analyzed);
    b32 result, should_wait = false;

    switch (node->type) {
        case AST_UNNAMED_MODULE: assert(false);

        case AST_STRUCT_DEF: 
            result = analyze_struct(state, node);
            break;

        case AST_UNION_DEF:
            result = analyze_union(state, node);
            break;

        case AST_ENUM_DEF: 
            result = analyze_enum(state, node);
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
            log_error_token("Wrong type of construct.", node->token);
            result = false;
            break;
    }

    return result;
}

b32 analyzer_preload_all_files(compiler_t *compiler) {
    profiler_func_start();
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
        log_error("Couldn't find a file.");
    } 
#ifdef DEBUG
    else {
        log_info("All files loaded!");
    }
#endif

    profiler_func_end();
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
    assert(compiler != NULL);
    allocator_t *talloc = get_temporary_allocator();

    assert(depth <= 8); // cause we have some hardcoded offset down there...

    add_left_pad(stderr, depth * 4);
    fprintf(stderr, " -> %-*s", (int)(32 - depth * 4), string_to_c_string(pair.key, talloc));


    hashmap_t<string_t, scope_entry_t> *scope = &pair.value.scope;

    if (pair.value.type == ENTRY_VAR) {
        fprintf(stderr, " VAR");
        print_var_info(pair.value.info);
    } else if (pair.value.type == ENTRY_TYPE) {
        fprintf(stderr, " TYPE");
    } else if (pair.value.type == ENTRY_FUNC) {
        fprintf(stderr, " FUNC");
        scope = &pair.value.func_params;
    }

    fprintf(stderr, "\n");

    for (u64 j = 0; j < scope->capacity; j++) {
        kv_pair_t<string_t, scope_entry_t> member = scope->entries[j];

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
    assert(compiler != NULL);
    log_update_color();

    fprintf(stderr, "--------GLOBAL-SCOPE---------\n");

    hashmap_t<string_t, scope_entry_t> *scope = list_get(&compiler->scopes, 0);

    for (u64 i = 0; i < scope->capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = scope->entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        print_entry(compiler, pair, 0);
    }

    fprintf(stderr, "-----------------------------\n");
}

analyzer_state_t init_state(compiler_t *compiler) {
    assert(compiler != NULL);

    stack_t<hashmap_t<string_t, scope_entry_t>*> current_search_stack = {};
    stack_t<string_t> internal_deps = {};
    stack_create(&internal_deps, 16);

    hashmap_t<string_t, stack_t<string_t>> symbol_deps = {};
    hashmap_create(&symbol_deps, 128, NULL, NULL);

    analyzer_state_t state = {};

    state.compiler = compiler;
    state.current_search_stack = current_search_stack;
    state.internal_deps = internal_deps;
    state.symbol_deps = symbol_deps;

    return state;
}

void clear_state(analyzer_state_t *state) {
    // @todo: delete all the entries inside
    hashmap_delete(&state->symbol_deps);

    assert(state->current_search_stack.index == 0);

    stack_delete(&state->current_search_stack);
}

b32 analyze_global_statements(analyzer_state_t *state, compiler_t *compiler) {
    b32 result       = true;
    b32 not_finished = true;

    u64 max_iterations = 10;
    while (max_iterations-- > 0 && not_finished) {
        not_finished = false;

        for (u64 i = 0; i < compiler->files.capacity; i++) {
            kv_pair_t<string_t, source_file_t> pair = compiler->files.entries[i];

            if (!pair.occupied) continue;
            if (pair.deleted)   continue;

            // we just need to try to compile, if we cant resolve something we add it, but just
            for (u64 j = 0; j < pair.value.parsed_roots.count; j++) {
                ast_node_t *node = *list_get(&pair.value.parsed_roots, j);

                if (node->analyzed) {
                    continue;
                } 

                not_finished = true;

                if (!analyze_global_statement(state, node)) {
                    result = false;
                }
            }

            if (!result) {
                not_finished = false;
                break;
            }
        }
    }

    if (not_finished) {
        log_error("Probably undefined symbols...");
        return false;
    }

    return result;
}

b32 analyze_code(analyzer_state_t *state, compiler_t *compiler) {
    b32 result = true;
    hashmap_t<string_t, scope_entry_t> *scope = list_get(&compiler->scopes, 0);

    for (u64 i = 0; i < scope->capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> *pair = scope->entries + i;

        if (!pair->occupied)      continue;
        if (pair->deleted)        continue;
        profiler_block_start(STRING("Code analyze step"));

        string_t key = pair->value.node->token.data.string;
        stack_push(&state->internal_deps, key);

        {
            stack_t<string_t> symbol_deps = {};
            hashmap_add(&state->symbol_deps, key, &symbol_deps);
        }

        if (!analyze_definition_expr(state, &pair->value)) {
            result = false;
        }

        assert(state->current_search_stack.index == 1);
        stack_pop(&state->internal_deps);
        profiler_block_end();
    }

    return result;
}

b32 analyze(compiler_t *compiler) {
    assert(compiler != NULL);
    b32 result = false;

    analyzer_state_t state = init_state(compiler);

    hashmap_t<string_t, scope_entry_t> *scope = list_get(&compiler->scopes, 0);

    stack_push(&state.current_search_stack, scope);
    {
        profiler_block_start(STRING("Global analysis"));
        state.state = STATE_GLOBAL_ANALYSIS;
        result = analyze_global_statements(&state, compiler);
        profiler_block_end();

        if (!result) {
            log_error("Errors on early step of analysis.");
            return false;
        }

        // @fix @todo: we get here memory leak!!
        hashmap_clear(&state.symbol_deps);
        state.internal_deps.index = 0;
        assert(state.current_search_stack.index == 1);

        profiler_block_start(STRING("Code analysis"));
        state.state = STATE_CODE_ANALYSIS;
        result = analyze_code(&state, compiler);
        profiler_block_end();

        if (!result) {
            log_error("Errors while analyzing code.");
            return false;
        }
    }
    stack_pop(&state.current_search_stack);
    clear_state(&state);
    // print_all_definitions(compiler);

    return result;
}
