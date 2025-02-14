#ifndef LOGGER_H
#define LOGGER_H

#include "stddefines.h"

#define INFO_COLOR     96, 224, 255
#define WARNING_COLOR 226, 175,  63 
#define ERROR_COLOR   255,  64,  64

#define LEFT_PAD_STANDART_OFFSET (4)


void log_push_color(u8 r, u8 g, u8 b);
void log_pop_color(void);
void log_update_color(void);

void log_write(u8 *text, u64 left_pad);
void log_info(u8 *text, u64 left_pad);
void log_warning(u8 *text, u64 left_pad);
void log_error(u8 *text, u64 left_pad);

void debug_break(void);

#define check(value) {\
    if ((b32)(value) == false) {\
        u8 fname[256] = { __FILE__ };\
        u64 line = __LINE__;\
        u8 buffer[512];\
        sprintf((char*)buffer, "check failed: %.256s, line: %zu", fname, line);\
        log_error(buffer, 0);\
    }\
}


#ifdef DEBUG
#define assert(result) { \
    if ((b32)(result) == false) { \
        u8 fname[256] = { __FILE__ };\
        u64 line = __LINE__;\
        u8 buffer[512];\
        sprintf((char*)buffer, "assertion failed: %.256s, line: %zu", fname, line);\
        log_error(buffer, 0);\
        debug_break();\
    } \
}
#else 
#define assert(result)
#endif

// @todo log_token also should be here
#endif // LOGGER_H
