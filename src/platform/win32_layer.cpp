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

string_t platform_get_current_directory(allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;

    DWORD length = GetCurrentDirectoryA(0, NULL);

    allocator_t *talloc = get_temporary_allocator();

    u8 *buffer = (u8*)mem_alloc(talloc, (u64)length);

    DWORD result = GetCurrentDirectoryA(length, (LPSTR)buffer);

    return string_copy({ result, buffer }, alloc);
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


