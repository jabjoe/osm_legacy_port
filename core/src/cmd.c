#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "io.h"
#include "timers.h"
#include "persist_config.h"
#include "sai.h"
#include "comms.h"
#include "measurements.h"
#include "hpm.h"
#include "modbus_measurements.h"
#include "update.h"
#include "ds18b20.h"
#include "common.h"
#include "htu21d.h"
#include "log.h"
#include "uart_rings.h"
#include "veml7700.h"
#include "cc.h"
#include "bat.h"
#include "sleep.h"
#include "can_impl.h"
#include "debug_mode.h"
#include "platform.h"
#include "version.h"
#include "base_types.h"


static char   * rx_buffer;
static unsigned rx_buffer_len = 0;


void count_cb(char * args)
{
    log_out("IOs     : %u", ios_get_count());
}


void version_cb(char * args)
{
    log_out("Version : %s", GIT_VERSION);
    version_arch_t arch = version_get_arch();
    char name[VERSION_NAME_LEN];
    memset(name, 0, VERSION_NAME_LEN);
    switch(arch)
    {
        case VERSION_ARCH_REV_B:
            strncpy(name, "Rev B", VERSION_NAME_LEN);
            break;
        case VERSION_ARCH_REV_C:
            strncpy(name, "Rev C", VERSION_NAME_LEN);
            break;
        default:
            log_out("Unknown architecture.");
            break;
    }
    log_out("Architecture is %s", name);
}


static void debug_cb(char * args)
{
    char * pos = skip_space(args);

    log_out("Debug mask : 0x%"PRIx32, log_debug_mask);

    if (pos[0])
    {
        if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
            pos += 2;

        unsigned mask = strtoul(pos, &pos, 16);

        mask |= DEBUG_SYS;

        log_debug_mask = mask;
        persist_set_log_debug_mask(mask);
        log_out("Setting debug mask to 0x%x", mask);
    }
}


static void timer_cb(char* args)
{
    char* pos = skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = get_since_boot_ms();
    timer_delay_us_64(delay_ms * 1000);
    log_out("Time elapsed: %"PRIu32, since_boot_delta(get_since_boot_ms(), start_time));
}


#define SERIAL_NUM_COMM_LEN         17

static void serial_num_cb(char* args)
{
    char* serial_num = persist_get_serial_number();
    char* p = skip_space(args);
    char comm_id[SERIAL_NUM_COMM_LEN];
    uint8_t len = strnlen(p, SERIAL_NUM_LEN);
    if (len == 0)
        goto print_exit;
    log_out("Updating serial number.");
    strncpy(serial_num, p, len);
    serial_num[len] = 0;
print_exit:
    if (!comms_get_id(comm_id, SERIAL_NUM_COMM_LEN))
    {
        log_out("%s", persist_get_serial_number());
        return;
    }
    log_out("Serial Number: %s-%s", serial_num, comm_id);
    return;
}


void cmds_process(char * command, unsigned len)
{
    static struct cmd_link_t cmds[] = {
        { "count",        "Counts of controls.",      count_cb                      , false , NULL},
        { "version",      "Print version.",           version_cb                    , false , NULL},
        { "debug",        "Set hex debug mask",       debug_cb                      , false , NULL},
        { "timer",        "Test usecs timer",         timer_cb                      , false , NULL},
        { "serial_num",   "Set/get serial number",    serial_num_cb                 , true  , NULL},
        { NULL },
    };

    struct cmd_link_t* tail;
    for (tail = cmds+1; tail; tail++)
        (tail-1)->next = tail;

    cmds_add_all(tail);

    if (!len)
        return;

    log_sys_debug("Command \"%s\"", command);

    rx_buffer = command;
    rx_buffer_len = len;

    bool found = false;
    log_out(LOG_START_SPACER);
    char * args;
    for(struct cmd_link_t * cmd = cmds; cmd->key; cmd++)
    {
        unsigned keylen = strlen(cmd->key);
        if(rx_buffer_len >= keylen &&
           !strncmp(cmd->key, rx_buffer, keylen) &&
           (rx_buffer[keylen] == '\0' || rx_buffer[keylen] == ' '))
        {
            found = true;
            args = skip_space(rx_buffer + keylen);
            cmd->cb(args);
            break;
        }
    }
    if (!found)
    {
        log_out("Unknown command \"%s\"", rx_buffer);
        log_out(LOG_SPACER);
        for(struct cmd_link_t * cmd = cmds; cmd->key; cmd++)
        {
            if (!cmd->hidden)
                log_out("%10s : %s", cmd->key, cmd->desc);
        }
    }
    log_out(LOG_END_SPACER);
}


void cmds_init()
{
}
