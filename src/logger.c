#include "logger.h"

void log_no_dec(const char *text, size_t left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, "%s\n", text);
}

void log_info(const char *text, size_t left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, "INFO: %s\n", text);
}

void log_warning(const char *text, size_t left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, "WARNING: %s\n", text);
}

void log_error(const char *text, size_t left_pad) {
    while (left_pad-- > 0) {
        fprintf(stderr, " ");
    }

    fprintf(stderr, "ERROR: %s\n", text);
}
