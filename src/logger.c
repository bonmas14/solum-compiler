#include "logger.h"

void log_no_dec(const char *text) {
    fprintf(stderr, "%s\n", text);
}

void log_info(const char *text) {
    fprintf(stderr, "INFO: %s\n", text);
}

void log_warning(const char *text) {
    fprintf(stderr, "WARNING: %s\n", text);
}

void log_error(const char *text) {
    fprintf(stderr, "ERROR: %s\n", text);
}
