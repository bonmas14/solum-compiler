#include "strings.h"
#include "memctl.h"
#include "logger.h"
#include "allocator.h"
#include "talloc.h"
#include <stdarg.h>

void mem_set(u8 *buffer, u8 value, u64 size) {
    if (size == 0) return;
    assert(buffer != NULL);

    while (size-- > 0) {
        *buffer++ = value;
    }
}

void mem_copy(u8 *dest, u8 *source, u64 size) {
    if (size == 0) return;

    assert(dest   != NULL);
    assert(source != NULL);

#ifdef DEBUG
    if (dest < source) {
        assert((uintptr_t)(source - dest) >= (uintptr_t)size);
    } else {
        assert((uintptr_t)(dest - source) >= (uintptr_t)size);
    }
#endif // DEBUG

    while (size-- > 0) {
        *dest++ = *source++;
    }
}

void mem_move(u8 *dest, u8 *source, u64 size) {
    if (size == 0) return;

    assert(dest   != NULL);
    assert(source != NULL);

    allocator_t *talloc = get_temporary_allocator();
    u8* buff = (u8*)mem_alloc(talloc, size);

    mem_copy(buff, source, size);
    mem_copy(dest, buff, size);
}

s32 mem_compare(u8 *left, u8 *right, u64 size) {
    while (size-- > 0) {
        if (*left++ == *right++)
            continue;

        return left[-1] > right[-1] ? 1 : -1;
    }

    return 0;
}

char *string_to_c_string(string_t a, allocator_t *alloc) {
    assert(a.data != NULL);

    if (alloc == NULL) {
        alloc = default_allocator;
    }

    u8* data = (u8*)mem_alloc(alloc, a.size + 1);

    if (data == NULL) {
        log_error(STRING("string conversion failed, buy more ram, or provide normal allocator..."));
        return NULL;
    }

    mem_move(data, a.data, a.size);
    return (char*)data;
}

u64 c_string_length(const char *c_str) {
    char* s = const_cast<char*>(c_str);

    while (*s != '\0') {
        s++;
    }

    return s - const_cast<char*>(c_str);
}

string_t string_swap(string_t input, u8 from, u8 to, allocator_t *alloc) {
    string_t output = string_copy(input, alloc);

    for (u64 i = 0; i < input.size; i++) {
        if (output.data[i] != from)
            continue;

        output.data[i] = to;
    }

    return output;
}

s32 string_compare(string_t a, string_t b) {
    if (a.size == b.size && a.size == 0) return 0;
    if (a.size == 0) return -1;
    if (b.size == 0) return 1;

    return mem_compare(a.data, b.data, a.size);
}

string_t string_join(list_t<string_t> input, string_t separator, allocator_t *alloc) {
    string_t     temp   = {};
    allocator_t *talloc = get_temporary_allocator();

    if (input.count == 1) {
        return string_copy(*list_get(&input, 0), alloc);
    }

    for (u64 i = 0; i < input.count; i++) {
        string_t member = *list_get(&input, i);
        if (i == 0) {
            temp = string_concat(temp, member, talloc);
        } else if (i != (input.count - 1)) {
            string_t separated = string_concat(separator, member, talloc);
            temp = string_concat(temp, separated, talloc);
        } else {
            string_t separated = string_concat(separator, member, talloc);
            return string_concat(temp, separated, alloc);
        }
    }

    return {};
}

list_t<string_t> string_split(string_t input, string_t pattern) {
    allocator_t *talloc = get_temporary_allocator();

    list_t<string_t> splits = {};

    if (input.size <= pattern.size) {
        return {};
    }

    if (pattern.size == 0) {
        return {};
    }

    u64 start = 0;

    for (u64 i = 0; i < (input.size - (pattern.size - 1)); i++) {
        if (mem_compare(input.data + i, pattern.data, pattern.size) != 0) {
            continue;
        }

        u64 size = i - start; 

        if (size == 0) {
            start = i + pattern.size;
            continue;
        }

        u8* buffer = (u8*)mem_alloc(talloc, size);
        mem_move(buffer, input.data + start, size);
        string_t output = { .size = size, .data = buffer };
        list_add(&splits, &output);
        start = i + pattern.size;
    }

    if (start != input.size - pattern.size) {
        if (mem_compare(input.data + start, pattern.data, pattern.size) == 0) {
            return splits;
        }

        u64 size = input.size - start; 
        if (size == 0) {
            return splits;
        }

        u8* buffer = (u8*)mem_alloc(talloc, size);
        mem_move(buffer, input.data + start, size);
        string_t output = { .size = size, .data = buffer };
        list_add(&splits, &output);
    }

    return splits;
}

string_t string_copy(string_t a, allocator_t *alloc) {
    if (a.data == NULL) {
        return {};
    }

    if (alloc == NULL) {
        alloc = default_allocator;
    }

    u8* data = (u8*)mem_alloc(alloc, a.size);

    if (data == NULL) {
        log_error(STRING("string copy failed, buy more ram, or provide normal allocator..."));
        return {};
    }

    mem_move(data, a.data, a.size);
    return { .size = a.size, .data = (u8*) data };
}

string_t string_concat(string_t a, string_t b, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    if (a.size == b.size && a.size == 0) {
        return {};
    }

    u8* data = (u8*)mem_alloc(alloc, a.size + b.size);

    if (a.size > 0) {
        assert(a.data != NULL);
        mem_move(data, a.data, a.size);
    }

    if (b.size > 0) {
        assert(b.data != NULL);
        mem_move((data + a.size), b.data, b.size);
    }

    return {.size = a.size + b.size, .data = (u8*) data };
}

string_t string_substring(string_t input, u64 start, u64 size, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    assert(size > 0);
    if (start > (input.size - size)) {
        log_error(STRING("start position with size overlapping the input string."));
        return {};
    }
    u8* data = (u8*)mem_alloc(alloc, size);
    mem_move(data, input.data + start, size);

    return {.size = size, .data = (u8*) data };
}

s64 string_index_of(string_t input, u8 value) {
    for (u64 i = 0; i < input.size; i++) {
        if (input.data[i] == value) return i;
    }

    return -1;
}

s64 string_last_index_of(string_t input, u8 value) {
    s64 index = -1;

    for (u64 i = 0; i < input.size; i++) {
        if (input.data[i] == value) index = i;
    }

    return index;
}

static string_t format_u64(u64 value) {
    if (value == 0) {
        return STRING("0");
    }
 
    allocator_t *talloc = get_temporary_allocator();
    string_t     output = {};

    while (value) {
        u8 digit = '0' + (value % 10);
        output = string_concat(output, { 1, &digit }, talloc);
        value /= 10;
    }

    for (u64 l = 0, r = output.size - 1; l < r; l++, r--) {
        u8 t           = output[l];
        output.data[l] = output[r];
        output.data[r] = t;
    }

    return output;
}

static string_t format_s64(s64 value) {
    if (value == 0) {
        return STRING("0");
    }
 
    allocator_t *talloc = get_temporary_allocator();
    string_t     output = {};

    b32 negative = false;

    if (value < 0) {
        value    = -value;
        negative = true;
    }

    while (value) {
        u8 digit = '0' + (value % 10);
        output = string_concat(output, { 1, &digit }, talloc);
        value /= 10;
    }

    if (negative) {
        u8 sign = '-';
        output = string_concat(output, { 1, &sign }, talloc);
    }

    for (u64 l = 0, r = output.size - 1; l < r; l++, r--) {
        u8 t           = output[l];
        output.data[l] = output[r];
        output.data[r] = t;
    }

    return output;
}

string_t string_format(allocator_t *alloc, string_t buffer...) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    va_list args;
    va_start(args, buffer);
    
    allocator_t *talloc = get_temporary_allocator();

    string_t o = {};
    
    for (u64 i = 0; i < buffer.size; i++) {
        if (buffer[i] != '%') {
            o = string_concat(o, { 1, buffer.data + i }, talloc);
            continue;
        }

        switch (buffer[++i]) {
            case '%':
            {
                o = string_concat(o, { 1, buffer.data + i }, talloc);
            } break;

            case 'u':
            {
                o = string_concat(o, format_u64(va_arg(args, u64)), talloc);
            } break;

            case 'd': 
            {
                o = string_concat(o, format_s64(va_arg(args, s64)), talloc);
            } break;

            case 's':
            {
                o = string_concat(o, va_arg(args, string_t), talloc);
            } break;
            default: break;
        }
    }
    // va_arg(args, );
    // va_arg(args, long long);

    va_end(args);
    return string_copy(o, alloc);
}

void string_tests(void) {
#ifdef DEBUG
    temp_reset();

    assert(c_string_length("Hello") == 5);
    assert(c_string_length("")      == 0);
    assert(c_string_length("What")  == 4);

    allocator_t *alloc = get_temporary_allocator();

    string_t result = {};

    result = string_concat(STRING("Hello "), STRING("world!"), alloc);
    assert(!string_compare(result, STRING("Hello world!")));

    assert(!string_compare(string_copy(result, alloc), result));
    
    list_t<string_t> splits = string_split(STRING("Eatin burger wit no honey mustard"), STRING(" "));

    assert(splits.count == 6);

    assert(!string_compare(splits.data[0], STRING("Eatin")));
    assert(!string_compare(splits.data[1], STRING("burger")));
    assert(!string_compare(splits.data[2], STRING("wit")));
    assert(!string_compare(splits.data[3], STRING("no")));
    assert(!string_compare(splits.data[4], STRING("honey")));
    assert(!string_compare(splits.data[5], STRING("mustard")));

    list_delete(&splits);

    splits = string_split(STRING("Eatin||burger||wit||no||honey||mustard"), STRING("||"));

    assert(splits.count == 6);

    assert(!string_compare(splits.data[0], STRING("Eatin")));
    assert(!string_compare(splits.data[1], STRING("burger")));
    assert(!string_compare(splits.data[2], STRING("wit")));
    assert(!string_compare(splits.data[3], STRING("no")));
    assert(!string_compare(splits.data[4], STRING("honey")));
    assert(!string_compare(splits.data[5], STRING("mustard")));

    list_delete(&splits);

    splits = string_split(STRING("Eatin||burger||wit||no||honey||mustard||a"), STRING("||"));

    assert(splits.count == 7);

    assert(!string_compare(splits.data[0], STRING("Eatin")));
    assert(!string_compare(splits.data[1], STRING("burger")));
    assert(!string_compare(splits.data[2], STRING("wit")));
    assert(!string_compare(splits.data[3], STRING("no")));
    assert(!string_compare(splits.data[4], STRING("honey")));
    assert(!string_compare(splits.data[5], STRING("mustard")));
    assert(!string_compare(splits.data[6], STRING("a")));

    list_delete(&splits);

    splits = string_split(STRING("a||Eatin||burger||wit||no||honey||mustard||a"), STRING("||"));

    assert(splits.count == 8);

    assert(!string_compare(splits.data[0], STRING("a")));
    assert(!string_compare(splits.data[1], STRING("Eatin")));
    assert(!string_compare(splits.data[2], STRING("burger")));
    assert(!string_compare(splits.data[3], STRING("wit")));
    assert(!string_compare(splits.data[4], STRING("no")));
    assert(!string_compare(splits.data[5], STRING("honey")));
    assert(!string_compare(splits.data[6], STRING("mustard")));
    assert(!string_compare(splits.data[7], STRING("a")));

    list_delete(&splits);

    splits = string_split(STRING("||Eatin||burger||wit||no||honey||mustard||"), STRING("||"));

    assert(splits.count == 6);

    assert(!string_compare(splits.data[0], STRING("Eatin")));
    assert(!string_compare(splits.data[1], STRING("burger")));
    assert(!string_compare(splits.data[2], STRING("wit")));
    assert(!string_compare(splits.data[3], STRING("no")));
    assert(!string_compare(splits.data[4], STRING("honey")));
    assert(!string_compare(splits.data[5], STRING("mustard")));

    assert(!string_compare(string_join(splits, STRING(" "), alloc), STRING("Eatin burger wit no honey mustard")));

    list_delete(&splits);

    assert(!string_compare(string_substring(STRING("HelloWorld!"), 5, 6, alloc), STRING("World!")));
    assert(!string_compare(string_substring(STRING("HelloWorld!"), 0, 11, alloc), STRING("HelloWorld!")));
    assert(!string_compare(string_substring(STRING("HelloWorld!"), 10, 1, alloc), STRING("!")));

    assert(string_index_of(STRING("CP/M"), (u8)'/') == 2);
    assert(string_last_index_of(STRING("https://github.com/bonmas14"), (u8)'/') == 18);

    assert(!string_compare(string_swap(STRING("/path/from/unix/systems/"), (u8)'/', (u8) '\\', alloc), STRING("\\path\\from\\unix\\systems\\"))); 

    assert(!string_compare(string_format(alloc, STRING("/path/%s/unix/a %d %u %%"), STRING("test"), (s64)-100, (u64)404), STRING("/path/test/unix/a -100 404 %"))); 

    temp_reset();
#endif
}
