#include "stddefines.h" 
#include "logger.h" 

#ifdef _WIN32

#include <windows.h>

void debug_break(void) {
    DebugBreak();
}

#else

void debug_break(void) {
    __builtin_trap();
}

#endif
