#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "pinmap.h"
#include "cc.h"
#include "rak3172.h"

#define __MODEL_CONFIG__

#define ENV01C_FLASH_ADDRESS               0x8000000
#define ENV01C_FLASH_PAGE_SIZE             2048

#define ENV01C_FLASH_CONFIG_PAGE           2
#define ENV01C_FLASH_MEASUREMENTS_PAGE     3
#define ENV01C_FW_PAGE                     4
#define ENV01C_NEW_FW_PAGE                 120

#define ENV01C_FW_PAGES                    100
#define ENV01C_FW_MAX_SIZE                 (ENV01C_FW_PAGES * ENV01C_FLASH_PAGE_SIZE)
#define ENV01C_PAGE2ADDR(_page_)           (ENV01C_FLASH_ADDRESS + (ENV01C_FLASH_PAGE_SIZE * _page_))
#define ENV01C_FW_ADDR                     ENV01C_PAGE2ADDR(ENV01C_FW_PAGE)
#define ENV01C_NEW_FW_ADDR                 ENV01C_PAGE2ADDR(ENV01C_NEW_FW_PAGE)
#define ENV01C_PERSIST_RAW_DATA            ((const uint8_t*)ENV01C_PAGE2ADDR(ENV01C_FLASH_CONFIG_PAGE))
#define ENV01C_PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)ENV01C_PAGE2ADDR(ENV01C_FLASH_MEASUREMENTS_PAGE))

#define ENV01C_PERSIST_VERSION             3

#define ENV01C_PERSIST_MODEL_CONFIG_T      persist_env01c_config_v1_t

#define ENV01C_MODEL_NAME                  "ENV01C"

#define CMD_LINELEN 128

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512

#define UART_2_IN_BUF_SIZE  64
#define UART_2_OUT_BUF_SIZE 64

#define UART_3_IN_BUF_SIZE  128
#define UART_3_OUT_BUF_SIZE 128

#define IOS_COUNT           7

#define ADC_CC_COUNT        3

#define comms_name              rak3172


typedef struct
{
    uint32_t                mins_interval;
    uint8_t                 _[12];
    /* 16 byte boundary ---- */
    modbus_bus_t            modbus_bus;
    /* 16 byte boundary ---- */
    comms_config_t          comms_config;
    /* 16 byte boundary ---- */
    cc_config_t             cc_configs[ADC_CC_COUNT];
    uint8_t                 __[16-(ADC_CC_COUNT * sizeof(cc_config_t)%16)];
    /* 16 byte boundary ---- */
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 ___[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    /* 16 byte boundary ---- */
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    uint8_t                 ____[16-((SAI_NUM_CAL_COEFFS * sizeof(float))%16)];
    /* 16 byte boundary ---- */
    uint32_t                sai_no_buf;
    uint8_t                 _____[16-(sizeof(uint32_t)%16)];
    /* 7 x 16 bytes          */
} persist_env01c_config_v1_t;
