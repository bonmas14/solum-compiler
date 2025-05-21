#ifndef MEMCTL_H
#define MEMCTL_H

#include "stddefines.h"

void mem_set(u8 *buffer, u8 value, u64 size);
void mem_copy(u8 *dest, u8 *source, u64 size);
void mem_move(u8 *dest, u8 *source, u64 size);
s32  mem_compare(u8 *left, u8 *right, u64 size);

#endif // MEMCTL_H
