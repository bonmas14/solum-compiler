#include "logger.h"

void set_console_color(u8 r, u8 g, u8 b) {
    fprintf(stderr, "\x1b[38;2;%u;%u;%um", r, g, b);
}

void log_no_dec(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, "%s\n", text);
}

void log_info(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    set_console_color(64, 192, 255);
    fprintf(stderr, "INFO: %s\n", text);
    set_console_color(255, 255, 255);
}

void log_warning(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }
    set_console_color(255, 192, 64);
    fprintf(stderr, "WARNING: %s\n", text);
    set_console_color(255, 255, 255);
}

void log_error(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    set_console_color(255, 64, 64);
    fprintf(stderr, "ERROR: %s\n", text);
    set_console_color(255, 255, 255);
}
