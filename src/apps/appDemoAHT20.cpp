#include "AppManager.h"

class AppDemoAHT20 : public AppBase
{
private:
    /* data */
public:
    AppDemoAHT20()
    {
        name = "demoaht20";
        title = "AHT20";
        description = "AHT20示例App";
        image = NULL;
        peripherals_requested = PERIPHERALS_AHT20_BIT;
        _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
    }
    void set();
    void setup();
};
static AppDemoAHT20 app;

void AppDemoAHT20::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void AppDemoAHT20::setup()
{
    char buf[50];
    if (peripherals.peripherals_current & PERIPHERALS_AHT20_BIT)
    {
        sensors_event_t humidity, temp;
        peripherals.load_append(PERIPHERALS_AHT20_BIT);
        xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
        peripherals.aht.getEvent(&humidity, &temp);
        xSemaphoreGive(peripherals.i2cMutex);
        sprintf(buf, "温度: %g ℃\n相对湿度：%g% rH\n当前为AHT20传感器", temp.temperature,humidity.relative_humidity);
    } else if (peripherals.peripherals_current & PERIPHERALS_SHT30_BIT)
    {
        peripherals.load_append(PERIPHERALS_SHT30_BIT);
        xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
        peripherals.sht.read();
        xSemaphoreGive(peripherals.i2cMutex);
        sprintf(buf, "温度: %g ℃\n相对湿度：%g% rH\n当前为SHT30传感器", peripherals.sht.getTemperature(), peripherals.sht.getHumidity());
    }

    uart->println(buf);
    GUI::msgbox("传感器信息", buf);
    appManager.goBack();
}
