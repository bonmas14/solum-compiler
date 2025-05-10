#include "stddefines.h" 
#include "logger.h" 

#ifdef _WIN32

#include <windows.h>

static u64 frequency;

void debug_init(void) {
    LARGE_INTEGER out;
    QueryPerformanceFrequency(&out);
    frequency = out.QuadPart;
}

f64 debug_get_time(void) {
    LARGE_INTEGER out;
    QueryPerformanceCounter(&out);
    return (f64)out.QuadPart / (f64)frequency;
}

void debug_break(void) {
    log_color_reset();
    DebugBreak();
}

#else

void debug_init(void) {
    
}

u64 debug_get_time(void) {
    return 0;
}

void debug_break(void) {
    log_color_reset();
    __builtin_trap();
}

#endif
