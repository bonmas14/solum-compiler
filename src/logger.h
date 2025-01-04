#pragma once

#include "stddefines.h"
#define LEFT_PAD_STANDART_OFFSET 4

void log_no_dec(const char *text, size_t left_pad);
void log_info(const char *text, size_t left_pad);
void log_warning(const char *text, size_t left_pad);
void log_error(const char *text, size_t left_pad);
