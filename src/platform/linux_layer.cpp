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

string_t platform_get_current_directory(allocator_t *alloc) {
    log_error("TODO: current directiory linux layer");
    return {};
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
        log_error("Could not open file.");
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
        log_error("Could not read file.");
        log_error(filename); 
        fclose(file);
        return false;
    }

    output->size = file_size;
    fclose(file);
    return true;
}

b32 platform_write_file(string_t name, string_t content) {
    const char *filename = string_to_c_string(name, get_temporary_allocator());

    FILE *file = fopen(filename, "wb");
    if (!file) {
        log_error("Could not write file.");
        return false;
    }
    if (content.data) {
        fwrite(content.data, 1, content.size, file);
    }
    fflush(file);
    fclose(file);
    return true; // we dont check if write was corrupted...
}

u32 platform_run_process(string_t exec_name, string_t args) {
    log_error("TODO: Run process on linux");
    return -100;
}

