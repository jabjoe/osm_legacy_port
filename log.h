#ifndef __LOG__
#define __LOG__

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "value.h"

extern bool log_async_log;
extern uint32_t log_debug_mask;

extern void platform_raw_msg(const char * s);

extern void log_debug(uint32_t flag, const char * s, ...) PRINTF_FMT_CHECK( 2, 3);

extern void log_bad_error(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_error(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_out(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_init(void);

extern void log_debug_data(uint32_t flag, void * data, unsigned size);

#define log_sys_debug(...) log_debug(DEBUG_SYS,  __VA_ARGS__)

extern void log_debug_value(uint32_t flag, const char * prefix, value_t * v);

#endif //__LOG__
