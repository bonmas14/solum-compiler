#ifndef IR_GEN_H
#define IR_GEN_H

#include "compiler.h"
#include "area_alloc.h"
#include "hashmap.h"

struct ir_opcode_t {
    u32 operation;
    union {
        struct {
            u64 math_l;
            u64 math_r;
        };

        u64 offset;
    };
};

// IMM 
// MV
// LD
// ST
// LD R
// ST R
//
// ADD
// SUB
// AND
// INV
// OR
// XOR
//
// HLT
// NOP
//
// JMP
// JEZ
// JNEZ
// JGZ
// JLZ
// JGEZ
// JLEZ
// BIT
//

enum ir_opcodes_t {

    IR_NOP       = 0x0,
    IR_BREAK,
    IR_CALL,
    IR_RETURN,
    
    IR_MATH_FLAG = 0x00010000,

    IR_ADD,
    IR_SUB,

    IR_MUL,
    IR_DIV,

    IR_MOD,

    IR_LS,
    IR_RS,

    IR_AND,
    IR_OR,
    IR_XOR,
    IR_INV,

    IR_COMPARE,

    IR_MEM_FLAG  = 0x00020000,


    IR_LOAD,
    IR_STORE,
    IR_IMM,
    IR_MOVE,

    IR_PUSH,
    IR_POP,

    // get value from pointer + offset

    IR_BRANCH_FLAG = 0x00040000,
    
    IR_JUMP,

};

#endif // IR_GEN_H
