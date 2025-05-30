#include "interop.h"

#include "ir.h"
#include "analyzer.h"
#include "list.h"
#include "stack.h"
#include "hashmap.h"
#include "talloc.h"
#include "strings.h"
#include "profiler.h"
#include <math.h>

struct memory_block_t {
    s64 size;
    s64 start;
};
struct interpreter_state_t {
    b32 running;
    ir_t *ir;
    u64 ip;
    stack_t<u64> fp;
    stack_t<s64> exec_stack;
    stack_t<s64> data_stack;
    stack_t<memory_block_t> allocations;
    stack_t<ir_function_t*> curr_func;
};

#define UNOP(irop, val) case irop: {\
    s64 a = stack_pop(&state->exec_stack);\
    stack_push(&state->exec_stack, val);\
} break;

#define BINOP(irop, val) case irop: {\
    s64 a = stack_pop(&state->exec_stack);\
    s64 b = stack_pop(&state->exec_stack);\
    stack_push(&state->exec_stack, val);\
} break;

#define BINCHKOP(irop, val) case irop: {\
    s64 a = stack_pop(&state->exec_stack);\
    s64 b = stack_pop(&state->exec_stack);\
    if (b != 0) stack_push(&state->exec_stack, val);\
    else        stack_push(&state->exec_stack, (s64) 0);\
} break;

static inline s64 allocate_memory(interpreter_state_t *state, s64 size) {
    UNUSED(size);

    memory_block_t block = {};
    block.start = state->data_stack.index;
    block.size  = size;

    for (s64 i = 0; i < size; i++) {
        stack_push(&state->data_stack, (s64) 0);
    }

    stack_push(&state->allocations, block);
    return state->data_stack.index - 1;
}

static inline void free_memory(interpreter_state_t *state, s64 size) {
    while (size > 0) {
        memory_block_t block = stack_pop(&state->allocations);
        state->data_stack.index = block.start;
        size -= block.size;
    }
}

static inline s64 get_variable_address(interpreter_state_t *state, s64 address) {
    address = stack_peek(&state->fp) + address;

    for (u64 i = 0; i < state->allocations.index; i++) {
        memory_block_t block = state->allocations[i];

        s64 stop_address = block.start + (block.size - 1);
        if (address >= block.start && address <= stop_address) {
            return block.start;
        }
    }

    assert(false);
    return -1;
}

static inline b32 push_function(interpreter_state_t *state, string_t string) {
    ir_function_t *func = hashmap_get(&state->ir->functions, string);

#ifdef VERBOSE
    log_update_color();
    log_write(string_format(get_temporary_allocator(), STRING(" Calling: %s\n"), string));
#endif

    if (func->is_external) {
        // actually call it here

        if (string_compare(string, STRING("putchar")) == 0) {
            string_t str = {};

            s64 value = stack_pop(&state->exec_stack);
            str.size = 1;
            str.data = (u8*)&value;

            log_write(str);
        } else if (string_compare(string, STRING("debug_break")) == 0) {
            debug_break();
        }


        return true;
    }

    stack_push(&state->curr_func, func);
    return false;
}

static inline void pop_function(interpreter_state_t *state) {
    stack_pop(&state->curr_func);
}

static inline void execute_ir_opcode(interpreter_state_t *state, ir_opcode_t op) {
    switch (op.operation) {
        case IR_NOP: break;

        BINOP   (IR_ADD, a + b);
        BINOP   (IR_SUB, a - b);
        BINOP   (IR_MUL, a * b);
        BINCHKOP(IR_DIV, a / b);
        BINCHKOP(IR_MOD, a % b);
        UNOP    (IR_NEG, -a);
            
        UNOP (IR_BIT_NOT,     (s64)(~a));
        BINOP(IR_BIT_AND,     (s64)(a & b));
        BINOP(IR_BIT_OR,      (s64)(a | b));
        BINOP(IR_BIT_XOR,     (s64)(a ^ b));
        BINOP(IR_SHIFT_LEFT,  (s64)(a << b));
        BINOP(IR_SHIFT_RIGHT, (s64)(a >> b));

        BINOP(IR_CMP_EQ,  (s64)(a == b));
        BINOP(IR_CMP_NEQ, (s64)(a != b));
        BINOP(IR_CMP_LT,  (s64)(a < b));
        BINOP(IR_CMP_GT,  (s64)(a > b));
        BINOP(IR_CMP_LTE, (s64)(a <= b));
        BINOP(IR_CMP_GTE, (s64)(a >= b));
            
        BINOP(IR_LOG_AND, (s64)(a && b));
        BINOP(IR_LOG_OR,  (s64)(a || b));
        UNOP (IR_LOG_NOT, (s64)(!a));

        case IR_STACK_FRAME_PUSH: 
                             stack_push(&state->fp, state->data_stack.index); 
                             break;
        case IR_STACK_FRAME_POP: 
                             {
                                 u64 fp = stack_pop(&state->fp);
                                 free_memory(state, state->data_stack.index - fp);
                             } break;

        case IR_PUSH_SIGN:   stack_push(&state->exec_stack, op.s_operand); 
                             break;
        case IR_PUSH_UNSIGN: stack_push(&state->exec_stack, op.s_operand);
                             break;
        case IR_PUSH_STACK:  stack_push(&state->exec_stack, state->data_stack.data[(u64)get_variable_address(state, (op.s_operand - 1))]); 
                             break;
        case IR_PUSH_SEA:    stack_push(&state->exec_stack, (s64)get_variable_address(state, (op.s_operand - 1)));
                             break;

        case IR_PUSH_GLOBAL: 
        {
            stack_push(&state->exec_stack, (s64) 0);
        } break;

        case IR_PUSH_GEA:
        {
            log_warning("push addr from global");
        } break;

        case IR_POP: {
            stack_pop(&state->exec_stack);
        } break;
            
        case IR_CLONE: {
            s64 val = stack_pop(&state->exec_stack);
            stack_push(&state->exec_stack, val);
            stack_push(&state->exec_stack, val);
        } break;
            
        case IR_ALLOC: {
            s64 addr = allocate_memory(state, op.s_operand);
            stack_push(&state->exec_stack, addr);
        } break;
            
        case IR_FREE: {
            free_memory(state, op.s_operand);
        } break;
            
        case IR_LOAD: {
            s64 addr = stack_pop(&state->exec_stack);
            if ((u64)addr > state->data_stack.index) {
                print_ir_opcode(op);
                log_error_token(string_format(get_temporary_allocator(), STRING("Access violation, address: %d"), addr), op.info);
                state->running = false;
                break;
            }
            s64 val  = state->data_stack.data[addr];
            stack_push(&state->exec_stack, val);
        } break;
            
        case IR_STORE: {
            s64 addr = stack_pop(&state->exec_stack);
            s64 val  = stack_pop(&state->exec_stack);

            if ((u64)addr > state->data_stack.index) {
                print_ir_opcode(op);
                log_error_token(string_format(get_temporary_allocator(), STRING("Access violation, address: %d"), addr), op.info);
                state->running = false;
                break;
            }

            state->data_stack.data[addr] = val;
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
            if (!push_function(state, op.string)) {
                stack_push(&state->data_stack, (s64)state->ip);
                state->ip = 0;
            }
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
            print_ir_opcode(op);
            state->running = false;
            break;
    }
}

void interop_func(ir_t *ir, string_t func_name) {
    profiler_func_start();
    interpreter_state_t state = {};
    state.running = true;

    state.ir = ir;

    stack_push(&state.curr_func, ir->functions[func_name]);

    ir_function_t *func = {};
    u64 i = 0;
    while (state.running) {
        if (i != state.curr_func.index) {
            i = state.curr_func.index;
            func = stack_peek(&state.curr_func);
        }
        ir_opcode_t op = func->code[state.ip++];

#ifdef VERBOSE
        log_update_color();
        fprintf(stderr, " %4llu - about to: ", state.ip);
        print_ir_opcode(op);
#endif
        execute_ir_opcode(&state, op);
    }

    log_info(string_format(get_temporary_allocator(), STRING("%d"), (s64)stack_pop(&state.exec_stack)));
    profiler_func_end();
}
