#include "strings.h"
#include "logger.h"

char *string_to_c_string(string_t a, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc  != NULL);
    assert(a.data != NULL);

    u8* data = (u8*)mem_alloc(alloc, a.size + 1);

    if (data == NULL) {
        log_error(STR("string conversion failed, buy more ram, or provide normal allocator..."));
        return NULL;
    }

    memcpy((void*)data, a.data, a.size);
    return (char*)data;
}

b32 string_compare(string_t a, string_t b) {
    assert(a.data != NULL);
    assert(b.data != NULL);

    if (a.size != b.size) return false;

    return memcmp(a.data, b.data, a.size) == 0;
}

string_t string_join(list_t<string_t> input, string_t separator, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

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

    if (input.count == 0) return {};

    assert(false);

    return {};
}

list_t<string_t> string_split(string_t input, string_t pattern, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    list_t<string_t> splits = {};

    if (input.size <= pattern.size) {
        log_error(STR("Pattern is bigger than input."));
        return splits;
    }

    if (pattern.size == 0) {
        log_error(STR("Pattern is empty."));
        return splits;
    }

    u64 start = 0;

    for (u64 i = 0; i < (input.size - (pattern.size - 1)); i++) {
        if (memcmp(input.data + i, pattern.data, pattern.size) != 0) {
            continue;
        }

        u64 size = i - start; 

        if (size == 0) {
            start = i + pattern.size;
            continue;
        }

        u8* buffer = (u8*)mem_alloc(alloc, size);
        if (buffer == NULL) { 
            log_error(STR("buy more ram... split allocation failed"));
            assert(false);
            return splits;
        }

        memcpy(buffer, input.data + start, size);
        string_t output = { .size = size, .data = buffer };
        list_add(&splits, &output);
        start = i + pattern.size;
    }

    if (start != input.size - pattern.size) {
        if (memcmp(input.data + start, pattern.data, pattern.size) == 0) {
            return splits;
        }

        u64 size = input.size - start; 
        if (size == 0) {
            return splits;
        }

        u8* buffer = (u8*)mem_alloc(alloc, size);

        if (buffer == NULL) { 
            log_error(STR("buy more ram... split allocation failed"));
            assert(false);
            return splits;
        }

        memcpy(buffer, input.data + start, size);
        string_t output = { .size = size, .data = buffer };
        list_add(&splits, &output);
    }

    return splits;
}

string_t string_copy(string_t a, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc  != NULL);
    assert(a.data != NULL);

    if (alloc == NULL)
        alloc = default_allocator;

    u8* data = (u8*)mem_alloc(alloc, a.size);

    if (data == NULL) {
        log_error(STR("string copy failed, buy more ram, or provide normal allocator..."));
        return (string_t) {.size = 0, .data = NULL };
    }

    memcpy((void*)data, a.data, a.size);
    return (string_t) {.size = a.size, .data = (u8*) data };
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
        memcpy((void*)data, a.data, a.size);
    }

    if (b.size > 0) {
        assert(b.data != NULL);
        memcpy((void*)(data + a.size), b.data, b.size);
    }

    return {.size = a.size + b.size, .data = (u8*) data };
}

string_t string_substring(string_t input, u64 start, u64 size, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    assert(size > 0);
    if (start > (input.size - size)) {
        log_error(STR("start position with size overlapping the input string."));
        return {};
    }
    u8* data = (u8*)mem_alloc(alloc, size);
    memcpy((void*)data, input.data + start, size);

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

void string_tests(void) {
    temp_reset();
    allocator_t *alloc = get_temporary_allocator();

    string_t result = {};

    result = string_concat(STRING("Hello "), STRING("world!"), alloc);
    assert(string_compare(result, STRING("Hello world!")));

    assert(string_compare(string_copy(result, alloc), result));
    
    list_t<string_t> splits = string_split(STRING("Eatin burger wit no honey mustard"), STRING(" "), alloc);

    assert(splits.count == 6);

    assert(string_compare(splits.data[0], STRING("Eatin")));
    assert(string_compare(splits.data[1], STRING("burger")));
    assert(string_compare(splits.data[2], STRING("wit")));
    assert(string_compare(splits.data[3], STRING("no")));
    assert(string_compare(splits.data[4], STRING("honey")));
    assert(string_compare(splits.data[5], STRING("mustard")));

    list_delete(&splits);

    splits = string_split(STRING("Eatin||burger||wit||no||honey||mustard"), STRING("||"), alloc);

    assert(splits.count == 6);

    assert(string_compare(splits.data[0], STRING("Eatin")));
    assert(string_compare(splits.data[1], STRING("burger")));
    assert(string_compare(splits.data[2], STRING("wit")));
    assert(string_compare(splits.data[3], STRING("no")));
    assert(string_compare(splits.data[4], STRING("honey")));
    assert(string_compare(splits.data[5], STRING("mustard")));

    list_delete(&splits);

    splits = string_split(STRING("Eatin||burger||wit||no||honey||mustard||a"), STRING("||"), alloc);

    assert(splits.count == 7);

    assert(string_compare(splits.data[0], STRING("Eatin")));
    assert(string_compare(splits.data[1], STRING("burger")));
    assert(string_compare(splits.data[2], STRING("wit")));
    assert(string_compare(splits.data[3], STRING("no")));
    assert(string_compare(splits.data[4], STRING("honey")));
    assert(string_compare(splits.data[5], STRING("mustard")));
    assert(string_compare(splits.data[6], STRING("a")));

    list_delete(&splits);

    splits = string_split(STRING("a||Eatin||burger||wit||no||honey||mustard||a"), STRING("||"), alloc);

    assert(splits.count == 8);

    assert(string_compare(splits.data[0], STRING("a")));
    assert(string_compare(splits.data[1], STRING("Eatin")));
    assert(string_compare(splits.data[2], STRING("burger")));
    assert(string_compare(splits.data[3], STRING("wit")));
    assert(string_compare(splits.data[4], STRING("no")));
    assert(string_compare(splits.data[5], STRING("honey")));
    assert(string_compare(splits.data[6], STRING("mustard")));
    assert(string_compare(splits.data[7], STRING("a")));

    list_delete(&splits);

    splits = string_split(STRING("||Eatin||burger||wit||no||honey||mustard||"), STRING("||"), alloc);

    assert(splits.count == 6);

    assert(string_compare(splits.data[0], STRING("Eatin")));
    assert(string_compare(splits.data[1], STRING("burger")));
    assert(string_compare(splits.data[2], STRING("wit")));
    assert(string_compare(splits.data[3], STRING("no")));
    assert(string_compare(splits.data[4], STRING("honey")));
    assert(string_compare(splits.data[5], STRING("mustard")));

    assert(string_compare(string_join(splits, STRING(" "), alloc), STRING("Eatin burger wit no honey mustard")));

    list_delete(&splits);

    assert(string_compare(string_substring(STRING("HelloWorld!"), 5, 6, alloc), STRING("World!")));
    assert(string_compare(string_substring(STRING("HelloWorld!"), 0, 11, alloc), STRING("HelloWorld!")));
    assert(string_compare(string_substring(STRING("HelloWorld!"), 10, 1, alloc), STRING("!")));


    assert(string_index_of(STRING("CP/M"), (u8)'/') == 2);
    assert(string_last_index_of(STRING("https://github.com/bonmas14"), (u8)'/') == 18);

    log_info(STR("STRINGS: OK"));

    temp_reset();
}

