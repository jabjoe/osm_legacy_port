#include <inttypes.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"
#include "common.h"
#include "io.h"
#include "pulsecount.h"


#define PULSECOUNT_COLLECTION_TIME_MS       1000;

#define PULSECOUNT_INSTANCES   {                                       \
    { { MEASUREMENTS_PULSE_COUNT_NAME_1, W1_PULSE_1_IO} ,              \
        W1_PULSE_1_PIN  , W1_PULSE_1_EXTI,                             \
        W1_PULSE_1_EXTI_IRQ,                                           \
        0, 0 },                                                        \
    { { MEASUREMENTS_PULSE_COUNT_NAME_2, W1_PULSE_2_IO} ,              \
        W1_PULSE_2_PIN , W1_PULSE_2_EXTI,                              \
        W1_PULSE_2_EXTI_IRQ,                                           \
        0, 0 }                                                         \
}


typedef struct
{
    special_io_info_t   info;
    uint16_t            pin;
    uint32_t            exti;
    uint8_t             exti_irq;
    volatile uint32_t   count;
    uint32_t            send_count;
} pulsecount_instance_t;


static pulsecount_instance_t _pulsecount_instances[] = PULSECOUNT_INSTANCES;


void W1_PULSE_ISR(void)
{
    uint16_t gpio_state = gpio_get(W1_PULSE_PORT, W1_PULSE_1_PIN | W1_PULSE_2_PIN);
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        pulsecount_instance_t* inst = &_pulsecount_instances[i];
        if (!io_is_pulsecount_now(inst->info.io))
            continue;
        if (!(gpio_state & inst->pin))
        {
            exti_reset_request(inst->exti);
            __sync_add_and_fetch(&inst->count, 1);
        }
    }
}


static void _pulsecount_init_instance(pulsecount_instance_t* instance)
{
    if (!instance)
        return;
    if (!io_is_pulsecount_now(instance->info.io))
        return;
    rcc_periph_clock_enable(PORT_TO_RCC(W1_PULSE_PORT));

    gpio_mode_setup(W1_PULSE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, instance->pin);

    exti_select_source(instance->exti, W1_PULSE_PORT);
    exti_set_trigger(instance->exti, EXTI_TRIGGER_FALLING);
    exti_enable_request(instance->exti);

    instance->count = 0;
    instance->send_count = 0;

    nvic_enable_irq(instance->exti_irq);
    pulsecount_debug("Pulsecount '%s' enabled", instance->info.name);
}


void pulsecount_init(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        _pulsecount_init_instance(&_pulsecount_instances[i]);
    }
}


static void _pulsecount_shutdown_instance(pulsecount_instance_t* instance)
{
    if (!instance)
        return;
    if (io_is_pulsecount_now(instance->info.io))
        return;
    exti_disable_request(instance->exti);
    nvic_disable_irq(instance->exti_irq);
    instance->count = 0;
    instance->send_count = 0;
    pulsecount_debug("Pulsecount disabled");
}


static void _pulsecount_shutdown(void)
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        _pulsecount_shutdown_instance(&_pulsecount_instances[i]);
    }
}


void pulsecount_enable(bool enable)
{
    if (enable)
        pulsecount_init();
    else
        _pulsecount_shutdown();
}


void _pulsecount_log_instance(pulsecount_instance_t* instance)
{
    if (!io_is_pulsecount_now(instance->info.io))
        return;
    log_out("%s : %"PRIu32, instance->info.name, instance->count);
}


void pulsecount_log()
{
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        _pulsecount_log_instance(&_pulsecount_instances[i]);
    }
}


measurements_sensor_state_t pulsecount_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = PULSECOUNT_COLLECTION_TIME_MS;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static bool _pulsecount_get_instance(pulsecount_instance_t** instance, char* name)
{
    if (!instance)
        return false;
    pulsecount_instance_t* inst;
    for (unsigned i = 0; i < ARRAY_SIZE(_pulsecount_instances); i++)
    {
        inst = &_pulsecount_instances[i];
        if (strncmp(name, inst->info.name, sizeof(inst->info.name) * sizeof(char)) == 0)
        {
            *instance = inst;
            return true;
        }
    }
    return false;
}


measurements_sensor_state_t pulsecount_begin(char* name)
{
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!io_is_pulsecount_now(instance->info.io))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    pulsecount_debug("%s at start %"PRIu32, instance->info.name, instance->count);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t pulsecount_get(char* name, value_t* value)
{
    if (!value)
        return false;
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    if (!io_is_pulsecount_now(instance->info.io))
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    instance->send_count = instance->count;
    pulsecount_debug("%s at end %"PRIu32, instance->info.name, instance->send_count);
    *value = value_from(instance->send_count);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void pulsecount_ack(char* name)
{
    pulsecount_instance_t* instance;
    if (!_pulsecount_get_instance(&instance, name))
        return;
    pulsecount_debug("%s ack'ed", instance->info.name);
    __sync_sub_and_fetch(&instance->count, instance->send_count);
    instance->send_count = 0;
}
