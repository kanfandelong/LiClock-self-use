#include <A_Config.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
#ifndef DMA
SPIClass DisplaySPI(FSPI);
ST7305 display(SCREEN_WIDTH, SCREEN_HEIGHT, &DisplaySPI, CONFIG_SPI_CS, CONFIG_PIN_DC, CONFIG_PIN_RST, CONFIG_PIN_BUSY);
#else
ST7305_DMA display(SCREEN_WIDTH, SCREEN_HEIGHT, SPI2_HOST, CONFIG_SPI_SCK, CONFIG_SPI_MOSI, CONFIG_SPI_MOSI, CONFIG_SPI_CS, CONFIG_PIN_DC, CONFIG_PIN_RST, CONFIG_PIN_BUSY);
#endif

DynamicJsonDocument config(1024);
DynamicJsonDocument cfu(2048);

void task_appManager(void *)
{
    log_i("应用管理器已启动");
    while (1)
    {
        appManager.update();
        delay(20);
    }
}
#include <LittleFS.h>
void setup()
{
    // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // 禁用掉电检测
    hal.init();
    hal.update();

    int auto_sleep_mv = hal.pref.getInt("auto_sleep_mv", 2800);
    char buf[128];
    if (hal.VCC < auto_sleep_mv)
    {
        sprintf(buf, "电池电压极低，当前电压为：%d mV，低于自动关机电压%d mV,设备自动关机", hal.VCC, auto_sleep_mv);
        GUI::info_msgbox("提示", buf);
        hal.powerOff(false);
    }

    esp_reset_reason_t reset_reason = esp_reset_reason();
    if (reset_reason == ESP_RST_POWERON)
    {
        if (hal.pref.getBool(set_rtc_in_rst))
        {
            GUI::info_msgbox("提示", "正在联网对时...");
            hal.autoConnectWiFi();
            NTPSync();
        }
        else
        {
            if (peripherals.peripherals_current & PERIPHERALS_DS3231_BIT)
            {
                GUI::info_msgbox("提示", "正在使用DS3231为ESP32对时...");
                delay(1000);
                struct timeval tv;
                struct tm *t;
                t = new tm;
                t->tm_year = hal.timeinfo.tm_year;
                t->tm_mon = hal.timeinfo.tm_mon;
                t->tm_mday = hal.timeinfo.tm_mday;
                t->tm_hour = hal.timeinfo.tm_hour;
                t->tm_min = hal.timeinfo.tm_min;
                t->tm_sec = hal.timeinfo.tm_sec;
                tv.tv_sec = mktime(t);
                tv.tv_usec = 0;
                settimeofday(&tv, NULL);
                delete t;
            }
            else
                log_w("没有DS3231，将无法在未联网的情况下校正本地时间!");
        }
    }

    alarms.load();
    alarms.check();
    Serial0.print("当前CPU频率：");
    Serial0.println(ESP.getCpuFreqMHz());
    log_i("启动appManager...");
    xTaskCreatePinnedToCore(task_appManager, "appManager", 8192, NULL, 4, NULL, 1);
    if (hal.pref.getInt("oobe", 0) <= 2)
    {
        appManager.gotoApp("oobe");
        return;
    }
    hal.getTime();
    if (hal.timeinfo.tm_year > (2016 - 1900))
    {
        // 判断是否是手动退出夜间模式
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 || esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
        {
            if (night_sleep != 0)
            {
                night_sleep = 0;
                night_sleep_today = hal.timeinfo.tm_mday;
            }
        }
        hal.checkNightSleep();
    }
    bool recoverLast = false;
    hal.wakeUpFromDeepSleep = false;
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        hal.wakeUpFromDeepSleep = true;
        recoverLast = appManager.recover(appManager.getRealClock());
    }
    if (recoverLast == false)
    {
        appManager.gotoApp(appManager.getRealClock());
    }
    if (hal.pref.getBool("temp_log", true))
    {
        log_i("进行温湿度记录");
        File temp_file = LittleFS.open("/System/temp.log", "a");
        if (!temp_file)
        {
            error("/System/temp.log打开失败");
            return;
        }
        if (temp_file.size() > 1024 * 512)
        {
            temp_file.close();
            LittleFS.remove("/System/temp.log");
            temp_file = LittleFS.open("/System/temp.log", "a");
        }
        temp_file.printf("%04d.%02d.%02d. %02d:%02d:%02d",
                         hal.timeinfo.tm_year + 1900,
                         hal.timeinfo.tm_mon + 1,
                         hal.timeinfo.tm_mday,
                         hal.timeinfo.tm_hour,
                         hal.timeinfo.tm_min,
                         hal.timeinfo.tm_sec);
        if (peripherals.peripherals_current & PERIPHERALS_AHT20_BIT)
        {
            sensors_event_t humidity, temp;
            peripherals.load_append(PERIPHERALS_AHT20_BIT);
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            peripherals.aht.getEvent(&humidity, &temp);
            xSemaphoreGive(peripherals.i2cMutex);
            temp_file.printf(" Temperature:%.2f℃ Humidity:%.2f%%\n", temp.temperature, humidity.relative_humidity);
        }
        else if (peripherals.peripherals_current & PERIPHERALS_SHT30_BIT)
        {
            peripherals.load_append(PERIPHERALS_SHT30_BIT);
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            peripherals.sht.read();
            xSemaphoreGive(peripherals.i2cMutex);
            temp_file.printf(" Temperature:%.2f℃ Humidity:%.2f%%\n", peripherals.sht.getTemperature(), peripherals.sht.getHumidity());
        }
        temp_file.close();
    }
    return;
}

int NTPCounter = 0;
void loop()
{
    vTaskDelete(NULL);
    vTaskDelay(portMAX_DELAY);
}

/*
if (LittleFS.exists("/test.app") == false)
{
    LittleFS.mkdir("/test.app");
    File f = LittleFS.open("/test.app/conf.lua", "w");
    f.print("title = \"测试\"\n");
    f.close();
    f = LittleFS.open("/test.app/main.lua", "w");
    f.print("function setup()\n");
    f.print("    print(\"Hello World!\")\n");
    f.print("    buzzer.append(1000, 100)\n");
    f.print("end\n");
    f.print("buzzer.append(2000, 100)\n");
    f.print("buzzer.append(0, 100)\n");
    f.close();
}*/