#include <string.h>

#include "measurements.h"

#ifndef __CONFIGTOOL__

#include "timers.h"
#include "io.h"
#include "adcs.h"
#include "can_impl.h"
#include "log.h"
#include "config.h"
#include "hpm.h"
#include "cc.h"
#include "bat.h"
#include "modbus_measurements.h"
#include "ds18b20.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "veml7700.h"
#include "sai.h"
#include "fw.h"


void sensors_init(void)
{
    timers_init();
    ios_init();
    sai_init();
    adcs_init();
    cc_init();
    htu21d_init();
    veml7700_init();
    ds18b20_temp_init();
    sai_init();
    pulsecount_init();
    modbus_init();
    can_impl_init();
}

bool measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !inf)
    {
        measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(measurements_inf_t));
    switch(def->type)
    {
        case FW_VERSION:    fw_version_inf_init(inf);  break;
        case PM10:          hpm_pm10_inf_init(inf);    break;
        case PM25:          hpm_pm25_inf_init(inf);    break;
        case MODBUS:        modbus_inf_init(inf);      break;
        case CURRENT_CLAMP: cc_inf_init(inf);          break;
        case W1_PROBE:      ds18b20_inf_init(inf);     break;
        case HTU21D_TMP:    htu21d_temp_inf_init(inf); break;
        case HTU21D_HUM:    htu21d_humi_inf_init(inf); break;
        case BAT_MON:       bat_inf_init(inf);         break;
        case PULSE_COUNT:   pulsecount_inf_init(inf);  break;
        case LIGHT:         veml7700_inf_init(inf);    break;
        case SOUND:         sai_inf_init(inf);         break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
            return false;
    }

    if (data)
        data->value_type = inf->value_type_cb(def->name);

    return true;
}


void measurements_repopulate(void)
{
    measurements_def_t def;
    measurements_setup_default(&def, MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
    measurements_add(&def);
}

#endif


unsigned measurements_add_defaults(measurements_def_t * measurements_arr)
{
    if (!measurements_arr)
        return 0;
    unsigned pos = 0;
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_setup_default(&measurements_arr[pos++], MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
    return pos;
}
