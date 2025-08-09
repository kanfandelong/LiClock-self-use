#pragma GCC optimize ("O3")

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
    
typedef struct {
    unsigned long timeMs;
    String text;
}LyricLine;                     // 歌词行结构体
    
SemaphoreHandle_t audio_control_sem = NULL;  // 音频任务的信号量
TaskHandle_t player_loop_task_handle = NULL; // 音频任务句柄
AudioFileSource *in = nullptr;               // 音频文件源
AudioFileSourceID3 *id3;                     // ID3信息解码处理
AudioGeneratorMP3 *generator;                // MP3解码器
AudioOutputI2S *output;                      // I2S输出
AudioOutputI2SNoDAC *noDAC;                  // I2S输出
// 以下变量保存至RTC内存，避免deepsleep后丢失
RTC_DATA_ATTR uint16_t currentSongIndex = 0; // 当前播放索引（音乐列表数组位置）
RTC_DATA_ATTR char buf[512] = "";            // 实际存储当前播放文件路径字符串
RTC_DATA_ATTR const char *music_file = NULL; // 当前播放文件的指针
RTC_DATA_ATTR bool is_ran = false;           // 用于判断播放器的启动状态（初次运行/已经运行过）

class AppMusicPlayer : public AppBase
{
private:
    /* data */
public:
    AppMusicPlayer()
    {
        name = "musicplayer";
        title = "音乐";
        description = "暂无";
        noDefaultEvent = true;
        peripherals_requested = PERIPHERALS_SD_BIT;
        image = APP_MusicPlayer_bits;
    }
    void set();
    // const char* remove_path_prefix(const char* path, const char* prefix);
    String getLyricPath(const char* musicPath);
    void loadLyrics(const char* path);
    void getLyric(unsigned long currentTime);
    int  findSongIndexInFileList();
    void select_file(bool user = false);
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
    String currentDir = "/";                // 当前歌曲目录
    String pathStr;                         // 当前歌曲位于的目录
    bool is_root = false;                   // 是否是根目录
    bool _play_end = false;                 // 播放完成标志
    bool filelist_ok = false;               // 歌曲列表就绪标志
    uint16_t maxSong = 0;                   // 歌曲总数
    char *titles[256] = {nullptr};          // 歌曲名内存指针数组,存储歌曲名所在的内存位置
    char char_buf[512];                     // 字符串拼接缓存
    menu_item *fileList = nullptr;          // 歌曲菜单数组

    unsigned long play_time_start;          // 播放开始时间
    unsigned long play_time_end;            // 播放结束时间
    unsigned long play_stop_time = 0;       // 播放停止时间
    unsigned long play_time_total = 0;      // 播放总时间
    unsigned long display_time = millis();  // 屏幕上次刷新时间

    id3_info info;                          // 歌曲ID3信息
    bool _end;                              // 播放器主任务函数while循环停止标志
    bool user_stop = false;                 // 用户停止播放标志
    bool nodac = false;                     // 无DAC标志
    bool in_littlefs = false;               // 文件是否位于LittleFS
    bool need_deep_sleep = false;           // 是否需要进入deepsleep
    bool lrcintf  = false;                  // 歌词位于的文件系统
    bool lrcisload = false;                 // 歌词加载状态
    bool app_exit = false;                  // 退出标志
    char currentLyric[64];                  // 当前显示的歌词
    float gain = 0.3;                       // 音频输出增益（音量）
    int play_count = 1;                     // 播放歌曲数量
    int _count = 20;                        // 播放歌曲上限（控制重启）
    int display_count = 0;                  // 屏幕刷新次数
    int currentLyricIndex = 0;              // 当前显示的歌词索引
    int lastLyricIndex = 0;                 // 上次显示的歌词索引
    int _lrcoffset = 0;                     // 歌词显示时间补偿
    int totalLyricLines = 0;                // 歌词总行数
    unsigned long lastLyricUpdate = 0;      // 上次歌词更新时间
    LyricLine* lyricArray = nullptr;        // 使用动态数组存储歌词
};
static AppMusicPlayer app;                  // 创建App对象

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
    is_ran = true;
    pinMode(25, OUTPUT);
    pinMode(26, OUTPUT);
    hal.pref.putFloat("gain", app.gain);
    int i = 0;
    while (app.titles[i] != nullptr)
    {
        free(app.titles[i]);
        app.titles[i] = nullptr;   
        ++i;
    }
    if (app.lyricArray != nullptr){
        delete[] app.lyricArray;
    }
    if (app.fileList != nullptr){
        delete[] app.fileList;
    }
}
/**
 * 播放器deepsleep函数
 */
static void player_deepsleep(){
    hal.pref.putFloat("gain", app.gain);
    is_ran = true;
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
                    player_loop_task_handle = NULL;
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
        delay(1); // 释放cpu
    }
}
/**
 * 设定应用显示状态
 */
void AppMusicPlayer::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
    log_i("APP %s,版本:%s  构建日期:%s %s", name, "0.0.7", __DATE__, __TIME__); 
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
 * @brief 根据音乐文件路径生成对应的歌词文件（.lrc）路径
 *
 * 该函数用于将音乐文件的路径转换为对应的歌词文件路径。
 * - 首先去除路径中的文件系统前缀（如 "/sd" 或 "/littlefs"）
 * - 然后提取目录路径和文件名，并将文件扩展名替换为 ".lrc"
 * - 最终拼接出歌词文件的完整路径，格式为 "[目录]/lrc/[文件名].lrc"
 *
 * 同时设置成员变量 `lrcintf` 来标识歌词文件应从哪个文件系统中读取：
 * - true 表示歌词位于 SD 卡文件系统（/sd）
 * - false 表示歌词位于 LittleFS 文件系统（/littlefs）
 *
 * @param musicPath 音乐文件的完整路径字符串（const char*）
 * @return 返回生成的歌词文件路径（String 类型）
 */
String AppMusicPlayer::getLyricPath(const char* musicPath) {
    String musicPathStr(musicPath);
    
    // 移除文件系统前缀（如/sd或/littlefs）
    String basePath;
    if (musicPathStr.startsWith("/sd")) {
        basePath = musicPathStr.substring(3); // 去除"/sd"
        lrcintf = true;
    } else if (musicPathStr.startsWith("/littlefs")) {
        basePath = musicPathStr.substring(9); // 去除"/littlefs"
        lrcintf = false;
    } else {
        basePath = musicPathStr;
    }

    // 分离目录和文件名
    int lastSlash = basePath.lastIndexOf('/');
    String dir = basePath.substring(0, lastSlash);
    String filename = basePath.substring(lastSlash + 1);

    // 替换扩展名为.lrc
    int dotIndex = filename.lastIndexOf('.');
    if (dotIndex != -1) {
        filename = filename.substring(0, dotIndex) + ".lrc";
    } else {
        filename += ".lrc";
    }

    // 拼接为完整路径
    String lrcPath;
    lrcPath = dir + "/lrc/" + filename;

    return lrcPath;
}

/**
 * @brief 统计指定歌词文件中的有效歌词时间行数量
 *
 * 该函数用于统计 `.lrc` 歌词文件中以左方括号 '[' 开头的行数，
 * 这些行通常表示带有时间戳的歌词内容，用于后续内存分配和歌词加载。
 *
 * - 输入路径为音乐文件路径，会自动将扩展名替换为 `.lrc`
 * - 使用 `SD` 或 `LittleFS` 文件系统打开歌词文件
 * - 每读取一行就判断是否为有效歌词时间行（即以 '[' 开头）
 *
 * @param path 音乐文件路径（用于生成对应的歌词文件路径）
 * @return 返回有效歌词时间行的数量；若文件无法打开则返回 -1
 */
int countLyricLines(const char* path) {
    int count = 0;
    String lrcPath = path;
    lrcPath.replace(".mp3", ".lrc");
    
    File file; 
    if (app.lrcintf)
        file = SD.open(lrcPath, "r");
    else
        file = LittleFS.open(lrcPath, "r");
    if (!file) return -1;

    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("[")) {
            count++;
        }
    }
    file.close();
    return count;
}

/**
 * @brief 加载并解析指定路径的 `.lrc` 歌词文件内容
 *
 * 该函数负责加载和解析与当前播放音乐对应的歌词文件（`.lrc`），
 * 包括以下主要步骤：
 * - 清理已存在的歌词数据
 * - 获取歌词文件路径
 * - 统计歌词行数并分配内存
 * - 打开歌词文件并逐行解析时间戳与歌词文本
 * - 按照时间顺序存储至预分配的 `lyricArray` 数组中
 *
 * 支持两种文件系统：
 * - SD 卡（通过 `SD.open()`）
 * - LittleFS（通过 `LittleFS.open()`）
 *
 * 解析出的时间戳将被统一转换为毫秒格式，便于后续播放时同步显示。
 *
 * @param path 音乐文件路径（用于生成对应的歌词文件路径）
 * @note 该函数会设置成员变量 `lrcisload = true` 表示加载成功
 */
void AppMusicPlayer::loadLyrics(const char* path) {

    lrcisload = false;
    unsigned long loadlrcbegin = millis();
    if (lyricArray != nullptr) {
        delete[] lyricArray;
        lyricArray = nullptr;
    }
    String lrcPath = getLyricPath(path);
    log_i("期望歌词路径：%s", lrcPath.c_str());
    totalLyricLines = countLyricLines(lrcPath.c_str());
    if (totalLyricLines == -1) {
        log_w("歌词文件不存在,中止加载操作");
        return;
    }

    // 预先分配内存
    lyricArray = new LyricLine[totalLyricLines];

    if (lyricArray == nullptr) {
        log_w("内存分配失败,中止加载操作");
        return;
    }
    
    File file; 
    if (lrcintf)
        file = SD.open(lrcPath, "r");
    else
        file = LittleFS.open(lrcPath, "r");

    if (!file) {
        log_w("歌词文件打开发生意外错误,中止加载操作");
        return;
    }
    log_i("开始加载歌词，歌词行数：%d", totalLyricLines);

    int index = 0;
    String line;
    String timeStr;
    String text;
    String msStr;
    while (file.available() && index < totalLyricLines) {
        line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("[")) {
            int closeBracket = line.indexOf(']');
            if (closeBracket != -1) {
                timeStr = line.substring(1, closeBracket);
                text = line.substring(closeBracket + 1);

                // 解析时间戳
                int colon = timeStr.indexOf(':');
                int dot = timeStr.indexOf('.');
                if (colon == -1 || dot == -1) continue;

                int minutes = timeStr.substring(0, colon).toInt();
                int seconds = timeStr.substring(colon + 1, dot).toInt();
                msStr = timeStr.substring(dot + 1);
                while (msStr.length() < 3) msStr += "0";
                int milliseconds = msStr.substring(0, 3).toInt();
                unsigned long timestamp = minutes * 60000 + seconds * 1000 + milliseconds;

                // 存储到预分配数组
                lyricArray[index].timeMs = timestamp;
                lyricArray[index].text = text;
                index++;
            }
        }
    }
    file.close();
    lrcisload = true;
    currentLyricIndex = 0;
    lastLyricIndex = 0;
    log_i("歌词加载完成，所有操作耗时：%lums", millis() - loadlrcbegin);
}
/**
 * @brief 根据当前播放时间获取对应的歌词文本
 *
 * 该函数用于根据传入的播放时间 `currentTime` 查找并返回
 * 当前应显示的歌词内容，实现播放进度与歌词的同步。
 *
 * 主要流程：
 * - 从当前索引开始查找第一个大于当前播放时间的歌词项
 * - 回退一个索引以确保获得的是当前应显示的歌词
 * - 将歌词文本复制到显示缓冲区 `currentLyric`
 *
 * @param currentTime 当前播放时间（单位：毫秒）
 * @note 显示歌词文本最大长度为 63 字节（由 [snprintf](file://C:\Users\admin\.platformio\packages\toolchain-xtensa-esp32\xtensa-esp32-elf\sys-include\stdio.h#L266) 控制）
 */
void AppMusicPlayer::getLyric(unsigned long currentTime) {
    // 从当前索引开始查找
    while (currentLyricIndex < totalLyricLines) {
        if (currentTime >= lyricArray[currentLyricIndex].timeMs) {
            currentLyricIndex++;
        } else {
            break;
        }
    }

    // 回退到正确位置
    if (currentLyricIndex > 0) {
        currentLyricIndex--;
    }

    snprintf(currentLyric, 64, "%s", lyricArray[currentLyricIndex].text.c_str());
}

int AppMusicPlayer::findSongIndexInFileList() {
    if (music_file == NULL || !filelist_ok) {
        return -1; // 参数无效
    }

    // 提取 music_file 的文件名部分
    String filename = String(music_file);
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash != -1) {
        filename = filename.substring(lastSlash + 1); // 取得文件名
    }

    // 遍历 title 查找匹配项
    for (int i = 0; titles[i] != NULL; ++i) { // 跳过 "返回" 项（索引0）
        if (filename.equals(String(titles[i]))) {
            return i; // 找到匹配项，返回索引
        }
    }

    log_w("未在歌曲列表中找到“%s”的找到匹配项", filename);
    return -1; // 未找到匹配项
}

/**
 * 文件选择函数，根据变量和preferences库读取数据判断文件选择方式
 */
void AppMusicPlayer::select_file(bool user){
    if (is_ran && music_file != NULL && !user){
        if (strstr(music_file, ".mp3") != nullptr) {
            if (strncmp(music_file, "/sd/", 4) == 0){
               if(!SD.exists(remove_path_prefix(music_file,"/sd")))
                   goto select;
            }
            else if (strncmp(music_file, "/littlefs/", 10) == 0) {
                if (!LittleFS.exists(remove_path_prefix(music_file,"/littlefs")))
                    goto select;
            }
            file_in(music_file);
        }
        else {
select:
            music_file = NULL;
            while (music_file == NULL) { 
                music_file = GUI::fileDialog("选择音乐文件", false, "mp3", NULL, currentDir);
            }
            file_in(music_file);
        }
    }
    else {
        music_file = NULL;
        while (music_file == NULL) { 
            music_file = GUI::fileDialog("选择音乐文件", false, "mp3", NULL, currentDir);
        }
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
    sprintf(buf, "%s", music_file); //复制歌曲路径到缓冲区
    music_file = buf;               //将歌曲路径指向缓冲器
    int index = findSongIndexInFileList();
    if (index == -1)
        currentSongIndex = 1;
    else
        currentSongIndex = index;
}
/**
 * 文件输入函数，自动处理文件系统并传入AudioFileSource
 */
void AppMusicPlayer::file_in(const char* path){
    if (hal.pref.getBool(hal.get_char_sha_key("lrc歌词"), false)){
        loadLyrics(path);
    }
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
    
    log_i("模式状态：%s %s %s %s", loopPlay ? "循环播放" : "非循环播放", autoPlay ? "自动播放" : "非自动播放", randomPlay ? "随机播放" : "非随机播放", btn ? "按键触发" : "非按键触发");
    
    if (!loopPlay && !autoPlay && !randomPlay && !btn) return;

    delete_playtask();
    free(in);

    // 循环播放模式
    if (loopPlay && !btn) {
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
    } else {
        // 统一处理前进/后退方向
        const int step = next ? 1 : -1;
        currentSongIndex += step;
        
        // 统一边界处理
        currentSongIndex = (currentSongIndex < 0) ? maxSong - 1 : 
                           (currentSongIndex > maxSong - 1) ? 0 : currentSongIndex;
    }

    // 处理播放列表逻辑
    if (randomPlay && !btn) {
        uint16_t random_val = (uint16_t)random(0, maxSong - 1);
        uint8_t a = 5;
        while ((currentSongIndex != random_val) && (a > 0)){
            random_val = (uint16_t)random(0, maxSong - 1);
            a--;
        }
        currentSongIndex = random_val;
        log_i("随机索引：%u", currentSongIndex);
    } else {
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
        xSemaphoreTake(audio_control_sem, 200 / portTICK_PERIOD_MS);
        delay(100);
        if (player_loop_task_handle != NULL){
            generator->stop();
            vTaskDelete(player_loop_task_handle);
            player_loop_task_handle = NULL;
        }
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
        if (fileList != nullptr){
            delete[] fileList;
            fileList = nullptr;
        }
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
    {false,"播放列表"},
    {false,"选择文件"},
    {false,"音量设置"},
    {true, "循环播放"},
    {true, "随机播放"},
    {true, "自动播放音乐列表"},
    {true, "使用25/26/0输出"},
    {true, "使用蜂鸣器输出"},
    {true, "audio_pll"},
    {true, "lrc歌词"},
    {false,"歌词显示补偿"},
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
                app_exit = true;
                break;
            case 2:
                if(!_play_end){
                    if (user_stop){
                        user_stop = false;
                        play_stop_time = millis() - play_stop_time;
                        sem();
                    }
                    else{
                        user_stop = true;
                        play_stop_time = millis();
                        sem();
                    }
                }
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
                select_file(true);   
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
            case 13:
                _lrcoffset = GUI::msgbox_number("单位ms", 4, _lrcoffset);
                hal.pref.putInt("_lrcoffset", _lrcoffset);
                break;
            case 14:
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
    play_stop_time = 0;
    uint8_t core = xPortGetCoreID();
    Serial.printf("run in core %d\r\n", core);  
    if (core == 0)
        xTaskCreatePinnedToCore(player_loop, "play_task", 8192, NULL, 5, &player_loop_task_handle, 1);
    else
        xTaskCreatePinnedToCore(player_loop, "play_task", 8192, NULL, 5, &player_loop_task_handle, 0);
    play_time_start = millis();
}
/**
 * 屏幕信息显示函数
 */
void AppMusicPlayer::show_display(){
    display.clearScreen();
    bool lrcupdate = false;
    if (lrcisload && !user_stop) {
        GUI::drawWindowsWithTitle(titles[currentSongIndex]);
        u8g2Fonts.setCursor(3, 30);
        getLyric(millis() - play_time_start - _lrcoffset - play_stop_time);
        u8g2Fonts.print(currentLyric);
        if (currentLyricIndex != lastLyricIndex) {
            lastLyricIndex = currentLyricIndex;
            lrcupdate = true;
            // log_i("%s", lyricArray[currentLyricIndex].text.c_str());
        }
        else 
            lrcupdate = false;
    }
    else {
        GUI::drawWindowsWithTitle("音乐播放器");
        u8g2Fonts.setCursor(3, 30);
        u8g2Fonts.print("无歌词"); 
    }
    // 电池
    if (hal.pref.getBool(hal.get_char_sha_key("精准电量显示"),false) && hal.VCC < 4300 && !hal.isCharging){
        display.drawXBitmap(274, 0, getBatteryIcon(true), 20, 16, 0);
        display.fillRect(277, 6, getBatterysoc(), 4, GxEPD_BLACK);
    }else
        display.drawXBitmap(274, 0, getBatteryIcon(), 20, 16, 0);

    u8g2Fonts.setCursor(2, 12);
    u8g2Fonts.printf("%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);

    u8g2Fonts.setCursor(3, 45);
    u8g2Fonts.printf("标题:%s", info.title.c_str());
    u8g2Fonts.setCursor(3, 60);
    u8g2Fonts.printf("歌手:%s", info.performer.c_str());
    u8g2Fonts.setCursor(3, 75);
    u8g2Fonts.printf("专辑:%s", info.album.c_str());
    u8g2Fonts.setCursor(3, 90);
    if (_play_end)
        u8g2Fonts.printf("播放结束 ");
    else
        u8g2Fonts.printf("播放中...");
    u8g2Fonts.setCursor(64, 90);
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
    if (!hal.pref.getBool(hal.get_char_sha_key("循环播放"), false))
        total_time = 0;
    u8g2Fonts.printf("  %02d:%02d/%02d:%02d  index:%d %d", play_time / 60, play_time % 60, total_time / 60, total_time % 60, currentSongIndex, play_count);
    u8g2Fonts.setCursor(3, 105);
    u8g2Fonts.printf("Gain:%.2f vcc:%dmV bat:%.3fV soc:%d%% soh:%d%%", gain, hal.VCC, hal.bat_info.voltage, hal.bat_info.soc, hal.bat_info.soh);
    u8g2Fonts.setCursor(3, 120);
    u8g2Fonts.printf("剩余堆内存：%.2fKB I:%dmA P:%dmW %dmAh", (float)ESP.getFreeHeap() / 1024.0, hal.bat_info.current.avg, hal.bat_info.power, hal.bat_info.capacity.remain);
    int max_count = (lrcisload ? 45 : 15);              // 控制全刷间隔，避免全刷影响歌词更新
    if (millis() - display_time > 1000 || lrcupdate) {  // 如果有歌词更新或屏幕刷新时间间隔超过1秒则刷新屏幕
        if (display_count > max_count) {
            display_count = 0;
            display.display();
        }
        else {
            display.display(true);
        }
        display_count++;
        display_time = millis();
    }
}
/**
 * 启动音乐播放任务的前置准备函数
 * @note 此函数会释放解码器、id3标签解析、音频输出占用的资源
 */
void AppMusicPlayer::player_set(){
    play_count++;
    info.album = "---";
    info.performer = "---";
    info.title = "---";
    if (nodac){
        free(noDAC);
        noDAC = new AudioOutputI2SNoDAC(0);
        noDAC->SetGain(gain);
        free(id3);
        id3 = new AudioFileSourceID3(in);
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        free(generator);
        generator = new AudioGeneratorMP3();
        generator->begin(id3, noDAC);
    }else{
        free(output);
        if (hal.pref.getBool(hal.get_char_sha_key("使用25/26/0输出"))){
            output = new AudioOutputI2S(0, 0, 8, hal.pref.getBool(hal.get_char_sha_key("audio_pll"), false));
            output->SetPinout(0, 25, 26);
            output->SetMclk(false);
        }
        else
            output= new AudioOutputI2S(0, 1, 8, hal.pref.getBool(hal.get_char_sha_key("audio_pll"), false));
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
    hal.cheak_freq(160);
    pinMode(25, ANALOG);
    pinMode(26, ANALOG);
    nodac = hal.pref.getBool(hal.get_char_sha_key("使用蜂鸣器输出"), false);
    _count = hal.pref.getInt("rst_count", 20);
    gain = hal.pref.getFloat("gain", 0.3);
    _lrcoffset = hal.pref.getInt("_lrcoffset", -50);
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
        noDAC = new AudioOutputI2SNoDAC(0);
        noDAC->SetGain(gain);
        id3 = new AudioFileSourceID3(in);
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        generator->begin(id3, noDAC);
    }else {
        if (hal.pref.getBool(hal.get_char_sha_key("使用25/26/0输出"))){
            output = new AudioOutputI2S(0, 0, 8, hal.pref.getBool(hal.get_char_sha_key("audio_pll"), false));
            output->SetPinout(0, 25, 26);
            output->SetMclk(false);
        }
        else
            output= new AudioOutputI2S(0, 1, 8, hal.pref.getBool(hal.get_char_sha_key("audio_pll"), false));
        output->SetGain(gain);
        id3 = new AudioFileSourceID3(in);
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        generator->begin(id3, output);
    }
    begin_player_task();
    show_display();
    _end = false;
    unsigned long wait_time = millis();
    while(!_end && !need_deep_sleep){
        if (hal.btnc.isPressing()){
            if (GUI::waitLongPress(PIN_BUTTONC)){
                player_menu();
                show_display();
            }
            else {
                show_display();
            }
        }
        if (app_exit)
            return;
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
            need_deep_sleep = true;
            GUI::info_msgbox("提示", "出现暂未解决的BUG,将会在重启后恢复播放");
            break;
        }
        if (_play_end) {
            delay(100);
            next_song();
            show_display();
            delay(333);
        } else
            wait_time = millis();
        if (millis() - display_time > (lrcisload ? 100 : 2000)) { // 如果歌词加载成功，则每100ms检查一次歌词
            show_display();
        }
        if ((millis() - wait_time > 30000) && _play_end){
            hal.wait_input();
            wait_time = millis();
        }
        delay(40);
    }    
}

