#include "AppManager.h"

#define RGB_PIN 25
#define RGB_POWER 26

class AppRGB : public AppBase
{
private:
    /* data */
public:
    AppRGB()
    {
        name = "RGB";
        title = "RGB";
        description = "RGB";
        image = NULL;
    }
    void set();
    void setup();
};
static AppRGB app;

void AppRGB::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void AppRGB::setup()
{
    pinMode(RGB_POWER, OUTPUT);
    pinMode(RGB_PIN, OUTPUT);
    bool m_random = true;
    uint8_t color[3];
    char buf[32];
    unsigned long t = millis();
    digitalWrite(RGB_POWER, HIGH);
    neopixelWrite(RGB_PIN, 255, 255, 255);
    display.fillScreen(GxEPD_BLACK);
    GUI::drawWindowsWithTitle("WS2812B RGB 灯珠测试");
    while (1)
    {
        if (m_random){
            color[0] = random(0, 255);
            color[1] = random(0, 255);
            color[2] = random(0, 255);
            neopixelWrite(RGB_PIN, color[0], color[1], color[2]);
            sprintf(buf, "R: %d,G: %d,B: %d", color[0], color[1], color[2]);
            GUI::info_msgbox("RGB",buf);
            delay(900);
        }
        delay(100);
        if (hal.btnl.isPressing()){
            if(GUI::waitLongPress(PIN_BUTTONL)){
                appManager.goBack();
                return;
            }
        }
        if (hal.btnc.isPressing()){
            color[0] = GUI::msgbox_hex("R", 2, color[0]);
            color[1] = GUI::msgbox_hex("G", 2, color[1]);
            color[2] = GUI::msgbox_hex("B", 2, color[2]);
            neopixelWrite(RGB_PIN, color[0], color[1], color[2]);
            sprintf(buf, "R: %d,G: %d,B: %d", color[0], color[1], color[2]);
            GUI::info_msgbox("RGB",buf);
        }
        if (hal.btnr.isPressing()){
            m_random = !m_random;
            while(hal.btnr.isPressing()){
                delay(50);
            }
        }
        if (millis() - t > 60000){
            t = millis();
            display.display();
        }
    }
}