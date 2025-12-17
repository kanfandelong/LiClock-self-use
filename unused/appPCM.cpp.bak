#include "AppManager.h"

// PWM配置
#define PWM_PIN 26       // 使用支持LEDC的GPIO（如25）
int PWM_FREQ = 32000;   // PWM频率（32kHz）
#define PWM_RESOLUTION 8 // PWM分辨率（8位）


#define sampleRate 16000 // 音频文件采样率
unsigned long sampleInterval = 1000000 / sampleRate; // 每个采样点的间隔时间（μs）
// LEDC参数
const int ledcChannel = 1;  // 通道0~15

// 双缓冲区配置
const int bufferSize = 4096; // 每个缓冲区的大小
uint8_t bufferA[bufferSize];
uint8_t bufferB[bufferSize];
volatile bool bufferAReady = false;
volatile bool bufferBReady = false;
volatile bool useBufferA = true; // 当前使用的缓冲区
bool bufinit = false;
bool play_end = false;
File audioFile;
TaskHandle_t readTaskHandle = NULL, playTaskHandle = NULL;
extern const char *filename; //保存从文件选择的完整文件名

void readTask(void *param) {
  while (1) {
    // 只有当前不播放的缓冲区才能被填充
    if (useBufferA && !bufferBReady) {
      //Serial.println("填充缓冲区B");
      size_t bytesRead = audioFile.read(bufferB, sizeof(bufferB));
      if (bytesRead > 0) {
        bufferBReady = true; // 标记缓冲区B就绪
      } else {
        audioFile.close();
        play_end = true;
        GUI::info_msgbox("提示", "播放结束");
        vTaskDelete(NULL);
      }
    } 
    else if (!useBufferA && !bufferAReady || !bufinit) {
      bufinit = true;
      //Serial.println("填充缓冲区A");
      size_t bytesRead = audioFile.read(bufferA, sizeof(bufferA));
      if (bytesRead > 0) {
        bufferAReady = true; // 标记缓冲区A就绪
      } else {
        audioFile.close();
        play_end = true;
        GUI::info_msgbox("提示", "播放结束");
        vTaskDelete(NULL);
      }
    }else
        vTaskDelay(1); // 让出CPU
  }
}

void playTask(void *param) {
    unsigned long nextTime = micros(); // 初始化下一个采样点的时间戳
    while (1) {
        if (bufferAReady && useBufferA) {
            // 播放缓冲区A
            for (int i = 0; i < bufferSize; i++) {
                ledcWrite(ledcChannel, bufferA[i]); // 直接输出8位无符号PCM数据
                nextTime += sampleInterval;
                long waitTime = nextTime - micros();
                if (waitTime > 0) {
                    delayMicroseconds(waitTime);
                } else {
                    nextTime = micros(); // 时间补偿
                }
            }
            bufferAReady = false; // 标记缓冲区A为空
            useBufferA = false;
        } else if (bufferBReady && !useBufferA) {
            // 播放缓冲区B
            for (int i = 0; i < bufferSize; i++) {
                ledcWrite(ledcChannel, bufferB[i]); // 直接输出8位无符号PCM数据
                nextTime += sampleInterval;
                long waitTime = nextTime - micros();
                if (waitTime > 0) {
                    delayMicroseconds(waitTime);
                } else {
                    nextTime = micros(); // 时间补偿
                }
            }
            bufferBReady = false; // 标记缓冲区B为空
            useBufferA = true;
        } else {
            vTaskDelay(1); // 让出CPU
            Serial.print(".");
            if (play_end) {
                vTaskDelete(NULL);
            }
        }
    }
}

class AppPCM : public AppBase
{
private:
    /* data */
public:
    AppPCM()
    {
        name = "PCM";
        title = "音频播放";
        description = "暂无";
        peripherals_requested = PERIPHERALS_SD_BIT;
        image = NULL;
    }
    void set();
    void setup();
    void menu();
    const char* remove_path_prefix(const char* path, const char* prefix);
};
static AppPCM app;

void AppPCM::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

void AppPCM::setup()
{
    display.clearScreen();
    GUI::drawWindowsWithTitle(title);
    // 打开音频文件
    begin:
    filename = GUI::fileDialog("文件管理", false, NULL, NULL);
    if (strncmp(filename, "/sd/", 4) == 0) {
        audioFile = SD.open(remove_path_prefix(filename,"/sd"));
    } 
    else if (strncmp(filename, "/littlefs/", 10) == 0) {
        audioFile = LittleFS.open(remove_path_prefix(filename,"/littlefs"));
    }
    pinMode(PWM_PIN, OUTPUT);
    // 配置PWM
    ledcSetup(ledcChannel, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PWM_PIN, ledcChannel);

    // 创建任务
    xTaskCreatePinnedToCore(readTask, "ReadTask", 4096, NULL, 1, &readTaskHandle, 0);
    xTaskCreatePinnedToCore(playTask, "PlayTask", 4096, NULL, 1, &playTaskHandle, 1);
    bool end = false;
    while (!end)
    {
        delay(10);
        if (hal.btnl.isPressing()){
            end = true;
        }
        if (hal.btnc.isPressing()){
            menu();
        }
        if (hal.btnr.isPressing()){
            bufinit = false;
            if (eTaskGetState(readTaskHandle) != eDeleted || eTaskGetState(readTaskHandle) != eInvalid)
                vTaskDelete(readTaskHandle);
            if (eTaskGetState(playTaskHandle) != eDeleted || eTaskGetState(playTaskHandle) != eInvalid)
                vTaskDelete(playTaskHandle);
            goto begin;
        }
    }
    appManager.goBack();
}
static const menu_item PCM_menu[] =
{
    {NULL,"返回"},
    {NULL,"采样点延时"},
    {NULL,"PWM频率"},
    {NULL,NULL},
};

void AppPCM::menu(){
    int res = 0;
    bool end = false;
    while (end == false)
    {
        res = GUI::menu(title, PCM_menu);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            sampleInterval = GUI::msgbox_number("采样点延时", 2, sampleInterval);
            break;
        case 2:
            PWM_FREQ = GUI::msgbox_number("PWM频率(KHz)", 3, PWM_FREQ / 1000) * 1000;
            ledcSetup(ledcChannel, PWM_FREQ, PWM_RESOLUTION);
        default:
            break;
        }
    }
    display.clearScreen();
    display.display();
}

/**
 * 去除路径特定前缀函数
 * @param path 完整路径
 * @param prefix 特定前缀
 * @return 去除前缀后的路径
 */
const char* AppPCM::remove_path_prefix(const char* path, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t path_len = strlen(path);

    // 检查路径是否以指定前缀开头
    if (strncmp(path, prefix, prefix_len) == 0) {
        // 返回去除前缀后的路径
        return path + prefix_len;
    }
    // 如果路径不以指定前缀开头，则返回原始路径
    return path;
}