#pragma once

extern volatile uint32_t since_boot_ms;

extern uint32_t since_boot_delta(uint32_t newer, uint32_t older);
