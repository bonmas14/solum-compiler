#ifndef LOGGER_H
#define LOGGER_H

#include "stddefines.h"
#define LEFT_PAD_STANDART_OFFSET (8)

void log_no_dec(const u8 *text, u64 left_pad);
void log_info(const u8 *text, u64 left_pad);
void log_warning(const u8 *text, u64 left_pad);
void log_error(const u8 *text, u64 left_pad);

#endif // LOGGER_H
