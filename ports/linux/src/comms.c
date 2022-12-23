#include <inttypes.h>
#include <string.h>
#include <stdio.h>


#include "comms.h"
#include "config.h"
#include "uart_rings.h"
#include "pinmap.h"
#include "log.h"
#include "common.h"


#define COMMS_DEFAULT_MTU       256
#define COMMS_ID_STR            "LINUX_COMMS"


uint16_t comms_get_mtu(void)
{
    return COMMS_DEFAULT_MTU;
}


bool comms_send_ready(void)
{
    return true;
}


bool comms_send_str(char* str)
{
    if(!uart_ring_out(COMMS_UART, str, strlen(str)))
        return false;
    return uart_ring_out(COMMS_UART, "\r\n", 2);
}


bool comms_send_allowed(void)
{
    return true;
}


void comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    char buf[3];
    for (uint16_t i = 0; i < arr_len; i++)
    {
        snprintf(buf, 3, "%.2"PRIx32, hex_arr[i]);
        uart_ring_out(COMMS_UART, buf, 2);
    }
    uart_ring_out(COMMS_UART, "\r\n", 2);
    on_comms_sent_ack(true);
}


void comms_init(void)
{
}


void comms_reset(void)
{
}


void comms_process(char* message)
{
}


bool comms_get_connected(void)
{
    return true;
}


void comms_loop_iteration(void)
{
}


void comms_config_setup_str(char * str)
{
    if (strstr(str, "dev-eui"))
    {
        log_out("Dev EUI: LINUX-DEV");
    }
    else if (strstr(str, "app-key"))
    {
        log_out("App Key: LINUX-APP");
    }
}


bool comms_get_id(char* str, uint8_t len)
{
    strncpy(str, COMMS_ID_STR, len);
    return true;
}


void comms_send_cb(char * args)
{
    char * pos = skip_space(args);
    comms_send_str(pos);
}


void comms_config_cb(char * args)
{
    comms_config_setup_str(skip_space(args));
}


static void comms_conn_cb(char* args)
{
    if (comms_get_connected())
    {
        log_out("1 | Connected");
    }
    else
    {
        log_out("0 | Disconnected");
    }
}


static void comms_dbg_cb(char* args)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
}


struct cmd_link_t* comms_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "comms_send",   "Send comms message",       comms_send_cb                 , false , NULL },
                                       { "comms_config", "Set comms config",         comms_config_cb               , false , NULL },
                                       { "comms_conn",   "LoRa connected",           comms_conn_cb                 , false , NULL },
                                       { "comms_dbg",    "Comms Chip Debug",         comms_dbg_cb                  , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}