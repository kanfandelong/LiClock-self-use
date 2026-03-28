#pragma once

#include <Arduino.h>
#include <Fonts/Picopixel.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>
#include "qrcode.h"
#include <esp_sntp.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include "esp_task_wdt.h"
#include <driver/rtc_io.h>
#include <esp_netif.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

extern "C" {
#include <dirent.h>
}

// #include <ESP32-targz.h>

#define code_version "2.1.1.6" // 代码版本号

#define DMA

#ifndef DMA
#include <ST7305.h>
#else
#include <ST7305_DMA.h>
#endif

#define TFT_BLACK     0x0000
#define TFT_WHITE     0xFFFF

#define SCREEN_WIDTH 384
#define SCREEN_HEIGHT 168
#define PIN_ADC 16

#define PIN_BUTTONL 7
#define PIN_BUTTONC 6
#define PIN_BUTTONR 5

// #define PIN_RTC_IRQ 25
// #define PIN_CHARGING 26

#define PIN_RTC_IRQ 4
#define PIN_CHARGING 15

#define PIN_SDVDD_CTRL 8
#define PIN_SD_CARDDETECT 45
#define PIN_SD_SCLK 21
#define PIN_SD_CMD 14
#define PIN_SD_D0 47
#define PIN_SD_D1 48
#define PIN_SD_D2 12
#define PIN_SD_D3 13

#define PIN_SD_CS PIN_SD_D3
#define PIN_SD_MOSI PIN_SD_CMD
#define PIN_SD_MISO PIN_SD_D0

#define PIN_I2S_MCLK 17
#define PIN_I2S_DOUT 10
#define PIN_I2S_BCLK 9
#define PIN_I2S_LRCK 11
#define PIN_DAC_EN 18
#define PIN_DAC_FMT 3
#define PIN_DAC_XSMT 46

#define PIN_BUZZER 42
#define PIN_SDA 1
#define PIN_SCL 2

#define wifi_config_file "/System/wifi_config.json"

// 下面这些尽量不要修改，因为改了不完全有效
#define GRAPH_HEIGHT 37
#define SAMPLE_COUNT 10
#define SAMPLE_STEP 1
#define PX_PER_SAMPLE (SCREEN_WIDTH / SAMPLE_STEP / (SAMPLE_COUNT - 2))
#define DEFAULT_CONFIG "{\"p1\":\"116.3975,39.9091\",\"p2\":\"15\",\"p3\":\"1\",\"p4\":\"23:30\",\"p5\":\"05:00\",\"p6\":\" \",\"p7\":\" \",\"p8\":\"0\",\"p9\":\"1\",\"p10\":\"1\",\"p11\":\"0\",\"p12\":\"\",\"p13\":\"CST-8\",\"p14\":\"/test.mp3\"}"
#define DEFAULT_WIFI_CONFIG "{\"networks\":[{\"ssid\":\"\",\"pass\":\"\"}]}"
#define TFmode "p9"
#define autontpsync "p10"
#define Textfile "p11"
#define isp_file "p12"
#define Time_Zone "p13"
typedef struct
{
    const uint8_t *data;
    uint16_t width;
    uint16_t height;
} ico_desc;
#include <ESPAsyncWebServer.h>

// #define USE_CDC

#ifdef USE_CDC
extern HWCDC *uart;
#else
extern HardwareSerial *uart;
#endif

extern AsyncWebServer server;
extern float rain_data_raw[];
extern int ydata[];
extern const ico_desc weather_icons_day[];
extern const ico_desc weather_frames[4];
extern const ico_desc weather_icons_night[];

extern esp_ip6_addr_t ipv6global;
extern esp_ip6_addr_t ipv6local;
extern const char *ipv6_to_str(const esp_ip6_addr_t *addr);
extern void enableIPv6();
void refreshIPV6Addr();
bool file_exist(const char *path);
const char *remove_path_prefix(const char *path, const char *prefix);

extern DynamicJsonDocument config;
extern DynamicJsonDocument cfu;
#ifndef DMA
extern ST7305 display;
#else
extern ST7305_DMA display;
#endif
extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
extern TJpg_Decoder TJpgDec;

extern bool force_full_update;
extern int part_refresh_count;
extern uint8_t night_sleep;       // 夜间模式屏幕状态，0：不在夜间模式，1：晚安，2：早上好
extern uint8_t night_sleep_today; // 今天是否进入过夜间模式
extern bool LuaRunning;           // 全局变量，表示Lua服务器是否运行，用于防止调试时误退出
extern bool serverRunning;

#define PARAM_GPS "p1"
#define PARAM_FULLUPDATE "p2"
#define PARAM_SLEEPATNIGHT "p3"
#define PARAM_SLEEPATNIGHT_START "p4"
#define PARAM_SLEEPATNIGHT_END "p5"
#define PARAM_SSID "p6"
#define PARAM_PASS "p7"
#define PARAM_CLOCKONLY "p8"

void processRain(float max);
void beginFileServer(bool for_TF = false);
void beginWebServer();
void updateWebServer();
const uint8_t *getBatteryIcon(bool forceEmptyIcon = false);
uint8_t getBatterysoc();

#include "hal.h"
#include "weather.h"
#include "lyrics.h"
#include "myNTP.h"
#include "AppManager.h"
#include "GUI.h"
#include "settings.h"
#include "alarm.h"
#include "peripherals.h"
#include "Buzzer.h"
#include "lua_trans.h"
#include "Serial_cmd.h"
#include "truetype.h"
extern const char *getRealPath(const char *fpath);
extern void setPath(const char *path);
extern bool log_system_init();
extern void log_system_deinit();
// extern void log_write(const char *file, int line, const char *fmt, ...);
extern void log_write(const char *fmt, ...);

extern const uint8_t u8g2_font_wqy12_t_gb2312_self[] U8G2_FONT_SECTION("u8g2_font_wqy12_t_gb2312_self");
// #define F_LOG(fmt, ...) log_write(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define info(fmt, ...) log_write(ARDUHAL_LOG_FORMAT(I, fmt), ##__VA_ARGS__)
#define warn(fmt, ...) log_write(ARDUHAL_LOG_FORMAT(W, fmt), ##__VA_ARGS__)
#define error(fmt, ...) log_write(ARDUHAL_LOG_FORMAT(E, fmt), ##__VA_ARGS__)