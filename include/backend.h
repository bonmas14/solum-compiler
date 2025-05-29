#ifndef BACKEND_H
#define BACKEND_H

#include "compiler.h"
#include "scanner.h"
#include "parser.h"
#include "analyzer.h"
#include "ir.h"

void nasm_compile_program(ir_t *state);

#endif // BACKEND_H
