#include "stddefines.h"
#include "list.h"
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


    IR_PUSH_s, // [s64]
    IR_PUSH_u, // [u64]

    IR_POP,
    IR_CLONE,

    IR_GLOBAL, // push value from .data [s32]


    IR_ALLOC, // [u64] allocate on stack
    IR_FREE,  // [u64] free from stack


    IR_RET,

    IR_BRK, // break   (int3 eq)
    IR_INV, // invalid (ud2  eq)
};

struct ir_opcode_t {
    u64 operation;
    token_t  info;
};

struct ir_function_t {
    b32 valid;
    string_t symbol;
    
    list_t<ir_opcode_t> code;
    list_t<u8> data;
};

struct ir_t {
    b32 is_valid;

    list_t<ir_opcode_t> code;
    list_t<ir_opcode_t> data;
    u64                 uninit_size;
    hashmap_t<string_t, u64> strings;
    hashmap_t<string_t, u64> ext_symbols;
};

struct scope_t {
    hashmap_t<string_t, u64> variables;
    hashmap_t<string_t, u64> constants;
    u64 stack_size;
};

ir_t compile_program(compiler_t *compiler);
