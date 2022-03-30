#pragma once

#include "measurements.h"
#include "config.h"


extern void                         adcs_init(void);

extern measurements_sensor_state_t  adcs_cc_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  adcs_cc_begin(char* name);
extern measurements_sensor_state_t  adcs_cc_get(char* name, value_t* value);

extern bool                         adcs_cc_collect(char* name, value_t* value);
extern bool                         adcs_cc_get_blocking(char* name, value_t* value);
extern bool                         adcs_cc_set_midpoint(uint16_t midpoint, char* name);
extern bool                         adcs_cc_get_midpoint(uint16_t* midpoint, char* name);
extern bool                         adcs_cc_calibrate(void);

extern bool                         adcs_cc_set_channels_active(uint8_t* active_channels, unsigned len);

extern measurements_sensor_state_t  adcs_bat_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  adcs_bat_begin(char* name);
extern measurements_sensor_state_t  adcs_bat_get(char* name, value_t* value);

extern bool                         adcs_bat_get_blocking(char* name, value_t* value);
