/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_arduino_version.h"
#include "esp32/rom/spi_flash.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_app_format.h"
#include "soc/efuse_reg.h"
#include "soc/rtc.h"
#include "soc/spi_reg.h"
#include "soc/soc.h"
#include "soc/efuse_reg.h"
#if CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/spi_flash.h"
#endif
#include "esp_bit_defs.h"

#include "Arduino.h"
// #include "esp32-hal-periman.h"

#define chip_report_printf log_printf
#define ARDUINO_HOST_OS "Windows10 x64 22H2 19045.6466"

#define printMemCapsInfo(caps) _printMemCapsInfo(MALLOC_CAP_##caps, #caps)
#define b2kb(b)                ((float)b / 1024.0)
#define b2mb(b)                ((float)b / (1024.0 * 1024.0))

void _printMemCapsInfo(uint32_t caps, const char *caps_name);
void printPkgVersion(void);
void printChipInfo(void);
void printFlashInfo(void);
void printPartitionsInfo(void);
void printSoftwareInfo(void);
void printBoardInfo(void);
void printPerimanInfo(void);
void printBeforeSetupInfo(void);
void printAfterSetupInfo(void);
