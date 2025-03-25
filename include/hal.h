#ifndef __HAL_H__
#define __HAL_H__
#include <A_Config.h>
#include <Preferences.h>
#include "OneButton.h"

typedef struct
{
    int16_t avg;       // 平均电流
    int16_t max;       // 最大电流
    int16_t stby;      // 待机电流
} _bat_current;

typedef struct
{
    uint16_t remain;
    uint16_t full;
    uint16_t avail;
    uint16_t avail_full;
    uint16_t remain_f;
    uint16_t full_f;
    uint16_t design;
} _bat_capacity;

typedef struct
{
    bool DSG;       // 电池放电标志
    bool FC;        // 电量充满标志
    bool CHG;       // 快速充电允许
} _bat_flag;

typedef struct
{
    uint8_t soc;            // 电池百分比电量
    uint8_t soh;            // 电池健康度
    float temp;          // 温度
    float voltage;       // 电池电压
    _bat_current current;   // 电池电流
    _bat_capacity capacity; // 电池容量
    int16_t power;          // 电池平均功率
    _bat_flag flag;
} _bat_info;


class HAL
{
public:
    void printBatteryInfo();
    void task_bat_info_update();
    bool connected_wifi(const char* ssid, const char* pass);
    bool wifi_config_manger();
    void savewifiConfig(StaticJsonDocument<2048>& wifi_config);
    void saveConfig();
    void loadConfig();
    void getTime();
    char* get_char_sha_key(const char *str);
    IPAddress getip();
    bool cheak_firmware_update();
    void cheak_freq();
    void WiFiConfigSmartConfig();
    void WiFiConfigManual();
    void ReqWiFiConfig();
    void task_buffer_handler();
    /**
     * @brief 等待用户输入
     * @param sleeptime 休眠时间，单位秒，0表示不休眠
     */
    void wait_input(uint32_t sleeptime = 0);
    /**
     * @brief 初始化
     * @return true 需要全屏刷新
     * @return false 不需要全屏刷新
     */
    bool init();
    void rtc_offset();
    bool autoConnectWiFi(bool need_wifi_config = true);
    void searchWiFi();
    static void set_sleep_set_gpio_interrupt();
    void powerOff(bool displayMessage = true);
    void goSleep(uint32_t sec = 0);
    void update();
    int getNTPMinute();
    void checkNightSleep();
    void setWakeupIO(int io1, int io2);
    bool copy(File &newFile, File &file);
    void rm_rf(const char *path);
    struct tm timeinfo;
    time_t now;
    int global_hour_offset = 0;
    int numNetworks = 0;
    int ppc = 0;
    int lpt = 0;
    int auto_sleep_mv = 0;
    time_t lastsync = 1;
    int32_t every = 100;
    int32_t delta = 0;
    int32_t upint = 2 * 60;                 // NTP同步间隔
    int32_t last_update_delta = 0x7FFFFFFF; // 上次更新时修正时间与实际时间的差值
    Preferences pref;
    Preferences nvs_;
    int16_t VCC = 0;
    _bat_info bat_info;
    bool USBPluggedIn = false;
    bool isCharging = false;
    bool TF_connected = false;
    bool dis_DS3231 = false;
    OneButton btnr = OneButton(PIN_BUTTONR);
    OneButton btnl = OneButton(PIN_BUTTONL);
    OneButton btnc = OneButton(PIN_BUTTONC);
    bool btn_activelow = true;
    void hookButton()
    {
        _hookButton = true;
        while (btnr.isPressing() || btnl.isPressing() || btnc.isPressing())
        {
            delay(10);
        }
        delay(10);
        Serial.println("Hooked");
    }
    void unhookButton()
    {
        while (btnr.isPressing() || btnl.isPressing() || btnc.isPressing())
        {
            delay(10);
        }
        delay(10);
        _hookButton = false;
        Serial.println("Unhooked");
    }
    void detachAllButtonEvents()
    {
        btnr.attachClick(NULL);
        btnr.attachDoubleClick(NULL);
        btnr.attachLongPressStart(NULL);
        btnr.attachDuringLongPress(NULL);
        btnr.attachLongPressStop(NULL);
        btnr.attachMultiClick(NULL);
        btnr.attachClick(NULL, NULL);
        btnr.attachDoubleClick(NULL, NULL);
        btnr.attachLongPressStart(NULL, NULL);
        btnr.attachDuringLongPress(NULL, NULL);
        btnr.attachLongPressStop(NULL, NULL);
        btnr.attachMultiClick(NULL, NULL);
        
        btnl.attachClick(NULL);
        btnl.attachDoubleClick(NULL);
        btnl.attachDuringLongPress(NULL);
        btnl.attachLongPressStop(NULL);
        btnl.attachMultiClick(NULL);
        btnl.attachClick(NULL, NULL);
        btnl.attachDoubleClick(NULL, NULL);
        btnl.attachLongPressStart(NULL, NULL);
        btnl.attachDuringLongPress(NULL, NULL);
        btnl.attachLongPressStop(NULL, NULL);
        btnl.attachMultiClick(NULL, NULL);

        btnc.attachClick(NULL);
        btnc.attachDoubleClick(NULL);
        btnc.attachLongPressStart(NULL);
        btnc.attachDuringLongPress(NULL);
        btnc.attachLongPressStop(NULL);
        btnc.attachMultiClick(NULL);
        btnc.attachClick(NULL, NULL);
        btnc.attachDoubleClick(NULL, NULL);
        btnc.attachLongPressStart(NULL, NULL);
        btnc.attachDuringLongPress(NULL, NULL);
        btnc.attachLongPressStop(NULL, NULL);
        btnc.attachMultiClick(NULL, NULL);
    }
    bool noDeepSleep = false;
    bool SleepUpdateMutex = false;
    bool _hookButton = false; // 不要修改这个
    bool wakeUpFromDeepSleep = false;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int _wakeupIO[2] = {PIN_BUTTONC, PIN_BUTTONL};

private:
};
extern HAL hal;
#endif