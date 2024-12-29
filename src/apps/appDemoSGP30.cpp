#include "AppManager.h"

class AppDemoSGP30 : public AppBase
{
private:
    /* data */
public:
    AppDemoSGP30()
    {
        name = "demosgp30";
        title = "SGP30";
        description = "空气质量传感";
        image = NULL;
        peripherals_requested = PERIPHERALS_SGP30_BIT;
        _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
        _reentrant = true;
    }
    void set();
    void setup();
};
static AppDemoSGP30 app;
void AppDemoSGP30::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void AppDemoSGP30::setup()
{
    xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
    GUI::msgbox("提示","正在初始化传感器\n请耐心等待");
    display.display(true);
    delay(4000);
    uint16_t TVOC,eCO2,H2,Ethanol;
    peripherals.sgp.IAQmeasure();
    TVOC = peripherals.sgp.TVOC;
    eCO2 = peripherals.sgp.eCO2;
    peripherals.sgp.IAQmeasureRaw();
    H2 = peripherals.sgp.rawH2;
    Ethanol = peripherals.sgp.rawEthanol;
    char datbuf[100];
    sprintf(datbuf, "TVOC: %u ppb\neCO2: %u ppm\nrawH2: %u ppm\nrawEthanol: %u ppm\n", TVOC,eCO2,H2,Ethanol);
    Serial.printf(datbuf);
    GUI::msgbox("传感器信息",datbuf);
    peripherals.sgp.softReset();
    xSemaphoreGive(peripherals.i2cMutex);
    Serial.printf("软复位SGP30，以进入休眠");
    appManager.goBack();
}

