#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include "stddefines.h"

enum argument_types_t {
    ARG_ERROR,
    ARG_UNKN,
    ARG_HELP,
    ARG_VERSION,
    ARG_NO_ANSI,
    ARG_VERBOSE,
    ARG_SHOW_LINK_TIME,
    ARG_OUTPUT_FILE_NAME,
};

struct argument_t {
    u64      type;
    string_t content;
};

argument_t parse_argument(string_t input);
#endif
