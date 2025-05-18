#include "compiler.h"
#include "parser.h"
#include "ir.h"

#include "allocator.h"
#include "talloc.h"
#include "strings.h"
#include "memctl.h"

#define EXPR_CASE(cs, tok) case cs:\
            compile_expression(state, node->right, should_shadow);\
            compile_expression(state, node->left, should_shadow);\
            emit_op(state, tok, node->token, 0);\
            break;

struct ir_state_t {
    compiler_t *compiler;
    ir_t ir;
    ir_function_t *current_function;
    stack_t<ir_opcode_t*> continue_stmt;
    stack_t<ir_opcode_t*> break_stmt;
    stack_t<hashmap_t<string_t, scope_entry_t>*> search_scopes;
    list_t<hashmap_t<string_t, scope_entry_t>>   local_scopes;
};

// ------ //

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, u64 data) {
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.u_operand = data;
    list_add(&state->current_function->code, &o);
    ir_opcode_t *out = list_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, s64 data, u64 dummy) {
    UNUSED(dummy);
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.s_operand = data;
    list_add(&state->current_function->code, &o);
    ir_opcode_t *out = list_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, string_t data) {
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.string = data;
    list_add(&state->current_function->code, &o);
    ir_opcode_t *out = list_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

const char* ir_code_to_string(u64 code) {
    switch (code) {
        case IR_NOP:         return "IR_NOP";
        case IR_PUSH_SIGN:   return "IR_PUSH_SIGN";
        case IR_PUSH_UNSIGN: return "IR_PUSH_UNSIGN";
        case IR_PUSH_F32:    return "IR_PUSH_F32";
        case IR_PUSH_F64:    return "IR_PUSH_F64";
        case IR_POP:         return "IR_POP";
        case IR_CLONE:       return "IR_CLONE";
        case IR_SWAP:        return "IR_SWAP";
        case IR_GLOBAL:      return "IR_GLOBAL";
        case IR_ALLOC:       return "IR_ALLOC";
        case IR_FREE:        return "IR_FREE";
        case IR_LOAD:        return "IR_LOAD";
        case IR_STORE:       return "IR_STORE";
        case IR_LEA:         return "IR_LEA";
        case IR_ADD:         return "IR_ADD";
        case IR_SUB:         return "IR_SUB";
        case IR_MUL:         return "IR_MUL";
        case IR_DIV:         return "IR_DIV";
        case IR_MOD:         return "IR_MOD";
        case IR_NEG:         return "IR_NEG";
        case IR_BIT_AND:     return "IR_BIT_AND";
        case IR_BIT_OR:      return "IR_BIT_OR";
        case IR_BIT_XOR:     return "IR_BIT_XOR";
        case IR_BIT_NOT:     return "IR_BIT_NOT";
        case IR_SHIFT_LEFT:  return "IR_SHIFT_LEFT";
        case IR_SHIFT_RIGHT: return "IR_SHIFT_RIGHT";
        case IR_CMP_EQ:      return "IR_CMP_EQ";
        case IR_CMP_NEQ:     return "IR_CMP_NEQ";
        case IR_CMP_LT:      return "IR_CMP_LT";
        case IR_CMP_GT:      return "IR_CMP_GT";
        case IR_CMP_LTE:     return "IR_CMP_LTE";
        case IR_CMP_GTE:     return "IR_CMP_GTE";
        case IR_LOG_AND:     return "IR_LOG_AND";
        case IR_LOG_OR:      return "IR_LOG_OR";
        case IR_LOG_NOT:     return "IR_LOG_NOT";
        case IR_JUMP:        return "IR_JUMP";
        case IR_JUMP_IF:     return "IR_JUMP_IF";
        case IR_JUMP_IF_NOT: return "IR_JUMP_IF_NOT";
        case IR_RET:         return "IR_RET";
        case IR_CALL:        return "IR_CALL";
        case IR_CALL_EXTERN: return "IR_CALL_EXTERN";
        case IR_BRK:         return "IR_BRK";
        case IR_INVALID:     return "IR_INVALID";
        default:             return "UNKNOWN_OP";
    }
}

void print_ir_opcode(ir_opcode_t op) {
    const char* op_name = ir_code_to_string(op.operation);
    
    log_update_color();
    printf("%10u: [IR] %-15s | Operand: 0x%016llx | %4lld | %.*s\n",
           op.info.l1,
           op_name,
           (unsigned long long)op.u_operand,
           (long long)op.s_operand,
           op.string.data == NULL ? 0 : (int)op.string.size,
           (char*)op.string.data);
}

// ------ // 

void compile_statement(ir_state_t *state, ast_node_t *node);

scope_entry_t *search_identifier(ir_state_t *state, string_t key, b32 shadow = false) {
    for (s64 i = (state->search_scopes.index - 1); i >= 0; i--) {
        hashmap_t<string_t, scope_entry_t> *search_scope = state->search_scopes.data[i];
        
        if (!hashmap_contains(search_scope, key))
            continue;

        if (shadow) {
            shadow = false;
            continue;
        }

        return hashmap_get(search_scope, key);
    }

    assert(false);
    return NULL;
}

void add_identifier_type_to_search(ir_state_t *state, string_t key) {
    scope_entry_t *entry = search_identifier(state, key);
    string_t type_name  = entry->info.type_name;
    scope_entry_t *type = search_identifier(state, type_name);

    switch (type->type) {
        case ENTRY_VAR:
        case ENTRY_FUNC:
            assert(false); 
            return;

        case ENTRY_TYPE:
            stack_push(&state->search_scopes, &type->scope);
            break;

        default:
            assert(false);
            return;
    }
}

struct ir_expression_t {
    b32            accessable;
    ir_opcode_t   *emmited_op;
    // type info later...
    stack_t<hashmap_t<string_t, scope_entry_t>*> search_info;
};

ir_expression_t compile_expression(ir_state_t *state, ast_node_t *node, b32 should_shadow) {
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
                        scope_entry_t *entry = search_identifier(state, node->token.data.string, should_shadow);

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
                    emit_op(state, IR_INVALID, node->token, 0xBBBBBBBBBBBBBBBB);
                    break;
            } break;
            break;

        case AST_UNARY_REF:
            expr = compile_expression(state, node->left, should_shadow);

            if (!expr.accessable) {
                log_error_token("Cant get address of unknown variable", node->left->token);
                state->ir.is_valid = false;
                break;
            } else {
                expr.emmited_op->operation = IR_LEA;
                expr.emmited_op->search_info = expr.search_info;
            }
            break;

        case AST_UNARY_DEREF:
            compile_expression(state, node->left, should_shadow);
            expr.emmited_op = emit_op(state, IR_LOAD, node->token, 0);
            expr.accessable = true;
            return expr;

        case AST_UNARY_INVERT:
            compile_expression(state, node->left, should_shadow);
            emit_op(state, IR_BIT_NOT, node->token, 0);
            break;

        case AST_UNARY_NEGATE:
            compile_expression(state, node->left, should_shadow);
            emit_op(state, IR_NEG, node->token, 0);
            break;

        case AST_UNARY_NOT:
            compile_expression(state, node->left, should_shadow);
            emit_op(state, IR_LOG_NOT, node->token, 0);
            break;

        case AST_BIN_CAST:
            // @todo
            //
            // log_error("AST_BIN_CAST compilation TODO");
            compile_expression(state, node->right, should_shadow);
            // compile_expression(state, node->left);
            // emit_op(state, IR_INVALID, node->token, 0);
            break;

        case AST_FUNC_CALL:
            compile_expression(state, node->right, should_shadow); // passing args
            expr = compile_expression(state, node->left, should_shadow);  // getting function address
            expr.emmited_op->operation = IR_CALL;
            break;

        case AST_MEMBER_ACCESS:
            assert(node->left->type == AST_PRIMARY);
            assert(node->left->token.type == TOKEN_IDENT);
            add_identifier_type_to_search(state, node->left->token.data.string);
            expr = compile_expression(state, node->right, should_shadow);

            if (!expr.accessable) {
                log_error_token("Bad member access expression: ", node->left->token);
                state->ir.is_valid = false;
                stack_pop(&state->search_scopes);
                break;
            }

            for (u64 i = 0; i < state->search_scopes.index; i++) {
                stack_push(&expr.search_info, state->search_scopes.data[i]);
            }

            stack_pop(&state->search_scopes);
            return expr;

        case AST_ARRAY_ACCESS: // @todo finish
            log_error("Cant use arrays");
            break;

            compile_expression(state, node->right, should_shadow);
            expr = compile_expression(state, node->left, should_shadow);

            if (!expr.accessable) {
                log_error_token("Bad array access expression: ", node->left->token);
                state->ir.is_valid = false;
            } else {
                expr.emmited_op->operation = IR_STORE;
                expr.emmited_op->search_info = expr.search_info;
            }
            return expr;

        case AST_BIN_ASSIGN:
            compile_expression(state, node->right, should_shadow);
            expr = compile_expression(state, node->left, should_shadow);

            if (!expr.accessable) {
                log_error_token("Bad assignment expression: ", node->left->token);
                state->ir.is_valid = false;
            } else {
                expr.emmited_op->operation = IR_LEA;
                expr.emmited_op->search_info = expr.search_info;
                emit_op(state, IR_STORE, node->token, 0);
            }
            break;

        case AST_BIN_SWAP:
            // @todo
            // log_error("AST_BIN_SWAP compilation TODO");
            compile_expression(state, node->right, should_shadow);
            compile_expression(state, node->left, should_shadow);
            emit_op(state, IR_INVALID, node->token, 0xABABABABABABABAB);
            break;

        case AST_SEPARATION:
            {
                ast_node_t *next = node->list_start;

                for (u64 i = 0; i < node->child_count; i++) {
                    compile_expression(state, next, should_shadow);
                    next = next->list_next;
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
    hashmap_t<string_t, scope_entry_t> *block = list_get(&state->compiler->scopes, node->scope_index);
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

    scope_entry_t *entry = search_identifier(state, node->token.data.string);

    if (entry->expr) {
        ir_expression_t expr = compile_expression(state, entry->expr, true);
        UNUSED(expr);
    } else {
        emit_op(state, IR_PUSH_UNSIGN, node->token, 0);
    }

    emit_op(state, IR_ALLOC, node->token, 16); // here we get the type size
    emit_op(state, IR_STORE, node->token, 0);
}

void compile_mul_variables(ir_state_t *state, ast_node_t *node) {
    assert(state->current_function != NULL);

    ast_node_t *next = node->list_start;
    for (u64 i = 0; i < node->child_count; i++) {
        scope_entry_t *entry = search_identifier(state, next->token.data.string);

        if (entry->expr) {
            ir_expression_t expr = compile_expression(state, entry->expr, true);
            UNUSED(expr);
        } else {
            emit_op(state, IR_PUSH_UNSIGN, node->token, 0);
        }

        emit_op(state, IR_ALLOC, node->token, 16); // here we get the type size
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
                compile_expression(state, node->left, false);
                ir_opcode_t *end = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);
                compile_block(state, node->right);
                end->s_operand = state->current_function->code.count - 1 - end->index; // @todo, this could be a problem...
            }
            break;
        case AST_IF_ELSE_STMT:
            {
                ir_opcode_t *branch, *end;
                compile_expression(state, node->left, false);
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

                compile_expression(state, node->left, false);
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
                compile_expression(state, node->left, false);
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
            compile_expression(state, node, false);
            break;
    }
}

void compile_function(ir_state_t *state, string_t key, scope_entry_t *entry) {
    if (entry->is_external) {
        return;
    }  

    assert(state->current_function == NULL);

    ir_function_t func = {};
    func.symbol = key;
    state->current_function = &func;
    stack_push(&state->search_scopes, &entry->func_params);
    {
        //                      def -> type -> params
        ast_node_t *node = entry->node->left->left;

        for (u64 i = 0; i < node->child_count; i++) {
            ast_node_t *next = node->list_start;

            for (u64 j = 0; j < ((node->child_count - 1) - i); j++) {
                next = next->list_next;
            }

            assert(state->current_function != NULL);

            scope_entry_t *entry = search_identifier(state, next->token.data.string);
            log_info(entry->node->token.data.string);

            emit_op(state, IR_ALLOC, node->token, 16); 
            emit_op(state, IR_STORE, node->token, 0);
        }



        compile_block(state, entry->expr);

        if (entry->return_typenames.count == 0) {
            emit_op(state, IR_RET, entry->node->token, 0);
        } else {
            emit_op(state, IR_INVALID, entry->node->token, 0);
        }

        for (u64 i = 0; i < func.code.count; i++) {
            print_ir_opcode(func.code.data[i]);
        }
    }
    stack_pop(&state->search_scopes);
    state->current_function = NULL;
    list_add(&state->ir.functions, &func);
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

    state.compiler    = compiler;
    state.ir.is_valid = true;

    hashmap_t<string_t, scope_entry_t> *scope = list_get(&compiler->scopes, 0);

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

