#include "AppManager.h"
#include "AudioFileSource.h"
#include "AudioFileSourceID3.h"
#include "AudioFileSourceSD.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioOutputI2S.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioGeneratorMP3.h"

static const uint8_t APP_MusicPlayer_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xc0, 0x07, 0x00, 0x00, 0xf8, 0x07, 0x00, 0x00, 0xff, 0x07,
    0x00, 0xe0, 0xff, 0x07, 0x00, 0xfc, 0xff, 0x07, 0x00, 0xff, 0x9f, 0x07,
    0x80, 0xff, 0x81, 0x07, 0x80, 0x3f, 0x80, 0x07, 0x80, 0x0f, 0x80, 0x07,
    0x80, 0x0f, 0x80, 0x07, 0x80, 0x0f, 0x80, 0x07, 0x80, 0x0f, 0x80, 0x07,
    0x80, 0x0f, 0x80, 0x07, 0x80, 0x0f, 0x80, 0x07, 0x80, 0x0f, 0xe0, 0x07,
    0x80, 0x0f, 0xf8, 0x07, 0x80, 0x0f, 0xf8, 0x07, 0xe0, 0x0f, 0xfc, 0x07,
    0xf0, 0x0f, 0xfc, 0x07, 0xf8, 0x0f, 0xfc, 0x07, 0xf8, 0x0f, 0xf8, 0x07,
    0xf8, 0x0f, 0xf0, 0x03, 0xf8, 0x07, 0xe0, 0x01, 0xf8, 0x07, 0x00, 0x00,
    0xf0, 0x03, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 音乐播放器图标

typedef struct
{
    String album = "---";       // 专辑
    String performer = "---";   // 歌手
    String title = "---";       // 标题
} id3_info; // ID3信息结构体
    
SemaphoreHandle_t audio_control_sem = NULL;  // 音频任务的信号量
TaskHandle_t player_loop_task_handle = NULL; // 音频任务句柄
AudioFileSource *in = nullptr;               // 音频文件源
AudioFileSourceID3 *id3;                     // ID3信息解码处理
AudioGeneratorMP3 *generator;                // MP3解码器
AudioOutputI2S *output;                      // I2S输出
AudioOutputI2SNoDAC *noDAC;                  // I2S输出
// 以下变量保存至RTC内存，避免deepsleep后丢失
RTC_DATA_ATTR uint16_t currentSongIndex = 0; // 当前播放索引（音乐列表数组位置）
RTC_DATA_ATTR const char *music_file;        // 当前播放文件的指针
RTC_DATA_ATTR char buf[300];                 // 实际存储当前播放文件路径字符串
RTC_DATA_ATTR bool file_is_ok = false;       // 用于判断播放器的启动状态（初次运行/从deepsleep恢复）

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
        noDefaultEvent = true;
        peripherals_requested = PERIPHERALS_SD_BIT;
        image = APP_MusicPlayer_bits;
    }
    void set();
    // const char* remove_path_prefix(const char* path, const char* prefix);
    void select_file();
    void file_in(const char* path);
    void next_song(bool next = true, bool btn = false);
    void sem();
    void delete_playtask();
    void bulid_music_list();
    bool music_list_menu(bool play = false);
    void player_menu();
    void begin_player_task();
    void show_display();
    void player_set();
    void setup();
    String currentDir;              // 当前歌曲目录
    String pathStr;                 // 当前歌曲位于的目录
    bool is_root = false;           // 是否是根目录
    menu_item *fileList;            // 歌曲菜单数组
    char *titles[256];              // 歌曲名内存指针数组
    char char_buf[512];             // 字符串拼接缓存
    bool filelist_ok = false;       // 歌曲列表就绪标志
    uint16_t maxSong = 0;           // 歌曲总数

    bool _play_end = false;                 // 播放完成标志
    unsigned long play_time_start;          // 播放开始时间
    unsigned long play_time_end;            // 播放结束时间
    unsigned long play_time_total = 0;      // 播放总时间
    unsigned long display_time = millis();  // 屏幕上次刷新时间

    id3_info info;                          // 歌曲ID3信息
    bool _end;                              // 播放器主任务函数while循环停止标志
    bool nodac = false;                     // 无DAC标志
    bool in_littlefs = false;               // 文件是否位于LittleFS
    float gain = 0.3;                       // 音频输出增益（音量）
    bool need_deep_sleep = false;           // 是否需要进入deepsleep
    int play_count = 1;                     // 播放歌曲数量
    int _count = 20;                        // 播放歌曲上限（控制重启）
};
static AppMusicPlayer app;

// 回调函数，用于处理ID3标签数据
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) 
{
    (void)cbData;
    if (isUnicode) {
        string += 2;
    }
    String outputString, id3_type; // 用于存储输出结果的 String 对象
    id3_type = type;
    while (*string) {
        char a = *(string++);
        if (isUnicode) {
            string++; // 如果是 Unicode，跳过第二个字节
        }
        outputString += a; // 将字符追加到 String 中
    }
    if (id3_type.equalsIgnoreCase((String)"title")) {
        app.info.title = outputString;
    }
    if (id3_type.equalsIgnoreCase((String)"album")) {
        app.info.album = outputString;
    }
    if (id3_type.equalsIgnoreCase((String)"performer")) {
        app.info.performer = outputString;
    }
    Serial.printf("ID3 callback for: %s = '%s'\n", type, outputString.c_str());
}

/**
 * 播放器退出函数
 */
static void player_exit(){
    pinMode(25, OUTPUT);
    pinMode(26, OUTPUT);
    hal.pref.putFloat("gain", app.gain);
    file_is_ok = false;
    int i = 0;
    while (app.titles[i] != NULL)
    {
        free(app.titles[i]);
        app.titles[i] = NULL;   
        ++i;
    }
}
/**
 * 播放器deepsleep函数
 */
static void player_deepsleep(){
    hal.pref.putFloat("gain", app.gain);
    if (app.need_deep_sleep)
        file_is_ok = true;
    else    
        file_is_ok = false;
}
/**
 * 播放器任务函数
 */
void player_loop(void *){
    while (1) {
        // 尝试获取信号量（等待直到成功）
        if (xSemaphoreTake(audio_control_sem, portMAX_DELAY) == pdTRUE) {
            // 安全操作解码器
            if (generator->isRunning()) {
                if (!generator->loop()) {
                    generator->stop();
                    app._play_end = true;
                    app.play_time_end = millis();
                    app.play_time_total = app.play_time_end - app.play_time_start;
                    vTaskDelete(NULL);
                    vTaskDelay(portMAX_DELAY);
                }
                xSemaphoreGive(audio_control_sem);  // 释放信号量
            }
            else
                delay(5);  // 避免意外情况
        }
        else
            delay(5);  // 避免意外情况
    }
}
/**
 * 设定应用显示状态
 */
void AppMusicPlayer::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
/**
 * 去除路径特定前缀函数
 * @param path 完整路径
 * @param prefix 特定前缀
 * @return 去除前缀后的路径
 */
/* const char* AppMusicPlayer::remove_path_prefix(const char* path, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t path_len = strlen(path);

    // 检查路径是否以指定前缀开头
    if (strncmp(path, prefix, prefix_len) == 0) {
        // 返回去除前缀后的路径
        return path + prefix_len;
    }
    // 如果路径不以指定前缀开头，则返回原始路径
    return path;
} */
/**
 * 文件选择函数，根据变量和preferences库读取数据判断文件选择方式
 */
void AppMusicPlayer::select_file(){
    if (file_is_ok){
        if (strstr(music_file, ".mp3") != nullptr) {
            // 示例：网络MP3流
            // if (strncmp(music_file, "/sd/", 4) == 0)
            // {
            //     in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
            //     pathStr = remove_path_prefix(music_file,"/sd");
            // }
            // else if (strncmp(music_file, "/littlefs/", 10) == 0) 
            // {
            //     in = new AudioFileSourceLittleFS(remove_path_prefix(music_file,"/littlefs"));
            //     pathStr = remove_path_prefix(music_file,"/littlefs");
            // }
            file_in(music_file);
        }
    }
    else
        music_file = GUI::fileDialog("选择音乐文件", false, "mp3", NULL);
    if (strstr(music_file, ".mp3") != nullptr) {
        // 示例：网络MP3流
        // if (strncmp(music_file, "/sd/", 4) == 0)
        // {
        //     in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
        //     pathStr = remove_path_prefix(music_file,"/sd");
        // }
        // else if (strncmp(music_file, "/littlefs/", 10) == 0) 
        // {
        //     in = new AudioFileSourceLittleFS(remove_path_prefix(music_file,"/littlefs"));
        //     pathStr = remove_path_prefix(music_file,"/littlefs");
        // }
        file_in(music_file);
    }
    // 解析目录
    int lastSlash = pathStr.lastIndexOf('/');
    currentDir = pathStr.substring(0, lastSlash);
    if (currentDir == "")
        is_root = true;
    filelist_ok = false;
    GUI::info_msgbox("提示", "正在创建音乐列表");
    bulid_music_list();
    if (file_is_ok && hal.pref.getBool(hal.get_char_sha_key("自动播放音乐列表"), false)){
        file_is_ok = false; 
        if (!hal.pref.getBool(hal.get_char_sha_key("循环播放"), false)){           
            if (hal.pref.getBool(hal.get_char_sha_key("随机播放"), false))
                currentSongIndex = random(0, maxSong - 1);
            else
                currentSongIndex++;
            currentSongIndex = (currentSongIndex < 0) ? maxSong - 1 : 
                               (currentSongIndex > maxSong - 1) ? 0 : currentSongIndex;
        }
        // sprintf(buf, "%s", ("/sd" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
        if (strncmp(music_file, "/sd/", 4) == 0)
            sprintf(buf, "%s", ("/sd" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
        else 
            sprintf(buf, "%s", ("/littlefs" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
        music_file = buf;
        //in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
        file_in(music_file);
    }
    sprintf(buf, "%s", music_file);
    music_file = buf;
}
/**
 * 文件输入函数，自动处理文件系统并传入AudioFileSource
 */
void AppMusicPlayer::file_in(const char* path){
    bool file_sd = false;
    const char* _path;
    if (strncmp(path, "/sd/", 4) == 0) {
        file_sd = true;
        sprintf(char_buf, "%s", remove_path_prefix(path,"/sd"));
        _path = char_buf;
        in = new AudioFileSourceSD(_path);
        in_littlefs = false;
    }
    else if (strncmp(path, "/littlefs/", 10) == 0) {
        file_sd = false;
        sprintf(char_buf, "%s", remove_path_prefix(path,"/littlefs"));
        _path = char_buf;
        in = new AudioFileSourceLittleFS(_path);
        in_littlefs = true;
    }
    pathStr = _path;
    if (!in->isOpen()){
        log_e("无法打开指定的文件（%s）以供播放,正在重试", path);
        if (file_sd)
            in = new AudioFileSourceSD(_path);
        else
            in = new AudioFileSourceLittleFS(_path);
        if (!in->isOpen() && file_sd){
            log_e("无法打开指定的文件（%s）以供播放，尝试重新挂载文件系统后播放", path);
            peripherals.tf_unload();
            delay(100);
            peripherals.load(PERIPHERALS_SD_BIT);
            in = new AudioFileSourceSD(_path);
            if (!in->isOpen()){
                log_e("无法打开指定的文件（%s）以供播放", path);
                need_deep_sleep = true;
            }
        }
    }
}
void AppMusicPlayer::next_song(bool next, bool btn) {
    const bool loopPlay = hal.pref.getBool(hal.get_char_sha_key("循环播放"), false);
    const bool autoPlay = hal.pref.getBool(hal.get_char_sha_key("自动播放音乐列表"), false);
    const bool randomPlay = hal.pref.getBool(hal.get_char_sha_key("随机播放"), false);
    
    if (!loopPlay && !autoPlay && !randomPlay && !btn) return;

    delete_playtask();
    free(in);

    if (!loopPlay && !autoPlay && !randomPlay){
        goto step1;
    }
    // 循环播放模式
    if (loopPlay) {
        if (btn)
            goto step1;
        else {
            file_in(music_file);
            player_set();
            begin_player_task();
            if (xSemaphoreTake(audio_control_sem, 100 / portTICK_PERIOD_MS) == pdFALSE){
                xSemaphoreGive(audio_control_sem);
            } else {
                xSemaphoreGive(audio_control_sem);
            }
            log_i("释放信号量");
            return;
        }
    }

    // 非自动播放模式
    if (!autoPlay) return;

    // 处理播放列表逻辑
    if (randomPlay) {
        currentSongIndex = random(0, maxSong - 1);
    } else {
        step1:
        // 统一处理前进/后退方向
        const int step = next ? 1 : -1;
        currentSongIndex += step;
        
        // 统一边界处理
        currentSongIndex = (currentSongIndex < 0) ? maxSong - 1 : 
                           (currentSongIndex > maxSong - 1) ? 0 : currentSongIndex;
    }

    // 路径生成
    if (strncmp(music_file, "/sd/", 4) == 0)
        sprintf(buf, "%s", ("/sd" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
    else 
        sprintf(buf, "%s", ("/littlefs" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
    music_file = buf;

    // 统一执行播放操作
    file_in(music_file);
    player_set();
    begin_player_task();
    if (xSemaphoreTake(audio_control_sem, 100 / portTICK_PERIOD_MS) == pdFALSE){
        xSemaphoreGive(audio_control_sem);
    } else {
        xSemaphoreGive(audio_control_sem);
    }
    log_i("释放信号量");
}
/**
 * 信号量函数，用于控制音频播放/暂停
 */
void AppMusicPlayer::sem(){
    if (xSemaphoreTake(audio_control_sem, 100 / portTICK_PERIOD_MS) == pdFALSE){
        xSemaphoreGive(audio_control_sem);
        log_i("释放信号量");
    } else {
        log_i("获取信号量");
    }
}
/**
 * 调用此函数以删除播放任务
 * @note 此函数不会判断任务是否存在，注意调用位置
 */
void AppMusicPlayer::delete_playtask(){                
    if (!_play_end){
        xSemaphoreTake(audio_control_sem,  100 / portTICK_PERIOD_MS);
        delay(100);
        generator->stop();
        vTaskDelete(player_loop_task_handle);
    }
}
/**
 * 创建音乐列表，从当前播放的文件夹在查找MP3文件并保存到titles数组中
 */
void AppMusicPlayer::bulid_music_list(){
    if (!filelist_ok){
        uint16_t song_count = 0;
        File root;
        if (is_root){
            if (!in_littlefs)
                root = SD.open("/");
            else
                root = LittleFS.open("/");
            Serial.printf("创建音乐列表,从根目录\n");
        }
        else{
            if (!in_littlefs)
                root = SD.open(currentDir);
            else
                root = LittleFS.open(currentDir);
            Serial.printf("创建音乐列表,从文件夹:%s\n", currentDir.c_str());
        }
        File dir = root.openNextFile();
        while (dir)
        {
            if (!dir.isDirectory() && String(dir.name()).endsWith(".mp3")) {
                song_count++;
            }
            dir.close();
            dir = root.openNextFile();
        }
        dir.close();
        root.close();
        int i = 0;
        while (titles[i] != NULL)
        {
            free(titles[i]);
            titles[i] = NULL;   
            ++i;
        }
        if (is_root){
            if (!in_littlefs)
                root = SD.open("/");
            else
                root = LittleFS.open("/");
        }
        else{
            if (!in_littlefs)
                root = SD.open(currentDir);
            else
                root = LittleFS.open(currentDir);
        }
        dir = root.openNextFile();
        maxSong = song_count;
        fileList = new menu_item[song_count + 2];
        memset(titles, 0, sizeof(titles));
        fileList[0].title = "返回";
        fileList[0].icon = NULL;
        i = 1;
        while (dir)
        {
            if (!dir.isDirectory() && String(dir.name()).endsWith(".mp3")) {
                titles[i - 1] = (char *)malloc(strlen(dir.name()) + 1);
                strcpy(titles[i - 1], dir.name());
                fileList[i].title = titles[i - 1];
                fileList[i].icon = NULL;
                i++;
                Serial.printf("%s\n", dir.name());
            }
            dir.close();
            dir = root.openNextFile();
        }
        dir.close();
        root.close();
        fileList[i].title = NULL;
        fileList[i].icon = NULL;
        filelist_ok = true;
    }
}
/**
 * 用于从音乐列表中选择音乐
 * @param play 播放任务是否运行，如果传入true，则需要调用此函数后调用file_in函数
 * @return 返回true表示选择了歌曲，false表示未选择歌曲
 */
bool AppMusicPlayer::music_list_menu(bool play){
    if (!filelist_ok)
        bulid_music_list();
    int res = GUI::menu("音乐列表", fileList);
    switch (res)
    {
    case 0:
        break;
    default:
        currentSongIndex = res - 1;
        currentSongIndex = (currentSongIndex < 0) ? maxSong - 1 : 
                           (currentSongIndex > maxSong - 1) ? 0 : currentSongIndex;
        if (strncmp(music_file, "/sd/", 4) == 0)
            sprintf(buf, "%s", ("/sd" + currentDir + "/" + (String)titles[res - 1]).c_str());
        else 
            sprintf(buf, "%s", ("/littlefs" + currentDir + "/" + (String)titles[res - 1]).c_str());
        music_file = buf;
        // in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
        if (!play)
            file_in(music_file);
        break;
    }
    if (res == 0)
        return false;
    else
        return true;
}
static const menu_select menu_player[] =
{
    {false,"返回"},
    {false,"退出"},
    {false,"播放/暂停"},
    {false,"从列表中选择"},
    {false,"选择文件"},
    {false,"音量设置"},
    {true, "循环播放"},
    {true, "随机播放"},
    {true, "自动播放音乐列表"},
    {true, "使用蜂鸣器输出"},
    {false,"重启间隔"},
    {false,NULL},
}; // 音乐播放器菜单
/**
 * 音乐播放器菜单函数，处理用户对应操作
 */
void AppMusicPlayer::player_menu(){
    bool end = false;
    while (!end)
    {
        int res = GUI::select_menu("菜单", menu_player);
        switch(res){
            case 0:
                end = true;
                break;
            case 1:
                end = true;
                _end = true;
                delete_playtask();
                free(in);
                if (nodac)
                    free(noDAC);
                else
                    free(output);
                free(id3);
                free(generator);
                appManager.goBack();
                break;
            case 2:
                sem();
                break;
            case 3:
                end = true;
                if (filelist_ok) {
                    if (!music_list_menu(true))
                        break;
                    delete_playtask();
                    play_time_total = 0;  
                    free(in);
                    file_in(music_file);
                } else {
                    delete_playtask();
                    free(in);
                    music_list_menu();
                }
                player_set();
                begin_player_task();
                sem();
                break;
            case 4:
                end = true;
                delete_playtask();
                play_time_total = 0;     
                free(in);
                select_file();   
                // free(output);
                // output= new AudioOutputI2S(0, 1);
                // output->SetGain(gain);
                // free(id3);
                // id3 = new AudioFileSourceID3(in);
                // id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
                // free(generator);
                // generator = new AudioGeneratorMP3();
                // generator->begin(id3, output);
                player_set();
                begin_player_task();
                sem();
                break;
            case 5:
                gain = (float)GUI::msgbox_number("0-400", 3, gain * 100.0) / 100.0;
                if (gain > 4.0) {
                    gain = 4.0;
                }
                if (gain < 0.0) {
                    gain = 0.0;
                }
                if (!nodac)
                    output->SetGain(gain);
                break;
            case 10:
                _count = GUI::msgbox_number("重启间隔 0-999", 3, _count);
                hal.pref.putInt("rst_count", _count);
                break;
            default:
                GUI::info_msgbox("警告", "非法的输入值");
                break;
        }
    }
}
/**
 * 启动音乐播放任务
 * @note 在完成播放后任务会删除自身
 */
void AppMusicPlayer::begin_player_task(){  
    _play_end = false;
    uint8_t core = xPortGetCoreID();
    Serial.printf("run in core %d\r\n", core);  
    if (core == 0)
        xTaskCreatePinnedToCore(player_loop, "player_loop_task", 8192, NULL, 5, &player_loop_task_handle, 1);
    else
        xTaskCreatePinnedToCore(player_loop, "player_loop_task", 8192, NULL, 5, &player_loop_task_handle, 0);
    play_time_start = millis();
}
/**
 * 屏幕信息显示函数
 */
void AppMusicPlayer::show_display(){
    display_time = millis();
    display.clearScreen();
    GUI::drawWindowsWithTitle("音乐播放器");
    u8g2Fonts.setCursor(3, 30);
    u8g2Fonts.printf("播放：%s", music_file);
    u8g2Fonts.setCursor(3, 45);
    if (_play_end)
        u8g2Fonts.printf("播放结束");
    else
        u8g2Fonts.printf("播放中...");
    u8g2Fonts.setCursor(64, 45);
    if (hal.pref.getBool(hal.get_char_sha_key("循环播放"), false)) {
        u8g2Fonts.printf("循环播放");
    } else if (hal.pref.getBool(hal.get_char_sha_key("自动播放音乐列表"), false)){
        if (hal.pref.getBool(hal.get_char_sha_key("随机播放"), false))
            u8g2Fonts.printf("列表随机播放");
        else
            u8g2Fonts.printf("列表顺序播放");
    } 
    uint32_t play_time = (millis() - play_time_start) / 1000;
    uint32_t total_time = play_time_total / 1000;
    u8g2Fonts.printf("  %02d:%02d/%02d:%02d  index:%d %d", play_time / 60, play_time % 60, total_time / 60, total_time % 60, currentSongIndex, play_count);
    u8g2Fonts.setCursor(3, 60);
    u8g2Fonts.printf("歌手:%s  ", info.performer.c_str());
    u8g2Fonts.setCursor(3, 75);
    u8g2Fonts.printf("标题:%s", info.title.c_str());
    u8g2Fonts.setCursor(3, 90);
    u8g2Fonts.printf("专辑:%s", info.album.c_str());
    u8g2Fonts.setCursor(3, 105);
    u8g2Fonts.printf("Gain:%.2f vcc:%dmV bat:%.3fV soc:%d%% soh:%d%%", gain, hal.VCC, hal.bat_info.voltage, hal.bat_info.soc, hal.bat_info.soh);
    u8g2Fonts.setCursor(3, 120);
    u8g2Fonts.printf("剩余堆内存：%.2fKB I:%dmA P:%dmW %dmAh", (float)ESP.getFreeHeap() / 1024.0, hal.bat_info.current.avg, hal.bat_info.power, hal.bat_info.capacity.remain);
    display.display(true);
}
/**
 * 启动音乐播放任务的前置准备函数
 * @note 此函数会释放解码器、id3标签解析、音频输出占用的资源
 */
void AppMusicPlayer::player_set(){
    play_count++;
    if (!hal.pref.getBool(hal.get_char_sha_key("循环播放"), false)) {
        info.album = "---";
        info.performer = "---";
        info.title = "---";
    }
    if (nodac){
        free(noDAC);
        noDAC = new AudioOutputI2SNoDAC(1);
        noDAC->SetGain(gain);
        free(id3);
        id3 = new AudioFileSourceID3(in);
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        free(generator);
        generator = new AudioGeneratorMP3();
        generator->begin(id3, noDAC);
    }else{
        free(output);
        output= new AudioOutputI2S(0, 1);
        output->SetGain(gain);
        free(id3);
        id3 = new AudioFileSourceID3(in);
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        free(generator);
        generator = new AudioGeneratorMP3();
        generator->begin(id3, output);
    }
}
/**
 * 音乐播放器主任务函数，由AppManager调用
 */
void AppMusicPlayer::setup(){
    pinMode(25, ANALOG);
    pinMode(26, ANALOG);
    nodac = hal.pref.getBool(hal.get_char_sha_key("使用蜂鸣器输出"), false);
    _count = hal.pref.getInt("rst_count", 20);
    hal.task_bat_info_update();
    gain = hal.pref.getFloat("gain", 0.3);
    exit = player_exit;
    deepsleep = player_deepsleep;
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 1;
    audioLogger = &Serial;
    audio_control_sem = xSemaphoreCreateBinary();  // 创建二进制信号量
    xSemaphoreGive(audio_control_sem);  // 初始化为可用状态
    select_file();
    generator = new AudioGeneratorMP3();
    if (nodac){
        noDAC = new AudioOutputI2SNoDAC(1);
        noDAC->SetGain(gain);
        id3 = new AudioFileSourceID3(in);
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        generator->begin(id3, noDAC);
    }else {
        output= new AudioOutputI2S(0, 1);
        output->SetGain(gain);
        id3 = new AudioFileSourceID3(in);
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        generator->begin(id3, output);
    }
    begin_player_task();
    show_display();
    _end = false;
    int display_count = 0;
    unsigned long wait_time = millis();
    while(!_end && !need_deep_sleep){
        if (hal.btnc.isPressing()){
            if (GUI::waitLongPress(PIN_BUTTONC)){
                player_menu();
                show_display();
                display_count++;
            }
            else {
                hal.task_bat_info_update();
                show_display();
                display_count++;
            }
        }
        if (hal.btnr.isPressing()) {
            if (GUI::waitLongPress(PIN_BUTTONR)) {
                next_song(true, true);
                int a = 0;
                while(hal.btnr.isPressing()){
                    delay(50);
                    if (a++ > 20) {
                        break;
                    }
                }
            } else {
                gain = gain + 0.1;
                if (gain > 4.0) {
                    gain = 4.0;
                }
                if (!nodac)
                    output->SetGain(gain);
            }
        }
        if (hal.btnl.isPressing()) {
            if (GUI::waitLongPress(PIN_BUTTONL)) {
                next_song(false, true);
                int a = 0;
                while(hal.btnl.isPressing()){
                    delay(50);
                    if (a++ > 20) {
                        break;
                    }
                }
            } else {
                gain = gain - 0.1;
                if (gain < 0.0) {
                    gain = 0.0;
                }
                if (!nodac)
                    output->SetGain(gain);
            }
        }
        if ((_count > 0 && play_count > _count && _play_end) || need_deep_sleep){
            file_is_ok = true;
            need_deep_sleep = true;
            GUI::info_msgbox("提示", "出现暂未解决的BUG,将会在重启后恢复播放");
            break;
        }/* 
        if (hal.pref.getBool(hal.get_char_sha_key("循环播放"), false) && _play_end) {
            free(in);
            // if (strncmp(music_file, "/sd/", 4) == 0)
            // {
            //     in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
            // }
            // else if (strncmp(music_file, "/littlefs/", 10) == 0) 
            // {
            //     in = new AudioFileSourceLittleFS(remove_path_prefix(music_file,"/littlefs"));
            // }
            file_in(music_file);
            // free(output);
            // output= new AudioOutputI2S(0, 1);
            // output->SetGain(gain);
            // free(id3);
            // id3 = new AudioFileSourceID3(in);
            // id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
            // free(generator);
            // generator = new AudioGeneratorMP3();
            // generator->begin(id3, output);
            player_set();
            begin_player_task();
            sem();
        }
        if (!hal.pref.getBool(hal.get_char_sha_key("循环播放"), false) && _play_end && hal.pref.getBool(hal.get_char_sha_key("自动播放音乐列表"), false)){
            free(in);
            if (hal.pref.getBool(hal.get_char_sha_key("随机播放"), false))
                currentSongIndex = random(0, maxSong);
            else
                currentSongIndex++;
            if (currentSongIndex > maxSong) {
                currentSongIndex = 0;
            }
            sprintf(buf, "%s", ("/sd" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
            music_file = buf;
            // in = new AudioFileSourceSD(remove_path_prefix(music_file,"/sd"));
            file_in(music_file);
            player_set();
            begin_player_task();
            sem();
        } */
        if (_play_end) {
            next_song();
            show_display();
            delay(333);
        } else
            wait_time = millis();
        if (millis() - display_time > 3000) {
            if (display_count > 15) {
                hal.task_bat_info_update();
                display_count = 0;
                display.clearScreen();
                display.display();
                show_display();
            }
            else
                show_display();
            display_count++;
        }
        if (millis() - wait_time > 30000){
            hal.wait_input();
            wait_time = millis();
        }
        delay(50);
    }    
}

