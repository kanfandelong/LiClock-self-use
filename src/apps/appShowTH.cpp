#include "AppManager.h"

#define SCREEN_WIDTH 296
#define SCREEN_HEIGHT 128
#define MAX_POINTS 296 // 屏幕宽度限制
#define MARGIN_TOP 15
#define MARGIN_BOTTOM 15
#define GRAPH_HEIGHT_ (SCREEN_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM)

// 温湿度数据结构
struct EnvData
{
    float temp;
    float humi;
};

class AppShowTH : public AppBase
{
private:
    /* data */
public:
    AppShowTH()
    {
        name = "THHistory";
        title = "温湿度历史";
        description = "温湿度历史数据图表";
        image = NULL;
    }
    void set();
    int parseDataFile(const char *path, EnvData *data, int maxCount);
    int sampleData(EnvData *src, int srcCount, EnvData *dest, int maxDestCount);
    void drawGraph(EnvData *data, int count); // 修改为绘制点图
    void setup();
};
static AppShowTH app;

void AppShowTH::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

// 文件解析函数（保持不变）
int AppShowTH::parseDataFile(const char *path, EnvData *data, int maxCount)
{
    File file = LittleFS.open(path, "r");
    if (!file)
        return 0;

    int count = 0;
    while (file.available() && count < maxCount)
    {
        String line = file.readStringUntil('\n');

        // 解析行数据 (示例格式: "2025.08.09. 01:49:02 Temperature:32.67℃ Humidity:67.61%")
        int tempPos = line.indexOf("Temperature:");
        int humiPos = line.indexOf("Humidity:");

        if (tempPos != -1 && humiPos != -1)
        {
            String tempStr = line.substring(tempPos + 12, humiPos - 3);
            String humiStr = line.substring(humiPos + 9, line.length() - 1);

            data[count].temp = tempStr.toFloat();
            data[count].humi = humiStr.toFloat();
            count++;
        }
    }

    file.close();
    return count;
}

// 数据抽样函数（保持不变）
int AppShowTH::sampleData(EnvData *src, int srcCount, EnvData *dest, int maxDestCount)
{
    if (srcCount <= maxDestCount)
    {
        // 数据量少，直接复制
        memcpy(dest, src, srcCount * sizeof(EnvData));
        return srcCount;
    }

    // 计算抽样步长
    float step = (float)srcCount / (float)maxDestCount;
    int destCount = 0;

    for (float i = 0; i < srcCount && destCount < maxDestCount; i += step)
    {
        dest[destCount++] = src[(int)i];
    }

    return destCount;
}

// 修改为绘制点图（不再绘制折线）
void AppShowTH::drawGraph(EnvData *data, int count)
{
    display.fillScreen(GxEPD_WHITE);

    // 查找温湿度范围
    float minTemp = data[0].temp, maxTemp = data[0].temp;
    float minHumi = data[0].humi, maxHumi = data[0].humi;

    for (int i = 1; i < count; i++)
    {
        if (data[i].temp < minTemp)
            minTemp = data[i].temp;
        if (data[i].temp > maxTemp)
            maxTemp = data[i].temp;
        if (data[i].humi < minHumi)
            minHumi = data[i].humi;
        if (data[i].humi > maxHumi)
            maxHumi = data[i].humi;
    }

    // 添加边界缓冲
    float tempRange = maxTemp - minTemp;
    float humiRange = maxHumi - minHumi;
    minTemp -= tempRange * 0.1;
    maxTemp += tempRange * 0.1;
    minHumi -= humiRange * 0.1;
    maxHumi += humiRange * 0.1;

    // 绘制坐标轴
    display.drawLine(0, MARGIN_TOP, SCREEN_WIDTH, MARGIN_TOP, GxEPD_BLACK);
    display.drawLine(0, SCREEN_HEIGHT - MARGIN_BOTTOM,
                     SCREEN_WIDTH, SCREEN_HEIGHT - MARGIN_BOTTOM, GxEPD_BLACK);
    
    // 添加中间分隔线以便区分温湿度区域
    display.drawLine(0, MARGIN_TOP + GRAPH_HEIGHT_/2, 
                     SCREEN_WIDTH, MARGIN_TOP + GRAPH_HEIGHT_/2, GxEPD_BLACK);

    // 绘制图例
    u8g2Fonts.setCursor(5, 13);
    u8g2Fonts.printf("Temp: %.1fC-%.1fC", minTemp, maxTemp);
    u8g2Fonts.setCursor(SCREEN_WIDTH - 80, 13);
    u8g2Fonts.printf("Humi: %.0f%%-%.0f%%", minHumi, maxHumi);

    // 绘制温度点（上半区）
    for (int i = 0; i < count; i++)
    {
        int x = map(i, 0, count - 1, 0, SCREEN_WIDTH);
        int y = map(data[i].temp, minTemp, maxTemp,
                   MARGIN_TOP + GRAPH_HEIGHT_/2, MARGIN_TOP);
        
        // 绘制单像素点（使用小十字提高可见性）
        display.drawPixel(x, y, GxEPD_BLACK);
        // display.drawPixel(x+1, y, GxEPD_BLACK);
        // display.drawPixel(x-1, y, GxEPD_BLACK);
        // display.drawPixel(x, y+1, GxEPD_BLACK);
        // display.drawPixel(x, y-1, GxEPD_BLACK);
    }

    // 绘制湿度点（下半区）
    for (int i = 0; i < count; i++)
    {
        int x = map(i, 0, count - 1, 0, SCREEN_WIDTH);
        int y = map(data[i].humi, minHumi, maxHumi,
                   SCREEN_HEIGHT - MARGIN_BOTTOM, MARGIN_TOP + GRAPH_HEIGHT_/2 + 1);
        
        // 绘制单像素点（使用小方块提高可见性）
        display.drawPixel(x, y, GxEPD_BLACK);
        // display.drawPixel(x+1, y, GxEPD_BLACK);
        // display.drawPixel(x, y+1, GxEPD_BLACK);
        // display.drawPixel(x+1, y+1, GxEPD_BLACK);
    }

    // 添加图例说明
    u8g2Fonts.setCursor(10, SCREEN_HEIGHT - 10);
    // u8g2Fonts.print("Temp: +  Humi: #");

    display.display();
}

void AppShowTH::setup()
{
    // 读取原始数据
    const int maxRawData = 5000; // 最大支持5000个数据点
    EnvData *rawData = new EnvData[maxRawData];
    int rawCount = parseDataFile("/System/temp.log", rawData, maxRawData);

    Serial.printf("Raw data points: %d\n", rawCount);
    if (rawCount == 0) {
        delete[] rawData;
        return;
    }

    // 抽样数据
    EnvData *sampledData = new EnvData[MAX_POINTS];
    int sampledCount = sampleData(rawData, rawCount, sampledData, MAX_POINTS);

    Serial.printf("Sampled data points: %d\n", sampledCount);

    // 绘制图表
    drawGraph(sampledData, sampledCount);
    
    // 释放内存
    delete[] rawData;
    delete[] sampledData;
    
    hal.wait_input();
    appManager.goBack();
}