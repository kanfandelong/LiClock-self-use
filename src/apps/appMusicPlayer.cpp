#include "AppManager.h"
#include "ESP8266Audio.h"

// 全局变量
//AudioPlayer player;
volatile bool g_playbackFinished = false;
SemaphoreHandle_t audio_control_sem = NULL;  // 音频任务的信号量
TaskHandle_t player_loop_task_handle = NULL;
AudioFileSource *in = nullptr;
AudioGeneratorMP3 *generator;
AudioOutputI2S *output;
const char *music_file;
bool _end;
bool _play_end = false;
bool _loop_play = false;
float gain = 0.3;
uint32_t decode_heap;

static void player_exit(){
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
                decode_heap = generator->preAllocSize();
                if (!generator->loop()) {
                    generator->stop();
                    _play_end = true;
                    vTaskDelete(NULL);
                }
            }
            else
                delay(5);
            xSemaphoreGive(audio_control_sem);  // 释放信号量
        }
        else
            delay(5);  // 避免忙等待
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
    void player_menu();
    void begin_player_task();
    void show_display();
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
    music_file = GUI::fileDialog("选择音乐文件", false, "mp3", NULL);  
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
    }
}
static const menu_item menu_player[] =
{
    {NULL,"返回"},
    {NULL,"退出"},
    {NULL,"选择文件"},
    {NULL,"音量设置"},
    {NULL,"循环播放"},
    {NULL,NULL},
};
void AppMusicPlayer::player_menu(){
    bool end = false;
    while (!end)
    {
        int res = GUI::menu("菜单", menu_player);
        switch(res){
            case 0:
                end = true;
                break;
            case 1:
                end = true;
                _end = true;
                xSemaphoreTake(audio_control_sem, portMAX_DELAY);
                delay(100);
                generator->stop();
                vTaskDelete(player_loop_task_handle);
                appManager.goBack();
                break;
            case 2:            
                if (!_play_end){
                    xSemaphoreTake(audio_control_sem, portMAX_DELAY);
                    delay(100);
                    generator->stop();
                    vTaskDelete(player_loop_task_handle);
                    delay(10);
                }
                select_file();
                output= new AudioOutputI2S(0, 1);
                output->SetGain(gain);
                generator->begin(in, output);
                begin_player_task();
                xSemaphoreGive(audio_control_sem);
                break;
            case 3:
                gain = (float)GUI::msgbox_number("0-40", 2, gain * 10.0) / 10.0;
                output->SetGain(gain);
                break;
            case 4:
                if (GUI::msgbox_yn("循环播放", "是否循环播放?"))
                    _loop_play = true;
                else 
                    _loop_play = false;
                break;
            default:
                GUI::info_msgbox("警告", "非法的输入值");
                break;
        }
    }
}
void AppMusicPlayer::begin_player_task(){  
    _play_end = false;
    uint8_t core = xPortGetCoreID();
    Serial.printf("run in core %d\r\n", core);  
    if (core == 0)
        xTaskCreatePinnedToCore(player_loop, "player_loop_task", 8192, NULL, 5, &player_loop_task_handle, 1);
    else
        xTaskCreatePinnedToCore(player_loop, "player_loop_task", 8192, NULL, 5, &player_loop_task_handle, 0);
}
void AppMusicPlayer::show_display(){
    display.clearScreen();
    GUI::drawWindowsWithTitle("音乐播放器");
    u8g2Fonts.setCursor(3, 30);
    u8g2Fonts.printf("播放：%s", music_file);
    u8g2Fonts.setCursor(3, 45);
    if (_play_end)
        u8g2Fonts.printf("播放结束");
    else
        u8g2Fonts.printf("播放中...");
    if (_loop_play) {
        u8g2Fonts.setCursor(3, 60);
        u8g2Fonts.printf("循环播放");
    }
    u8g2Fonts.setCursor(3, 75);
    u8g2Fonts.printf("剩余堆内存：%.2fKB 解码器内存占用：%.2fKB", (float)ESP.getFreeHeap() / 1024.0, (float)decode_heap / 1024.0);
    u8g2Fonts.setCursor(3, 90);
    u8g2Fonts.printf("Gain：%.2f   电源电压：%d mV", gain, hal.VCC);
    display.display(true);
}
void AppMusicPlayer::setup(){
    bool cpuset = setCpuFrequencyMhz(240);
    Serial.begin(115200);
    if(cpuset){ESP_LOGI("change CpuFrequency","ok");}
    else {ESP_LOGI("change CpuFrequency", "err");}
    exit = player_exit;
    audioLogger = &Serial;
    pinMode(32, OUTPUT);
    audio_control_sem = xSemaphoreCreateBinary();  // 创建二进制信号量
    xSemaphoreGive(audio_control_sem);  // 初始化为可用状态  bclkPin = 26;wclkPin = 25;// doutPin = 22;doutPin = 32;
    generator = new AudioGeneratorMP3();
    output= new AudioOutputI2S(0, 1);
    output->SetGain(gain);
    select_file();
    generator->begin(in, output);
    begin_player_task();
    show_display();
    _end = false;
    int a = 0;
    int display_count = 0;
    while(!_end){
        if (GUI::waitLongPress(PIN_BUTTONC)){
            player_menu();
            show_display();
        }
        if (hal.btnl.isPressing()) {
            gain = gain + 0.1;
            if (gain > 4.0) {
                gain = 4.0;
            }
            output->SetGain(gain);
        }
        if (hal.btnr.isPressing()) {
            gain = gain - 0.1;
            if (gain < 0.0) {
                gain = 0.0;
            }
            output->SetGain(gain);
        }
        if (_loop_play && _play_end) {
            if (strncmp(music_file, "/sd/", 4) == 0)
            {
                in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
            }
            else if (strncmp(music_file, "/littlefs/", 10) == 0) 
            {
                in = new AudioFileSourceLittleFS(remove_path_prefix(music_file,"/littlefs"));
            }
            output= new AudioOutputI2S(0, 1);
            output->SetGain(gain);
            generator->begin(in, output);
            begin_player_task();
            xSemaphoreGive(audio_control_sem);
        }
        if (_play_end) {
            show_display();
            delay(50);
        }
        if (a > 60) {
            a = 0;
            if (display_count > 15) {
                display_count = 0;
                display.clearScreen();
                display.display();
                show_display();
            }
            else
                show_display();
            display_count++;
        }
        a++;
        delay(50);
    }    
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 1;
}

