#include <stdint.h>
#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>

#include "pinmap.h"
#include "cmd.h"
#include "log.h"
#include "uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "timers.h"
#include "io.h"
#include "sai.h"
#include "uart_rings.h"
#include "persist_config.h"
#include "lorawan.h"


void hard_fault_handler(void)
{
    platform_raw_msg("----big fat crash -----");
    while(true);
}

volatile uint32_t ms_ticks = 0;

void SysTick_Handler(void)
{
    ms_ticks++;
}

static void clock_setup(void)
{
    rcc_osc_on(RCC_HSI16);

    flash_prefetch_enable();
    flash_set_ws(4);
    flash_dcache_enable();
    flash_icache_enable();
    /* 16MHz / 4(M) = > 4 * 40(N) = 160MHz VCO => 80MHz main pll  */
    rcc_set_main_pll(RCC_PLLCFGR_PLLSRC_HSI16, 4, 40,
                     0, 0, RCC_PLLCFGR_PLLR_DIV2);
    rcc_osc_on(RCC_PLL);
    /* either rcc_wait_for_osc_ready() or do other things */

    /* Enable clocks for peripherals we need */
    rcc_periph_clock_enable(RCC_SYSCFG);

    rcc_set_sysclk_source(RCC_CFGR_SW_PLL); /* careful with the param here! */
    rcc_wait_for_sysclk_status(RCC_PLL);
    /* FIXME - eventually handled internally */
    rcc_ahb_frequency = 80e6;
    rcc_apb1_frequency = 80e6;
    rcc_apb2_frequency = 80e6;
}


int main(void)
{
    clock_setup();

    uarts_setup();

    systick_set_frequency(10, rcc_ahb_frequency);
    systick_counter_enable();

    platform_raw_msg("----start----");
    log_sys_debug("Frequency : %lu", rcc_ahb_frequency);
    log_sys_debug("Version : %s", GIT_VERSION);

    log_init();
    init_persistent();
    cmds_init();
    ios_init();
    sai_init();
    lorawan_init();

    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);

    log_async_log = true;

    uint32_t finish_tick = 0;
    while(true)
    {
        for(unsigned n = 0; n < rcc_ahb_frequency / 1000; n++)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
        }

        if (audio_dumping && (finish_tick < ms_ticks))
        {
            audio_dumping = false;
        }

        if (start_audio_dumping)
        {
            start_audio_dumping = false;
            audio_dumping = true;
            finish_tick = ms_ticks + 5 * 1000;
        }

        //lw_loop();

        gpio_toggle(LED_PORT, LED_PIN);
    }

    return 0;
}
