#include "AppManager.h"
#include "RDA5807.h"

#define SDA_1 26
#define SCL_1 25

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
    void drawRadioUI(bool frequencyMode);
    void drawModeIndicator(uint16_t symbol, int16_t margin);
    void drawInfoPanel(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent);
    void setup();

    RDA5807 rda;
    uint16_t frequency;
    uint8_t volume;
    uint8_t rssi;
    uint16_t display_count = 0;
    bool mono;
};
static AppRadio app;

static void radio_exit()
{
    app.rda.powerDown();
    app.frequency = hal.pref.putUInt("frequency", 10170);
    app.volume = hal.pref.putUChar("volume", 6);
    app.mono = hal.pref.putBool("mono", false);
    init_flag = false;
}

void AppRadio::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void AppRadio::drawRadioUI(bool frequencyMode)
{
    // display.setFullWindow();
    // 设置全局绘制参数
    u8g2Fonts.setBackgroundColor(TFT_WHITE);
    u8g2Fonts.setForegroundColor(TFT_BLACK);
    display.fillScreen(TFT_WHITE);

    // ----------------- 时间显示（右上角） -----------------
    u8g2Fonts.setFont(u8g2_font_7x13B_tf); // 小号等宽字体
    u8g2Fonts.setCursor(display.width() - 70, 15); // 右侧留边距
    u8g2Fonts.printf("%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);

    // ----------------- 频率显示（中央大字体） -----------------
    String freqString = String(frequency) + " MHz";
    u8g2Fonts.setFont(u8g2_font_fub35_tf); // 大型字体
    uint16_t freqWidth = u8g2Fonts.getUTF8Width(freqString.c_str());
    u8g2Fonts.setCursor((display.width() - freqWidth)/2, 75);
    u8g2Fonts.print(freqString);

    // ----------------- 信号强度（左下角） -----------------
    // 文本标签
    u8g2Fonts.setFont(u8g2_font_7x13B_tf);
    u8g2Fonts.setCursor(10, display.height() - 25);
    u8g2Fonts.print("RSSI:");

    // 进度条背景
    display.drawRect(10, display.height() - 20, 100, 12, TFT_BLACK);
    // 填充进度条（按百分比）
    display.fillRect(10, display.height() - 20, map(rssi, 0, 64, 0, 100), 12, TFT_BLACK);

    // ----------------- 音量显示（右下角） -----------------
    // 文本标签
    u8g2Fonts.setCursor(display.width() - 80, display.height() - 25);
    u8g2Fonts.print("VOL:");

    // 音量进度条背景
    display.drawRect(display.width() - 110, display.height() - 20, 100, 12, TFT_BLACK);
    // 填充进度条
    display.fillRect(display.width() - 110, display.height() - 20, map(volume, 0, 15, 0, 100), 12, TFT_BLACK);

    // ---------- 模式状态指示器 ----------
    drawModeIndicator(frequencyMode ? 0x25B2 : 0x25BC, 10); // 三角形图标表示当前模式

    if (display_count > 20){
        display.display();
        display_count = 0;
    }
    else{
        display.display();
        display_count++;
    }
}

// 辅助函数：模式状态指示
void AppRadio::drawModeIndicator(uint16_t symbol, int16_t margin)
{
    u8g2Fonts.setFont(u8g2_font_unifont_t_symbols);
    u8g2Fonts.drawGlyph(margin, display.height() - 15, symbol);
    u8g2Fonts.drawGlyph(display.width() - margin - 16, display.height() - 15, symbol);
}

// 辅助函数：绘制信息面板
void AppRadio::drawInfoPanel(int16_t x, int16_t y, int16_t w, int16_t h)
{
    // 面板容器
    display.drawRect(x, y, w, h, TFT_BLACK);

    // 音量指示
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    u8g2Fonts.setCursor(x + 5, y + 15);
    u8g2Fonts.print("VOL:");
    for (uint8_t i = 0; i < volume; i += 3)
    {
        display.fillRect(x + 40 + i * 3, y + 10, 2, 10, TFT_BLACK);
    }

    // 频段标识
    u8g2Fonts.setCursor(x + 5, y + 35);
    u8g2Fonts.print("FM 87.5-108");
}

// 辅助函数：绘制进度条
void AppRadio::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent)
{
    display.drawRect(x, y, w, h, TFT_BLACK);
    int16_t fillWidth = (w - 2) * percent / 100;
    display.fillRect(x + 1, y + 1, fillWidth, h - 2, TFT_BLACK);
}

void AppRadio::setup()
{
    pinMode(SDA_1, OUTPUT | PULLUP);
    pinMode(SCL_1, OUTPUT | PULLUP);
    rda.setup(CLOCK_32K, OSCILLATOR_TYPE_PASSIVE, RLCK_NO_CALIBRATE_MODE_OFF, SDA_1, SCL_1);

    if (!init_flag)
    {
        GUI::info_msgbox("提示", "正在初始化RDA5807...");
        frequency = hal.pref.getUInt("frequency", 10170);
        volume = hal.pref.getUChar("volume", 6);
        mono = hal.pref.getBool("mono", false);
        rda.setVolume(volume);
        rda.setMono(mono); // Force stereo
        // rda.setRBDS(true);  //  set RDS and RBDS. See setRDS.
        // rda.setRDS(true);
        // rda.setRdsFifo(true);
        rda.setAFC(true);
        rda.setBand(RDA_FM_BAND_WORLD);
        rda.setFrequency(frequency); // It is the frequency you want to select in MHz multiplied by 100.
        rda.setSeekThreshold(50);    // Sets RSSI Seek Threshold (0 to 127)
    }
    rssi = rda.getRssi();
    Serial0.printf("frequency: %d, rssi: %d, volume: %d\n", frequency, rssi, volume);
    drawRadioUI(true);
    bool end = false;
    while (!end)
    {
        if (hal.btnl.isPressing())
        {
            end = true;
            appManager.goBack();
        }
    }
}