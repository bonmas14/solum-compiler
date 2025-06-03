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
        return false;
    }

    DWORD bytes_read = 0;

    if (!ReadFile(file, 
            (LPVOID)output->data,
            (DWORD)file_size, 
            &bytes_read,
            NULL)) {
        log_error(STRING("Couldn't read file."));
        CloseHandle(file);
        return false;
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

b32 platform_write_file(string_t name, string_t content) {
    assert(content.data != NULL);
    assert(name.data    != NULL);
    assert(name.size > 0);

    if (name.size > MAX_PATH) return false;

    LPSTR filename = string_to_c_string(name, get_temporary_allocator());

    HANDLE file = CreateFileA(filename, 
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, 
            NULL);

    if (file == INVALID_HANDLE_VALUE) {
        log_error(STRING("Couldn't open file for writing."));
        return false;
    }

    DWORD bytes_written = 0;

    if (!WriteFile(file, 
            (LPVOID)content.data,
            (DWORD)content.size, 
            &bytes_written,
            NULL)) {
        log_error(STRING("Couldn't write a file."));
        CloseHandle(file);
        return false;
    }

    if ((u64)bytes_written < content.size) {
        log_error(STRING("Corrupted writing."));
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);
    return true;
}

u32 platform_run_process(string_t exec_name, string_t args) {
    PROCESS_INFORMATION info  = {};
    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof(STARTUPINFOA);

    allocator_t *talloc = get_temporary_allocator();

    string_t command_line = string_concat(exec_name, STRING(" "), talloc);
    command_line = string_concat(command_line, args, talloc);

    b32 status = CreateProcessA(
            NULL,
            string_to_c_string(command_line, talloc),
            NULL,
            NULL, 
            false,
            NORMAL_PRIORITY_CLASS,
            NULL,
            NULL,
            &startup_info,
            &info);

    if (!status || info.hProcess == INVALID_HANDLE_VALUE) {
        log_error(STRING("Couldn't create process."));
        return 10000;
    }

    DWORD result = WaitForSingleObject(info.hProcess, INFINITE);
    switch (result) {
        case WAIT_OBJECT_0:
            break;
        case WAIT_FAILED:
            log_error("Waiting failed");
            break;
        default:
            log_error("!!!!!");
            break;
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(info.hProcess, &exit_code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    return exit_code;
}
