#ifndef IR_GEN_H
#define IR_GEN_H

#include "compiler.h"
#include "list.h"
#include "hashmap.h"

struct ir_opcode_t {
    u64 operation;
    u64 arg1;
    u64 arg2;
    u64 arg3;
};

/*
#[derive(Default, Clone)]
pub struct ПП {
    pub код: Vec<Инструкция>,
    pub иниц_данные: Vec<u8>,
    pub размер_неиниц_данных: usize,
    pub заплатки_неиниц_указателей: Vec<usize>,
    pub строки: HashMap<String, usize>,
    pub внешние_символы: HashMap<String, usize>,
    pub библиотеки: HashMap<String, usize>,
}
*/

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
    IR_NOP = 0x0,
    
    IR_PUSH, // [1] value
    IR_POP,  // [0]
             
    IR_IF,   // [2] comp, cjump ...
    IR_ELSE, // [1] jump ...
    IR_ELIF, // [3] jump comp cjump ...
    IR_RET,  // [0]

    IR_WHILE, // [2] cond, cjump .... <jump
    
    IR_SET,  
    IR_GET,

    IR_INVERT,

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

    IR_LOAD,
    IR_STORE,
    IR_IMM,
    IR_MOVE,

    IR_JUMP,
};

list_t<ir_opcode_t> gen_statement_ir(compiler_t *state, ast_node_t *statement);
void print_ir_opcode(ir_opcode_t *code);

#endif // IR_GEN_H
