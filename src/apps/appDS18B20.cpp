#include "AppManager.h"
#include "DallasTemperature.h"

#define DS18B20_pin 32

OneWire  ds(DS18B20_pin);
DallasTemperature sensors(&ds);

static const menu_item DS18B20_menu[] =
{
    {NULL,"返回"},
    {NULL,"设置温度分辨率"},
    {NULL,NULL},
};

class AppTemp : public AppBase
{
private:
    /* data */
public:
    AppTemp()
    {
        name = "ds18b20";
        title = "DS18B20";
        description = "读取DS18B20温度";
        image = NULL;
    }
    void set();
    float getTemp();
    void menu();
    void setup();
};
static AppTemp app;


void AppTemp::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

float AppTemp::getTemp(){
  sensors.requestTemperatures(); // 发送温度读取请求
  return sensors.getTempCByIndex(0); // 读取第一个传感器的温度
}

void AppTemp::menu(){
    int res = 0;
    bool end = false;
    while (end == false)
    {
        res = GUI::menu(title, DS18B20_menu);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            sensors.setResolution(GUI::msgbox_number("设置温度分辨率", 2, sensors.getResolution())); 
            break;
        default:
            break;
        }
    }
}

void AppTemp::setup(){
    bool end = false;
    sensors.begin();
    char buf[64];
    int i = 0;
    while(!end){
        sprintf(buf, "DS18B20: %.2f°C", getTemp());
        GUI::info_msgbox(title, buf);
        i++;
        if (hal.btnc.isPressing()){
            menu();
        }
        if (hal.btnl.isPressing()){
            end = true;
        }
        if (hal.btnr.isPressing()){
            hal.wait_input();
        }
        esp_sleep_enable_timer_wakeup(450000UL);
        if (hal.btn_activelow){
            esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 0);
            esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ALL_LOW);
            gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_LOW_LEVEL);
        }else{
            esp_sleep_enable_ext1_wakeup((1ULL << PIN_BUTTONC) | (1ULL << PIN_BUTTONL) | (1ULL << PIN_BUTTONR), ESP_EXT1_WAKEUP_ANY_HIGH);
        }
        esp_sleep_enable_gpio_wakeup();
        log_i("进入lightsleep");
        esp_light_sleep_start();
        log_i("退出lightsleep");
        i++;
        if (i > 20){
            i = 0;
            end = true;
            appManager.noDeepSleep = false;
            appManager.nextWakeup = 1;
        }
    }
}

