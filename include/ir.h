#ifndef IR_H
#define IR_H

#include "stddefines.h"
#include "list.h"
#include "compiler.h"
#include "analyzer.h"
#include "stack.h"
#include "hashmap.h"
#include "scanner.h"

// 
// #   #  ###  ##### #####
// ##  # #   #   #   #     
// # # # #   #   #   ### 
// #  ## #   #   #   #    
// #   #  ###    #   #####   
//
// OUTPUT OF THIS IR GENERATOR SHOULD NOT DEPEND ON
// PREVIOUS STEPS. 
//
// WE THROW AWAY THAT CRAP!
// 

enum ir_codes_t {
    IR_NOP = 0x0,

    // Push operations
    IR_PUSH_SIGN,   // Push signed 64-bit integer
    IR_PUSH_UNSIGN, // Push unsigned 64-bit integer
    IR_PUSH_F32,    // Push 32-bit float
    IR_PUSH_F64,    // Push 64-bit float

    // Stack manipulation
    IR_POP,         // Pop top of stack
    IR_CLONE,       // Duplicate top element

    // Memory operations for ir
    IR_GLOBAL,      // Push global variable   (variable index)
    IR_LEA,         // Load effective address (variable index)
    
    // ------- second stack memory!
    IR_ALLOC,       // Allocate memory   (amount)
    IR_FREE,        // Free     memory   (amount)
    IR_LOAD,        // Load from address [address]
    IR_STORE,       // Store to address  [address] [value]

    // math [left] [right]
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_NEG,

    // Bitwise operations
    IR_BIT_AND,    
    IR_BIT_OR,     
    IR_BIT_XOR,    
    IR_BIT_NOT,    
    IR_SHIFT_LEFT, 
    IR_SHIFT_RIGHT,

    // Integer comparisons (push 1/0)
    IR_CMP_EQ,     
    IR_CMP_NEQ,    
    IR_CMP_LT,     
    IR_CMP_GT,     
    IR_CMP_LTE,    
    IR_CMP_GTE,    

    // Logical operations
    IR_LOG_AND,    
    IR_LOG_OR,     
    IR_LOG_NOT,    

    // Control flow (offset)
    IR_JUMP,       // Unconditional jump (offset operand)
    IR_JUMP_IF,    // Jump if top != 0 ()
    IR_JUMP_IF_NOT,// Jump if top == 0
    IR_RET,        // Return from function

    // Function calls
    IR_CALL,       // Call function (address on stack)
    IR_CALL_EXTERN,// Call external symbol (string operand)

    // System
    IR_BRK,        // Breakpoint
    IR_INVALID,        // Invalid instruction
};

struct ir_opcode_t {
    u64 operation;      // ir_codes_t value
    token_t info;       // Source token for debugging
    u64     index;

    stack_t<hashmap_t<string_t, scope_entry_t>*> search_info;

    union {
        u64 u_operand;
        s64 s_operand;
        f64 f_operand;
    };

    string_t string;
};

struct ir_variable_t {
    u32 size, alignment;
};

struct ir_function_t {
    b32 is_valid;
    
    list_t<ir_opcode_t> code;
    list_t<ir_variable_t> vars;
};

struct ir_t {
    b32 is_valid;

    hashmap_t<string_t, ir_function_t> functions;
    hashmap_t<string_t, scope_entry_t> variables;
    // hashmap_t<string_t, u8> strings;
};

void print_ir_opcode(ir_opcode_t op);
ir_t compile_program(compiler_t *compiler);

#endif
