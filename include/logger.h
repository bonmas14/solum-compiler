#ifndef LOGGER_H
#define LOGGER_H

#include "stddefines.h"
#include "allocator.h"
#include "talloc.h"

#define INFO_COLOR     96, 224, 255
#define WARNING_COLOR 226, 175,  63 
#define ERROR_COLOR   255,  64,  64

#define LEFT_PAD_STANDART_OFFSET (4)


void add_left_pad(FILE * file, u64 amount);

void log_push_color(u8 r, u8 g, u8 b);
void log_pop_color(void);
void log_update_color(void);

void log_print(string_t string);

void log_write(u8 *text);
void log_info(u8 *text);
void log_warning(u8 *text);
void log_error(u8 *text);

#define str_concat(a, b) string_concat(a, b, default_allocator)
#define string_temp_concat(a, b) string_concat(a, b, get_temporary_allocator())
string_t string_concat(string_t a, string_t b, allocator_t *alloc);

void debug_break(void);

#define check_value(value) {\
    if ((b32)(value) == false) {\
        u8 fname[256] = { __FILE__ };\
        u64 line = __LINE__;\
        u8 buffer[512];\
        sprintf((char*)buffer, "check failed: %.256s, line: %zu", (char*)fname, (size_t)line);\
        log_error(buffer);\
    }\
}


#ifdef DEBUG
#define assert(result) { \
    if ((b32)(result) == false) { \
        u8 fname[256] = { __FILE__ };\
        u64 line = __LINE__;\
        u8 buffer[512];\
        sprintf((char*)buffer, "assertion failed: %.256s, line: %zu", (char*)fname, (size_t)line);\
        log_error(buffer);\
        debug_break();\
    } \
}
#else 
#define assert(...)
#endif

// @todo log_token also should be here
#endif // LOGGER_H
