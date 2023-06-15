#include <string.h>

#include <nvs_flash.h>
#include <nvs.h>

#include "pinmap.h"
#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "platform.h"
#include "platform_model.h"


persist_storage_t               persist_data __attribute__((aligned (16))) = {0};
persist_measurements_storage_t  persist_measurements __attribute__((aligned (16))) = {0};


static void _persistent_wipe(void)
{
    memset(&persist_data, 0, sizeof(persist_data));
    memset(&persist_measurements, 0, sizeof(persist_measurements));
    persist_data.version = PERSIST_VERSION;
    persist_data.log_debug_mask = DEBUG_SYS;
    persist_data.config_count = 0;
    if (strlen(MODEL_NAME) > MODEL_NAME_LEN)
        memcpy(&persist_data.model_name[0], MODEL_NAME, MODEL_NAME_LEN);
    else
        snprintf(persist_data.model_name, MODEL_NAME_LEN, MODEL_NAME);
    model_persist_config_model_init(&persist_data.model_config);
}


void     persist_commit()
{
    esp_err_t err;
    nvs_handle_t config_handle;

    ESP_ERROR_CHECK(nvs_open("osmconfig", NVS_READWRITE, &config_handle));

    nvs_erase_all(config_handle);

    err = nvs_set_blob(config_handle, "osm_data", &persist_data, sizeof(persist_data));
    if (err != ESP_OK)
        log_error("Failed write persist_data : %s", esp_err_to_name(err));

    err = nvs_set_blob(config_handle, "osm_meas", &persist_measurements, sizeof(persist_measurements));
    if (err != ESP_OK)
        log_error("Failed write persist_measurements : %s", esp_err_to_name(err));
    nvs_commit(config_handle);
    nvs_close(config_handle);
}


bool persistent_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    nvs_handle_t config_handle;

    ESP_ERROR_CHECK(nvs_open("osmconfig", NVS_READONLY, &config_handle));
    size_t persist_data_read = 0, persist_measurements_read = 0;
    nvs_get_blob(config_handle, "osm_data", &persist_data, &persist_data_read);
    nvs_get_blob(config_handle, "osm_meas", &persist_measurements, &persist_measurements_read);

    nvs_close(config_handle);

    if (!persist_data_read || !persist_measurements_read)
    {
        log_error("No config loaded");
        goto setup_exit;
    }

    log_sys_debug("Loaded osm_data :%d", (int)persist_data_read);
    log_sys_debug("Loaded osm_meas :%d", (int)persist_measurements_read);
    if (persist_data.version != PERSIST_VERSION)
    {
        log_error("Invalid config loaded (version %u)", persist_data.version);
        goto setup_exit;
    }

    return true;
setup_exit:
    _persistent_wipe();
    return false;
}


void platform_persist_wipe(void)
{
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    _persistent_wipe();
    persist_commit();
}


