#pragma once

#define TOOL_FLASH_ADDRESS               0x8000000
#define TOOL_FLASH_PAGE_SIZE             2048

#define TOOL_FLASH_CONFIG_PAGE           2
#define TOOL_FLASH_MEASUREMENTS_PAGE     3
#define TOOL_FW_PAGE                     4
#define TOOL_NEW_FW_PAGE                 120

#define TOOL_FW_PAGES                    100
#define TOOL_FW_MAX_SIZE                 (TOOL_FW_PAGES * TOOL_FLASH_PAGE_SIZE)
#define TOOL_PAGE2ADDR(_page_)           (TOOL_FLASH_ADDRESS + (TOOL_FLASH_PAGE_SIZE * _page_))
#define TOOL_FW_ADDR                     TOOL_PAGE2ADDR(TOOL_FW_PAGE)
#define TOOL_NEW_FW_ADDR                 TOOL_PAGE2ADDR(TOOL_NEW_FW_PAGE)
#define TOOL_PERSIST_RAW_DATA            ((const uint8_t*)TOOL_PAGE2ADDR(TOOL_FLASH_CONFIG_PAGE))
#define TOOL_PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)TOOL_PAGE2ADDR(TOOL_FLASH_MEASUREMENTS_PAGE))

#define TOOL_PERSIST_VERSION             3

#define TOOL_PERSIST_MODEL_CONFIG_T      persist_tool_config_v1_t



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
typedef union
{
} persist_tool_config_v1_t;
#pragma GCC diagnostic pop


