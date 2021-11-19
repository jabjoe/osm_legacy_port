#ifndef __LOG__
#define __LOG__

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

extern bool log_async_log;
extern uint32_t log_debug_mask;

extern void platform_raw_msg(const char * s);

extern void log_debug(uint32_t flag, const char * s, ...) PRINTF_FMT_CHECK( 2, 3);

extern void log_uart(unsigned uart, bool blocking, const char * s, ...) PRINTF_FMT_CHECK( 3, 4);

extern void log_error(const char * s, ...);
extern void log_out(const char * s, ...);

#define log_sys_debug(...) log_debug(DEBUG_SYS,  __VA_ARGS__)

#define log_init()

#endif //__LOG__
