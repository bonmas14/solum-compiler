#ifndef USER_DEFINES
#define USER_DEFINES

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float  f32;
typedef double f64;

typedef uint32_t b32;

struct string_t {
    u64 size;
    u8 *data;
};

struct symbol_t {
    u64 size;
    u64 table_index;
};

#define STR(s) reinterpret_cast<const u8*>(s)

// @nocheckin
#define STRING(s) (string_t) { .size = sizeof(s), .data = reinterpret_cast<const u8*>(s) }

#ifdef DEBUG
#define assert(result) { \
    if ((result) == false) { \
        u8 fname[256] = { __FILE__ };\
        u64 line = __LINE__;\
        u8 buffer[512];\
        sprintf((char*)buffer, "assertion failed: %.256s, line: %zu", fname, line);\
        log_error(buffer, 0);\
    } \
}
#else 
#define assert(result)
#endif


#endif // USER_DEFINES
