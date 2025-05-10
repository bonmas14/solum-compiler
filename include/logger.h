#ifndef LOGGER_H
#define LOGGER_H

#include "stddefines.h"
#include "platform.h"

#define INFO_COLOR     96, 224, 255
#define WARNING_COLOR 226, 175,  63 
#define ERROR_COLOR   255,  64,  64

#define LEFT_PAD_STANDART_OFFSET (4)

void add_left_pad(FILE * file, u64 amount);

void log_push_color(u8 r, u8 g, u8 b);
void log_pop_color(void);
void log_update_color(void);
void log_color_reset(void);

void log_write(string_t text);
void log_info(string_t text);
void log_warning(string_t text);
void log_error(string_t text);

#define check_value(value) {\
    if ((b32)(value) == false) {\
        u8 buffer[1024];\
        sprintf((char*)buffer, "failed check in file: '%.256s', in function '%.128s', line: %zu", __FILE__, __func__, (size_t)__LINE__);\
        log_error((string_t) { .size = c_string_length((char*)buffer), .data = (u8*)buffer });\
    }\
}

#ifdef DEBUG
#define assert(result) {\
    if ((b32)(result) == false) {\
        u8 buffer[1024];\
        sprintf((char*)buffer, "assertion failed in file: '%.256s', in function '%.128s', line: %zu", __FILE__, __func__, (size_t)__LINE__);\
        log_error((string_t) { .size = c_string_length((char*)buffer), .data = (u8*)buffer });\
        debug_break();\
    } \
}
#else 
#define assert(...)
#endif

// @todo log_token also should be here
#endif // LOGGER_H
