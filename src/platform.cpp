#include "stddefines.h" 
#include "logger.h" 
#include "strings.h"
#include "allocator.h"
#include "talloc.h"

#ifdef _WIN32

#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

static u64 frequency;

void debug_init(void) {
    LARGE_INTEGER out;
    QueryPerformanceFrequency(&out);
    frequency = out.QuadPart;
}

f64 debug_get_time(void) {
    LARGE_INTEGER out;
    QueryPerformanceCounter(&out);
    return (f64)out.QuadPart / (f64)frequency;
}

void debug_break(void) {
    log_reset_color();
    DebugBreak();
}

b32 platform_file_exists(string_t name) {
    if (name.size > MAX_PATH) return false;
    LPSTR filename = string_to_c_string(name, get_temporary_allocator());
    return (b32)PathFileExistsA(filename);
}

b32 platform_read_file_into_string(string_t name, allocator_t *alloc, string_t *output) {
    if (alloc == NULL) alloc = default_allocator;

    assert(alloc != NULL);
    assert(output != NULL);
    assert(name.data != NULL);
    assert(name.size > 0);

    if (name.size > MAX_PATH) return false;

    LPSTR filename = string_to_c_string(name, get_temporary_allocator());

    HANDLE file = CreateFileA(filename, 
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, 
            NULL);

    if (file == INVALID_HANDLE_VALUE) {
        log_error(STRING("Couldn't load file."));
        return false;
    }

    u64 file_size = 0; {
        LARGE_INTEGER size = {};

        GetFileSizeEx(file, &size);

        file_size = (u64)size.QuadPart;
    }

    if (file_size == 0) {
        CloseHandle(file);
        return false;
    }

    output->data = (u8*)mem_alloc(default_allocator, file_size);

    if (output->data == NULL) {
        log_error(STRING("Couldn't allocate memory for file contents."));
        CloseHandle(file);
    }

    DWORD bytes_read = 0;

    if (!ReadFile(file, 
            (LPVOID)output->data,
            (DWORD)file_size, 
            &bytes_read,
            NULL)) {
        log_error(STRING("Couldn't read file."));
        CloseHandle(file);
    }

    if ((u64)bytes_read < file_size) {
        log_error(STRING("Corrupted read."));
        CloseHandle(file);
        return false;
    }

    output->size = file_size;
    CloseHandle(file);
    return true;
}

#else
#include <stdio.h>
#include <time.h>

static time_t start_time = 0;

void debug_init(void) {
    timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) {
        log_error(STRING("Cant access timer..."));
        return;
    }
    start_time = t.tv_sec;
}

f64 debug_get_time(void) {
    timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) {
        log_error(STRING("Cant access timer..."));
        return 0;
    }

    time_t s = t.tv_sec - start_time;
    return (f64)s + (f64)t.tv_nsec / 1000000000.0;
}

void debug_break(void) {
    log_reset_color();
    __builtin_trap();
}

b32 platform_file_exists(string_t name) {
    FILE *file = fopen(string_to_c_string(name, get_temporary_allocator()), "rb");

    if (file == NULL) return false;

    fclose(file);
    return true;
}

b32 platform_read_file_into_string(string_t filename, allocator_t *alloc, string_t *output) {
    if (alloc == NULL) alloc = default_allocator;

    assert(alloc != NULL);
    assert(output != NULL);
    assert(filename.data != NULL);
    assert(filename.size > 0);

    FILE *file = fopen(string_temp_to_c_string(filename), "rb");

    if (file == NULL) {
        log_error(STRING("Scanner: Could not open file."));
        log_error(filename); 
        return false;
    }

    fseek(file, 0L, SEEK_END);
    u64 file_size = ftell(file);
    rewind(file);

    if (file_size == 0) {
        fclose(file);
        return false;
    }

    output->data = (u8*)mem_alloc(default_allocator, file_size);

    u64 bytes_read = fread(output->data, sizeof(u8), file_size, file);

    if (bytes_read < file_size) {
        log_error(STRING("Scanner: Could not read file."));
        log_error(filename); 
        fclose(file);
        return false;
    }

    output->size = file_size;
    fclose(file);
    return true;
}

#endif
