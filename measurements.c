#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/cm3/cortex.h>

#include "measurements.h"
#include "lorawan.h"
#include "log.h"
#include "config.h"
#include "hpm.h"
#include "sys_time.h"


#define MEASUREMENTS__UNSET_VALUE   UINT32_MAX
#define MEASUREMENTS__STR_SIZE      16

#define MEASUREMENTS__PAYLOAD_VERSION       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_AVERAGED     (uint8_t)0x02


#define MEASUREMENTS__EXPONENT_TEMPERATURE      -3
#define MEASUREMENTS__EXPONENT_HUMIDITY         -1
#define MEASUREMENTS__EXPONENT_PM10              0
#define MEASUREMENTS__EXPONENT_PM25              0


#define MEASUREMENTS__COLLECT_TIME__TEMPERATURE__MS 10000
#define MEASUREMENTS__COLLECT_TIME__HUMIDITY__MS    10000
#define MEASUREMENTS__COLLECT_TIME__HPM__MS         10000


typedef struct
{
    const uint8_t   uuid;
    const uint16_t  data_id;
    const char*     name;
    uint8_t         interval;           // multiples of 5 mins
    uint8_t         sample_rate;        // multiples of 1 minute. Must be greater than or equal to 1
    void            (*init_cb)(void);
    bool            (*get_cb)(value_t* value);
    value_t         sum;
    value_t         max;
    value_t         min;
    uint8_t         num_samples;
} measurement_list_t;


typedef struct
{
    measurement_list_t*     read_data;
    measurement_list_t*     write_data;
    uint16_t                len;
} data_structure_t;


typedef struct
{
    uint8_t     uuid;
    uint8_t     num_samples_init;
    uint8_t     num_samples_collected;
    uint32_t    preinit_time;
} measurement_hidden_t;


static measurement_list_t data_0[LW__MAX_MEASUREMENTS];
static measurement_list_t data_1[LW__MAX_MEASUREMENTS];

static data_structure_t data;

static uint32_t last_sent_ms = 0;
static uint32_t interval_count = 0;
static uint8_t measurements_hex_arr[MEASUREMENTS__HEX_ARRAY_SIZE] = {0};
static uint16_t measurements_hex_arr_pos = 0;


static void hpm_pm10_init(void);
static bool hpm_pm10_get(value_t* value);
static void hpm_pm25_init(void);
static bool hpm_pm25_get(value_t* value);


static measurement_list_t data_template[] = { { MEASUREMENT_UUID__PM10 , LW_ID__PM10  , "pm10"          ,  1, 5, hpm_pm10_init, hpm_pm10_get, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, 0} ,
                                              { MEASUREMENT_UUID__PM25 , LW_ID__PM25  , "pm25"          ,  1, 5, hpm_pm25_init, hpm_pm25_get, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, 0} };

static measurement_hidden_t data_working_array[ARRAY_SIZE(data_template)];


uint32_t hpm_init_time = 0;
uint16_t hpm_pm10, hpm_pm25;


static void hpm_preinit(void)
{
    uint32_t now = since_boot_ms;
    if (hpm_init_time == 0 || since_boot_delta(now, hpm_init_time) > MEASUREMENTS__COLLECT_TIME__HPM__MS)
    {
        hpm_enable(true);
        hpm_request();
        hpm_init_time = now;
    }
}


static void hpm_pm10_init(void)
{
    hpm_preinit();
}

static bool hpm_pm10_get(value_t* value)
{
    *value = 0;
    uint16_t hpm_pm10_cp, hpm_pm25_cp;
    if (hpm_get(&hpm_pm10_cp, &hpm_pm25_cp))
    {
        hpm_pm10 = hpm_pm10_cp;
        hpm_pm25 = hpm_pm25_cp;
    }
    *value = hpm_pm10;
    return true;
}


uint16_t measurements_num_measurements(void)
{
    if (data.read_data != NULL)
    {
        return data.len;
    }
    return ARRAY_SIZE(data_template);
}


static void hpm_pm25_init(void)
{
    hpm_preinit();
}


static bool hpm_pm25_get(value_t* value)
{
    *value = 0;
    uint16_t hpm_pm10_cp, hpm_pm25_cp;
    if (hpm_get(&hpm_pm10_cp, &hpm_pm25_cp))
    {
        hpm_pm10 = hpm_pm10_cp;
        hpm_pm25 = hpm_pm25_cp;
    }
    *value = hpm_pm25;
    return true;
}


static bool measurement_get_preinit_time(uint16_t data_id, uint32_t* time)
{
    switch (data_id)
    {
        case LW_ID__TEMPERATURE:
            *time = MEASUREMENTS__COLLECT_TIME__TEMPERATURE__MS;
            break;
        case LW_ID__HUMIDITY:
            *time = MEASUREMENTS__COLLECT_TIME__HUMIDITY__MS;
            break;
        case LW_ID__PM10:
            *time = MEASUREMENTS__COLLECT_TIME__HPM__MS;
            break;
        case LW_ID__PM25:
            *time = MEASUREMENTS__COLLECT_TIME__HPM__MS;
            break;
        default:
            return false;
    }
    return true;
}


void measurements_init(void)
{
    uint8_t num_measurements = ARRAY_SIZE(data_template);
    measurement_hidden_t* measurement_working;
    for (int i = 0; i < num_measurements; i++)
    {
        measurement_working = &data_working_array[i];
        measurement_working->uuid = data.write_data[i].uuid;
        measurement_working->num_samples_init = 0;
        measurement_working->num_samples_collected = 0;
        if (!measurement_get_preinit_time(data.write_data[i].data_id, &measurement_working->preinit_time))
        {
            log_error("Could not find preinit time for %s.", data.write_data[i].name);;
            measurement_working->preinit_time = 0;
        }
    }

    memcpy((measurement_list_t*)data_0, data_template, sizeof(data_template));
    memcpy((measurement_list_t*)data_1, data_template, sizeof(data_template));
    data.read_data = (measurement_list_t*)data_0;
    data.write_data = (measurement_list_t*)data_1;
    data.len = num_measurements;
}


static void measurements_copy(void)
{
    CM_ATOMIC_BLOCK()
    {
        memcpy(data.read_data, data.write_data, sizeof(data_template));
        memcpy(data.write_data, (measurement_list_t*)data_template, sizeof(data_template));
    }
}


bool measurements_set_interval_uuid(uint8_t uuid, uint8_t interval)
{
    volatile measurement_list_t* measurement;
    for (uint8_t i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        if (measurement->uuid == uuid)
        {
            measurement->interval = interval;
            return true;
        }
    }
    return false;
}


static bool measurements_get_measurement(char* name, volatile measurement_list_t* measurement)
{
    for (uint8_t i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        if (strncmp(measurement->name, name, strlen(measurement->name)) == 0)
        {
            return true;
        }
    }
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    volatile measurement_list_t* measurement = {0};
    if (!measurements_get_measurement(name, measurement))
    {
        return false;
    }
    measurement->interval = interval;
    return true;
}


bool measurements_set_sample_rate(char* name, uint8_t sample_rate)
{
    volatile measurement_list_t* measurement = {0};
    if (!measurements_get_measurement(name, measurement))
    {
        return false;
    }
    measurement->sample_rate = sample_rate;
    return true;
}


static bool measurement_get_exponent(uint16_t data_id, int8_t* exponent)
{
    switch (data_id)
    {
        case LW_ID__TEMPERATURE:
            *exponent = MEASUREMENTS__EXPONENT_TEMPERATURE;
            break;
        case LW_ID__HUMIDITY:
            *exponent = MEASUREMENTS__EXPONENT_HUMIDITY;
            break;
        case LW_ID__PM10:
            *exponent = MEASUREMENTS__EXPONENT_PM10;
            break;
        case LW_ID__PM25:
            *exponent = MEASUREMENTS__EXPONENT_PM25;
            break;
        default:
            return false;
    }
    return true;
}


typedef union
{
    uint32_t d;
    struct
    {
        uint8_t ll;
        uint8_t lh;
        uint8_t hl;
        uint8_t hh;
    };
} uint32_to_uint8_t;


typedef union
{
    uint16_t d;
    struct
    {
        uint8_t l;
        uint8_t h;
    };
} uint16_to_uint8_t;


static uint8_t measurements_arr_append(uint8_t val)
{
    if (measurements_hex_arr_pos >= MEASUREMENTS__HEX_ARRAY_SIZE)
    {
        log_error("Measurement array is full.");
        return 1;
    }
    measurements_hex_arr[measurements_hex_arr_pos++] = val;
    return 0;
}


static bool measurements_to_arr(volatile measurement_list_t* measurement)
{
    bool single = measurement->sample_rate == 1;
    uint16_to_uint8_t mean;
    mean.d = measurement->sum / measurement->num_samples;
    int8_t exponent;

    if (!measurement_get_exponent(measurement->data_id, &exponent))
    {
        log_error("Cannot find exponent for %s (%"PRIu16").", measurement->name, measurement->data_id);
        return false;
    }

    uint8_t r = 0;
    r += measurements_arr_append(measurement->data_id);
    if (single)
    {
        r += measurements_arr_append(MEASUREMENTS__DATATYPE_SINGLE);
        r += measurements_arr_append(exponent);
        r += measurements_arr_append(mean.h);
        r += measurements_arr_append(mean.l);
    }
    else
    {
        uint16_to_uint8_t min;
        uint16_to_uint8_t max;
        min.d = measurement->min;
        max.d = measurement->max;
        r += measurements_arr_append(MEASUREMENTS__DATATYPE_AVERAGED);
        r += measurements_arr_append(exponent);
        r += measurements_arr_append(mean.h);
        r += measurements_arr_append(mean.l);
        r += measurements_arr_append(min.h);
        r += measurements_arr_append(min.l);
        r += measurements_arr_append(max.h);
        r += measurements_arr_append(max.l);
    }
    return (r == 0);
}


void measurements_send(void)
{
    measurements_copy();
    volatile measurement_list_t* measurement;
    memset(measurements_hex_arr, 0, LW__MAX_MEASUREMENTS);
    measurements_hex_arr_pos = 0;
    uint16_t num_meas_qd = 0;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.read_data[i];
        data_working_array[i].num_samples_init = 0;
        data_working_array[i].num_samples_collected = 0;
        if (measurement->interval && (interval_count % measurement->interval == 0))
        {
            if (measurement->sum == MEASUREMENTS__UNSET_VALUE || measurement->num_samples == 0)
            {
                log_error("Measurement requested but value not set.");
                continue;
            }
            if (measurements_to_arr(measurement))
            {
                num_meas_qd++;
            }
        }
    }
    if (num_meas_qd > 0)
    {
        lw_send(measurements_hex_arr, measurements_hex_arr_pos+1);
    }
}


static void measurements_sample(void)
{
    volatile measurement_list_t* measurement;
    measurement_hidden_t* measurement_working;
    uint32_t sample_interval;
    value_t new_value;
    uint32_t now = since_boot_ms;
    uint32_t time_since_interval;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        measurement_working = &data_working_array[i];

        sample_interval = measurement->interval * INTERVAL__TRANSMIT_MS / measurement->sample_rate;
        time_since_interval = since_boot_delta(now, last_sent_ms);

        // If it takes time to get a sample, it is begun here.
        if (time_since_interval >= (measurement_working->num_samples_init * sample_interval) + sample_interval/2 - measurement_working->preinit_time)
        {
            measurement_working->num_samples_init++;
            measurement->init_cb();
        }

        // The sample is collected every interval/sample_rate but offset by 1/2.
        // ||   .   .   .   .   .   ||   .   .   .   .   .   ||
        //    ^   ^   ^   ^   ^   ^    ^   ^   ^   ^   ^   ^
        if (time_since_interval >= (measurement_working->num_samples_collected * sample_interval) + sample_interval/2)
        {
            measurement_working->num_samples_collected++;
            if (!measurement->get_cb(&new_value))
            {
                log_error("Could not get the %s value.", measurement->name);
                return;
            }
            if (measurement->sum == MEASUREMENTS__UNSET_VALUE)
            {
                measurement->sum = 0;
            }
            if (measurement->min == MEASUREMENTS__UNSET_VALUE)
            {
                measurement->min = new_value;
            }
            if (measurement->max == MEASUREMENTS__UNSET_VALUE)
            {
                measurement->max = new_value;
            }
            measurement->sum += new_value;
            measurement->num_samples++;
            if (new_value > measurement->max)
            {
                measurement->max = new_value;
            }
            else if (new_value < measurement->min)
            {
                measurement->min = new_value;
            }
        }
    }
}


void measurements_loop(void)
{
    uint32_t now = since_boot_ms;
    measurements_sample();

    if (since_boot_delta(now, last_sent_ms) > INTERVAL__TRANSMIT_MS)
    {
        if (interval_count > UINT32_MAX - 1)
        {
            interval_count = 0;
        }
        interval_count++;
        measurements_send();
        last_sent_ms = now;
    }
}