#include "logger.h"
#include "list.h"
#include "strings.h"
#include "talloc.h"

#define LOGGER_COLOR_STACK_SIZE 256

static b32 update_requested;
static u32 current_index;
static u32 stack[LOGGER_COLOR_STACK_SIZE];

void log_push_color(u8 r, u8 g, u8 b) {
    stack[current_index] = r | (g << 8) | (b << 16);
    current_index = (current_index + 1) % LOGGER_COLOR_STACK_SIZE;
    update_requested = true;
}

void log_pop_color(void) {
    current_index = (current_index - 1) % LOGGER_COLOR_STACK_SIZE;
    stack[current_index] = 0;
    update_requested = true;
}

void log_update_color(void) {
    if (!update_requested) return;
    update_requested = false;

    u32 index = (current_index - 1) % LOGGER_COLOR_STACK_SIZE;

    u8 r = stack[index];
    u8 g = stack[index] >> 8;
    u8 b = stack[index] >> 16;

    fprintf(stderr, "\x1b[38;2;%u;%u;%um", r, g, b);
}

void log_reset_color(void) {
    update_requested = true;

    fprintf(stderr, "\x1b[0m");
}

void add_left_pad(FILE * file, u64 amount) {
    while (amount-- > 0) fprintf(file, " ");
}

void log_write(string_t text) {
    log_update_color();
    fprintf(stderr, "%s", string_to_c_string(text, get_temporary_allocator()));
}

void log_info(string_t text) {
    log_push_color(INFO_COLOR);
    log_update_color();
    fprintf(stderr, "INFO: %s\n", string_to_c_string(text, get_temporary_allocator()));
    log_pop_color();
}

void log_warning(string_t text) {
    log_push_color(WARNING_COLOR);
    log_update_color();
    fprintf(stderr, "WARNING: %s\n", string_to_c_string(text, get_temporary_allocator()));
    log_pop_color();
}

void log_error(string_t text) {
    log_push_color(ERROR_COLOR);
    log_update_color();
    fprintf(stderr, "ERROR: %s\n", string_to_c_string(text, get_temporary_allocator()));
    log_pop_color();
}

void log_write(const char *text) {
    log_write(STRING(text));
}

void log_info(const char *text) {
    log_info(STRING(text));
}

void log_warning(const char *text) {
    log_warning(STRING(text));
}

void log_error(const char *text) {
    log_error(STRING(text));
}

/*
string_t log_sprintf(string_t string, void * data, ...) {
    va_list parameters;
}

string_t log_sprintf(string_t string, void * data, ...) {
    va_list parameters;

    va_start(parameters, data);
    
    long p1 = va_arg(parameters, char);
    va_arg(parameters, long long);

    va_end();
}
*/
