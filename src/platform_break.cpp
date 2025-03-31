#include "stddefines.h" 
#include "logger.h" 

#ifdef _WIN32

#include <windows.h>

void debug_break(void) {
    log_color_reset();
    DebugBreak();
}

#else

void debug_break(void) {
    log_color_reset();
    __builtin_trap();
}

#endif
