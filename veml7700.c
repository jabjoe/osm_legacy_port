/*
Datasheet: https://eu.mouser.com/datasheet/2/427/VISH_S_A0012091125_1-2572303.pdf
         (Accessed: 31.01.2022)
*/


#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "config.h"
#include "pinmap.h"
#include "i2c.h"
#include "log.h"
#include "measurements.h"
#include "sys_time.h"
#include "veml7700.h"
#include "timers.h"


#define MEASUREMENT_COLLECTION_MS  1000

#define VEML7700_CONF_REG_ALS_GAIN_MASK     0x1800
#define VEML7700_CONF_REG_ALS_IT_MASK       0x3c0
#define VEML7700_CONF_REG_ALS_PERS_MASK     0x30
#define VEML7700_CONF_REG_ALS_INT_EN_MASK   0x2
#define VEML7700_CONF_REG_ALS_SD_MASK       0x1

#define VEML7700_PSM_MASK                   0x6
#define VEML7700_PSM_EN_MASK                0x1

#define VEML7700_CONF_REG_ALS_GAIN(x)       ( VEML7700_CONF_REG_ALS_GAIN_MASK   & (x << 11) )
#define VEML7700_CONF_REG_ALS_IT(x)         ( VEML7700_CONF_REG_ALS_IT_MASK     & (x << 6)  )
#define VEML7700_CONF_REG_ALS_PERS(x)       ( VEML7700_CONF_REG_ALS_PERS_MASK   & (x << 4)  )
#define VEML7700_CONF_REG_ALS_INT_EN(x)     ( VEML7700_CONF_REG_ALS_INT_EN_MASK & (x << 1)  )
#define VEML7700_CONF_REG_ALS_SD(x)         ( VEML7700_CONF_REG_ALS_SD_MASK     & (x << 0)  )

#define VEML7700_ALS_GAIN_1                 VEML7700_CONF_REG_ALS_GAIN(0)
#define VEML7700_ALS_GAIN_2                 VEML7700_CONF_REG_ALS_GAIN(1)
#define VEML7700_ALS_GAIN_1_8               VEML7700_CONF_REG_ALS_GAIN(2)
#define VEML7700_ALS_GAIN_1_4               VEML7700_CONF_REG_ALS_GAIN(3)

#define VEML7700_ALS_IT_100                 VEML7700_CONF_REG_ALS_IT(0)
#define VEML7700_ALS_IT_200                 VEML7700_CONF_REG_ALS_IT(1)
#define VEML7700_ALS_IT_400                 VEML7700_CONF_REG_ALS_IT(2)
#define VEML7700_ALS_IT_800                 VEML7700_CONF_REG_ALS_IT(3)
#define VEML7700_ALS_IT_50                  VEML7700_CONF_REG_ALS_IT(8)
#define VEML7700_ALS_IT_25                  VEML7700_CONF_REG_ALS_IT(12)

#define VEML7700_ALS_PERS_1                 VEML7700_CONF_REG_ALS_PERS(0)
#define VEML7700_ALS_PERS_2                 VEML7700_CONF_REG_ALS_PERS(1)
#define VEML7700_ALS_PERS_4                 VEML7700_CONF_REG_ALS_PERS(2)
#define VEML7700_ALS_PERS_8                 VEML7700_CONF_REG_ALS_PERS(4)

#define VEML7700_ALS_INT_EN_ENABLED         VEML7700_CONF_REG_ALS_INT_EN(0)
#define VEML7700_ALS_INT_EN_DISABLED        VEML7700_CONF_REG_ALS_INT_EN(1)

#define VEML7700_ALS_SD_ON                  VEML7700_CONF_REG_ALS_SD(0)
#define VEML7700_ALS_SD_OFF                 VEML7700_CONF_REG_ALS_SD(1)

#define VEML7700_PSM(x)                     ( VEML7700_PSM_MASK    & (x << 1) )
#define VEML7700_PSM_EN(x)                  ( VEML7700_PSM_EN_MASK & (x << 0) )

#define VEML7700_PSM_MODE_1                 VEML7700_PSM(0)
#define VEML7700_PSM_MODE_2                 VEML7700_PSM(1)
#define VEML7700_PSM_MODE_3                 VEML7700_PSM(2)
#define VEML7700_PSM_MODE_4                 VEML7700_PSM(3)

#define VEML7700_PSM_EN_DISABLE             VEML7700_PSM_EN(0)
#define VEML7700_PSM_EN_ENABLE              VEML7700_PSM_EN(1)


typedef enum
{
    VEML7700_CMD_ALS_CONF_0     = 0x00,
    VEML7700_CMD_ALS_WH         = 0x01,
    VEML7700_CMD_ALS_WL         = 0x02,
    VEML7700_CMD_POWER_SAVING   = 0x03,
    VEML7700_CMD_ALS            = 0x04,
    VEML7700_CMD_WHITE          = 0x05,
    VEML7700_CMD_ALS_INT        = 0x06,
} veml7700_cmd_t;


static uint16_t     veml7700_conf_reg_val       = VEML7700_ALS_GAIN_1 | VEML7700_ALS_IT_100 | VEML7700_ALS_PERS_1 | VEML7700_ALS_INT_EN_DISABLED;
static uint16_t     veml7700_conf_power         = VEML7700_PSM_MODE_4 | VEML7700_PSM_EN_ENABLE;


static void _veml7700_get_u16(uint8_t d[2], uint16_t *r)
{
    light_debug("Got 0x%"PRIx8" 0x%"PRIx8, d[0], d[1]);
    *r = (d[0] << 8) | d[1];
}


static bool _veml7700_read_reg16(veml7700_cmd_t reg, uint16_t * r)
{
    if (!r)
    {
        return false;
    }
    uint8_t reg8 = reg;
    uint8_t d[2] = {0};
    light_debug("Read command 0x%"PRIx8, reg8);

    i2c_transfer7(VEML7700_I2C, I2C_VEML7700_ADDR, &reg8, 1, d, 2);
    _veml7700_get_u16(d, r);
    return true;
}


static void _veml7700_write_reg16(veml7700_cmd_t reg, uint16_t data)
{
    uint8_t reg8 = reg;
    uint8_t data16[2];
    data16[0] = data && 0x00FF;
    data16[1] = data && 0xFF00;
    light_debug("Send command 0x%"PRIx8" 0x%"PRIx16, reg8, data);
    i2c_transfer7(VEML7700_I2C, I2C_VEML7700_ADDR, &reg8, 1, data16, 2);
}


int32_t _veml7700_conv_lux(uint16_t data, uint16_t gain, uint16_t resp)
{
    return data / (gain * resp);
}


uint32_t veml7700_measurements_collection_time(void)
{
    return MEASUREMENT_COLLECTION_MS;
}


bool veml7700_light_measurements_init(char* name)
{
    _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, veml7700_conf_reg_val | VEML7700_ALS_SD_ON);
    return true;
}


bool veml7700_light_measurements_get(char* name, value_t* value)
{
    uint16_t lux;
    if (!_veml7700_read_reg16(VEML7700_CMD_ALS, &lux))
    {
        return false;
    }
    *value = value_from(lux);
    return true;
}


void veml7700_init(void)
{
    i2c_init(VEML7700_I2C_INDEX);
    _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, veml7700_conf_reg_val | VEML7700_ALS_SD_OFF);
    _veml7700_write_reg16(VEML7700_CMD_POWER_SAVING, veml7700_conf_power);
}


bool veml7700_get_lux(uint16_t* lux)
{
    log_out("veml7700_conf_reg_val = %"PRIu16, veml7700_conf_reg_val);
    log_out("veml7700_conf_power = %"PRIu16, veml7700_conf_power);
    if (!lux)
    {
        light_debug("Handed in null pointer.");
        return false;
    }
    _veml7700_write_reg16(VEML7700_CMD_ALS_CONF_0, veml7700_conf_reg_val | VEML7700_ALS_SD_ON);
    timer_delay_us_64(4200 * 1000);
    if (!_veml7700_read_reg16(VEML7700_CMD_ALS, lux))
    {
        light_debug("Could not read data from sensor.");
        return false;
    }
    return true;
}
