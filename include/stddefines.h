#ifndef USER_DEFINES
#define USER_DEFINES

#define UNUSED(x) (void)(x)

#define KB(s) ((u64)(s) * 1024LL)
#define MB(s) (KB(s) * 1024LL)
#define GB(s) (MB(s) * 1024LL)

// @todo: should be platform specific
#define PG(s) ((u64)(s) * KB(4))

#define MAX(a, b) (a) > (b) ? (a) : (b)
#define MIN(a, b) (a) < (b) ? (a) : (b)

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

typedef uint8_t b8;
typedef uint32_t b32;

struct string_t {
    u64 size;
    u8 *data;
};

struct allocator_t;
extern allocator_t * default_allocator;

u64 c_string_length(const char *c_str);
#define STR(s) reinterpret_cast<u8*>(const_cast<char*>(s))
#define STRING(s) (string_t) { .size = c_string_length(s), .data = STR(s) }

#endif // USER_DEFINES
