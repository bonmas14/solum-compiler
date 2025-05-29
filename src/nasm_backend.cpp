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


#define LOAD(reg)\
                INSERT_LINE();\
                nasm_add_line(state, STRING("sub r15, 1"), 1);\
                INSERT_LINE();\
                t = string_format(talloc, STRING("mov " reg ", QWORD[r14 + r15 * 8]"));\
                nasm_add_line(state, t, 1);

#define BINOP(action)\
                INSERT_LINE();\
                nasm_add_line(state, t, 1);\
                t = string_format(talloc, STRING("xor rcx, rcx"));\
                \
                LOAD("rax");\
                INSERT_LINE();\
                nasm_add_line(state, t, 1);\
                t = string_format(talloc, STRING(action" rcx, rax"));\
                \
                LOAD("rax");\
                INSERT_LINE();\
                nasm_add_line(state, t, 1);\
                t = string_format(talloc, STRING(action" rcx, rax"));\
                \
                INSERT_LINE();\
                t = string_format(talloc, STRING("mov QWORD[r14 + r15 * 8], rcx"));\
                nasm_add_line(state, t, 1);\
                INSERT_LINE();\
                nasm_add_line(state, STRING("add r15, 1"), 1);


void nasm_compile_func(string_t name, nasm_state_t *state) {
    nasm_add_string(state, string_format(get_temporary_allocator(), STRING("%s:\n"), name));

    if (!state->func->code.count) {
        if (state->func->is_external) {
            nasm_add_line(state, STRING("ret"), 1);
            return;
        }

        assert(false);
        return;
    }


    allocator_t *talloc = get_temporary_allocator();

    #define INSERT_LINE() nasm_add_line(state, string_format(get_temporary_allocator(), STRING("%%line %u \"%s\""), op.info.l0, op.info.from->filename), 1)

    for (u64 i = 0; i < state->func->code.count; i++) {
        ir_opcode_t op = state->func->code[i];

        nasm_add_line(state, string_format(get_temporary_allocator(), STRING(".IROP_%u:"), i), 1);

        // @todo, for jumps we need to create auto labels!

        string_t t = {};
        switch (op.operation) {
            case IR_STACK_FRAME_PUSH:
                INSERT_LINE();
                nasm_add_line(state, STRING("push rbp"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("mov rbp, rsp"), 1);
                break;

            case IR_STACK_FRAME_POP:
                INSERT_LINE();
                nasm_add_line(state, STRING("mov rsp, rbp"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("pop rbp"), 1);
                break;

            case IR_PUSH_SIGN:
                INSERT_LINE();
                t = string_format(talloc, STRING("mov QWORD[r14 + r15 * 8], %u"), op.u_operand);
                nasm_add_line(state, t, 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("add r15, 1"), 1);
                break;
            case IR_PUSH_UNSIGN:
                INSERT_LINE();
                t = string_format(talloc, STRING("mov QWORD[r14 + r15 * 8], %u"), op.u_operand);
                nasm_add_line(state, t, 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("add r15, 1"), 1);
                break;
            case IR_PUSH_STACK:
                INSERT_LINE();
                t = string_format(talloc, STRING("mov rax, QWORD[rbp - %u * 8]"), op.u_operand);
                nasm_add_line(state, t, 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("mov QWORD[r14 + r15 * 8], rax"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("add r15, 1"), 1);
                break;
            case IR_PUSH_SEA:
                INSERT_LINE();
                t = string_format(talloc, STRING("lea rax, QWORD[rbp - %u * 8]"), op.u_operand);
                nasm_add_line(state, t, 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("mov QWORD[r14 + r15 * 8], rax"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("add r15, 1"), 1);
                break;

            case IR_ALLOC:
                // Stack probe...
                INSERT_LINE();
                t = string_format(talloc, STRING("mov rax, %u * 8"), op.u_operand);
                nasm_add_line(state, t, 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("call __stack_probe"), 1);

                INSERT_LINE();
                nasm_add_line(state, STRING("sub rsp, rax"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("mov rax, rsp"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("mov QWORD[r14 + r15 * 8], rax"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("add r15, 1"), 1);
                break;

            case IR_ADD: BINOP("add"); break;
            case IR_SUB: BINOP("sub"); break;
            case IR_MUL: BINOP("imul"); break;
            case IR_DIV: BINOP("idiv"); break;
            // case IR_MOD: BINOP("idiv"); break; // cause mod result in different register!

            case IR_JUMP: 
                nasm_add_line(state, string_format(get_temporary_allocator(), STRING("jmp .IROP_%u"), (u64)((s64)i + 1 + op.s_operand)), 1);
                break;

            case IR_FREE:
                INSERT_LINE();
                t = string_format(talloc, STRING("add rsp, %u * 8"), op.u_operand);
                nasm_add_line(state, t, 1);
                break;

            case IR_STORE:
                LOAD("rbx");
                LOAD("rax");

                INSERT_LINE();
                t = string_format(talloc, STRING("mov rax, QWORD[r14 + r15 * 8]"), op.u_operand);
                nasm_add_line(state, t, 1);
                INSERT_LINE();
                t = string_format(talloc, STRING("add QWORD[rbx], rax"), op.u_operand);
                nasm_add_line(state, t, 1);
                break;

            case IR_CALL:
                INSERT_LINE();
                nasm_add_line(state, string_format(get_temporary_allocator(), STRING("call %s"), op.string), 1);
                break;

            case IR_RET:
                INSERT_LINE();
                nasm_add_line(state, STRING("ret"), 1);
                break;

            case IR_BRK:
                INSERT_LINE();
                nasm_add_line(state, STRING("int3"), 1);
                break;

            case IR_INVALID:
                INSERT_LINE();
                nasm_add_line(state, STRING("ud2"), 1);
                break;

            default:
                INSERT_LINE();
                nasm_add_line(state, STRING("ud2"), 1);
                break;
        }
    }

    nasm_add_line(state, STRING("int3"), 1);
    nasm_add_line(state, STRING("int3"), 1);
    nasm_add_line(state, STRING("int3"), 1);
    nasm_add_line(state, STRING(""), 0);
}

void nasm_compile_program(ir_t *state) {
    nasm_state_t nasm = {};


    nasm.ir = state;
    nasm.is_valid = true;
    array_create(&nasm.code, (u64) 1024, create_arena_allocator(PG(4)));

    
    nasm_add_line(&nasm, STRING("; Created by bonmas14."));
    nasm_add_line(&nasm, STRING("; "));
    nasm_add_line(&nasm, STRING("; solum-compiler auto generated code"));
    nasm_add_line(&nasm, STRING("; nasm-win-x64"));
    nasm_add_line(&nasm, STRING(""));
    nasm_add_line(&nasm, STRING("default rel"));
    nasm_add_line(&nasm, STRING(""));
    nasm_add_line(&nasm, STRING("section .bss"));
    nasm_add_line(&nasm, STRING("exec_stack:"));
    nasm_add_line(&nasm, STRING("resb 4096 * 8"), 1);
    nasm_add_line(&nasm, STRING(""));
    
    nasm_add_line(&nasm, STRING("section .text"));
    nasm_add_line(&nasm, STRING(""));
    nasm_add_line(&nasm, STRING("global mainCRTStartup"));
    nasm_add_line(&nasm, STRING(""));
    nasm_add_line(&nasm, STRING("mainCRTStartup:"));
    
    nasm_add_line(&nasm, STRING("lea r14, [exec_stack]"), 1);
    nasm_add_line(&nasm, STRING("xor r15, r15"), 1);
    
    nasm_add_line(&nasm, STRING("call main"), 1);
    nasm_add_line(&nasm, STRING("ret"), 1);
    nasm_add_line(&nasm, STRING(""));

    nasm_add_line(&nasm, STRING("__stack_probe: ; rax = allocation size"), 1);
    nasm_add_line(&nasm, STRING("test rax, rax"), 1);
    nasm_add_line(&nasm, STRING("jz .done               ; skip if size=0"), 1);
    nasm_add_line(&nasm, STRING("mov r10, rsp"), 1);
    nasm_add_line(&nasm, STRING("sub r10, rax           ; r10 = allocation start"), 1);
    nasm_add_line(&nasm, STRING("mov r11, r10"), 1);
    nasm_add_line(&nasm, STRING("and r11, -4096         ; align start down to page boundary"), 1);
    nasm_add_line(&nasm, STRING("lea rcx, [rsp-1]"), 1);
    nasm_add_line(&nasm, STRING("and rcx, -4096         ; align current page down"), 1);
    nasm_add_line(&nasm, STRING("cmp rcx, r11"), 1);
    nasm_add_line(&nasm, STRING("jb .done               ; no pages to touch"), 1);
    nasm_add_line(&nasm, STRING(".loop:"), 1);
    nasm_add_line(&nasm, STRING("test dword [rcx], 0    ; touch page"), 1);
    nasm_add_line(&nasm, STRING("sub rcx, 4096          ; move to next page"), 1);
    nasm_add_line(&nasm, STRING("cmp rcx, r11"), 1);
    nasm_add_line(&nasm, STRING("jae .loop"), 1);
    nasm_add_line(&nasm, STRING(".done:"), 1);
    nasm_add_line(&nasm, STRING("ret"), 1);

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
