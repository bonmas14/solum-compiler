#ifndef BACKEND_H
#define BACKEND_H

#include "compiler.h"
#include "scanner.h"
#include "parser.h"
#include "analyzer.h"
#include "allocator.h"
#include "ir.h"

struct backend_t {
    allocator_t *alloc;
    list_t<u8>   code;
};

backend_t nasm_compile_program(ir_t *state);

#endif // BACKEND_H
