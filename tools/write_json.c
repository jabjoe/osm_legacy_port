#include "json_x_img.h"


static bool _write_modbus_json(struct json_object * root)
{
    modbus_bus_t* modbus = &osm_config.modbus_bus;

    if (modbus->version == MODBUS_BLOB_VERSION &&
        modbus->max_dev_num == MODBUS_MAX_DEV &&
        modbus->max_reg_num == MODBUS_DEV_REGS)
    {
        struct json_object * modbus_node = json_object_new_object();

        json_object_object_add(root, "modbus_bus", modbus_node);

        if (modbus->binary_protocol)
            json_object_object_add(modbus_node, "binary_protocol", json_object_new_boolean(true));

        char con_str[32];

        snprintf(con_str, sizeof(con_str), "%"PRIu32" %u%c%s", modbus->baudrate, modbus->databits, uart_parity_as_char(modbus->parity), uart_stop_bits_as_str(modbus->stopbits));

        json_object_object_add(modbus_node, "con_str", json_object_new_string(con_str));

        struct json_object * modbus_devices_node = json_object_new_object();
        json_object_object_add(modbus_node, "modbus_devices", modbus_devices_node);

        for(unsigned n = 0; n < MODBUS_MAX_DEV; n++)
        {
            modbus_dev_t * dev = &modbus->modbus_devices[n];
            if (dev->name[0])
            {
                struct json_object * dev_node = json_object_new_object();

                char name[MODBUS_NAME_LEN+1];

                memcpy(name, dev->name, MODBUS_NAME_LEN);
                name[MODBUS_NAME_LEN] = 0;

                json_object_object_add(modbus_devices_node, name, dev_node);

                json_object_object_add(dev_node, "unit_id", json_object_new_int(dev->slave_id));

                struct json_object * registers_node = json_object_new_object();

                json_object_object_add(dev_node, "registers", registers_node);

                for (unsigned i = 0; i < MODBUS_DEV_REGS; i++)
                {
                    modbus_reg_t * reg = &dev->regs[i];
                    if (reg->name[0])
                    {
                        struct json_object * register_node = json_object_new_object();

                        memcpy(name, reg->name, MODBUS_NAME_LEN);
                        name[MODBUS_NAME_LEN] = 0;

                        json_object_object_add(registers_node, name, register_node);

                        const char * type = modbus_reg_type_get_str(reg->type);

                        if (!type)
                        {
                            log_error("Config has invalid register type for \"%s\"", name);
                            return false;
                        }

                        json_object_object_add(register_node, "type", json_object_new_string(type));
                        json_object_object_add(register_node, "reg", json_object_new_int(reg->reg_addr));
                        json_object_object_add(register_node, "func", json_object_new_int(reg->func));
                    }
                }
            }
        }
    }
    return true;
}


static void _write_measurements_json(struct json_object * root)
{
    struct json_object * measurements_node = json_object_new_object();

    json_object_object_add(root, "measurements", measurements_node);

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def_t * def = &osm_config.measurements_arr[i];
        if (def->name[0])
        {
            struct json_object * measurement_node = json_object_new_object();

            json_object_object_add(measurements_node, def->name, measurement_node);
            json_object_object_add(measurement_node, "interval", json_object_new_int(def->interval));
            json_object_object_add(measurement_node, "samplecount", json_object_new_int(def->samplecount));
        }
    }
}


static void _write_ios_json(struct json_object * root)
{
    struct json_object * ios_node = json_object_new_object();
    json_object_object_add(root, "ios", ios_node);

    for(unsigned n = 0; n < IOS_COUNT; n++)
    {
        uint16_t state = osm_config.ios_state[n];
        struct json_object * io_node = json_object_new_object();

        char name[8];
        snprintf(name, sizeof(name), "%u", n);

        json_object_object_add(ios_node, name, io_node);

        json_object_object_add(io_node, "pull", json_object_new_string(io_get_pull_str(state)));

        char * dir = (state & IO_AS_INPUT)?"IN":"OUT";

        json_object_object_add(io_node, "direction", json_object_new_string(dir));

        if (io_is_special(state) && state & IO_SPECIAL_EN)
            json_object_object_add(io_node, "use_special", json_object_new_boolean(true));

        if (!(state & IO_AS_INPUT) && (state & IO_OUT_ON))
            json_object_object_add(io_node, "out_high", json_object_new_boolean(true));
    }
}



int write_json_from_img(const char * filename)
{
    FILE * f = fopen(filename,"r");

    if (!f)
    {
        perror("Failed to open file.");
        return EXIT_FAILURE;
    }
    if (fread(&osm_config, sizeof(osm_config), 1, f) != 1)
    {
        perror("Failed to read file.");
        fclose(f);
        return EXIT_FAILURE;
    }
    fclose(f);

    struct json_object * root = json_object_new_object();

    json_object_object_add(root, "version", json_object_new_int(PERSIST__VERSION));

    json_object_object_add(root, "log_debug_mask", json_object_new_int(DEBUG_SYS));

    json_object_object_add(root, "mins_interval", json_object_new_int(15));

    json_object_object_add(root, "lw_dev_eui", json_object_new_string_len(osm_config.lw_dev_eui, LW__DEV_EUI_LEN));
    json_object_object_add(root, "lw_app_key", json_object_new_string_len(osm_config.lw_app_key, LW__APP_KEY_LEN));

    json_object_object_add(root, "cc_midpoint", json_object_new_int(osm_config.adc_midpoint));

    if (!_write_modbus_json(root))
    {
        json_object_put(root);
        return EXIT_FAILURE;
    }

    _write_measurements_json(root);

    _write_ios_json(root);

    json_object_to_fd(1, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);

    return EXIT_SUCCESS;
}