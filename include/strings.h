#ifndef STRINGS_H
#define STRINGS_H

#include "stddefines.h"
#include "allocator.h"
#include "list.h"

#define string_temp_concat(a, b) string_concat(a, b, get_temporary_allocator())
#define string_temp_to_c_string(a) string_to_c_string(a, get_temporary_allocator())
#define string_temp_copy(a) string_copy(a, get_temporary_allocator())

#ifdef _WIN32
#define HOST_SYSTEM_SLASH (u8)'\\'
#define SWAP_SLASH (u8)'/'
#else
#define HOST_SYSTEM_SLASH (u8)'/'
#define SWAP_SLASH (u8)'\\'
#endif

#define index_of_last_file_slash(input) string_last_index_of(string_swap(input, SWAP_SLASH, HOST_SYSTEM_SLASH, get_temporary_allocator()), HOST_SYSTEM_SLASH)

list_t<string_t> string_split(string_t input, string_t pattern,   allocator_t *alloc);
string_t string_join(list_t<string_t> input,  string_t separator, allocator_t *alloc);

char   * string_to_c_string(string_t a, allocator_t *alloc);

string_t string_swap(string_t input, u8 from, u8 to, allocator_t *alloc);
string_t string_concat(string_t a, string_t b, allocator_t *alloc);
string_t string_copy(string_t a, allocator_t *alloc);
string_t string_substring(string_t input, u64 start, u64 size, allocator_t *alloc);

u64 c_string_length(const char *c_str);

b32 string_compare(string_t a, string_t b);
s64 string_index_of(string_t input, u8 value);
s64 string_last_index_of(string_t input, u8 value);

void string_tests(void);

#endif // STRINGS_H
