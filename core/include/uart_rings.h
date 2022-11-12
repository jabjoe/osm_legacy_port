#pragma once

#include <stdbool.h>
#include "ring.h"

extern unsigned uart_ring_in(unsigned uart, const char* s, unsigned len);
extern unsigned uart_ring_out(unsigned uart, const char* s, unsigned len);

extern bool uart_ring_out_busy(unsigned uart);
extern bool uart_rings_out_busy(void);

extern void uart_ring_in_drain(unsigned uart);
extern void uart_rings_in_drain();
extern void uart_rings_out_drain();

extern void uart_rings_check();

extern void uart_rings_init(void);

extern void ext_uart_ring_in_process(ring_buf_t * ring);
extern bool ext_uart_ring_do_out_drain(ring_buf_t * ring);
