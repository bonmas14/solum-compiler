#ifndef IR_H
#define IR_H

#include "stddefines.h"
#include "stack.h"
#include "hashmap.h"
#include "list.h"
#include "array.h"

#include "compiler.h"
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

    IR_SETUP_GLOBAL, // (address of global) [value]

    // Stack manipulation
    IR_PUSH_SIGN,   // Push s64 integer
    IR_PUSH_UNSIGN, // Push u64 integer
    
    IR_PUSH_STACK,  // Push stack  variable          (offset)
    IR_PUSH_GLOBAL, // Push global variable          (offset)
    IR_PUSH_GEA,    // Push global effective address (offset)
    IR_PUSH_SEA,    // Push stack  effective address (offset)

    IR_POP,         // Pop top of stack
    IR_CLONE,       // Duplicate top element
    
    // ------- second stack memory!
    
    IR_STACK_FRAME_PUSH,
    IR_STACK_FRAME_POP,

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

    // Bitwise operations [left] [right]
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
    IR_LOG_NOT,    

    // Control flow (offset)
    IR_JUMP,       // Unconditional jump (offset operand)
    IR_JUMP_IF,    // Jump if top != 0 ()
    IR_JUMP_IF_NOT,// Jump if top == 0
    IR_RET,        // Return from function

    // Function calls
    IR_CALL,       // Call function (address on stack)

    // System
    IR_BRK,        // Breakpoint
    IR_INVALID,        // Invalid instruction
};

struct ir_opcode_t {
    u64 operation;      // ir_codes_t value
    token_t info;       // Source token for debugging
    u64     index;

    union {
        u64 u_operand;
        s64 s_operand;
        f64 f_operand;
    };

    string_t string;
};

struct ir_function_t {
    b32 is_external;
    u64 stack_index;
    u64 global_index;
    array_t<ir_opcode_t> code;
};

struct ir_t {
    b32 is_valid;

    allocator_t code;
    hashmap_t<string_t, ir_function_t> functions;
    array_t<s64>                       globals;
};

string_t get_ir_opcode_info(ir_opcode_t op);
void print_ir_opcode(ir_opcode_t op);
ir_t compile_program(compiler_t *compiler);

#endif
