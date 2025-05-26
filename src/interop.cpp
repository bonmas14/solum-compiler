#include <interop.h>

#include "ir.h"
#include "analyzer.h"
#include "list.h"
#include "stack.h"
#include "hashmap.h"
#include <math.h>

struct interpreter_state_t {
    b32 running;
    u64 ip;
    stack_t<s64> exec_stack;
    stack_t<s64> data_stack;
};

static s64 allocate_memory(interpreter_state_t *state, u64 size) {
    UNUSED(size);

    stack_push(&state->data_stack, (s64) 0);
    return (state->data_stack.index - 1) * sizeof(s64);
}

static void free_memory(interpreter_state_t *state, u64 size) {
    UNUSED(size);
    stack_pop(&state->data_stack);
}

static void execute_ir_opcode(interpreter_state_t *state, ir_opcode_t op) {
    switch (op.operation) {
        case IR_NOP: break;
        
        case IR_PUSH_SIGN:   stack_push(&state->exec_stack, op.s_operand); break;
        case IR_PUSH_UNSIGN: stack_push(&state->exec_stack, op.s_operand); break;
        case IR_POP:         stack_pop(&state->exec_stack); break;
            
        case IR_CLONE: {
            s64 val = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, val);
            stack_push(&state->exec_stack, val);
        } break;
            
        case IR_PUSH_GLOBAL: {
            // interpreter_push(state, 0xDEADBEEF);
        } break;
            
        case IR_ALLOC: {
            s64 size = op.u_operand;
            s64 addr = allocate_memory(state, size);
            stack_push(&state->exec_stack, addr);
        } break;
            
        case IR_FREE: {
            s64 size = op.u_operand;
            free_memory(state, op.u_operand);
        } break;
            
        case IR_LOAD: {
            s64 addr = stack_pop(&state->exec_stack);
            s64 val  = 404;
            stack_push(&state->exec_stack, val);
        } break;
            
        case IR_STORE: {
            s64 addr = stack_pop(&state->exec_stack);
            s64 val  = stack_pop(&state->exec_stack);
        } break;
            
        case IR_ADD: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, a + b);
        } break;
            
        case IR_SUB: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, a - b);
        } break;
            
        case IR_MUL: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, a * b);
        } break;
            
        case IR_DIV: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            if (b != 0) stack_push(&state->exec_stack, a / b);
            stack_push(&state->exec_stack, (s64) 0);
        } break;
            
        case IR_MOD: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            if (b != 0) stack_push(&state->exec_stack, a % b);
            stack_push(&state->exec_stack, (s64) 0);
        } break;
            
        case IR_NEG: {
            s64 a = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, -a);
        } break;
            
        case IR_CMP_EQ: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, (s64)(a == b));
        } break;
            
        case IR_CMP_NEQ: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, (s64)(a != b));
        } break;
            
        case IR_CMP_LT: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, (s64)(a < b));
        } break;
            
        case IR_CMP_GT: {
            s64 a = stack_pop(&state->exec_stack);
            s64 b = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, (s64)(a > b));
        } break;
            
        case IR_JUMP: {
            state->ip += op.s_operand; // -1 because we increment ip after
        } break;
            
        case IR_JUMP_IF: {
            s64 cond = stack_pop(&state->exec_stack);
            if (cond) {
                state->ip += op.s_operand;
            }
        } break;
            
        case IR_JUMP_IF_NOT: {
            s64 cond = stack_pop(&state->exec_stack);
            if (!cond) {
                state->ip += op.s_operand;
            }
        } break;

        case IR_RET: {
            state->running = false;
        } break;

        case IR_CALL_EXTERN: {
            log_error("External call not implemented.");
            state->running = false;
        } break;

        case IR_BRK: {
            log_error("Debug break");
            debug_break();
        } break;

        case IR_INVALID: {
            log_error("Invalid opcode!");
            state->running = false;
            assert(false);
        } break;
            
        default:
            log_error("Unknown IR opcode.");
            state->running = false;
            break;
    }
}

void interop_func(ir_t *ir, string_t func_name) {
    interpreter_state_t state = {};
    state.running = true;

    ir_function_t func = *ir->functions[func_name];

    while (state.running && state.ip < func.code.count) {
        ir_opcode_t op = func.code[state.ip];
        state.ip++;
        execute_ir_opcode(&state, op);

#ifdef VERBOSE
        print_ir_opcode(op);
#endif
    }

    if (state.running) {
        log_error("Unexpected value.");
    }
}
