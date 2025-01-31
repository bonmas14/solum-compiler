#ifndef COMPILER_H
#define COMPILER_H

#include "stddefines.h"
#include "logger.h"

struct scanner_t;
struct parser_t;
struct analyzer_t;

struct compiler_t {
    scanner_t  *scanner;
    parser_t   *parser;
    analyzer_t *analyzer;
};

#endif
