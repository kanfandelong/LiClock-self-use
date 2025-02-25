#include "AppManager.h"
#include "ESP8266Audio.h"

// 全局变量
//AudioPlayer player;
volatile bool g_playbackFinished = false;
SemaphoreHandle_t audio_control_sem = NULL;  // 音频任务的信号量
TaskHandle_t player_loop_task_handle = NULL;
AudioFileSource *in = nullptr;
AudioGeneratorMP3 *generator;
AudioOutput *output;
const char *music_file;
bool btn_int = false, menu = false;
bool btn_l = false;

static void player_exit(){
    if (!generator->isRunning()){
        generator->stop();
    }
    if (!generator->loop()) generator->stop();
    pinMode(32, INPUT);
    int date = hal.pref.getInt("CpuFreq", 80);
    int freq = ESP.getCpuFreqMHz();
    if (freq != date)
    {
        bool cpuset = setCpuFrequencyMhz(date);
        Serial.begin(115200);
        ESP_LOGI("HAL", "CpuFreq: %dMHZ -> %dMHZ ......", freq, date);
        if(cpuset){ESP_LOGI("HAL","ok");}
        else {ESP_LOGI("hal", "err");}
    }
}

void player_loop(void *){
    while (1) {
        // 尝试获取信号量（等待直到成功）
        if (xSemaphoreTake(audio_control_sem, portMAX_DELAY) == pdTRUE) {
            // 安全操作解码器
            if (generator->isRunning()) {
                if (!generator->loop()) {
                    generator->stop();
                }
            }
            else
                delay(5);
            xSemaphoreGive(audio_control_sem);  // 释放信号量
        }
        else
            delay(1);  // 避免忙等待
    }
}
class AppMusicPlayer : public AppBase
{
private:
    /* data */
public:
    AppMusicPlayer()
    {
        name = "musicplater";
        title = "音乐";
        description = "暂无";
        peripherals_requested = PERIPHERALS_SD_BIT;
        image = NULL;
    }
    void set();
    const char* remove_path_prefix(const char* path, const char* prefix);
    void select_file();
    void setup();
};
static AppMusicPlayer app;
void AppMusicPlayer::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
/**
 * 去除路径特定前缀函数
 * @param path 完整路径
 * @param prefix 特定前缀
 * @return 去除前缀后的路径
 */
const char* AppMusicPlayer::remove_path_prefix(const char* path, const char* prefix) {
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
void AppMusicPlayer::select_file(){
    music_file = GUI::fileDialog("选择音乐文件", false, "mp3", NULL, (String)"/mp3");    
    if (strstr(music_file, ".mp3") != nullptr) {
        // 示例：网络MP3流
        if (strncmp(music_file, "/sd/", 4) == 0)
        {
            in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
        }
        else if (strncmp(music_file, "/littlefs/", 10) == 0) 
        {
            in = new AudioFileSourceLittleFS(remove_path_prefix(music_file,"/littlefs"));
        }
        generator->begin(in, output);
    }
}
void AppMusicPlayer::setup(){
    bool cpuset = setCpuFrequencyMhz(240);
    Serial.begin(115200);
    if(cpuset){ESP_LOGI("change CpuFrequency","ok");}
    else {ESP_LOGI("change CpuFrequency", "err");}
    exit = player_exit;
    uint8_t core = xPortGetCoreID();
    Serial.printf("run in core %d\r\n", core);
    audioLogger = &Serial;
    pinMode(32, OUTPUT);
    audio_control_sem = xSemaphoreCreateBinary();  // 创建二进制信号量
    xSemaphoreGive(audio_control_sem);  // 初始化为可用状态  bclkPin = 26;wclkPin = 25;// doutPin = 22;doutPin = 32;
    menu = true;
    generator = new AudioGeneratorMP3();
    output= new AudioOutputI2S(0, 1);
    select_file();
    generator->begin(in, output);
    if (core == 0)
        xTaskCreatePinnedToCore(player_loop, "player_loop_task", 8192, NULL, 5, &player_loop_task_handle, 1);
    else
        xTaskCreatePinnedToCore(player_loop, "player_loop_task", 8192, NULL, 5, &player_loop_task_handle, 0);
    // xTaskCreatePinnedToCore(player_loop, "player_loop_task", 4096, NULL, 5, &player_loop_task_handle, 1);
    GUI::info_msgbox("", "播放中...");
    menu = false;
    bool end = false;
    while(!end){
        if (hal.btnl.isPressing()) {
            end = true;
            appManager.goBack();
        }
        if (hal.btnc.isPressing()){
            xSemaphoreTake(audio_control_sem, portMAX_DELAY);
            GUI::info_msgbox("", "暂停解码器...");
            delay(100);
            generator->stop();
            delay(10);
            select_file();
            generator->begin(in, output);
            xSemaphoreGive(audio_control_sem);
        }
        if (!generator->isRunning()) {
            xSemaphoreTake(audio_control_sem, portMAX_DELAY);
            GUI::info_msgbox("", "播放完成");
            delay(50);
        }
        //delay(500);
    }    
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 1;
}

