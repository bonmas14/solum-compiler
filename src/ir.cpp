#include "compiler.h"
#include "array.h"
#include "parser.h"
#include "ir.h"

#include "allocator.h"
#include "talloc.h"
#include "arena.h"
#include "strings.h"
#include "memctl.h"

// just for now this will be here
// #define VERBOSE

#define EXPR_CASE(cs, tok) case cs:\
            compile_expression(state, node->right, shadow);\
            compile_expression(state, node->left,  shadow);\
            emit_op(state, tok, node->token, 0);\
            break;

struct ir_state_t {
    compiler_t *compiler;
    ir_t ir;
    ir_function_t *current_function;
    stack_t<ir_opcode_t*> continue_stmt;
    stack_t<ir_opcode_t*> break_stmt;
    stack_t<ast_node_t*>  reverse;
    stack_t<hashmap_t<string_t, scope_entry_t>*> search_scopes;
    list_t<hashmap_t<string_t, scope_entry_t>>   local_scopes;
};

// ------ //

const char* ir_code_to_string(u64 code) {
    switch (code) {
        case IR_NOP:         return "NOP";
        case IR_PUSH_SIGN:   return "PUSH_SIGN";
        case IR_PUSH_UNSIGN: return "PUSH_UNSIGN";
        case IR_PUSH_F32:    return "PUSH_F32";
        case IR_PUSH_F64:    return "PUSH_F64";
        case IR_POP:         return "POP";
        case IR_CLONE:       return "CLONE";
        case IR_GLOBAL:      return "GLOBAL";
        case IR_ALLOC:       return "ALLOC";
        case IR_FREE:        return "FREE";
        case IR_LOAD:        return "LOAD";
        case IR_STORE:       return "STORE";
        case IR_LEA:         return "LEA";
        case IR_ADD:         return "ADD";
        case IR_SUB:         return "SUB";
        case IR_MUL:         return "MUL";
        case IR_DIV:         return "DIV";
        case IR_MOD:         return "MOD";
        case IR_NEG:         return "NEG";
        case IR_BIT_AND:     return "BIT_AND";
        case IR_BIT_OR:      return "BIT_OR";
        case IR_BIT_XOR:     return "BIT_XOR";
        case IR_BIT_NOT:     return "BIT_NOT";
        case IR_SHIFT_LEFT:  return "SHIFT_LEFT";
        case IR_SHIFT_RIGHT: return "SHIFT_RIGHT";
        case IR_CMP_EQ:      return "CMP_EQ";
        case IR_CMP_NEQ:     return "CMP_NEQ";
        case IR_CMP_LT:      return "CMP_LT";
        case IR_CMP_GT:      return "CMP_GT";
        case IR_CMP_LTE:     return "CMP_LTE";
        case IR_CMP_GTE:     return "CMP_GTE";
        case IR_LOG_AND:     return "LOG_AND";
        case IR_LOG_OR:      return "LOG_OR";
        case IR_LOG_NOT:     return "LOG_NOT";
        case IR_JUMP:        return "JUMP";
        case IR_JUMP_IF:     return "JUMP_IF";
        case IR_JUMP_IF_NOT: return "JUMP_IF_NOT";
        case IR_RET:         return "RET";
        case IR_CALL:        return "CALL";
        case IR_CALL_EXTERN: return "CALL_EXTERN";
        case IR_BRK:         return "BRK";
        case IR_INVALID:     return "INVALID";
        default:             return "UNKNOWN_OP";
    }
}

void print_ir_opcode(ir_opcode_t op) {
    const char* op_name = ir_code_to_string(op.operation);
    
    log_update_color();
    if (op.s_operand != 0) {
        printf("[IR] | %-14s | %lld\n",
                op_name,
                (long long)op.s_operand);
    } else {
        printf("[IR] | %-14s | %.*s\n",
                op_name,
                op.string.data == NULL ? 0 : (int)op.string.size,
                (char*)op.string.data);
    }
}

#ifdef NDEBUG
#define print_ir_opcode(...)
#endif

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, u64 data) {
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.u_operand = data;

    array_add(&state->current_function->code, o);
    ir_opcode_t *out = array_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, s64 data, u64 dummy) {
    UNUSED(dummy);
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.s_operand = data;

    array_add(&state->current_function->code, o);
    ir_opcode_t *out = array_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, string_t data) {
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.string = data;

    array_add(&state->current_function->code, o);
    ir_opcode_t *out = array_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

// ------ // 

void compile_statement(ir_state_t *state, ast_node_t *node);

scope_entry_t *search_identifier(ir_state_t *state, string_t key, string_t shadow) {
    b32 shadowed = false;
    for (s64 i = (state->search_scopes.index - 1); i >= 0; i--) {
        hashmap_t<string_t, scope_entry_t> *search_scope = state->search_scopes.data[i];
        
        if (!hashmap_contains(search_scope, key))
            continue;

        if (!shadowed && string_compare(shadow, key) == 0) {
            shadowed = true;
            continue;
        }

        return hashmap_get(search_scope, key);
    }

    assert(false);
    return NULL;
}

b32 add_identifier_type_to_search(ir_state_t *state, string_t key) {
    scope_entry_t *entry = search_identifier(state, key, {});
    if (entry->info.type != TYPE_UNKN) {
        return false;
    }

    string_t type_name   = entry->info.type_name;
    scope_entry_t *type  = search_identifier(state, type_name, {});

    switch (type->type) {
        case ENTRY_VAR:
        case ENTRY_FUNC:
            assert(false); 
            break;

        case ENTRY_TYPE:
            stack_push(&state->search_scopes, &type->scope);
            break;

        default:
            assert(false);
            break;
    }

    return true;
}

struct ir_expression_t {
    b32            accessable;
    u32            offset;
    ir_opcode_t   *emmited_op;
    // type info later...
    stack_t<hashmap_t<string_t, scope_entry_t>*> search_info;
};

ir_expression_t compile_expression(ir_state_t *state, ast_node_t *node, string_t shadow) {
    assert(state->current_function != NULL);
    UNUSED(state);
    UNUSED(node);

    ir_expression_t expr = {};

    switch (node->type) {
        EXPR_CASE(AST_BIN_GR, IR_CMP_GT);
        EXPR_CASE(AST_BIN_LS, IR_CMP_LT);
        EXPR_CASE(AST_BIN_GEQ, IR_CMP_GTE);
        EXPR_CASE(AST_BIN_LEQ, IR_CMP_LTE);
        EXPR_CASE(AST_BIN_EQ, IR_CMP_EQ);
        EXPR_CASE(AST_BIN_NEQ, IR_CMP_NEQ);
        EXPR_CASE(AST_BIN_LOG_OR, IR_LOG_OR);
        EXPR_CASE(AST_BIN_LOG_AND, IR_LOG_AND);
        EXPR_CASE(AST_BIN_ADD, IR_ADD);
        EXPR_CASE(AST_BIN_SUB, IR_SUB);
        EXPR_CASE(AST_BIN_MUL, IR_MUL);
        EXPR_CASE(AST_BIN_DIV, IR_DIV);
        EXPR_CASE(AST_BIN_MOD, IR_MOD);
        EXPR_CASE(AST_BIN_BIT_XOR, IR_BIT_XOR);
        EXPR_CASE(AST_BIN_BIT_OR, IR_BIT_OR);
        EXPR_CASE(AST_BIN_BIT_AND, IR_BIT_AND);
        EXPR_CASE(AST_BIN_BIT_LSHIFT, IR_SHIFT_LEFT);
        EXPR_CASE(AST_BIN_BIT_RSHIFT, IR_SHIFT_RIGHT);

        case AST_PRIMARY: switch (node->token.type)
            {
                case TOKEN_CONST_FP:
                    log_error("AST_PRIMARY:TOKEN_IDENT compilation TODO");
                    emit_op(state, IR_INVALID, node->token, 0);
                    break;
                case TOKEN_CONST_INT:
                    emit_op(state, IR_PUSH_SIGN, node->token, (s64)node->token.data.const_int);
                    break;
                case TOKEN_CONST_STRING:
                    log_error("AST_PRIMARY:TOKEN_STRING compilation TODO");
                    emit_op(state, IR_INVALID, node->token, 0);
                    break;
                case TOKEN_IDENT: 
                    {
                        scope_entry_t *entry = search_identifier(state, node->token.data.string, shadow);

                        if (entry->type == ENTRY_TYPE) {
                            log_error_token("Cant use types in expression", node->token);
                            emit_op(state, IR_INVALID, node->token, 0);
                            state->ir.is_valid = false;
                            break;
                        }

                        expr.accessable = true;
                        expr.emmited_op = emit_op(state, IR_GLOBAL, node->token, node->token.data.string);
                        // @todo, this should be already created stuff...
                        return expr;
                    } break;
                case TOK_TRUE:
                    emit_op(state, IR_PUSH_SIGN, node->token, 1);
                    break;
                case TOK_FALSE:
                    emit_op(state, IR_PUSH_SIGN, node->token, 0);
                    break;
                default:
                    emit_op(state, IR_INVALID, node->token, 0xFFFFFFFFFFFFFFFF);
                    break;
            } break;
            break;

        case AST_UNARY_REF:
            expr = compile_expression(state, node->left, shadow);

            if (!expr.accessable) {
                log_error_token("Cant get address of unknown variable", node->left->token);
                state->ir.is_valid = false;
            } else {
                expr.emmited_op->operation = IR_LEA;
                expr.emmited_op->search_info = expr.search_info;
            }
            break;

        case AST_UNARY_DEREF:
            compile_expression(state, node->left, shadow);
            expr.emmited_op = emit_op(state, IR_LOAD, node->token, 0);
            expr.accessable = true;
            return expr;

        case AST_UNARY_INVERT:
            compile_expression(state, node->left, shadow);
            emit_op(state, IR_BIT_NOT, node->token, 0);
            break;

        case AST_UNARY_NEGATE:
            compile_expression(state, node->left, shadow);
            emit_op(state, IR_NEG, node->token, 0);
            break;

        case AST_UNARY_NOT:
            compile_expression(state, node->left, shadow);
            emit_op(state, IR_LOG_NOT, node->token, 0);
            break;

        case AST_BIN_CAST:
            // @todo
            //
            // log_error("AST_BIN_CAST compilation TODO");
            compile_expression(state, node->right, shadow);
            // compile_expression(state, node->left);
            // emit_op(state, IR_INVALID, node->token, 0);
            break;

        case AST_FUNC_CALL:
            compile_expression(state, node->right, shadow);
            expr = compile_expression(state, node->left, shadow);
            expr.emmited_op->operation = IR_CALL;
            break;

        case AST_MEMBER_ACCESS:
            assert(node->left->type       == AST_PRIMARY);
            assert(node->left->token.type == TOKEN_IDENT);

            if (!add_identifier_type_to_search(state, node->left->token.data.string)) {
                log_error_token("Identifier is a primitive type: ", node->left->token);
                state->ir.is_valid = false;
                stack_pop(&state->search_scopes);
                break;
            }

            expr = compile_expression(state, node->right, shadow);

            if (!expr.accessable) {
                log_error_token("Bad member access expression: ", node->left->token);
                state->ir.is_valid = false;
                stack_pop(&state->search_scopes);
                break;
            }

            if (expr.emmited_op->operation != IR_GLOBAL) {
                log_error_token("Bad access target: ", node->left->token);
                state->ir.is_valid = false;
                stack_pop(&state->search_scopes);
                break;
            }

            // expr.emmited_op.string = ;

            for (u64 i = 0; i < state->search_scopes.index; i++) {
                stack_push(&expr.search_info, state->search_scopes.data[i]);
            }

            stack_pop(&state->search_scopes);
            return expr;

        case AST_ARRAY_ACCESS: // @todo finish
            log_error("Cant use arrays");
            break;

            compile_expression(state, node->right, shadow);
            expr = compile_expression(state, node->left, shadow);

            if (!expr.accessable) {
                log_error_token("Bad array access expression: ", node->left->token);
                state->ir.is_valid = false;
            } else {
                expr.emmited_op->operation = IR_STORE;
                expr.emmited_op->search_info = expr.search_info;
            }
            return expr;

        case AST_BIN_ASSIGN:
            compile_expression(state, node->right, shadow);
            expr = compile_expression(state, node->left, shadow);

            if (!expr.accessable) {
                log_error_token("Bad assignment expression: ", node->left->token);
                state->ir.is_valid = false;
            } else {
                expr.emmited_op->operation = IR_LEA;
                expr.emmited_op->search_info = expr.search_info;
                emit_op(state, IR_STORE, node->left->token, 0);
            }
            break;

        case AST_BIN_SWAP:
            {
                compile_expression(state, node->right, shadow);
                ast_node_t *next = node->left->list_start;

                for (u64 i = 0; i < node->left->child_count; i++) {
                    ir_expression_t expr = compile_expression(state, next, shadow);

                    if (!expr.accessable) {
                        log_error_token("Bad swap part: ", next->token);
                        state->ir.is_valid = false;
                    } else {
                        expr.emmited_op->operation = IR_LEA;
                        expr.emmited_op->search_info = expr.search_info;
                        emit_op(state, IR_STORE, next->token, 0);
                    }

                    next = next->list_next;
                }
            }
            break;

        case AST_SEPARATION:
            {
                ast_node_t *next = node->list_start;

                state->reverse.index = 0;
                for (u64 i = 0; i < node->child_count; i++) {
                    stack_push(&state->reverse, next);
                    next = next->list_next;
                }

                for (u64 i = 0; i < node->child_count; i++) {
                    compile_expression(state, stack_pop(&state->reverse), shadow);
                }
            }
            break;

        default:
            log_error("UNKN!!!");
            break;
    }

    if (expr.search_info.data != NULL) {
        stack_delete(&expr.search_info);
    }

    return {};
}

void compile_block(ir_state_t *state, ast_node_t *node) {
    hashmap_t<string_t, scope_entry_t> *block = array_get(&state->compiler->scopes, node->scope_index);
    stack_push(&state->search_scopes, block);

    ast_node_t *stmt = node->list_start;

    for (u64 i = 0; i < node->child_count; i++) {
        compile_statement(state, stmt);
        stmt = stmt->list_next;
    }

    stack_pop(&state->search_scopes);
}

void compile_variable(ir_state_t *state, ast_node_t *node) {
    assert(state->current_function != NULL);

    scope_entry_t *entry = search_identifier(state, node->token.data.string, {});

    if (entry->expr) {
        ir_expression_t expr = compile_expression(state, entry->expr, node->token.data.string);
        UNUSED(expr);
    } else {
        emit_op(state, IR_PUSH_UNSIGN, node->token, 0);
    }

    ir_variable_t var = {};
    var.size      = 8;
    var.alignment = 8;
    state->current_function->vars += var;

    emit_op(state, IR_ALLOC, node->token, var.size); // here we get the type size
    emit_op(state, IR_STORE, node->token, 0);
}

void compile_mul_variables(ir_state_t *state, ast_node_t *node) {
    assert(state->current_function != NULL);

    ast_node_t *next = node->list_start;
    for (u64 i = 0; i < node->child_count; i++) {
        scope_entry_t *entry = search_identifier(state, next->token.data.string, {});

        if (entry->expr) {
            ir_expression_t expr = compile_expression(state, entry->expr, next->token.data.string);
            UNUSED(expr);
        } else {
            emit_op(state, IR_PUSH_UNSIGN, node->token, 0);
        }

        ir_variable_t var = {};
        var.size      = 8;
        var.alignment = 8;
        state->current_function->vars += var;

        emit_op(state, IR_ALLOC, node->token, var.size); // here we get the type size
        emit_op(state, IR_STORE, node->token, 0);

        next = next->list_next;
    }
}

void compile_statement(ir_state_t *state, ast_node_t *node) {
    switch (node->type) {
        case AST_BIN_UNKN_DEF:  compile_variable(state, node); break;
        case AST_UNARY_VAR_DEF: compile_variable(state, node); break;
        case AST_BIN_MULT_DEF:  compile_mul_variables(state, node); break;
        case AST_TERN_MULT_DEF: compile_mul_variables(state, node); break;

        case AST_BLOCK_IMPERATIVE: compile_block(state, node); break;

        case AST_IF_STMT: 
            {
                compile_expression(state, node->left, {});
                ir_opcode_t *end = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);
                compile_block(state, node->right);
                end->s_operand = state->current_function->code.count - 1 - end->index; // @todo, this could be a problem...
            }
            break;
        case AST_IF_ELSE_STMT:
            {
                ir_opcode_t *branch, *end;
                compile_expression(state, node->left, {});
                branch = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);

                compile_block(state, node->center);

                end = emit_op(state, IR_JUMP, node->token, 0);
                branch->s_operand = end->index - branch->index;

                compile_statement(state, node->right);
                end->s_operand = state->current_function->code.count - 1 - end->index; // @todo, this could be a problem...
            }
            break;
        case AST_WHILE_STMT: 
            {
                ir_opcode_t *end;
                
                u64 pos = state->current_function->code.count;

                u64 start_continue_index = state->continue_stmt.index;
                u64 start_break_index    = state->break_stmt.index;

                compile_expression(state, node->left, {});
                end = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);
                compile_block(state, node->right);
                emit_op(state, IR_JUMP, node->token, -((state->current_function->code.count + 1) - pos), 0);
                end->s_operand = state->current_function->code.count - end->index; // @todo, this could be a problem...
            
                while (start_continue_index < state->continue_stmt.index) {
                    ir_opcode_t *op = stack_pop(&state->continue_stmt);
                    op->s_operand = state->current_function->code.count - 1 - op->index;
                }

                while (start_break_index < state->break_stmt.index) {
                    ir_opcode_t *op = stack_pop(&state->break_stmt);
                    op->s_operand = state->current_function->code.count - 1 - op->index;
                }
            }
            break;
        case AST_RET_STMT: 
            {
                compile_expression(state, node->left, {});
                emit_op(state, IR_RET, node->token, 0);
            }
            break;
        case AST_BREAK_STMT: 
            {
                stack_push(&state->break_stmt, emit_op(state, IR_JUMP, node->token, 0)); // jump outa loop
            }
            break;
        case AST_CONTINUE_STMT:
            {
                stack_push(&state->continue_stmt, emit_op(state, IR_JUMP, node->token, 0)); // jump to start of loop
            }
            break;

        default:
            compile_expression(state, node, {});
            break;
    }
}

void compile_function(ir_state_t *state, string_t key, scope_entry_t *entry) {
    if (entry->is_external) {
        return;
    }  

    assert(state->current_function == NULL);

    ir_function_t func = {};

    array_create(&func.code, 8, state->ir.code);
    state->current_function = &func;
    stack_push(&state->search_scopes, &entry->func_params);
    {
        //                      def -> type -> params
        ast_node_t *node = entry->node->left->left;
        ast_node_t *next = node->list_start;

        for (u64 i = 0; i < node->child_count; i++) {
            scope_entry_t *entry = search_identifier(state, next->token.data.string, {});

            UNUSED(entry); // here we check types, etc
            
            ir_variable_t var = {};
            var.size      = 8;
            var.alignment = 8;
            func.vars += var;

            emit_op(state, IR_ALLOC, node->token, var.size); 
            emit_op(state, IR_STORE, node->token, 0);

            next = next->list_next;
        }

        compile_block(state, entry->expr);

        if (entry->return_typenames.count == 0) {
            emit_op(state, IR_RET, entry->node->token, 0);
        } else {
            emit_op(state, IR_INVALID, entry->node->token, 0);
        }

#ifdef VERBOSE
        u64 last_line = 0;

        for (u64 i = 0; i < func.code.count; i++) {
            if (last_line != func.code[i].info.l0) {
                log_write("\n");
                print_lines_of_code(func.code[i].info, 0, 0, 0);
                last_line = func.code[i].info.l0;
            }

            print_ir_opcode(func.code[i]);
        }
#endif
    }
    stack_pop(&state->search_scopes);
    state->current_function = NULL;
    hashmap_add(&state->ir.functions, key, &func);
}

void evaluate_variable(ir_state_t *state, scope_entry_t *entry) {
    UNUSED(state);
    UNUSED(entry);
}

void compile_global_statement(ir_state_t *state, string_t key, scope_entry_t *entry) {
    switch (entry->stmt->type) {
        case AST_BIN_UNKN_DEF: 
            if (entry->type == ENTRY_FUNC) {
                compile_function(state, key, entry);
                break;
            }
            
            evaluate_variable(state, entry); // also evaluate expression...
            break;

        case AST_UNARY_VAR_DEF: evaluate_variable(state, entry); break;
        case AST_BIN_MULT_DEF:  evaluate_variable(state, entry); break;
        case AST_TERN_MULT_DEF: evaluate_variable(state, entry); break;
        default: break;
    }
}

ir_t compile_program(compiler_t *compiler) {
    UNUSED(compiler);

    assert(compiler != NULL);

    ir_state_t state = {};

    state.ir.code     = create_arena_allocator(1024 * sizeof(ir_opcode_t));
    state.compiler    = compiler;
    state.ir.is_valid = true;

    hashmap_t<string_t, scope_entry_t> *scope = array_get(&compiler->scopes, 0);

    stack_push(&state.search_scopes, scope);

    for (u64 i = 0; i < scope->capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = scope->entries[i];
    
        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        if (pair.value.type == ENTRY_TYPE) {
            continue;
        }

        compile_global_statement(&state, pair.key, &pair.value);
    }

    stack_delete(&state.search_scopes);

    return state.ir;
}

