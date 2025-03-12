#include "AppManager.h"
#include "RDA5807.h"

#define SDA_1 25
#define SCL_1 26

RTC_DATA_ATTR bool init_flag = false;

class AppRadio : public AppBase
{
private:
    /* data */
public:
    AppRadio()
    {
        name = "Radio";
        title = "收音机";
        description = "模板";
        image = NULL;
    }
    void set();
    void setup();

    RDA5807 rda;
    uint16_t frequency;
    uint8_t volume;
    uint8_t rssi;
    uint8_t i2c_add[5];
    bool mono;
};
static AppRadio app;

static void radio_exit(){
    app.rda.powerDown();
    app.frequency = hal.pref.putUInt("frequency", 10170);
    app.volume = hal.pref.putUChar("volume", 6);
    app.mono = hal.pref.putBool("mono", false);
}

void AppRadio::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

void AppRadio::setup()
{
    pinMode(SDA_1, OUTPUT | PULLUP);
    pinMode(SCL_1, OUTPUT | PULLUP);
    rda.setup(CLOCK_32K, OSCILLATOR_TYPE_PASSIVE, RLCK_NO_CALIBRATE_MODE_OFF, SDA_1, SCL_1);

    uint8_t num = rda.checkI2C(i2c_add);
    F_LOG("搜索到%d个i2c设备", num);
    for (int i = 0; i < num; i++)
        F_LOG("i2c地址为0x%x", i2c_add[i]);
    delay(100);
    if (!init_flag){
        GUI::info_msgbox("提示", "正在初始化RDA5807...");
        frequency = hal.pref.getUInt("frequency", 10170);
        volume = hal.pref.getUChar("volume", 6);
        mono = hal.pref.getBool("mono", false);
        rda.setVolume(volume);
        rda.setMono(mono);  // Force stereo
        // rda.setRBDS(true);  //  set RDS and RBDS. See setRDS.
        //rda.setRDS(true);
        //rda.setRdsFifo(true);
        rda.setAFC(true);
        rda.setBand(RDA_FM_BAND_WORLD);
        rda.setFrequency(frequency);  // It is the frequency you want to select in MHz multiplied by 100.
        rda.setSeekThreshold(50);            // Sets RSSI Seek Threshold (0 to 127)
    }
    bool end = false;
    while (!end)
    {
        if (hal.btnl.isPressing()){
            end = true;
            appManager.goBack();
        }
    }
}