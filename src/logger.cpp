#include "logger.h"

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

    fprintf(stderr, "INFO: %s\n", text);
}

void log_warning(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, "WARNING: %s\n", text);
}

void log_error(const u8 *text, u64 left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, "ERROR: %s\n", text);
}
