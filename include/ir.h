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
};

struct ir_opcode_t {
    u64 operation;
    token_t  info;
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

ir_t compile_program(compiler_t *compiler, scope_entry_t *entry_function);
