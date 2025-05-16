#include "compiler.h"
#include "analyzer.h"
#include "parser.h"
#include "ir.h"

#include "allocator.h"
#include "talloc.h"
#include "strings.h"
#include "memctl.h"


struct ir_state_t {
    compiler_t *compiler;
    ir_t ir;
    ir_function_t *current_function;
    stack_t<hashmap_t<string_t, scope_entry_t>*> search_scopes;
    list_t<hashmap_t<string_t, scope_entry_t>>   local_scopes;
};

// ------ //

void emit_op(ir_state_t *state, u64 op, token_t debug, u64 data = 0) {
    ir_opcode_t o = { op, debug, data };
    list_add(&state->current_function->code, &o);
}

const char* ir_code_to_string(u64 code) {
    switch (code) {
        case IR_NOP:          return "IR_NOP";
        case IR_PUSH_s:       return "IR_PUSH_s";
        case IR_PUSH_u:       return "IR_PUSH_u";
        case IR_PUSH_F32:     return "IR_PUSH_F32";
        case IR_PUSH_F64:     return "IR_PUSH_F64";
        case IR_POP:          return "IR_POP";
        case IR_CLONE:        return "IR_CLONE";
        case IR_SWAP:         return "IR_SWAP";
        case IR_GLOBAL:       return "IR_GLOBAL";
        case IR_ALLOC:        return "IR_ALLOC";
        case IR_FREE:         return "IR_FREE";
        case IR_LOAD:         return "IR_LOAD";
        case IR_STORE:        return "IR_STORE";
        case IR_LEA:          return "IR_LEA";
        case IR_ADD:          return "IR_ADD";
        case IR_SUB:          return "IR_SUB";
        case IR_MUL:         return "IR_MUL";
        case IR_DIV:         return "IR_DIV";
        case IR_MOD:         return "IR_MOD";
        case IR_NEG:         return "IR_NEG";
        case IR_BIT_AND:      return "IR_BIT_AND";
        case IR_BIT_OR:       return "IR_BIT_OR";
        case IR_BIT_XOR:      return "IR_BIT_XOR";
        case IR_BIT_NOT:      return "IR_BIT_NOT";
        case IR_SHIFT_LEFT:   return "IR_SHIFT_LEFT";
        case IR_SHIFT_RIGHT:  return "IR_SHIFT_RIGHT";
        case IR_CMP_EQ:       return "IR_CMP_EQ";
        case IR_CMP_NEQ:      return "IR_CMP_NEQ";
        case IR_CMP_LT:       return "IR_CMP_LT";
        case IR_CMP_GT:       return "IR_CMP_GT";
        case IR_CMP_LTE:      return "IR_CMP_LTE";
        case IR_CMP_GTE:      return "IR_CMP_GTE";
        case IR_LOG_AND:      return "IR_LOG_AND";
        case IR_LOG_OR:       return "IR_LOG_OR";
        case IR_LOG_NOT:     return "IR_LOG_NOT";
        case IR_JUMP:         return "IR_JUMP";
        case IR_JUMP_IF:      return "IR_JUMP_IF";
        case IR_JUMP_IF_NOT:  return "IR_JUMP_IF_NOT";
        case IR_RET:          return "IR_RET";
        case IR_CALL:         return "IR_CALL";
        case IR_CALL_EXTERN:  return "IR_CALL_EXTERN";
        case IR_BRK:          return "IR_BRK";
        case IR_INV:          return "IR_INV";
        default:              return "UNKNOWN_OP";
    }
}

void print_ir_opcode(ir_opcode_t op) {
    const char* op_name = ir_code_to_string(op.operation);
    
    log_update_color();
    printf("[IR] %-15s | Operand: 0x%016llx\n",
           op_name,
           (unsigned long long)op.operand);
}

// ------ // 

void compile_statement(ir_state_t *state, ast_node_t *node);

void compile_expression(ir_state_t *state, ast_node_t *node) {
    assert(state->current_function != NULL);
    UNUSED(state);
    UNUSED(node);

    switch (node->type) {
        case AST_PRIMARY:
            emit_op(state, IR_PUSH_s, node->token);
            // just easiest
            break;

        case AST_UNARY_DEREF:
            compile_expression(state, node->left);
            emit_op(state, IR_LOAD, node->token);
            break;
        case AST_UNARY_REF:
            log_error("AST_UNARY_REF compilation TODO");
            compile_expression(state, node->left);
            emit_op(state, IR_INV, node->token);
            break;
        case AST_UNARY_NEGATE:
            log_error("AST_UNARY_NEGATE compilation TODO");
            compile_expression(state, node->left);
            emit_op(state, IR_INV, node->token);
            break;
        case AST_UNARY_NOT:
            compile_expression(state, node->left);
            emit_op(state, IR_LOG_NOT, node->token);
            break;


        case AST_BIN_CAST:
            log_error("AST_BIN_CAST compilation TODO");
            compile_expression(state, node->right);
            // compile_expression(state, node->left);
            emit_op(state, IR_INV, node->token);
            break;
        case AST_MEMBER_ACCESS:
            log_error("AST_MEMBER_ACCESS compilation TODO");
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_INV, node->token);
            break;
        case AST_ARRAY_ACCESS:
            log_error("AST_ARRAY_ACCESS compilation TODO");
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_INV, node->token);
            break;
        case AST_FUNC_CALL:
            log_error("AST_FUNC_CALL compilation TODO");
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_INV, node->token);
            break;

        case AST_BIN_ASSIGN:
            log_error("AST_BIN_ASSIGN compilation TODO");
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_INV, node->token);
            break;

        case AST_BIN_GR:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_CMP_GT, node->token);
            break;
        case AST_BIN_LS:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_CMP_LT, node->token);
            break;
        case AST_BIN_GEQ:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_CMP_GTE, node->token);
            break;
        case AST_BIN_LEQ:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_CMP_LTE, node->token);
            break;
        case AST_BIN_EQ:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_CMP_EQ, node->token);
            break;
        case AST_BIN_NEQ:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_CMP_NEQ, node->token);
            break;
        case AST_BIN_LOG_OR:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_LOG_OR, node->token);
            break;
        case AST_BIN_LOG_AND:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_LOG_AND, node->token);
            break;
        case AST_BIN_ADD:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_ADD, node->token);
            break;
        case AST_BIN_SUB:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_SUB, node->token);
            break;
        case AST_BIN_MUL:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_MUL, node->token);
            break;
        case AST_BIN_DIV:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_DIV, node->token);
            break;
        case AST_BIN_MOD:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_MOD, node->token);
            break;
        case AST_BIN_BIT_XOR:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_BIT_XOR, node->token);
            break;
        case AST_BIN_BIT_OR:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_BIT_OR, node->token);
            break;
        case AST_BIN_BIT_AND:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_BIT_AND, node->token);
            break;
        case AST_BIN_BIT_LSHIFT:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_SHIFT_LEFT, node->token);
            break;
        case AST_BIN_BIT_RSHIFT:
            compile_expression(state, node->right);
            compile_expression(state, node->left);
            emit_op(state, IR_SHIFT_RIGHT, node->token);
            break;

        case AST_BIN_SWAP:
            log_error("AST_BIN_SWAP compilation TODO");
            break;

        case AST_SEPARATION:
            {
                ast_node_t *next = node->list_start;

                for (u64 i = 0; i < node->child_count; i++) {
                    compile_expression(state, next);
                    next = next->list_next;
                }
            }
            break;

        default:
            log_error("UNKN!!!");
            break;
    }


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
    UNUSED(state);
    UNUSED(node);
}

void compile_mul_variables(ir_state_t *state, ast_node_t *node) {
    assert(state->current_function != NULL);
    UNUSED(state);
    UNUSED(node);
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
                compile_expression(state, node->left);
                emit_op(state, IR_JUMP_IF, node->token);
                compile_block(state, node->right);
            }
            break;
        case AST_IF_ELSE_STMT:
            {
                compile_expression(state, node->left);
                emit_op(state, IR_JUMP_IF, node->token);
                compile_block(state, node->center);
                emit_op(state, IR_JUMP, node->token);
                compile_statement(state, node->right);
            }
            break;
        case AST_WHILE_STMT: 
            {
                compile_expression(state, node->left);
                emit_op(state, IR_JUMP_IF_NOT, node->token);
                compile_block(state, node->right);
                emit_op(state, IR_JUMP, node->token);
            }
            break;
        case AST_RET_STMT: 
            {
                compile_expression(state, node->left);
                emit_op(state, IR_RET, node->token);
            }
            break;
        case AST_BREAK_STMT: 
            {
                emit_op(state, IR_JUMP, node->token); // jump outa loop
            }
            break;
        case AST_CONTINUE_STMT:
            {
                emit_op(state, IR_JUMP, node->token); // jump to start of loop
            }
            break;
            break;

        default:
            compile_expression(state, node);
            break;
    }
}

void compile_function(ir_state_t *state, string_t key, scope_entry_t *entry) {
    if (entry->is_external) {

        /*
        string_t t = {};

        t = string_temp_concat(STRING("Ext. func: "), key);
        t = string_temp_concat(t, STRING(", as: "));
        t = string_temp_concat(t, entry->ext_from);

        if (entry->ext_name.size) {
            t = string_temp_concat(t, STRING(":"));
            t = string_temp_concat(t, entry->ext_name);
        }

        log_info(t);
        */

        return;
    }  

    assert(state->current_function == NULL);

    ir_function_t func = {};
    func.symbol = key;

    state->current_function = &func;
    compile_block(state, entry->expr);
    state->current_function = NULL;

    for (u64 i = 0; i < func.code.count; i++) {
        print_ir_opcode(func.code.data[i]);
    }

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

