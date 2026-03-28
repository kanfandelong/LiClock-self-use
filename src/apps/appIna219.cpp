#include "AppManager.h"
#include <Adafruit_INA219.h>

#define SDA_1 26
#define SCL_1 25

class AppIna219 : public AppBase
{
private:
    static const int READINGS_COUNT = 10;  // 平均计算使用的读数数量
    float currentReadings[READINGS_COUNT]; // 存储电流读数的数组
    int readingIndex = 0;                  // 当前读数索引
    int validReadings = 0;                 // 有效读数数量

public:
    AppIna219()
    {
        name = "ina219";
        title = "电流检测";
        description = "INA219电流检测";
        image = NULL;
        // 初始化电流读数数组
        for (int i = 0; i < READINGS_COUNT; i++) {
            currentReadings[i] = 0.0f;
        }
    }
    Adafruit_INA219 ina219;
    void set();
    void setup();
    
    // 计算平均电流的函数
    float calculateAverageCurrent() {
        if (validReadings == 0) return 0.0f;
        
        float sum = 0.0f;
        for (int i = 0; i < validReadings; i++) {
            sum += currentReadings[i];
        }
        return sum / validReadings;
    }
};
static AppIna219 app;

void AppIna219::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

void AppIna219::setup()
{
    GUI::info_msgbox("提示", "正在初始化INA219...");
    Wire1.setPins(SDA_1, SCL_1);
    Wire1.setClock(400000);
    if (!ina219.begin(&Wire1))
    {
        uart->println("Failed to find INA219 chip");
        GUI::info_msgbox("警告", "无法初始化INA219...");
        delay(1000);
        return;
    }
    if (GUI::msgbox_yn("提示", "请选择量程?", "32V 1A", "16V 400mA"))
        ina219.setCalibration_32V_1A();
    else
        ina219.setCalibration_16V_400mA();
    
    bool end = false;
    int i = 0;
    while(!end){
        float current_mA = ina219.getCurrent_mA();
        
        // 存储当前读数并更新索引
        currentReadings[readingIndex] = current_mA;
        readingIndex = (readingIndex + 1) % READINGS_COUNT;
        if (validReadings < READINGS_COUNT) validReadings++;
        
        // 计算平均电流
        float averageCurrent = calculateAverageCurrent();
        
        // 显示数据
        display.fillScreen(TFT_WHITE);
        u8g2Fonts.setCursor(3, 30);
        u8g2Fonts.printf("电压: %.3f V", ina219.getBusVoltage_V());
        u8g2Fonts.setCursor(3, 45);
        u8g2Fonts.printf("电流: %.3f mA", current_mA);
        u8g2Fonts.setCursor(3, 60);
        u8g2Fonts.printf("平均电流: %.3f mA", averageCurrent);
        u8g2Fonts.setCursor(3, 75);
        u8g2Fonts.printf("功率: %.3f mW", ina219.getPower_mW());
        
        i++;
        if (i > 20){
            i = 0;
            display.display(); // 全刷新
        }
        else {
            display.display(); // 局部刷新
        }
        
        delay(400);
        if (hal.btnl.isPressing()){
            end = true;
            appManager.goBack();
        }
    }
}