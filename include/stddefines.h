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

#define STR(s) reinterpret_cast<const u8*>(s)

// @nocheckin
#define STRING(s) (string_t) { .size = sizeof(s), .data = reinterpret_cast<const u8*>(s) }

#ifdef DEBUG
#define assert(result) { \
    if ((result) == false) { \
        log_error(STR("assertion failed."), 0);\
    } \
}
#else 
#define assert(result)
#endif


#endif // USER_DEFINES
