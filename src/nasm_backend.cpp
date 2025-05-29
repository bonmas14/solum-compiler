#include "backend.h"
#include "arena.h"
#include "array.h"
#include "talloc.h"

#include "strings.h"

struct nasm_state_t {
    b32         is_valid;
    ir_t       *ir;
    array_t<u8> code;
    ir_function_t    *func;
    stack_t<string_t> labels;
};

    // @todo @speed...
void nasm_add_string(nasm_state_t *state, string_t data, u64 tab = 0) {
    for (u64 i = 0; i < (tab * 4); i++) {
        array_add(&state->code, (u8)' ');
    }

    for (u64 i = 0; i < data.size; i++) {
        array_add(&state->code, data[i]);
    }
}

void nasm_add_line(nasm_state_t *state, string_t data, u64 tab = 0) {
    nasm_add_string(state, data, tab);
    nasm_add_string(state, STRING("\n"));
}


void nasm_compile_func(string_t name, nasm_state_t *state) {
    nasm_add_string(state, string_format(get_temporary_allocator(), STRING("%s:\n"), name));

    for (u64 i = 0; i < state->func->code.count; i++) {
        ir_opcode_t op = state->func->code[i];

        // @todo, for jumps we need to create auto labels!

        switch (op.operation) {
            case IR_STACK_FRAME_PUSH:
                nasm_add_line(state, STRING("push rbp"), 1);
                nasm_add_line(state, STRING("mov rbp, rsp"), 1);
                break;

            case IR_STACK_FRAME_POP:
                nasm_add_line(state, STRING("mov rsp, rbp"), 1);
                nasm_add_line(state, STRING("pop rbp"), 1);
                break;

            case IR_CALL:
                nasm_add_line(state, string_format(get_temporary_allocator(), STRING("call %s"), op.string), 1);
                break;

            case IR_RET:
                nasm_add_line(state, STRING("ret"), 1);
                break;

            case IR_BRK:
                nasm_add_line(state, STRING("int3"), 1);
                break;

            case IR_INVALID:
                nasm_add_line(state, STRING("ud2"), 1);
                break;

            default:
                nasm_add_line(state, STRING("ud2"), 1);
                break;
        }
    }

    nasm_add_line(state, STRING("int3"), 1);
    nasm_add_line(state, STRING("ret"), 1);
    nasm_add_line(state, STRING(""), 0);
}

void nasm_compile_program(ir_t *state) {
    nasm_state_t nasm = {};


    nasm.ir = state;
    nasm.is_valid = true;
    array_create(&nasm.code, (u64) 1024, create_arena_allocator(PG(4)));

    nasm_add_line(&nasm, STRING("default rel"));
    nasm_add_line(&nasm, STRING(""));
    nasm_add_line(&nasm, STRING("section .text"));
    nasm_add_line(&nasm, STRING(""));
    nasm_add_line(&nasm, STRING("global main"));
    nasm_add_line(&nasm, STRING(""));

    for (u64 i = 0; i < state->functions.capacity; i++) {
        kv_pair_t<string_t, ir_function_t> *pair = state->functions.entries + i;

        if (!pair->occupied) continue;
        if (pair->deleted)   continue;

        nasm.func = &pair->value;
        nasm_compile_func(pair->key, &nasm);
    }

    FILE *file = fopen("output.nasm", "wb");

    for (u64 i = 0; i < nasm.code.count; i++) {
        log_update_color();
        fprintf(file, "%c", nasm.code[i]);
    }

    fflush(file);
    fclose(file);
}
