#include "arg_parser.h"
#include "allocator.h"
#include "talloc.h"
#include "strings.h"

argument_t parse_argument(string_t input) {
    input = string_copy(input, default_allocator);

    if (input.size < 2) return { ARG_UNKN, input };
    if (input[0] != '-' || input[1] != '-') return { ARG_UNKN, input };

    input = { input.size - 2, input.data + 2 };

    if (string_compare(STRING("help"),      input) == 0)  return { ARG_HELP,             input };
    if (string_compare(STRING("no-ansi"),   input) == 0)  return { ARG_NO_ANSI,          input };
    if (string_compare(STRING("verbose"),   input) == 0)  return { ARG_VERBOSE,          input };
    if (string_compare(STRING("version"),   input) == 0)  return { ARG_VERSION,          input };
    if (string_compare(STRING("output"),    input) == 0)  return { ARG_OUTPUT_FILE_NAME, input };
    if (string_compare(STRING("link-time"), input) == 0)  return { ARG_SHOW_LINK_TIME,   input };

    return { ARG_ERROR, input };
}
