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


static char   * rx_buffer;
static unsigned rx_buffer_len = 0;


typedef struct
{
    const char * key;
    const char * desc;
    void (*cb)(char * args);
    bool hidden;
} cmd_t;




static char * skip_to_space(char * pos)
{
    while(*pos && *pos != ' ')
        pos++;
    return pos;
}


void io_cb(char *args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);
    pos = skip_space(pos);
    bool do_read = false;

    if (*pos == ':')
    {
        do_read = true;
        bool as_input;
        pos = skip_space(pos + 1);
        if (strncmp(pos, "IN", 2) == 0 || *pos == 'I')
        {
            pos = skip_to_space(pos);
            as_input = true;
        }
        else if (strncmp(pos, "OUT", 3) == 0 || *pos == 'O')
        {
            pos = skip_to_space(pos);
            as_input = false;
        }
        else
        {
            log_error("Malformed gpio type command");
            return;
        }

        io_pupd_t pull = IO_PUPD_NONE;

        pos = skip_space(pos);

        if (*pos && *pos != '=')
        {
            if ((strncmp(pos, "UP", 2) == 0) || (pos[0] == 'U'))
            {
                pos = skip_to_space(pos);
                pull = IO_PUPD_UP;
            }
            else if (strncmp(pos, "DOWN", 4) == 0 || pos[0] == 'D')
            {
                pos = skip_to_space(pos);
                pull = IO_PUPD_DOWN;
            }
            else if (strncmp(pos, "NONE", 4) == 0 || pos[0] == 'N')
            {
                pos = skip_to_space(pos);
            }
            else
            {
                log_error("Malformed gpio pull command");
                return;
            }
            pos = skip_space(pos);
        }

        io_configure(io, as_input, pull);
    }

    if (*pos == '=')
    {
        pos = skip_space(pos + 1);
        if (strncmp(pos, "ON", 2) == 0 || *pos == '1')
        {
            pos = skip_to_space(pos);
            if (!io_is_input(io))
            {
                io_on(io, true);
                if (!do_read)
                    log_out("IO %02u = ON", io);
            }
            else log_error("IO %02u is input but output command.", io);
        }
        else if (strncmp(pos, "OFF", 3) == 0 || *pos == '0')
        {
            pos = skip_to_space(pos);
            if (!io_is_input(io))
            {
                io_on(io, false);
                if (!do_read)
                    log_out("IO %02u = OFF", io);
            }
            else log_error("IO %02u is input but output command.", io);
        }
        else
        {
            log_error("Malformed gpio on/off command");
            return;
        }
    }
    else do_read = true;

    if (do_read)
        io_log(io);
}


void cmd_enable_pulsecount_cb(char * args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);
    if (args == pos)
        goto bad_exit;
    pos = skip_space(pos);
    uint8_t pupd;
    if (pos[0] == 'U')
        pupd = IO_PUPD_UP;
    else if (pos[0] == 'D')
        pupd = IO_PUPD_DOWN;
    else if (pos[0] == 'N')
        pupd = IO_PUPD_NONE;
    else
        goto bad_exit;

    if (io_enable_pulsecount(io, pupd))
        log_out("IO %02u pulsecount enabled", io);
    else
        log_out("IO %02u has no pulsecount", io);
    return;
bad_exit:
    log_out("<io> <U/D/N>");
}


void cmd_enable_onewire_cb(char * args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);

    if (io_enable_w1(io))
        log_out("IO %02u onewire enabled", io);
    else
        log_out("IO %02u has no onewire", io);
}


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


void comms_send_cb(char * args)
{
    char * pos = skip_space(args);
    comms_send_str(pos);
}


void comms_config_cb(char * args)
{
    comms_config_setup_str(skip_space(args));
}


static void interval_cb(char * args)
{
    char* name = args;
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit(p[0]))
    {
        uint8_t new_interval = strtoul(p, NULL, 10);

        if (measurements_set_interval(name, new_interval))
        {
            log_out("Changed %s interval to %"PRIu8, name, new_interval);
        }
        else log_out("Unknown measuremnt");
    }
    else
    {
        uint8_t interval;
        if (measurements_get_interval(name, &interval))
        {
            log_out("Interval of %s = %"PRIu8, name, interval);
        }
        else
        {
            log_out("Unknown measuremnt");
        }
    }
}


static void samplecount_cb(char * args)
{
    char* name = args;
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit(p[0]))
    {
        uint8_t new_samplecount = strtoul(p, NULL, 10);

        if (measurements_set_samplecount(name, new_samplecount))
        {
            log_out("Changed %s samplecount to %"PRIu8, name, new_samplecount);
        }
        else log_out("Unknown measuremnt");
    }
    else
    {
        uint8_t samplecount;
        if (measurements_get_samplecount(name, &samplecount))
        {
            log_out("Samplecount of %s = %"PRIu8, name, samplecount);
        }
        else
        {
            log_out("Unknown measuremnt");
        }
    }
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


static void modbus_setup_cb(char *args)
{
    /*<BIN/RTU> <SPEED> <BITS><PARITY><STOP>
     * EXAMPLE: RTU 115200 8N1
     */
    modbus_setup_from_str(args);
}


static void modbus_add_dev_cb(char * args)
{
    /*<unit_id> <LSB/MSB> <LSW/MSW> <name>
     * (name can only be 4 char long)
     * EXAMPLES:
     * 0x1 MSB MSW TEST
     */
    if (!modbus_add_dev_from_str(args))
    {
        log_out("<unit_id> <LSB/MSB> <LSW/MSW> <name>");
    }
}


static void modbus_add_reg_cb(char * args)
{
    /*<unit_id> <reg_addr> <modbus_func> <type> <name>
     * (name can only be 4 char long)
     * Only Modbus Function 3, Hold Read supported right now.
     * 0x1 0x16 3 F   T-Hz
     * 1 22 3 F       T-Hz
     * 0x2 0x30 3 U16 T-As
     * 0x2 0x32 3 U32 T-Vs
     */
    char * pos = skip_space(args);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    uint16_t unit_id = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
        pos += 2;

    uint16_t reg_addr = strtoul(pos, &pos, 16);

    pos = skip_space(pos);

    uint8_t func = strtoul(pos, &pos, 10);

    pos = skip_space(pos);

    modbus_reg_type_t type = modbus_reg_type_from_str(pos, (const char**)&pos);
    if (type == MODBUS_REG_TYPE_INVALID)
        return;

    pos = skip_space(pos);

    char * name = pos;

    modbus_dev_t * dev = modbus_get_device_by_id(unit_id);
    if (!dev)
    {
        log_out("Unknown modbus device.");
        return;
    }

    if (modbus_dev_add_reg(dev, name, type, func, reg_addr))
    {
        log_out("Added modbus reg %s", name);
        if (!modbus_measurement_add(modbus_dev_get_reg_by_name(dev, name)))
            log_out("Failed to add modbus reg to measurements!");
    }
    else log_out("Failed to add modbus reg.");
}


static void measurements_cb(char *args)
{
    measurements_print();
}


static void measurements_enable_cb(char *args)
{
    if (args[0])
        measurements_enabled = (args[0] == '1');

    log_out("measurements_enabled : %c", (measurements_enabled)?'1':'0');
}


static void measurement_get_cb(char* args)
{
    char * p = skip_space(args);
    measurements_reading_t reading;
    measurements_value_type_t type;
    if (!measurements_get_reading(p, &reading, &type))
    {
        log_out("Failed to get measurement reading.");
        return;
    }
    char text[16];
    if (!measurements_reading_to_str(&reading, type, text, 16))
    {
        log_out("Could not convert the reading to a string.");
        return;
    }
    log_out("%s: %s", p, text);
}


static void fw_add(char *args)
{
    args = skip_space(args);
    unsigned len = strlen(args);
    if (len%2)
    {
        log_error("Invalid fw chunk.");
        return;
    }
    char * end = args + len;
    while(args < end)
    {
        char * next = args + 2;
        char t = *next;
        *next=0;
        uint8_t d = strtoul(args, NULL, 16);
        *next=t;
        args=next;
        if (!fw_ota_add_chunk(&d, 1))
        {
            log_error("Invalid fw.");
            return;
        }
    }
    log_out("FW %u chunk added", len/2);
}


static void fw_fin(char *args)
{
    args = skip_space(args);
    uint16_t crc = strtoul(args, NULL, 16);
    if (fw_ota_complete(crc))
        log_out("FW added");
    else
        log_error("FW adding failed.");
}


static void reset_cb(char *args)
{
    platform_reset_sys();
}


static void cc_cb(char* args)
{
    char* p;
    uint8_t cc_num = strtoul(args, &p, 10);
    measurements_reading_t value_1;
    if (p == args)
    {
        measurements_reading_t value_2, value_3;
        if (!cc_get_all_blocking(&value_1, &value_2, &value_3))
        {
            log_out("Could not get CC values.");
            return;
        }
        log_out("CC1 = %"PRIi64".%"PRIi64" A", value_1.v_i64/1000, value_1.v_i64%1000);
        log_out("CC2 = %"PRIi64".%"PRIi64" A", value_2.v_i64/1000, value_2.v_i64%1000);
        log_out("CC3 = %"PRIi64".%"PRIi64" A", value_3.v_i64/1000, value_3.v_i64%1000);
        return;
    }
    if (cc_num > 3 || cc_num == 0)
    {
        log_out("cc [1/2/3]");
        return;
    }
    char name[4];
    snprintf(name, 4, "CC%"PRIu8, cc_num);
    if (!cc_get_blocking(name, &value_1))
    {
        log_out("Could not get adc value.");
        return;
    }

    log_out("CC = %"PRIi64"mA", value_1.v_i64);
}


static void cc_calibrate_cb(char *args)
{
    cc_calibrate();
}


static void cc_mp_cb(char* args)
{
    // 2046 CC1
    char* p;
    float new_mp = strtof(args, &p);
    p = skip_space(p);
    uint32_t new_mp32;
    if (p == args)
    {
        cc_get_midpoint(&new_mp32, p);
        log_out("MP: %"PRIu32".%03"PRIu32, new_mp32/1000, new_mp32%1000);
        return;
    }
    new_mp32 = new_mp * 1000;
    p = skip_space(p);
    if (!cc_set_midpoint(new_mp32, p))
        log_out("Failed to set the midpoint.");
}


static void cc_gain(char* args)
{
    // <index> <ext_A> <int_mV>
    // 1       100     50
    cc_config_t* cc_conf = persist_get_cc_configs();
    char* p;
    uint8_t index = strtoul(args, &p, 10);
    p = skip_space(p);
    if (strlen(p) == 0)
    {
        for (uint8_t i = 0; i < ADC_CC_COUNT; i++)
        {
            log_out("CC%"PRIu8" EXT max: %"PRIu32".%03"PRIu32"A", i+1, cc_conf[i].ext_max_mA/1000, cc_conf[i].ext_max_mA%1000);
            log_out("CC%"PRIu8" INT max: %"PRIu32".%03"PRIu32"V", i+1, cc_conf[i].int_max_mV/1000, cc_conf[i].int_max_mV%1000);
        }
        return;
    }
    if (index == 0 || index > ADC_CC_COUNT + 1 || p == args)
        goto syntax_exit;
    index--;

    char* q;
    float ext_A = strtof(p, &q);
    if (q == p)
        goto print_exit;
    q = skip_space(q);
    uint32_t int_mA = strtoul(q, &p, 10);
    if (p == q)
        goto syntax_exit;
    cc_conf[index].ext_max_mA = ext_A * 1000;
    cc_conf[index].int_max_mV = int_mA;
    log_out("Set the CC gain:");
print_exit:
    log_out("EXT max: %"PRIu32".%03"PRIu32"A", cc_conf[index].ext_max_mA/1000, cc_conf[index].ext_max_mA%1000);
    log_out("INT max: %"PRIu32".%03"PRIu32"V", cc_conf[index].int_max_mV/1000, cc_conf[index].int_max_mV%1000);
    return;
syntax_exit:
    log_out("Syntax: cc_gain <channel> <ext max A> <ext min mV>");
    log_out("e.g cc_gain 3 100 50");
}


static void timer_cb(char* args)
{
    char* pos = skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = get_since_boot_ms();
    timer_delay_us_64(delay_ms * 1000);
    log_out("Time elapsed: %"PRIu32, since_boot_delta(get_since_boot_ms(), start_time));
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


static void wipe_cb(char* args)
{
    persistent_wipe();
}


static void interval_mins_cb(char* args)
{
    if (args[0])
    {
        uint32_t new_interval_mins = strtoul(args, NULL, 10);
        if (!new_interval_mins)
            new_interval_mins = 5;

        log_out("Setting interval minutes to %"PRIu32, new_interval_mins);
        persist_set_mins_interval(new_interval_mins);
        transmit_interval = new_interval_mins;
    }
    else
    {
        log_out("Current interval minutes is %"PRIu32, transmit_interval);
    }
}


static void bat_cb(char* args)
{
    measurements_reading_t value;
    if (!bat_get_blocking(NULL, &value))
    {
        log_out("Could not get bat value.");
        return;
    }

    log_out("Bat %"PRIi64".%02"PRIi64, value.v_i64 / 100, value.v_i64 %100);
}


static void comms_dbg_cb(char* args)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
}


static void sound_cal_cb(char* args)
{
    char* p;
    uint8_t index = strtoul(args, &p, 10);
    if (index < 1 || index > SAI_NUM_CAL_COEFFS)
    {
        log_out("Index out of range.");
        return;
    }
    p = skip_space(p);
    float coeff = strtof(p, NULL);
    if (!sai_set_coeff(index-1, coeff))
        log_out("Could not set the coefficient.");
}


static void repop_cb(char* args)
{
    measurements_repopulate();
    log_out("Repopulated measurements.");
}


static void no_comms_cb(char* args)
{
    bool enable = strtoul(args, NULL, 10);
    measurements_set_debug_mode(enable);
}


void sleep_cb(char* args)
{
    char* p;
    uint32_t sleep_ms = strtoul(args, &p, 10);
    if (p == args)
    {
        log_out("<TIME(MS)>");
        return;
    }
    log_out("Sleeping for %"PRIu32"ms.", sleep_ms);
    sleep_for_ms(sleep_ms);
}


void power_mode_cb(char* args)
{
    measurements_power_mode_t mode;
    if (args[0] == 'A')
        mode = MEASUREMENTS_POWER_MODE_AUTO;
    else if (args[0] == 'B')
        mode = MEASUREMENTS_POWER_MODE_BATTERY;
    else if (args[0] == 'P')
        mode = MEASUREMENTS_POWER_MODE_PLUGGED;
    else
        return;
    measurements_power_mode(mode);
}


static void can_impl_cb(char* args)
{
    can_impl_send_example();
}


static void debug_mode_cb(char* args)
{
    uint32_t mask = persist_get_log_debug_mask();
    if (mask & DEBUG_MODE)
    {
        debug_mode(); /* Toggle it off.*/
        persist_set_log_debug_mask(mask & ~DEBUG_MODE);
        return;
    }
    persist_set_log_debug_mask(mask | DEBUG_MODE);
    platform_raw_msg("Rebooting in debug_mode.");
    reset_cb(args);
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
    static cmd_t cmds[] = {
        { "ios",          "Print all IOs.",           ios_log                       , false },
        { "io",           "Get/set IO set.",          io_cb                         , false },
        { "en_pulse",     "Enable Pulsecount IO.",    cmd_enable_pulsecount_cb      , false },
        { "en_w1",        "Enable OneWire IO.",       cmd_enable_onewire_cb         , false },
        { "count",        "Counts of controls.",      count_cb                      , false },
        { "version",      "Print version.",           version_cb                    , false },
        { "comms_send",   "Send comms message",       comms_send_cb                 , false },
        { "comms_config", "Set comms config",         comms_config_cb               , false },
        { "comms_conn",   "LoRa connected",           comms_conn_cb                 , false },
        { "comms_dbg",    "Comms Chip Debug",         comms_dbg_cb                  , false },
        { "no_comms",     "Dont need comms for measurements", no_comms_cb           , false },
        { "interval",     "Set the interval",         interval_cb                   , false },
        { "samplecount",  "Set the samplecount",      samplecount_cb                , false },
        { "debug",        "Set hex debug mask",       debug_cb                      , false },
        { "mb_setup",     "Change Modbus comms",      modbus_setup_cb               , false },
        { "mb_dev_add",   "Add modbus dev",           modbus_add_dev_cb             , false },
        { "mb_reg_add",   "Add modbus reg",           modbus_add_reg_cb             , false },
        { "mb_reg_del",   "Delete modbus reg",        modbus_measurement_del_reg    , false },
        { "mb_dev_del",   "Delete modbus dev",        modbus_measurement_del_dev    , false },
        { "mb_log",       "Show modbus setup",        modbus_log                    , false },
        { "save",         "Save config",              persist_commit                , false },
        { "measurements", "Print measurements",       measurements_cb               , false },
        { "meas_enable",  "Enable measuremnts.",      measurements_enable_cb        , false },
        { "get_meas",     "Get a measurement",        measurement_get_cb            , false },
        { "fw+",          "Add chunk of new fw.",     fw_add                        , false },
        { "fw@",          "Finishing crc of new fw.", fw_fin                        , false },
        { "reset",        "Reset device.",            reset_cb                      , false },
        { "cc",           "CC value",                 cc_cb                         , false },
        { "cc_cal",       "Calibrate the cc",         cc_calibrate_cb               , false },
        { "cc_mp",        "Set the CC midpoint",      cc_mp_cb                      , false },
        { "cc_gain",      "Set the max int and ext",  cc_gain                       , false },
        { "timer",        "Test usecs timer",         timer_cb                      , false },
        { "wipe",         "Factory Reset",            wipe_cb                       , false },
        { "interval_mins","Get/Set interval minutes", interval_mins_cb              , false },
        { "bat",          "Get battery level.",       bat_cb                        , false },
        { "cal_sound",    "Set the cal coeffs.",      sound_cal_cb                  , false },
        { "repop",        "Repopulate measurements.", repop_cb                      , false },
        { "sleep",        "Sleep",                    sleep_cb                      , false },
        { "power_mode",   "Power mode setting",       power_mode_cb                 , false },
        { "can_impl",     "Send example CAN message", can_impl_cb                   , false },
        { "debug_mode",   "Set/unset debug mode",     debug_mode_cb                 , false },
        { "serial_num",   "Set/get serial number",    serial_num_cb                 , true  },
        { NULL },
    };

    if (!len)
        return;

    log_sys_debug("Command \"%s\"", command);

    rx_buffer = command;
    rx_buffer_len = len;

    bool found = false;
    log_out(LOG_START_SPACER);
    char * args;
    for(cmd_t * cmd = cmds; cmd->key; cmd++)
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
        for(cmd_t * cmd = cmds; cmd->key; cmd++)
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
