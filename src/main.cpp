#include <A_Config.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

#if defined(E029A01)
    GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display(GxEPD2_290(/*CS=5*/ CONFIG_SPI_CS, /*DC=*/CONFIG_PIN_DC, /*RST=*/CONFIG_PIN_RST, /*BUSY=*/CONFIG_PIN_BUSY)); // 注意：此类略微修改过，使用两个缓冲区
#else
    #if defined(T5D)
        GxEPD2_BW<GxEPD2_290_T5D, GxEPD2_290_T5D::HEIGHT> display(GxEPD2_290_T5D(/*CS=5*/ CONFIG_SPI_CS, /*DC=*/CONFIG_PIN_DC, /*RST=*/CONFIG_PIN_RST, /*BUSY=*/CONFIG_PIN_BUSY));
    #endif
    #if defined(T5)
        GxEPD2_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(/*CS=5*/ CONFIG_SPI_CS, /*DC=*/CONFIG_PIN_DC, /*RST=*/CONFIG_PIN_RST, /*BUSY=*/CONFIG_PIN_BUSY));
    #endif
    #if defined(T5D_gray)
        GxEPD2_BW<GxEPD2_290_T5D_gray, GxEPD2_290_T5D_gray::HEIGHT> display(GxEPD2_290_T5D_gray(/*CS=5*/ CONFIG_SPI_CS, /*DC=*/CONFIG_PIN_DC, /*RST=*/CONFIG_PIN_RST, /*BUSY=*/CONFIG_PIN_BUSY));
    #endif    
#endif

DynamicJsonDocument config(1024);
DynamicJsonDocument cfu(2048);

void task_appManager(void *)
{
    while (1)
    {
        appManager.update();
        delay(20);
    }
}
#include <LittleFS.h>
void setup()
{
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
    hal.init();
    hal.update();
    esp_reset_reason_t reset_reason = esp_reset_reason();
    if(reset_reason == ESP_RST_POWERON && config[autontpsync] == "1")
    {
        GUI::info_msgbox("提示", "正在对时...");
        hal.autoConnectWiFi();
        NTPSync();
    }
    int auto_sleep_mv = hal.pref.getInt("auto_sleep_mv", 2800);
    char buf[128];
    if(hal.VCC < auto_sleep_mv)
    {
        sprintf(buf, "电池电压极低，当前电压为：%d mV，低于自动关机电压%d mV,设备自动关机", hal.VCC, auto_sleep_mv);
        GUI::info_msgbox("提示", buf);
        hal.powerOff(false);
    }

    alarms.load();
    alarms.check();
    Serial.print("当前CPU频率：");
    Serial.println(ESP.getCpuFreqMHz());
    xTaskCreate(task_appManager, "appManager", 8192, NULL, 3, NULL);
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