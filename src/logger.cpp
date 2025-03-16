#include "logger.h"

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

void add_left_pad(FILE * file, u64 amount) {
    while (amount-- > 0) fprintf(file, " ");
}

void log_write(u8 *text, u64 left_pad) {
    add_left_pad(stderr, left_pad);
    log_update_color();
    fprintf(stderr, "%s", text);
}

void log_info(u8 *text, u64 left_pad) {
    add_left_pad(stderr, left_pad);

    log_push_color(INFO_COLOR);
    log_update_color();
    fprintf(stderr, "INFO: %s\n", text);
    log_pop_color();
}

void log_warning(u8 *text, u64 left_pad) {
    add_left_pad(stderr, left_pad);

    log_push_color(WARNING_COLOR);
    log_update_color();
    fprintf(stderr, "WARNING: %s\n", text);
    log_pop_color();
}

void log_error(u8 *text, u64 left_pad) {
    add_left_pad(stderr, left_pad);

    log_push_color(ERROR_COLOR);
    log_update_color();
    fprintf(stderr, "ERROR: %s\n", text);
    log_pop_color();
}
