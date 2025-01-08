#ifndef LOGGER_H
#define LOGGER_H

#include "stddefines.h"
#define LEFT_PAD_STANDART_OFFSET 4

void log_no_dec(u8 *text, u64 left_pad);
void log_info(u8 *text, u64 left_pad);
void log_warning(u8 *text, u64 left_pad);
void log_error(u8 *text, u64 left_pad);

#endif // LOGGER_H
