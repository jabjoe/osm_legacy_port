#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include "config.h"
#include "base_types.h"


extern void debug_mode(void);

extern void model_debug_mode_enable(bool enabled);

extern struct cmd_link_t* debug_mode_add_commands(struct cmd_link_t* tail);
extern void debug_mode_enable_all(void) __attribute__((weak));
