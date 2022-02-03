#include <string.h>

#include "modbus_measurements.h"
#include "modbus.h"
#include "value.h"

/* On tests it's about 300ms per register @9600 with the RI-F220
 * Max 4 devices with max 16 registers
 * 300 * 16 * 4 = 19200
 */
#define MODBUS_COLLECTION_MS 20000


uint32_t modbus_measurements_collection_time(void)
{
    return MODBUS_COLLECTION_MS;
}

bool modbus_measurements_init(char* name)
{
    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg)
        return false;
    return modbus_start_read(reg);
}


bool modbus_measurements_get(char* name, value_t* value)
{
    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg || !value)
        return false;
    switch(modbus_reg_get_type(reg))
    {
        case MODBUS_REG_TYPE_U16:
        {
            uint16_t v;

            if (!modbus_reg_get_u16(reg, &v))
                return false;

            *value = value_from(v);
            return true;
        }
        case MODBUS_REG_TYPE_U32:
        {
            uint32_t v;

            if (!modbus_reg_get_u32(reg, &v))
                return false;

            *value = value_from(v);
            return true;
        }
        case MODBUS_REG_TYPE_FLOAT:
        {
            float v;

            if (!modbus_reg_get_float(reg, &v))
                return false;

            *value = value_from((int64_t)(v * 1000));
            return true;
        }
        default: break;
    }
   return false;
}


bool modbus_measurement_add(modbus_reg_t * reg)
{
    if (!reg)
        return false;

    measurement_def_t meas_def;

    modbus_reg_get_name(reg, meas_def.name);

    measurements_del(meas_def.name);

    meas_def.name[MODBUS_NAME_LEN] = 0;
    meas_def.samplecount = 1;
    meas_def.interval    = 1;
    meas_def.type        = MODBUS;

    return measurements_add(&meas_def);
}


void modbus_measurement_del_reg(char * name)
{
    modbus_reg_t * reg = modbus_get_reg(name);
    if (!reg)
        return;
    measurements_del(name);
    modbus_reg_del(reg);
}

void modbus_measurement_del_dev(char * dev_name)
{
    modbus_dev_t * dev = modbus_get_device_by_name(dev_name);
    if (!dev)
        return;

    modbus_reg_t * reg = modbus_dev_get_reg(dev, 0);
    while (reg)
    {
        char name[MODBUS_NAME_LEN+1];
        modbus_reg_get_name(reg, name);
        measurements_del(name);
        modbus_reg_del(reg);
        reg = modbus_dev_get_reg(dev, 0);
    }
    modbus_dev_del(dev);
}
