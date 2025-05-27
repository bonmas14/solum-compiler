#include "interop.h"

#include "ir.h"
#include "analyzer.h"
#include "list.h"
#include "stack.h"
#include "hashmap.h"
#include "talloc.h"
#include "strings.h"
#include <math.h>

struct interpreter_state_t {
    b32 running;
    ir_t *ir;
    u64 ip;
    stack_t<u64> fp;
    stack_t<s64> exec_stack;
    stack_t<s64> data_stack;
    stack_t<ir_function_t*> curr_func;
};

static s64 allocate_memory(interpreter_state_t *state, u64 size) {
    UNUSED(size);

    stack_push(&state->data_stack, (s64) 0);
    return (state->data_stack.index - 1);
}

static void free_memory(interpreter_state_t *state, u64 size) {
    UNUSED(size);
    stack_pop(&state->data_stack);
}

static void push_function(interpreter_state_t *state, string_t string) {
    stack_push(&state->curr_func, hashmap_get(&state->ir->functions, string));
}

static void pop_function(interpreter_state_t *state) {
    stack_pop(&state->curr_func);
}

static void execute_ir_opcode(interpreter_state_t *state, ir_opcode_t op) {
    switch (op.operation) {
        case IR_NOP: 
            break;
        
        case IR_STACK_FRAME_PUSH:
            stack_push(&state->fp, state->data_stack.index); 
            break;

        case IR_STACK_FRAME_POP:
            state->data_stack.index = stack_pop(&state->fp); 
            break;

        case IR_PUSH_SIGN:   
            stack_push(&state->exec_stack, op.s_operand); 
            break;
        case IR_PUSH_UNSIGN: 
            stack_push(&state->exec_stack, op.s_operand);
            break;

        case IR_PUSH_STACK: 
        {
            stack_push(&state->exec_stack, state->data_stack.data[stack_peek(&state->fp) + op.u_operand]);
        } break;
        case IR_PUSH_SEA: 
        {
            stack_push(&state->exec_stack, (s64)op.u_operand);
        } break;

        case IR_PUSH_GLOBAL: 
        {
            stack_push(&state->exec_stack, (s64) 0);
        } break;
        case IR_PUSH_GEA: 
        {
            log_warning("push addr from global");
        } break;

        case IR_POP:
            stack_pop(&state->exec_stack); 
            break;
            
        case IR_CLONE: {
            s64 val = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, val);
            stack_push(&state->exec_stack, val);
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
            s64 val  = state->data_stack.data[addr];
            stack_push(&state->exec_stack, val);
        } break;
            
        case IR_STORE: {
            s64 addr = stack_pop(&state->exec_stack);
            s64 val  = stack_pop(&state->exec_stack);
            state->data_stack.data[addr] = val;
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
            state->ip += op.s_operand;
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
            if (state->data_stack.index == 0) {
                state->running = false;
            } else {
                s64 ip_addr = stack_pop(&state->data_stack);
                state->ip = (u64)ip_addr; // assume that it will be not too big...
                pop_function(state);
            }
        } break;
        case IR_CALL:
        {
            push_function(state, op.string);
            stack_push(&state->data_stack, (s64)state->ip);
            state->ip = 0;
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

    state.ir = ir;

    stack_push(&state.curr_func, ir->functions[func_name]);
    while (state.running) {
        ir_function_t *func = stack_peek(&state.curr_func);
        ir_opcode_t op = func->code[state.ip++];
        execute_ir_opcode(&state, op);
#ifdef VERBOSE
        log_info(string_format(get_temporary_allocator(), STRING("e:%u d:%u"), (u64)state.exec_stack.index, (u64)state.data_stack.index));
        print_ir_opcode(op);
#endif
    }

    log_info(string_format(get_temporary_allocator(), STRING("%d"), (s64)stack_pop(&state.exec_stack)));
}
