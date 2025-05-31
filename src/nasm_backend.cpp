#include "backend.h"
#include "arena.h"
#include "array.h"
#include "talloc.h"
#include "strings.h"

struct nasm_state_t {
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


#define INSERT_LINE() nasm_add_line(state, string_format(get_temporary_allocator(), STRING("%%line %u \"%s\""), op.info.l0, op.info.from->filename), 1)

#define LOAD(reg)\
                INSERT_LINE();\
                nasm_add_line(state, STRING("dec r15"), 1);\
                INSERT_LINE();\
                nasm_add_line(state, STRING("mov " reg ", QWORD[r14 + r15 * 8]"), 1);

#define STORE(reg)\
                INSERT_LINE();\
                nasm_add_line(state, STRING("mov QWORD[r14 + r15 * 8], " reg), 1);\
                INSERT_LINE();\
                nasm_add_line(state, STRING("inc r15"), 1);

#define BINOP(action)\
                LOAD("rax");\
                LOAD("rbx");\
                INSERT_LINE();\
                nasm_add_line(state, STRING(action" rax, rbx"), 1);\
                STORE("rax");

#define DIVOP(result_reg)\
                LOAD("rax");\
                INSERT_LINE();\
                nasm_add_line(state, STRING("cqo"), 1);\
                LOAD("rbx");\
                INSERT_LINE();\
                nasm_add_line(state, STRING("idiv rbx"), 1);\
                STORE(result_reg);

#define SHIFTOP(action)\
                LOAD("rax");\
                LOAD("rcx");\
                INSERT_LINE();\
                nasm_add_line(state, STRING(action" rax, cl"), 1);\
                STORE("rax");


#define BINCMPOP(cmov)\
                INSERT_LINE();\
                nasm_add_line(state, STRING("xor rcx, rcx"), 1);\
                LOAD("rax");\
                LOAD("rbx");\
                INSERT_LINE();\
                nasm_add_line(state, STRING("cmp rax, rbx"), 1);\
                INSERT_LINE();\
                nasm_add_line(state, STRING("mov rax, 1"), 1);\
                INSERT_LINE();\
                nasm_add_line(state, STRING(cmov" rcx, rax"), 1);\
                STORE("rcx");

void nasm_compile_func(string_t name, nasm_state_t *state) {
    if (!state->func->code.count) {
        if (state->func->is_external) {
            if (string_compare(name, STRING("putchar")) == 0) {
                return;
            }

            if (string_compare(name, STRING("debug_break")) == 0) {
                return;
            }
        }

        assert(false);
        return;
    }

    nasm_add_string(state, string_format(get_temporary_allocator(), STRING("%s:\n"), name));

    allocator_t *talloc = get_temporary_allocator();

    for (u64 i = 0; i < state->func->code.count; i++) {
        ir_opcode_t op = state->func->code[i];

        nasm_add_line(state, string_format(get_temporary_allocator(), STRING(".IROP_%u: ; %s"), i, get_ir_opcode_info(op)), 0);

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
                nasm_add_line(state, STRING("inc r15"), 1);
                break;
            case IR_PUSH_UNSIGN:
                INSERT_LINE();
                t = string_format(talloc, STRING("mov QWORD[r14 + r15 * 8], %u"), op.u_operand);
                nasm_add_line(state, t, 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("inc r15"), 1);
                break;
            case IR_PUSH_STACK:
                INSERT_LINE();
                t = string_format(talloc, STRING("mov rax, QWORD[rbp - %u * 8]"), op.u_operand);
                nasm_add_line(state, t, 1);
                STORE("rax");
                break;
            case IR_PUSH_SEA:
                INSERT_LINE();
                t = string_format(talloc, STRING("lea rax, QWORD[rbp - %u * 8]"), op.u_operand);
                nasm_add_line(state, t, 1);
                STORE("rax");
                break;

            case IR_POP:
                LOAD("rax");
                break;

            case IR_CLONE:
                LOAD("rax");
                STORE("rax");
                STORE("rax");
                break;

            case IR_ALLOC:
                // Stack probe...
                
                if ((op.u_operand * 8) > PG(1)) {
                    INSERT_LINE();
                    t = string_format(talloc, STRING("mov rax, %u * 8"), op.u_operand);
                    nasm_add_line(state, t, 1);
                    INSERT_LINE();
                    nasm_add_line(state, STRING("call __stack_probe"), 1);
                    INSERT_LINE();
                    nasm_add_line(state, STRING("sub rsp, rax"), 1);
                } else {
                    INSERT_LINE();
                    t = string_format(talloc, STRING("sub rsp, %u * 8"), op.u_operand);
                    nasm_add_line(state, t, 1);
                    INSERT_LINE();
                    nasm_add_line(state, STRING("mov QWORD[rsp], 0"), 1);
                }

                STORE("rsp");
                break;

            case IR_ADD: BINOP("add"); break;
            case IR_SUB: BINOP("sub"); break;
            case IR_MUL: BINOP("imul"); break;
            case IR_DIV: DIVOP("rax"); break;
            case IR_MOD: DIVOP("rdx"); break;

            case IR_CMP_EQ:  BINCMPOP("cmove");  break;
            case IR_CMP_NEQ: BINCMPOP("cmovne"); break;
            case IR_CMP_LT:  BINCMPOP("cmovl");  break;
            case IR_CMP_GT:  BINCMPOP("cmovg");  break;
            case IR_CMP_LTE: BINCMPOP("cmovle"); break;
            case IR_CMP_GTE: BINCMPOP("cmovge"); break;

            case IR_BIT_AND:     BINOP("and");   break;
            case IR_BIT_OR:      BINOP("or");    break;
            case IR_BIT_XOR:     BINOP("xor");   break;
            case IR_SHIFT_LEFT:  SHIFTOP("sal"); break;
            case IR_SHIFT_RIGHT: SHIFTOP("sar"); break;
                             
             case IR_BIT_NOT:
                LOAD("rax");
                nasm_add_line(state, STRING("mov rbx, -1"), 1);
                INSERT_LINE();
                nasm_add_line(state, STRING("xor rax, rbx"), 1);
                STORE("rax");
                break;

            case IR_JUMP: 
                INSERT_LINE();
                nasm_add_line(state, string_format(get_temporary_allocator(), STRING("jmp .IROP_%u"), (u64)((s64)i + 1 + op.s_operand)), 1);
                break;
            case IR_JUMP_IF: 
                LOAD("rax");
                INSERT_LINE();
                nasm_add_line(state, STRING("cmp rax, 0"), 1);
                INSERT_LINE();
                nasm_add_line(state, string_format(get_temporary_allocator(), STRING("jne .IROP_%u"), (u64)((s64)i + 1 + op.s_operand)), 1);
                break;
            case IR_JUMP_IF_NOT: 
                LOAD("rax");
                INSERT_LINE();
                nasm_add_line(state, STRING("cmp rax, 0"), 1);
                INSERT_LINE();
                nasm_add_line(state, string_format(get_temporary_allocator(), STRING("je .IROP_%u"), (u64)((s64)i + 1 + op.s_operand)), 1);
                break;

            case IR_FREE:
                INSERT_LINE();
                t = string_format(talloc, STRING("add rsp, %u * 8"), op.u_operand);
                nasm_add_line(state, t, 1);
                break;

            case IR_STORE:
                LOAD("rbx"); // first... address
                LOAD("rax");
                INSERT_LINE();
                nasm_add_line(state, STRING("mov QWORD[rbx], rax"), 1);
                break;

            case IR_LOAD:
                LOAD("rbx");
                INSERT_LINE();
                nasm_add_line(state, STRING("mov rax, QWORD[rbx]"), 1);
                STORE("rax");
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
                assert(false);
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

backend_t nasm_compile_program(ir_t *state) {
    nasm_state_t nasm = {};

    nasm.ir = state;
    array_create(&nasm.code, (u64) 1024, create_arena_allocator(PG(4)));

    { // header and RT   
        nasm_add_line(&nasm, STRING("; Created by bonmas14."));
        nasm_add_line(&nasm, STRING("; "));
        nasm_add_line(&nasm, STRING("; solum-compiler auto generated code"));
        nasm_add_line(&nasm, STRING("; nasm-win-x64"));
        nasm_add_line(&nasm, STRING(""));
        nasm_add_line(&nasm, STRING("default rel"));
        nasm_add_line(&nasm, STRING(""));
        nasm_add_line(&nasm, STRING("section .bss"));
        nasm_add_line(&nasm, STRING("exec_stack: resq 4096"));
        nasm_add_line(&nasm, STRING("putchar_buffer: resb 1"));
        nasm_add_line(&nasm, STRING("putchar_handle: resq 1"));
        nasm_add_line(&nasm, STRING(""));

        nasm_add_line(&nasm, STRING("section .text"));
        nasm_add_line(&nasm, STRING(""));
        nasm_add_line(&nasm, STRING("global mainCRTStartup"));
        nasm_add_line(&nasm, STRING("extern GetStdHandle, WriteConsoleA"));
        nasm_add_line(&nasm, STRING(""));
        nasm_add_line(&nasm, STRING("mainCRTStartup:"));

        nasm_add_line(&nasm, STRING("lea r14, [exec_stack]"), 1);
        nasm_add_line(&nasm, STRING("xor r15, r15"), 1);

        nasm_add_line(&nasm, STRING("call putchar_setup"), 1);
        nasm_add_line(&nasm, STRING("call main"), 1);
        nasm_add_line(&nasm, STRING("ret"), 1);
        nasm_add_line(&nasm, STRING(""));
    }

    { // stack probe function
        nasm_add_line(&nasm, STRING("__stack_probe: ; rax = allocation size"), 0);
        nasm_add_line(&nasm, STRING("test rax, rax"), 1);
        nasm_add_line(&nasm, STRING("jz .done"), 1);
        nasm_add_line(&nasm, STRING("mov r10, rsp"), 1);
        nasm_add_line(&nasm, STRING("sub r10, rax"), 1);
        nasm_add_line(&nasm, STRING("mov r11, r10"), 1);
        nasm_add_line(&nasm, STRING("and r11, -4096"), 1);
        nasm_add_line(&nasm, STRING("lea rcx, [rsp-1]"), 1);
        nasm_add_line(&nasm, STRING("and rcx, -4096"), 1);
        nasm_add_line(&nasm, STRING("cmp rcx, r11"), 1);
        nasm_add_line(&nasm, STRING("jb .done"), 1);
        nasm_add_line(&nasm, STRING(".loop:"), 0);
        nasm_add_line(&nasm, STRING("test dword [rcx], 0"), 1);
        nasm_add_line(&nasm, STRING("sub rcx, 4096"), 1);
        nasm_add_line(&nasm, STRING("cmp rcx, r11"), 1);
        nasm_add_line(&nasm, STRING("jae .loop"), 1);
        nasm_add_line(&nasm, STRING(".done:"), 0);
        nasm_add_line(&nasm, STRING("ret"), 1);
    }

    { // putchar code
        nasm_add_line(&nasm, STRING("putchar_setup:"));
        nasm_add_line(&nasm, STRING("push rbp"), 1);
        nasm_add_line(&nasm, STRING("mov rbp, rsp"), 1);
        nasm_add_line(&nasm, STRING("and rsp, -16"), 1);
        nasm_add_line(&nasm, STRING("mov rcx, -11"), 1);
        nasm_add_line(&nasm, STRING("sub rsp, 32"), 1);
        nasm_add_line(&nasm, STRING("call GetStdHandle"), 1);
        nasm_add_line(&nasm, STRING("add rsp, 32"), 1);
        nasm_add_line(&nasm, STRING("mov [putchar_handle], rax"), 1);
        nasm_add_line(&nasm, STRING("mov rsp, rbp"), 1);
        nasm_add_line(&nasm, STRING("pop rbp"), 1);
        nasm_add_line(&nasm, STRING("ret"), 1);

        nasm_add_line(&nasm, STRING("putchar:"));
        nasm_add_line(&nasm, STRING("push rbp"), 1);
        nasm_add_line(&nasm, STRING("mov rbp, rsp"), 1);
        nasm_add_line(&nasm, STRING("and rsp, -16"), 1);

        nasm_add_line(&nasm, STRING("dec r15"), 1);
        nasm_add_line(&nasm, STRING("mov rax, QWORD[r14 + r15 * 8]"), 1);
        nasm_add_line(&nasm, STRING("mov QWORD[r14 + r15 * 8], 0"), 1);
        nasm_add_line(&nasm, STRING("mov BYTE[putchar_buffer], al"), 1);

        nasm_add_line(&nasm, STRING("mov rcx, [putchar_handle]"), 1);
        nasm_add_line(&nasm, STRING("lea rdx, [putchar_buffer]"), 1);
        nasm_add_line(&nasm, STRING("mov r8, 1"), 1);
        nasm_add_line(&nasm, STRING("mov r9, 0 "), 1);
        nasm_add_line(&nasm, STRING("push 0   "), 1);
        nasm_add_line(&nasm, STRING("sub rsp, 32"), 1);
        nasm_add_line(&nasm, STRING("call WriteConsoleA"), 1);
        nasm_add_line(&nasm, STRING("add rsp, 32"), 1);
        nasm_add_line(&nasm, STRING("mov rsp, rbp"), 1);
        nasm_add_line(&nasm, STRING("pop rbp"), 1);
        nasm_add_line(&nasm, STRING("ret"), 1);
    }

    { // debug break
        nasm_add_line(&nasm, STRING("debug_break:"));
        nasm_add_line(&nasm, STRING("int3"), 1);
        nasm_add_line(&nasm, STRING("ret"), 1);
    }

    for (u64 i = 0; i < state->functions.capacity; i++) {
        kv_pair_t<string_t, ir_function_t> *pair = state->functions.entries + i;

        if (!pair->occupied) continue;
        if (pair->deleted)   continue;

        nasm.func = &pair->value;
        nasm_compile_func(pair->key, &nasm);
    }

    list_t<u8> list = array_to_list(&nasm.code);

    return {list.alloc, list};
}
