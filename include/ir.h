#include "stddefines.h"
#include "list.h"
#include "stack.h"
#include "hashmap.h"
#include "scanner.h"

enum ir_codes_t {
    IR_NOP = 0x0,
};

struct ir_opcode_t {
    u64 operation;
    token_t  info;
};


struct ir_t {
    list_t<ir_opcode_t> code;
    list_t<ir_opcode_t> data;
    list_t<ir_opcode_t> uninit;
    hashmap_t<string_t, u64> strings;
    hashmap_t<string_t, u64> ext_symbols;
};

struct compiled_var_t {

};

struct compiled_const_t {
    token_t name;
};

struct scope_t {
    hashmap_t<string_t, u64> variables;
    hashmap_t<string_t, u64> constants;
    u64 stack_size;
};

struct local_vars_t {
    stack_t<scope_t> vars;
};
