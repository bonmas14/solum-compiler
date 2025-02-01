#include "logger.h"

#define LOGGER_COLOR_STACK_SIZE 128

static u32 current_index;
static u32 stack[LOGGER_COLOR_STACK_SIZE];

void log_push_color(u8 r, u8 g, u8 b) {
    stack[current_index] = r | (g << 8) | (b << 16);

    current_index = (current_index + 1) % LOGGER_COLOR_STACK_SIZE;
}

void log_pop_color(void) {
    current_index = (current_index - 1) % LOGGER_COLOR_STACK_SIZE;
    stack[current_index] = 0;
}

void log_update_color(void) {
    u32 index = (current_index - 1) % LOGGER_COLOR_STACK_SIZE;

    u8 r = stack[index];
    u8 g = stack[index] >> 8;
    u8 b = stack[index] >> 16;

    fprintf(stderr, "\x1b[38;2;%u;%u;%um", r, g, b);
}

void log_no_dec(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    log_update_color();
    fprintf(stderr, "%s\n", text);
}

void log_info(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    log_push_color(64, 192, 255);
    log_update_color();
    fprintf(stderr, "INFO: %s\n", text);
    log_pop_color();
}

void log_warning(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }
    log_push_color(255, 192, 64);
    log_update_color();
    fprintf(stderr, "WARNING: %s\n", text);
    log_pop_color();
}

void log_error(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    log_push_color(255, 64, 64);
    log_update_color();
    fprintf(stderr, "ERROR: %s\n", text);
    log_pop_color();
}
