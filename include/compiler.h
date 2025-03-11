#ifndef COMPILER_H
#define COMPILER_H

#include "stddefines.h"
#include "arena.h"
#include "area_alloc.h"
#include "hashmap.h"
#include "logger.h"

#define STRING_ALLOCATOR_INIT_SIZE 4096
#define INIT_NODES_SIZE 1024

#define MAX_COUNT_OF_PARAMS 16
#define MAX_COUNT_OF_RETS 1 

struct scanner_t;
struct parser_t;
struct analyzer_t;
struct codegen_t;

struct compiler_t {
    b32 is_valid;

    arena_t *string_allocator;
    arena_t *node_allocator;

    scanner_t  *scanner;
    parser_t   *parser;
    analyzer_t *analyzer;
    codegen_t  *codegen;
};

compiler_t create_compiler_instance(void);

#endif
