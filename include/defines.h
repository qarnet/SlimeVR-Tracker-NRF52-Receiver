#ifndef DEFINES_H_
#define DEFINES_H_

#include <nrf52840.h>

#define DFU_MODE (0x57)
#define DFU_OTA_MODE (0xA8)
#define DFU_SERIAL_MODE (0x4e)
#define BOOTLOADER_MAGIC_VALUE (0xf01669ef)

inline boot_into_dfu(uint8_t dfu_mode)
{
    NRF_POWER->GPREGRET = dfu_mode;
    NVIC_SystemReset();
}

#endif