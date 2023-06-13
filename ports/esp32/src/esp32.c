#include <string.h>

#include <driver/i2c.h>
#include <driver/gpio.h>
#include <soc/clk_tree_defs.h>

#include "platform.h"

#include "pinmap.h"
#include "log.h"
#include "platform_model.h"
#include "comms.h"
#include "sleep.h"

#include "adcs.h"

static volatile uint32_t since_boot_ms = 0;

uint32_t platform_get_frequency(void)
{
    return SOC_CLK_XTAL32K_FREQ_APPROX;
}


void platform_blink_led_init(void)
{
}


void platform_blink_led_toggle(void)
{
}


void platform_watchdog_reset(void)
{
}


void platform_watchdog_init(uint32_t ms)
{
}


void platform_init(void)
{
    gpio_config_t de_485_conf = {
        .pin_bit_mask = BIT64(DE_485_PIN),
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    gpio_config(&de_485_conf);
}


void platform_set_rs485_mode(bool driver_enable)
{
    /* ST3485E
     *
     * 2. RE Receiver output enable. RO is enabled when RE is low; RO is
     * high impedance when RE is high. If RE is high and DE is low, the
     * device will enter a low power shutdown mode.

     * 3. DE Driver output enable. The driver outputs are enabled by
     * bringing DE high. They are high impedance when DE is low. If RE
     * is high DE is low, the device will enter a low-power shutdown
     * mode. If the driver outputs are enabled, the part functions as
     * line driver, while they are high impedance, it functions as line
     * receivers if RE is low.
     *
     * */

    if (driver_enable)
    {
        modbus_debug("driver:enable");
        gpio_set_level(DE_485_PIN, 1);
    }
    else
    {
        modbus_debug("driver:disable");
        gpio_set_level(DE_485_PIN, 0);
    }
}


void platform_reset_sys(void)
{
}


persist_storage_t* platform_get_raw_persist(void)
{
    return (persist_storage_t*)PERSIST_RAW_DATA;
}


persist_measurements_storage_t* platform_get_measurements_raw_persist(void)
{
    return (persist_measurements_storage_t*)PERSIST_RAW_MEASUREMENTS;
}


bool platform_persist_commit(persist_storage_t* persist_data, persist_measurements_storage_t* persist_measurements)
{
    return false;
}

void platform_persist_wipe(void)
{
}


bool platform_overwrite_fw_page(uintptr_t dst, unsigned abs_page, uint8_t* fw_page)
{
    return false;
}


void platform_clear_flash_flags(void)
{
}


void platform_start(void)
{
    ;
}


bool platform_running(void)
{
    return true;
}


void platform_deinit(void)
{
}


void platform_setup_adc(adc_setup_config_t* config)
{
}


void platform_adc_set_regular_sequence(uint8_t num_adcs_types, adcs_type_t* adcs_types)
{
}


void platform_adc_start_conversion_regular(void)
{
}


void platform_adc_power_off(void)
{
}


void platform_adc_set_num_data(unsigned num_data)
{
}


void platform_hpm_enable(bool enable)
{
}


void platform_tight_loop(void) {}


uint32_t get_since_boot_ms(void)
{
    return since_boot_ms;
}


void platform_gpio_init(const port_n_pins_t * gpio_pin)
{
}


void platform_gpio_setup(const port_n_pins_t * gpio_pin, bool is_input, uint32_t pull)
{
}

void platform_gpio_set(const port_n_pins_t * gpio_pin, bool is_on)
{
}

bool platform_gpio_get(const port_n_pins_t * gpio_pin)
{
    return false;
}