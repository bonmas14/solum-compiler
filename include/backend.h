#ifndef BACKEND_H
#define BACKEND_H

#include "compiler.h"
#include "scanner.h"
#include "parser.h"
#include "analyzer.h"

struct codegen_state_t;

void generate_code(compiler_t *compiler);

#endif // BACKEND_H
