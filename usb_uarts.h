#ifndef __USB__
#define __USB__

extern void usb_init();
extern void usb_uart_send(unsigned uart, void * data, unsigned len);

#endif //__USB__