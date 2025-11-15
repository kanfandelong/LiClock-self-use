#include "AppManager.h"
#include "ESP8266Audio.h"

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
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // 音乐播放器图标

static const uint8_t pause_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x78, 0x00, 0x00, 0xf8, 0x01, 0x00, 0xf8, 0x07, 0x00, 0xf8, 0x1f, 0x00,
    0xf8, 0x7f, 0x00, 0xf8, 0xff, 0x01, 0xf8, 0xff, 0x07, 0xf8, 0xff, 0x1f,
    0xf8, 0xff, 0x1f, 0xf8, 0xff, 0x1f, 0xf8, 0xff, 0x07, 0xf8, 0xff, 0x01,
    0xf8, 0x7f, 0x00, 0xf8, 0x1f, 0x00, 0xf8, 0x07, 0x00, 0xf8, 0x01, 0x00,
    0xf8, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t play_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x81, 0x0f, 0xf8, 0xc3, 0x1f,
    0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f,
    0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f,
    0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f,
    0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f, 0xf8, 0xc3, 0x1f,
    0xf8, 0xc3, 0x1f, 0xf0, 0x81, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

typedef struct
{
    String album = "---";     // 专辑
    String performer = "---"; // 歌手
    String title = "---";     // 标题
    uint32_t tlen = 0;
} id3_info; // ID3信息结构体

typedef struct
{
    unsigned long timeMs = 0;
    String text;
} LyricLine; // 歌词行结构体

static const char play_generator_str[][32] = {"UNKONWN_Generator", "MP3_Generator", "Flac_Generator", "AAC_Generator", "OPUS_Generator", "WAV_Generator"};

typedef enum
{
    UNKONWN_Generator,
    MP3_Generator,
    Flac_Generator,
    AAC_Generator,
    OPUS_Generator,
    WAV_Generator,
} generator_t;

SemaphoreHandle_t audio_control_sem = NULL;  // 音频任务的信号量
TaskHandle_t player_loop_task_handle = NULL; // 音频任务句柄
AudioFileSource *in = nullptr;               // 音频文件源
AudioFileSourceID3 *id3 = nullptr;           // ID3信息解码处理
AudioGenerator *generator = nullptr;
AudioGeneratorMP3 *mp3_generator = nullptr;   // MP3解码器
AudioGeneratorFLAC *flac_generator = nullptr; // flac解码器
AudioGeneratorAAC *aac_generator = nullptr;
AudioGeneratorOpus *opus_generator = nullptr;
AudioGeneratorWAV *wav_generator = nullptr;
AudioOutput *output;
AudioOutputI2S *i2s_output; // I2S输出
AudioOutputI2SNoDAC *noDAC; // I2S输出
// 以下变量保存至RTC内存，避免deepsleep后丢失
RTC_DATA_ATTR int32_t currentSongIndex = 0;  // 当前播放索引（音乐列表数组位置）
RTC_DATA_ATTR char buf[512] = "";            // 实际存储当前播放文件路径字符串
RTC_DATA_ATTR const char *music_file = NULL; // 当前播放文件的指针
// RTC_DATA_ATTR bool is_ran = false;           // 用于判断播放器的启动状态（初次运行/已经运行过）

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
    String getLyricPath(const char *musicPath);
    void loadLyrics(const char *path);
    void getLyric(unsigned long currentTime);
    int findSongIndexInFileList();
    void select_file(bool user = false);
    void file_in(const char *path);
    void next_song(bool next = true, bool btn = false);
    void sem();
    void delete_playtask();
    void bulid_music_list();
    bool music_list_menu(bool play = false);
    void player_menu();
    void player_set_menu();
    void begin_player_task();
    void show_display();
    void show_display_debug();
    void show_display_mormal();
    bool generator_set(const char *path, AudioFileSource *source, AudioOutput *out);
    bool player_set();
    void setup();
    String currentDir = "/";       // 当前歌曲目录
    String pathStr;                // 当前歌曲位于的目录
    bool is_root = false;          // 是否是根目录
    bool _play_end = false;        // 播放完成标志
    bool filelist_ok = false;      // 歌曲列表就绪标志
    uint16_t maxSong = 0;          // 歌曲总数
    char **titles = nullptr;       // 歌曲名内存指针数组指针,存储歌曲名所在的内存位置
    char char_buf[512];            // 字符串拼接缓存
    menu_item *fileList = nullptr; // 歌曲菜单数组

    unsigned long play_time_start;         // 播放开始时间
    unsigned long play_time_end;           // 播放结束时间
    unsigned long play_stop_time = 0;      // 播放停止时间
    unsigned long play_time_total = 0;     // 播放总时间
    unsigned long display_time = millis(); // 屏幕上次刷新时间

    id3_info info; // 歌曲ID3信息
    generator_t play_generator = UNKONWN_Generator;
    bool _end;                    // 播放器主任务函数while循环停止标志
    bool user_stop = false;       // 用户停止播放标志
    bool nodac = false;           // 无DAC标志
    bool in_littlefs = false;     // 文件是否位于LittleFS
    bool need_deep_sleep = false; // 是否需要进入deepsleep
    bool lrcintf = false;         // 歌词位于的文件系统
    bool lrcisload = false;       // 歌词加载状态
    bool app_exit = false;        // 退出标志
    bool bits_per_chan = false;
    bool display_debug_mode = false;
    char currentLyric[3][80];  // 当前显示的歌词
    float gain = 0.3;          // 音频输出增益（音量）
    int play_count = 1;        // 播放歌曲数量
    int _count = 20;           // 播放歌曲上限（控制重启）
    int display_count = 0;     // 屏幕刷新次数
    int currentLyricIndex = 0; // 当前显示的歌词索引
    int lastLyricIndex = 0;    // 上次显示的歌词索引
    int _lrcoffset = 0;        // 歌词显示时间补偿
    int totalLyricLines = 0;   // 歌词总行数
    int apll = 0;
    unsigned long lastLyricUpdate = 0; // 上次歌词更新时间
    LyricLine *lyricArray = nullptr;   // 使用动态数组存储歌词
};
static AppMusicPlayer app; // 创建App对象

// 辅助函数：UTF-16到UTF-8转换
String utf16ToUtf8(const uint8_t *utf16Str, size_t length, bool hasBOM = true)
{
    String result;
    const uint8_t *ptr = utf16Str;
    size_t pos = 0;

    // 检查字节顺序标记(BOM)
    bool bigEndian = true; // 默认为大端序
    if (hasBOM && length >= 2)
    {
        if (ptr[0] == 0xFF && ptr[1] == 0xFE)
        {
            bigEndian = false; // 小端序 (FF FE)
            pos += 2;
        }
        else if (ptr[0] == 0xFE && ptr[1] == 0xFF)
        {
            bigEndian = true; // 大端序 (FE FF)
            pos += 2;
        }
    }

    // 处理UTF-16编码
    while (pos + 1 < length)
    {
        uint16_t codePoint;

        if (bigEndian)
        {
            codePoint = (ptr[pos] << 8) | ptr[pos + 1];
        }
        else
        {
            codePoint = (ptr[pos + 1] << 8) | ptr[pos];
        }
        pos += 2;

        if (codePoint == 0)
            break; // 字符串结束

        // 转换为UTF-8
        if (codePoint <= 0x7F)
        {
            // 单字节UTF-8
            result += static_cast<char>(codePoint);
        }
        else if (codePoint <= 0x7FF)
        {
            // 双字节UTF-8
            result += static_cast<char>(0xC0 | (codePoint >> 6));
            result += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else if (codePoint >= 0xD800 && codePoint <= 0xDBFF && pos + 1 < length)
        {
            // 处理UTF-16代理对 (高代理)
            uint16_t lowSurrogate;
            if (bigEndian)
            {
                lowSurrogate = (ptr[pos] << 8) | ptr[pos + 1];
            }
            else
            {
                lowSurrogate = (ptr[pos + 1] << 8) | ptr[pos];
            }

            if (lowSurrogate >= 0xDC00 && lowSurrogate <= 0xDFFF)
            {
                // 有效的低代理
                pos += 2;
                uint32_t fullCodePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowSurrogate - 0xDC00);

                // 转换为4字节UTF-8
                result += static_cast<char>(0xF0 | (fullCodePoint >> 18));
                result += static_cast<char>(0x80 | ((fullCodePoint >> 12) & 0x3F));
                result += static_cast<char>(0x80 | ((fullCodePoint >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (fullCodePoint & 0x3F));
            }
            else
            {
                // 无效的代理对，跳过
                result += '?';
            }
        }
        else
        {
            // 三字节UTF-8 (基本多文种平面字符)
            result += static_cast<char>(0xE0 | (codePoint >> 12));
            result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
    }

    return result;
}

// 辅助函数：检测UTF-8编码
bool isUtf8(const char *str)
{
    if (!str)
        return false;

    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(str);
    while (*bytes)
    {
        if ((*bytes & 0x80) == 0x00)
        {
            // ASCII字符
            bytes++;
        }
        else if ((*bytes & 0xE0) == 0xC0)
        {
            // 双字节UTF-8
            if ((bytes[1] & 0xC0) != 0x80)
                return false;
            bytes += 2;
        }
        else if ((*bytes & 0xF0) == 0xE0)
        {
            // 三字节UTF-8
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80)
                return false;
            bytes += 3;
        }
        else if ((*bytes & 0xF8) == 0xF0)
        {
            // 四字节UTF-8
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80)
                return false;
            bytes += 4;
        }
        else
        {
            return false;
        }
    }
    return true;
}

// 用户回调函数，用于处理ID3标签数据
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
    String outputString;
    String id3_type = type;

    if (isUnicode)
    {
        // 计算字符串长度
        size_t length = 0;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(string);
        while (ptr[length] != 0 || ptr[length + 1] != 0)
        {
            length += 2;
            if (length > 254)
                break; // 防止缓冲区溢出
        }

        // 转换为UTF-8
        outputString = utf16ToUtf8(reinterpret_cast<const uint8_t *>(string), length);
    }
    else
    {
        // 检查是否已经是UTF-8
        if (isUtf8(string))
        {
            outputString = string;
        }
        else
        {
            // 假设是ISO-8859-1编码，转换为UTF-8
            const uint8_t *ptr = reinterpret_cast<const uint8_t *>(string);
            while (*ptr)
            {
                if (*ptr < 0x80)
                {
                    outputString += static_cast<char>(*ptr);
                }
                else
                {
                    // 转换为双字节UTF-8
                    outputString += static_cast<char>(0xC0 | (*ptr >> 6));
                    outputString += static_cast<char>(0x80 | (*ptr & 0x3F));
                }
                ptr++;
            }
        }
    }

    // 存储到相应的字段
    if (id3_type.equalsIgnoreCase("title"))
    {
        app.info.title = outputString;
    }
    else if (id3_type.equalsIgnoreCase("album"))
    {
        app.info.album = outputString;
    }
    else if (id3_type.equalsIgnoreCase("performer"))
    {
        app.info.performer = outputString;
    }
    else if (id3_type.equalsIgnoreCase("tlen"))
    {
        app.info.tlen = strtoul(outputString.c_str(), NULL, 10);
    }

    Serial.printf("%s callback for: %s = '%s'\n", cbData, type, outputString.c_str());
}
/**
 * 播放器退出函数
 */
static void player_exit()
{
    // is_ran = true;
    pinMode(25, OUTPUT);
    pinMode(26, OUTPUT);
    hal.pref.putFloat("gain", app.gain);
    int i = 0;
    if (app.titles != nullptr && app.maxSong != 0)
    {
        for (int i = 0; i < app.maxSong; i++)
        {
            if (app.titles[i] != nullptr)
            {
                free(app.titles[i]); // 释放每个字符串的内存
                app.titles[i] = nullptr;
            }
        }
        delete[] app.titles;
        app.titles = nullptr;
    }
    if (app.lyricArray != nullptr)
    {
        delete[] app.lyricArray;
    }
    if (app.fileList != nullptr)
    {
        delete[] app.fileList;
    }
}
/**
 * 播放器deepsleep函数
 */
static void player_deepsleep()
{
    hal.pref.putFloat("gain", app.gain);
    // is_ran = true;
}

void delete_generator()
{
    if (generator != nullptr)
    {
        delete generator;
        generator = nullptr;
    }
    mp3_generator = nullptr;
    flac_generator = nullptr;
    opus_generator = nullptr;
    wav_generator = nullptr;
    aac_generator = nullptr;
}

void delete_output()
{
    if (output != nullptr)
    {
        delete output;
        output = nullptr;
    }
    i2s_output = nullptr;
    noDAC = nullptr;
}
/**
 * 播放器任务函数
 */
void player_loop(void *)
{
    log_i("开始音频解码任务");
    log_i("目标音频文件：%s", music_file);
    log_i("目标解码器：%s", play_generator_str[app.play_generator]);
    log_i("当前栈的历史剩余最小值：%ld", uxTaskGetStackHighWaterMark(NULL));
    app.play_time_start = millis();
    while (1)
    {
        // 尝试获取信号量（等待直到成功）
        if (xSemaphoreTake(audio_control_sem, portMAX_DELAY) == pdTRUE)
        {
            // 安全操作解码器
            if (generator->isRunning())
            {
                if (!generator->loop())
                {
                    generator->stop();
                    app._play_end = true;
                    app.play_time_end = millis();
                    app.play_time_total = app.play_time_end - app.play_time_start;
                    log_i("当前栈的历史剩余最小值：%ld", uxTaskGetStackHighWaterMark(NULL));
                    player_loop_task_handle = NULL;
                    // xSemaphoreGive(audio_control_sem); // 释放信号量
                    vTaskDelete(NULL);
                    vTaskDelay(portMAX_DELAY);
                }
                xSemaphoreGive(audio_control_sem); // 释放信号量
            }
            else
                delay(5); // 避免意外情况
        }
        else
            delay(5); // 避免意外情况
        delay(1);     // 释放cpu
    }
}
/**
 * 设定应用显示状态
 */
void AppMusicPlayer::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
    log_i("APP %s,版本:%s  构建日期:%s %s", name, "0.1.3", __DATE__, __TIME__);
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
String AppMusicPlayer::getLyricPath(const char *musicPath)
{
    log_i("生成歌词文件路径");
    String musicPathStr(musicPath);

    // 移除文件系统前缀（如/sd或/littlefs）
    String basePath;
    if (musicPathStr.startsWith("/sd"))
    {
        basePath = musicPathStr.substring(3); // 去除"/sd"
        lrcintf = true;
    }
    else if (musicPathStr.startsWith("/littlefs"))
    {
        basePath = musicPathStr.substring(9); // 去除"/littlefs"
        lrcintf = false;
    }
    else
    {
        basePath = musicPathStr;
    }

    // 分离目录和文件名
    int lastSlash = basePath.lastIndexOf('/');
    String dir = basePath.substring(0, lastSlash);
    String filename = basePath.substring(lastSlash + 1);

    // 替换扩展名为.lrc
    int dotIndex = filename.lastIndexOf('.');
    if (dotIndex != -1)
    {
        filename = filename.substring(0, dotIndex) + ".lrc";
    }
    else
    {
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
int countLyricLines(const char *path)
{
    log_i("开始获取歌词行数");
    int count = 0;
    String lrcPath = path;
    // lrcPath.replace(".mp3", ".lrc");

    File file;
    if (app.lrcintf)
        file = SD.open(lrcPath, "r");
    else
        file = LittleFS.open(lrcPath, "r");
    if (!file)
        return -1;

    bool debug = hal.pref.getBool("lrc_debug");

    if (file.available() >= 3) {
        char bom[3];
        file.readBytes(bom, 3);
        if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
            // 跳过BOM
            file.seek(3);
            if (debug) log_i("检测到并跳过UTF-8 BOM标记");
        }
    }
    // 记录开始时间，用于检测超时
    unsigned long startTime = millis();
    const unsigned long timeout = 5000; // 5秒超时

    String line;
    while (file.available())
    {
        // 检查超时
        if (millis() - startTime > timeout)
        {
            log_e("歌词行数获取超时");
            GUI::msgbox("错误", "歌词行数获取超时");
            file.close();
            return -1;
        }
        line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("["))
        {
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
void AppMusicPlayer::loadLyrics(const char *path)
{

    lrcisload = false;
    unsigned long loadlrcbegin = millis();
    if (lyricArray != nullptr)
    {
        delete[] lyricArray;
        lyricArray = nullptr;
    }
    String lrcPath = getLyricPath(path);
    log_i("期望歌词路径：%s", lrcPath.c_str());
    totalLyricLines = countLyricLines(lrcPath.c_str());
    if (totalLyricLines == -1)
    {
        log_w("歌词文件不存在,中止加载操作");
        return;
    }

    // 预先分配内存
    lyricArray = new LyricLine[totalLyricLines];

    if (lyricArray == nullptr)
    {
        log_e("内存分配失败,中止加载操作");
        return;
    }

    File file;
    if (lrcintf)
        file = SD.open(lrcPath, "r");
    else
        file = LittleFS.open(lrcPath, "r");

    if (!file)
    {
        log_e("歌词文件打开发生意外错误,中止加载操作");
        return;
    }
    log_i("开始加载歌词，歌词行数：%d", totalLyricLines);

    bool debug = hal.pref.getBool("lrc_debug");

    if (file.available() >= 3) {
        char bom[3];
        file.readBytes(bom, 3);
        if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
            // 跳过BOM
            file.seek(3);
            if (debug) log_i("检测到并跳过UTF-8 BOM标记");
        }
    }

    int index = 0;
    String line;
    String timeStr;
    String text;
    String msStr;
    // 记录开始时间，用于检测超时
    unsigned long startTime = millis();
    const unsigned long timeout = 5000; // 5秒超时
    while (file.available() && index < totalLyricLines)
    {
        // 检查超时
        if (millis() - startTime > timeout)
        {
            log_e("歌词加载超时");
            GUI::msgbox("错误", "歌词加载超时");
            file.close();
            lrcisload = false;
            return;
        }
        line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("["))
        {
            int closeBracket = line.indexOf(']');
            if (closeBracket != -1)
            {
                timeStr = line.substring(1, closeBracket);
                text = line.substring(closeBracket + 1);

                // 解析时间戳
                int colon = timeStr.indexOf(':');
                int dot = timeStr.indexOf('.');
                if (colon == -1 || dot == -1)
                    continue;

                int minutes = timeStr.substring(0, colon).toInt();
                int seconds = timeStr.substring(colon + 1, dot).toInt();
                msStr = timeStr.substring(dot + 1);
                while (msStr.length() < 3)
                    msStr += "0";
                int milliseconds = msStr.substring(0, 3).toInt();
                unsigned long timestamp = minutes * 60000 + seconds * 1000 + milliseconds;

                // 存储到预分配数组
                lyricArray[index].timeMs = timestamp;
                lyricArray[index].text = text;
                if (debug)
                    log_i("time: %ld lrc text:%s", timestamp, text.c_str());
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
void AppMusicPlayer::getLyric(unsigned long currentTime)
{
    // 重置当前歌词索引如果超出范围
    if (currentLyricIndex >= totalLyricLines)
    {
        currentLyricIndex = 0;
    }

    // 处理时间倒流的情况：向前查找
    if (currentLyricIndex > 0 && currentTime < lyricArray[currentLyricIndex].timeMs)
    {
        while (currentLyricIndex > 0 && lyricArray[currentLyricIndex].timeMs > currentTime)
        {
            currentLyricIndex--;
        }
    }
    // 正常播放：向后查找
    else
    {
        // 修改这里的逻辑，确保能正确找到第一行
        int tempIndex = 0;
        while (tempIndex < totalLyricLines && lyricArray[tempIndex].timeMs <= currentTime)
        {
            tempIndex++;
        }
        currentLyricIndex = (tempIndex > 0) ? tempIndex - 1 : 0;
    }

    // 显示前三行歌词（当前行及前后行）
    // 前一行
    if (currentLyricIndex == 0)
    {
        snprintf(currentLyric[0], 64, "%s", "-");
    }
    else
    {
        snprintf(currentLyric[0], 64, "%s", lyricArray[currentLyricIndex - 1].text.c_str());
    }

    // 当前行
    char buf[78];
    snprintf(buf, 78, "%s", lyricArray[currentLyricIndex].text.c_str());
    snprintf(currentLyric[1], 80, "> %s <", buf);

    // 后一行
    if (currentLyricIndex + 1 >= totalLyricLines)
    {
        snprintf(currentLyric[2], 80, "%s", "-");
    }
    else
    {
        snprintf(currentLyric[2], 80, "%s", lyricArray[currentLyricIndex + 1].text.c_str());
    }
}

int AppMusicPlayer::findSongIndexInFileList()
{
    if (music_file == NULL || !filelist_ok)
    {
        return -1; // 参数无效
    }

    // 提取 music_file 的文件名部分
    String filename = String(music_file);
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash != -1)
    {
        filename = filename.substring(lastSlash + 1); // 取得文件名
    }

    // 遍历 title 查找匹配项
    for (int i = 0; titles[i] != NULL; ++i)
    { // 跳过 "返回" 项（索引0）
        if (filename.equals(String(titles[i])))
        {
            return i; // 找到匹配项，返回索引
        }
    }

    log_w("未在歌曲列表中找到“%s”的找到匹配项", filename);
    return -1; // 未找到匹配项
}

/**
 * 文件选择函数，根据变量和preferences库读取数据判断文件选择方式
 */
void AppMusicPlayer::select_file(bool user)
{
    if (music_file != NULL && !user) // is_ran &&
    {
        String name = music_file;
        if (name.endsWith(".mp3") || name.endsWith(".wav") || name.endsWith(".aac") || name.endsWith(".opus") || name.endsWith(".flac"))
        {
            if (strncmp(music_file, "/sd/", 4) == 0)
            {
                if (!SD.exists(remove_path_prefix(music_file, "/sd")))
                    goto select;
            }
            else if (strncmp(music_file, "/littlefs/", 10) == 0)
            {
                if (!LittleFS.exists(remove_path_prefix(music_file, "/littlefs")))
                    goto select;
            }
            file_in(music_file);
        }
        else
        {
        select:
            music_file = NULL;
            while (music_file == NULL)
            {
                music_file = GUI::fileDialog("选择音乐文件", false, "mp3\nwav\naac\nopus\nflac", NULL, currentDir);
            }
            file_in(music_file);
        }
    }
    else
    {
        music_file = NULL;
        while (music_file == NULL)
        {
            music_file = GUI::fileDialog("选择音乐文件", false, "mp3\nwav\naac\nopus\nflac", NULL, currentDir);
        }
        file_in(music_file);
    }
    sprintf(buf, "%s", music_file); // 复制歌曲路径到缓冲区
    music_file = buf;               // 将歌曲路径指向缓冲器
    // 解析目录
    log_i("%s", pathStr.c_str());
    int lastSlash = pathStr.lastIndexOf('/');
    if (lastSlash == 0)
    {
        is_root = true;
    }
    else
    {
        currentDir = pathStr.substring(0, lastSlash);
    }
    filelist_ok = false;
    GUI::info_msgbox("提示", "正在创建音乐列表");
    bulid_music_list();
    int index = findSongIndexInFileList();
    if (index == -1)
        currentSongIndex = 1;
    else
        currentSongIndex = index;
}
/**
 * 文件输入函数，自动处理文件系统并传入AudioFileSource
 */
void AppMusicPlayer::file_in(const char *path)
{
    if (in != nullptr)
        delete in;
    if (hal.pref.getBool(hal.get_char_sha_key("lrc歌词"), false))
    {
        loadLyrics(path);
    }
    bool file_sd = false;
    const char *_path;
    if (strncmp(path, "/sd/", 4) == 0)
    {
        file_sd = true;
        sprintf(char_buf, "%s", remove_path_prefix(path, "/sd"));
        _path = char_buf;
        in = new AudioFileSourceSD(_path);
        in_littlefs = false;
    }
    else if (strncmp(path, "/littlefs/", 10) == 0)
    {
        file_sd = false;
        sprintf(char_buf, "%s", remove_path_prefix(path, "/littlefs"));
        _path = char_buf;
        in = new AudioFileSourceLittleFS(_path);
        in_littlefs = true;
    }
    pathStr = _path;
    if (!in->isOpen())
    {
        log_e("无法打开指定的文件（%s）以供播放,正在重试", path);
        if (file_sd)
            in = new AudioFileSourceSD(_path);
        else
            in = new AudioFileSourceLittleFS(_path);
        if (!in->isOpen() && file_sd)
        {
            log_e("无法打开指定的文件（%s）以供播放，尝试重新挂载文件系统后播放", path);
            peripherals.tf_unload();
            delay(100);
            peripherals.load(PERIPHERALS_SD_BIT);
            in = new AudioFileSourceSD(_path);
            if (!in->isOpen())
            {
                log_e("无法打开指定的文件（%s）以供播放", path);
                need_deep_sleep = true;
            }
        }
    }
}
void AppMusicPlayer::next_song(bool next, bool btn)
{
    const bool loopPlay = hal.pref.getBool(hal.get_char_sha_key("单曲循环"), false);
    const bool autoPlay = hal.pref.getBool(hal.get_char_sha_key("顺序播放"), false);
    const bool randomPlay = hal.pref.getBool(hal.get_char_sha_key("随机播放"), false);

    log_i("模式状态：单曲%s 列表%s 列表%s 函数由%s", loopPlay ? "单曲循环" : "非单曲循环", autoPlay ? "自动播放" : "非自动播放", randomPlay ? "随机播放" : "非随机播放", btn ? "用户按键触发" : "函数逻辑");

    if (!loopPlay && !autoPlay && !randomPlay && !btn)
        return;

    delete_playtask();

    // 循环播放模式
    if (loopPlay && !btn)
    {
        file_in(music_file);
        player_set();
        if (xSemaphoreTake(audio_control_sem, 100 / portTICK_PERIOD_MS) == pdFALSE)
        {
            xSemaphoreGive(audio_control_sem);
        }
        else
        {
            xSemaphoreGive(audio_control_sem);
        }
        log_i("释放信号量");
        begin_player_task();
        return;
    }
    else if (randomPlay)
    {
        uint16_t random_val = (uint16_t)random(0, maxSong - 1);
        uint8_t a = 5;
        while ((currentSongIndex == random_val) && (a > 0))
        {
            random_val = (uint16_t)random(0, maxSong - 1);
            a--;
        }
        currentSongIndex = random_val;
        log_i("随机索引：%u", currentSongIndex);
    }
    else
    {
        // 统一处理前进/后退方向
        const int step = next ? 1 : -1;
        currentSongIndex += step;

        // 统一边界处理
        currentSongIndex = (currentSongIndex < 0) ? maxSong - 1 : (currentSongIndex > maxSong - 1) ? 0
                                                                                                   : currentSongIndex;
    }

    // 处理播放列表逻辑
    // if (randomPlay && !btn) {
    // } else {
    //     // 统一处理前进/后退方向
    //     const int step = next ? 1 : -1;
    //     currentSongIndex += step;

    //     // 统一边界处理
    //     currentSongIndex = (currentSongIndex < 0) ? maxSong - 1 :
    //                        (currentSongIndex > maxSong - 1) ? 0 : currentSongIndex;
    // }

    // 路径生成
    if (strncmp(music_file, "/sd/", 4) == 0)
        sprintf(buf, "%s", ("/sd" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
    else
        sprintf(buf, "%s", ("/littlefs" + currentDir + "/" + (String)titles[currentSongIndex]).c_str());
    music_file = buf;

    // 统一执行播放操作
    file_in(music_file);
    player_set();
    if (xSemaphoreTake(audio_control_sem, 100 / portTICK_PERIOD_MS) == pdFALSE)
    {
        xSemaphoreGive(audio_control_sem);
    }
    else
    {
        xSemaphoreGive(audio_control_sem);
    }
    log_i("释放信号量");
    begin_player_task();
}
/**
 * 信号量函数，用于控制音频播放/暂停
 */
void AppMusicPlayer::sem()
{
    if (xSemaphoreTake(audio_control_sem, 100 / portTICK_PERIOD_MS) == pdFALSE)
    {
        xSemaphoreGive(audio_control_sem);
        log_i("释放信号量");
    }
    else
    {
        log_i("获取信号量");
    }
}
/**
 * 调用此函数以删除播放任务
 * @note 此函数不会判断任务是否存在，注意调用位置
 */
void AppMusicPlayer::delete_playtask()
{
    if (!_play_end && player_loop_task_handle != NULL)
    {
        xSemaphoreTake(audio_control_sem, 200 / portTICK_PERIOD_MS);
        delay(100);
        if (player_loop_task_handle != NULL)
        {
            generator->stop();
            vTaskDelete(player_loop_task_handle);
            player_loop_task_handle = NULL;
        }
    }
}
/**
 * 创建音乐列表，从当前播放的文件夹在查找MP3文件并保存到titles数组中
 */
void AppMusicPlayer::bulid_music_list()
{
    if (!filelist_ok)
    {
        uint16_t song_count = 0;
        File root;
        uint64_t start = millis(), end;
        // 定义链表节点结构
        struct MusicNode {
            char* name;
            MusicNode* next;
        };
        
        MusicNode* head = nullptr;
        MusicNode* tail = nullptr;

        if (is_root)
        {
            if (!in_littlefs)
                root = SD.open("/");
            else
                root = LittleFS.open("/");
            Serial.printf("创建音乐列表,从根目录\n");
        }
        else
        {
            if (!in_littlefs)
                root = SD.open(currentDir);
            else
                root = LittleFS.open(currentDir);
            Serial.printf("创建音乐列表,从文件夹:%s\n", currentDir.c_str());
        }
        File dir = root.openNextFile();
        String name = "";
        while (dir)
        {
            name = dir.name();
            if (!dir.isDirectory() && 
                (name.endsWith(".mp3") || name.endsWith(".wav") || 
                 name.endsWith(".aac") || name.endsWith(".opus") || name.endsWith(".flac")))
            {
                // 创建新节点
                MusicNode* newNode = new MusicNode;
                newNode->name = strdup(dir.name()); // 复制文件名
                newNode->next = nullptr;
                
                // 添加到链表尾部
                if (head == nullptr)
                {
                    head = newNode;
                    tail = newNode;
                }
                else
                {
                    tail->next = newNode;
                    tail = newNode;
                }
                
                song_count++;
                Serial.printf("%s\n", dir.name());
            }
            dir.close();
            dir = root.openNextFile();
        }
        dir.close();        
        root.close();
        // 清理之前的资源
        if (titles != nullptr && maxSong != 0)
        {
            for (int i = 0; i < maxSong; i++)
            {
                if (titles[i] != nullptr)
                {
                    free(titles[i]);
                    titles[i] = nullptr;
                }
            }
            delete[] titles;
            titles = nullptr;
        }
        
        if (fileList != nullptr)
        {
            delete[] fileList;
            fileList = nullptr;
        }

        // 分配数组内存
        maxSong = song_count;
        fileList = new menu_item[song_count + 2];
        titles = new char*[song_count];
        memset(titles, 0, sizeof(char*[song_count]));

        // 设置返回项
        fileList[0].title = "返回";
        fileList[0].icon = NULL;

        // 将链表数据转移到数组
        MusicNode* current = head;
        int i = 1;
        while (current != nullptr && i <= song_count)
        {
            titles[i-1] = current->name; // 直接使用链表中的字符串指针
            fileList[i].title = titles[i-1];
            fileList[i].icon = NULL;
            
            MusicNode* temp = current;
            current = current->next;
            delete temp; // 释放节点，但不释放字符串内存
            i++;
        }

        // 设置结束标志
        fileList[i].title = NULL;
        fileList[i].icon = NULL;
        filelist_ok = true;
        
        end = millis();
        log_i("创建音乐列表结束，共计%ld个音频文件，耗时 %lld ms", song_count, end - start);
    }
}
/**
 * 用于从音乐列表中选择音乐
 * @param play 播放任务是否运行，如果传入true，则需要调用此函数后调用file_in函数
 * @return 返回true表示选择了歌曲，false表示未选择歌曲
 */
bool AppMusicPlayer::music_list_menu(bool play)
{
    
    hal.can_light_sleep = false;
    if (!filelist_ok)
        bulid_music_list();
    int res = GUI::menu("音乐列表", fileList, 8, 8, currentSongIndex + 1);
    switch (res)
    {
    case 0:
        break;
    default:
        currentSongIndex = res - 1;
        currentSongIndex = (currentSongIndex < 0) ? maxSong - 1 : (currentSongIndex > maxSong - 1) ? 0
                                                                                                   : currentSongIndex;

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
    hal.can_light_sleep = true;
    if (res == 0)
        return false;
    else
        return true;
}
static const menu_select menu_player[] =
    {
        {false, "< 返回", nullptr},
        {false, "退出", nullptr},
        {false, "播放/暂停", nullptr},
        {false, "播放列表", nullptr},
        {false, "选择文件", nullptr},
        {false, "设置音量", nullptr},
        {true, "单曲循环", nullptr},
        {true, "随机播放", nullptr},
        {true, "顺序播放", nullptr},
        {false, "其他设置", nullptr},
        {false, NULL, nullptr},
}; // 音乐播放器菜单
static const menu_select menu_set_player[] =
    {
        {false, "< 返回", nullptr},
        {true, "lrc歌词", nullptr},
        {false, "歌词显示补偿", nullptr},
        {true, "使用25/26/0输出", nullptr},
        {true, "32bit通道宽度", "bits_per_chan"},
        {true, "使用蜂鸣器输出", nullptr},
        {true, "audio_pll", nullptr},
        {false, "重启间隔", nullptr},
        {true, "显示debug信息", "music_debug"},
        {true, "打印歌词debug信息", "lrc_debug"},
        {false, NULL, nullptr},
}; // 音乐播放器菜单
/**
 * 音乐播放器菜单函数，处理用户对应操作
 */
void AppMusicPlayer::player_menu()
{
    hal.can_light_sleep = false;
    int res = 0;
    bool end = false;
    while (!end)
    {
        res = GUI::select_menu("菜单", menu_player, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            end = true;
            _end = true;
            hal.pref.putString("music_file", (String)music_file);
            delete_playtask();
            delete in;
            delete_output();
            if (id3 != nullptr)
                delete id3;
            delete_generator();
            appManager.goBack();
            app_exit = true;
            break;
        case 2:
            if (!_play_end)
            {
                if (user_stop)
                {
                    user_stop = false;
                    play_stop_time = millis() - play_stop_time;
                    sem();
                }
                else
                {
                    user_stop = true;
                    play_stop_time = millis();
                    sem();
                }
            }
            break;
        case 3:
            if (filelist_ok)
            {
                if (!music_list_menu(true))
                {
                    end = false;
                    break;
                }
                end = true;
                delete_playtask();
                play_time_total = 0;
                file_in(music_file);
            }
            else
            {
                delete_playtask();
                music_list_menu();
            }
            player_set();
            sem();
            begin_player_task();
            break;
        case 4:
            end = true;
            delete_playtask();
            play_time_total = 0;
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
            sem();
            begin_player_task();
            break;
        case 5:
            gain = (float)GUI::msgbox_number("0-400", 3, gain * 100.0) / 100.0;
            if (gain > 4.0)
            {
                gain = 4.0;
            }
            if (gain < 0.0)
            {
                gain = 0.0;
            }
            if (!nodac)
                output->SetGain(gain);
            break;

        case 9:
            player_set_menu();
            break;
        default:
            GUI::info_msgbox("警告", "非法的输入值");
            break;
        }
    }
    hal.can_light_sleep = true;
}

void AppMusicPlayer::player_set_menu()
{
    int res = 0;
    bool end = false;
    while (!end)
    {
        res = GUI::select_menu("菜单", menu_set_player, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 2:
            _lrcoffset = GUI::msgbox_number("单位ms", 4, _lrcoffset);
            hal.pref.putInt("_lrcoffset", _lrcoffset);
            break;
        case 7:
            _count = GUI::msgbox_number("重启间隔 0-999", 3, _count);
            hal.pref.putInt("rst_count", _count);
            break;
        default:
            GUI::info_msgbox("警告", "非法的输入值");
            break;
        }
    }
    display_debug_mode = hal.pref.getBool("music_debug", false);
}

/**
 * 启动音乐播放任务
 * @note 在完成播放后任务会删除自身
 */
void AppMusicPlayer::begin_player_task()
{
    _play_end = false;
    play_stop_time = 0;
    play_time_start = millis();
    uint8_t core = xPortGetCoreID();
    uint32_t stack_size = 9216;
    if (play_generator == OPUS_Generator)
        stack_size = 16384;
    log_i("将为解码任务分配%ld字节堆栈", stack_size);
    Serial.printf("app运行在: core%d\r\n", core);
    if (core == 0)
        xTaskCreatePinnedToCore(player_loop, "play_task", stack_size, NULL, 5, &player_loop_task_handle, 1);
    else
        xTaskCreatePinnedToCore(player_loop, "play_task", stack_size, NULL, 5, &player_loop_task_handle, 0);
}
/**
 * 屏幕信息显示函数
 */
void AppMusicPlayer::show_display()
{
    if (display_debug_mode)
        show_display_debug();
    else
        show_display_mormal();
}

void AppMusicPlayer::show_display_debug()
{
    display.clearScreen();
    bool lrcupdate = false;
    if (lrcisload) //  && !user_stop
    {
        if (titles[currentSongIndex] != nullptr)
        {
            GUI::drawWindowsWithTitle(titles[currentSongIndex]);
        }
        else
        {
            GUI::drawWindowsWithTitle("音乐播放器");
        }
        getLyric(millis() - play_time_start - _lrcoffset - play_stop_time);
        int x = 0;
        x = u8g2Fonts.getUTF8Width(currentLyric[0]);
        u8g2Fonts.setCursor((296 - x) / 2, 30);
        u8g2Fonts.print(currentLyric[0]);
        x = u8g2Fonts.getUTF8Width(currentLyric[1]);
        u8g2Fonts.setCursor((296 - x) / 2, 45);
        u8g2Fonts.printf(currentLyric[1]);
        x = u8g2Fonts.getUTF8Width(currentLyric[2]);
        u8g2Fonts.setCursor((296 - x) / 2, 60);
        u8g2Fonts.printf(currentLyric[2]);
        if (currentLyricIndex != lastLyricIndex)
        {
            lastLyricIndex = currentLyricIndex;
            lrcupdate = true;
            // log_i("%s", lyricArray[currentLyricIndex].text.c_str());
        }
        else
            lrcupdate = false;
        char buf[80];
        snprintf(buf, 80, "%s  %s", info.title.c_str(), info.performer.c_str());
        x = u8g2Fonts.getUTF8Width(buf);
        u8g2Fonts.setCursor((296 - x) / 2, 75);
        u8g2Fonts.printf(buf);
    }
    else
    {
        if (titles[currentSongIndex] != nullptr)
        {
            GUI::drawWindowsWithTitle(titles[currentSongIndex]);
        }
        else
        {
            GUI::drawWindowsWithTitle("音乐播放器");
        }
        int x = 0;
        x = u8g2Fonts.getUTF8Width(info.title.c_str());
        u8g2Fonts.setCursor((296 - x) / 2, 30);
        u8g2Fonts.print(info.title);

        x = u8g2Fonts.getUTF8Width(info.performer.c_str());
        u8g2Fonts.setCursor((296 - x) / 2, 45);
        u8g2Fonts.print(info.performer);

        x = u8g2Fonts.getUTF8Width(info.album.c_str());
        u8g2Fonts.setCursor((296 - x) / 2, 60);
        u8g2Fonts.print(info.album);

        u8g2Fonts.setCursor(3, 75);
        // if (info.title.equalsIgnoreCase((String) "---"))
        // {
        //     u8g2Fonts.print(titles[currentSongIndex]);
        // }
        // else
        // {
        // }
    }
    // 电池
    if (hal.pref.getBool(hal.get_char_sha_key("精准电量显示"), false) && hal.VCC < 4300 && !hal.isCharging)
    {
        display.drawXBitmap(274, 0, getBatteryIcon(true), 20, 16, 0);
        display.fillRect(277, 6, getBatterysoc(), 4, GxEPD_BLACK);
    }
    else
        display.drawXBitmap(274, 0, getBatteryIcon(), 20, 16, 0);

    u8g2Fonts.setCursor(2, 12);
    u8g2Fonts.printf("%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);

    // u8g2Fonts.setCursor(3, 75);
    // u8g2Fonts.printf("%s  %s", info.title.c_str(), info.performer.c_str());
    // u8g2Fonts.printf("专辑:%s", info.album.c_str());
    u8g2Fonts.setCursor(3, 99);
    if (_play_end)
        u8g2Fonts.printf("播放结束 ");
    else
        u8g2Fonts.printf("播放中...");
    u8g2Fonts.setCursor(64, 99);
    if (hal.pref.getBool(hal.get_char_sha_key("单曲循环"), false))
    {
        u8g2Fonts.printf("单曲循环");
    }
    else if (hal.pref.getBool(hal.get_char_sha_key("顺序播放"), false))
    {
        if (hal.pref.getBool(hal.get_char_sha_key("随机播放"), false))
            u8g2Fonts.printf("随机");
        else
            u8g2Fonts.printf("顺序");
    }
    u8g2Fonts.printf("  index:%d %d", currentSongIndex, play_count);
    uint32_t play_time = (millis() - play_time_start - play_stop_time) / 1000;
    uint32_t total_time = 0;
    if (!hal.pref.getBool(hal.get_char_sha_key("单曲循环"), false))
        play_time_total = 0;
    if (info.tlen != 0)
        total_time = info.tlen / 1000;
    else
        total_time = play_time_total / 1000;
    if (info.tlen != 0 && (millis() - play_time_start - play_stop_time) > info.tlen)
        play_time = info.tlen / 1000;
    float w = ((float)play_time / (float)total_time) * (float)200;
    if (w > 200.0)
        w = 200.0;
    display.fillCircle(48 + (int16_t)w, 83, 3, GxEPD_BLACK);
    display.drawRoundRect(48, 82, 200, 3, 1, GxEPD_BLACK);
    display.drawLine(48, 83, 48 + (int16_t)w, 83, GxEPD_BLACK);

    u8g2Fonts.setCursor(18, 87);
    u8g2Fonts.printf("%02d:%02d", play_time / 60, play_time % 60);
    u8g2Fonts.setCursor(253, 87);
    u8g2Fonts.printf("%02d:%02d", total_time / 60, total_time % 60);

    u8g2Fonts.setCursor(3, 112);
    u8g2Fonts.printf("Gain:%.2f vcc:%dmV bat:%.3fV soc:%d%% soh:%d%%", gain, hal.VCC, hal.bat_info.voltage, hal.bat_info.soc, hal.bat_info.soh);
    u8g2Fonts.setCursor(3, 125);
    u8g2Fonts.printf("剩余堆内存：%.2fKB I:%dmA P:%dmW %dmAh", (float)ESP.getFreeHeap() / 1024.0, hal.bat_info.current.avg, hal.bat_info.power, hal.bat_info.capacity.remain);
    int max_count = (lrcisload ? 45 : 15); // 控制全刷间隔，避免全刷影响歌词更新
    if (millis() - display_time > 1000 || lrcupdate)
    { // 如果有歌词更新或屏幕刷新时间间隔超过1秒则刷新屏幕
        if (display_count > max_count)
        {
            display_count = 0;
            display.display();
        }
        else
        {
            display.display(true);
        }
        display_count++;
        display_time = millis();
        // log_i("解码任务栈高水位标记：%ld",uxTaskGetStackHighWaterMark(player_loop_task_handle));
    }
}

void AppMusicPlayer::show_display_mormal()
{
    display.clearScreen();
    bool lrcupdate = false;
    if (lrcisload) //  && !user_stop
    {
        if (titles[currentSongIndex] != nullptr)
        {
            GUI::drawWindowsWithTitle(titles[currentSongIndex]);
        }
        else
        {
            GUI::drawWindowsWithTitle("音乐播放器");
        }
        getLyric(millis() - play_time_start - _lrcoffset - play_stop_time);
        int x = 0;
        x = u8g2Fonts.getUTF8Width(currentLyric[0]);
        u8g2Fonts.setCursor((296 - x) / 2, 30);
        u8g2Fonts.print(currentLyric[0]);
        x = u8g2Fonts.getUTF8Width(currentLyric[1]);
        u8g2Fonts.setCursor((296 - x) / 2, 45);
        u8g2Fonts.printf(currentLyric[1]);
        x = u8g2Fonts.getUTF8Width(currentLyric[2]);
        u8g2Fonts.setCursor((296 - x) / 2, 60);
        u8g2Fonts.printf(currentLyric[2]);
        if (currentLyricIndex != lastLyricIndex)
        {
            lastLyricIndex = currentLyricIndex;
            lrcupdate = true;
            // log_i("%s", lyricArray[currentLyricIndex].text.c_str());
        }
        else
            lrcupdate = false;
        char buf[80];
        snprintf(buf, 80, "%s  %s", info.title.c_str(), info.performer.c_str());
        x = u8g2Fonts.getUTF8Width(buf);
        u8g2Fonts.setCursor((296 - x) / 2, 75);
        u8g2Fonts.printf(buf);
    }
    else
    {
        if (titles[currentSongIndex] != nullptr)
        {
            GUI::drawWindowsWithTitle(titles[currentSongIndex]);
        }
        else
        {
            GUI::drawWindowsWithTitle("音乐播放器");
        }
        int x = 0;
        x = u8g2Fonts.getUTF8Width(info.title.c_str());
        u8g2Fonts.setCursor((296 - x) / 2, 30);
        u8g2Fonts.print(info.title);

        x = u8g2Fonts.getUTF8Width(info.performer.c_str());
        u8g2Fonts.setCursor((296 - x) / 2, 45);
        u8g2Fonts.print(info.performer);

        x = u8g2Fonts.getUTF8Width(info.album.c_str());
        u8g2Fonts.setCursor((296 - x) / 2, 60);
        u8g2Fonts.print(info.album);

        u8g2Fonts.setCursor(3, 75);
        // if (info.title.equalsIgnoreCase((String) "---"))
        // {
        //     u8g2Fonts.print(titles[currentSongIndex]);
        // }
        // else
        // {
        // }
    }
    // 电池
    if (hal.pref.getBool(hal.get_char_sha_key("精准电量显示"), false) && hal.VCC < 4400 && !hal.isCharging)
    {
        display.drawXBitmap(274, 0, getBatteryIcon(true), 20, 16, 0);
        display.fillRect(277, 6, getBatterysoc(), 4, GxEPD_BLACK);
    }
    else
        display.drawXBitmap(274, 0, getBatteryIcon(), 20, 16, 0);

    u8g2Fonts.setCursor(2, 12);
    u8g2Fonts.printf("%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);

    // u8g2Fonts.setCursor(3, 75);
    // u8g2Fonts.printf("%s  %s", info.title.c_str(), info.performer.c_str());
    // u8g2Fonts.printf("专辑:%s", info.album.c_str());
    // u8g2Fonts.setCursor(3, 99);
    // if (_play_end)
    //     u8g2Fonts.printf("播放结束 ");
    // else
    //     u8g2Fonts.printf("播放中...");
    u8g2Fonts.setCursor(3, 125);
    if (hal.pref.getBool(hal.get_char_sha_key("单曲循环"), false))
    {
        u8g2Fonts.printf("单曲循环");
    }
    else if (hal.pref.getBool(hal.get_char_sha_key("顺序播放"), false))
    {
        if (hal.pref.getBool(hal.get_char_sha_key("随机播放"), false))
            u8g2Fonts.printf("随机");
        else
            u8g2Fonts.printf("顺序");
    }
    char gain_buf[16];
    sprintf(gain_buf, "音量:%d", (uint16_t)(gain * 100.0));
    u8g2Fonts.setCursor(293 - u8g2Fonts.getUTF8Width(gain_buf), 125);
    u8g2Fonts.printf(gain_buf);
    // u8g2Fonts.printf("  index:%d %d", currentSongIndex, play_count);
    if (_play_end || user_stop)
        display.drawXBitmap(136, 77, pause_bits, 24, 24, GxEPD_BLACK);
    else
        display.drawXBitmap(136, 77, play_bits, 24, 24, GxEPD_BLACK);

    uint32_t play_time = (millis() - play_time_start - play_stop_time) / 1000;
    uint32_t total_time = 0;
    if (!hal.pref.getBool(hal.get_char_sha_key("单曲循环"), false))
        play_time_total = 0;
    if (info.tlen != 0)
        total_time = info.tlen / 1000;
    else
        total_time = play_time_total / 1000;

    if (info.tlen != 0 && (millis() - play_time_start - play_stop_time) > info.tlen)
        play_time = info.tlen / 1000;
    float w = ((float)play_time / (float)total_time) * (float)200;
    if (w > 200.0)
        w = 200.0;
    int y = 112;
    display.fillCircle(48 + (int16_t)w, y - 4, 3, GxEPD_BLACK);
    display.drawRoundRect(48, y - 5, 200, 3, 1, GxEPD_BLACK);
    display.drawLine(48, y - 4, 48 + (int16_t)w, y - 4, GxEPD_BLACK);

    u8g2Fonts.setCursor(18, y);
    u8g2Fonts.printf("%02d:%02d", play_time / 60, play_time % 60);
    u8g2Fonts.setCursor(253, y);
    u8g2Fonts.printf("%02d:%02d", total_time / 60, total_time % 60);

    // u8g2Fonts.setCursor(3, 112);
    // u8g2Fonts.printf("Gain:%.2f vcc:%dmV bat:%.3fV soc:%d%% soh:%d%%", gain, hal.VCC, hal.bat_info.voltage, hal.bat_info.soc, hal.bat_info.soh);
    // u8g2Fonts.setCursor(3, 125);
    // u8g2Fonts.printf("剩余堆内存：%.2fKB I:%dmA P:%dmW %dmAh", (float)ESP.getFreeHeap() / 1024.0, hal.bat_info.current.avg, hal.bat_info.power, hal.bat_info.capacity.remain);
    int max_count = (lrcisload ? 45 : 15); // 控制全刷间隔，避免全刷影响歌词更新
    if (millis() - display_time > 1000 || lrcupdate)
    { // 如果有歌词更新或屏幕刷新时间间隔超过1秒则刷新屏幕
        if (display_count > max_count)
        {
            display_count = 0;
            display.display();
        }
        else
        {
            display.display(true);
        }
        display_count++;
        display_time = millis();
        // log_i("解码任务栈高水位标记：%ld",uxTaskGetStackHighWaterMark(player_loop_task_handle));
    }
}

bool AppMusicPlayer::generator_set(const char *path, AudioFileSource *source, AudioOutput *out)
{
    String play_file = path;
    // 确定解码器
    if (play_file.endsWith(".mp3"))
    {
        play_generator = MP3_Generator;
        if (id3 != nullptr)
        {
            delete id3;
            id3 = nullptr;
        }
        id3 = new AudioFileSourceID3(source);
        id3->RegisterMetadataCB(MDCallback, (void *)"ID3TAG");
    }
    else if (play_file.endsWith(".flac"))
        play_generator = Flac_Generator;
    else if (play_file.endsWith(".wav"))
        play_generator = WAV_Generator;
    else if (play_file.endsWith(".aac") || play_file.endsWith(".m4a"))
    {
        play_generator = AAC_Generator;
        if (id3 != nullptr)
        {
            delete id3;
            id3 = nullptr;
        }
        id3 = new AudioFileSourceID3(source);
        id3->RegisterMetadataCB(MDCallback, (void *)"ID3TAG");
    }
    else if (play_file.endsWith(".opus") || play_file.endsWith(".ogg"))
        play_generator = OPUS_Generator;

    // 释放解码器
    delete_generator();
    // 实例化解码器
    switch (play_generator)
    {
    case MP3_Generator:
        mp3_generator = new AudioGeneratorMP3();
        generator = mp3_generator;
        break;
    case Flac_Generator:
        flac_generator = new AudioGeneratorFLAC();
        generator = flac_generator;
        break;
    case AAC_Generator:
        aac_generator = new AudioGeneratorAAC();
        generator = aac_generator;
        break;
    case WAV_Generator:
        wav_generator = new AudioGeneratorWAV();
        generator = wav_generator;
        break;
    case OPUS_Generator:
        opus_generator = new AudioGeneratorOpus();
        generator = opus_generator;
        break;
    default:
        break;
    }
    if (generator != nullptr)
    {
        if (play_generator != MP3_Generator && play_generator != AAC_Generator)
        {
            if (!generator->begin(source, out))
            {
                log_e("未能初始化音频解码器！");
                GUI::msgbox("错误", "未能初始化音频解码器！");
                return false;
            }
        }
        else
        {
            if (!generator->begin(id3, out))
            {
                log_e("未能初始化音频解码器！");
                GUI::msgbox("错误", "未能初始化音频解码器！");
                return false;
            }
        }
    }
    else
    {
        log_e("未能实例化音频解码器！");
        GUI::msgbox("错误", "未能实例化音频解码器！这可能是音乐文件格式不受支持导致的。");
        return false;
    }
    return true;
}

/**
 * 启动音乐播放任务的前置准备函数
 * @note 此函数会释放解码器、id3标签解析、音频输出占用的资源
 */
bool AppMusicPlayer::player_set()
{
    play_count++;
    info.album = "---";
    info.performer = "---";
    info.title = "---";
    info.tlen = 0;
    if (nodac)
    {
        delete_output();
        noDAC = new AudioOutputI2SNoDAC(0);
        output = noDAC;
        noDAC->SetGain(gain);
        delete_generator();
        return generator_set(music_file, in, output);
    }
    else
    {
        delete_output();
        if (hal.pref.getBool(hal.get_char_sha_key("使用25/26/0输出")))
        {
            i2s_output = new AudioOutputI2S(0, 0, 8, apll);
            output = i2s_output;
            i2s_output->SetPinout(0, 25, 26);
            i2s_output->SetMclk(false);
            if (bits_per_chan)
                i2s_output->Set_bits_per_chan(I2S_BITS_PER_CHAN_32BIT);
        }
        else
        {
            i2s_output = new AudioOutputI2S(0, 1, 8, apll);
            output = i2s_output;
        }
        output->SetGain(gain);
        delete_generator();
        return generator_set(music_file, in, output);
    }
}
/**
 * 音乐播放器主任务函数，由AppManager调用
 */
void AppMusicPlayer::setup()
{
    hal.cheak_freq(160);
    pinMode(25, ANALOG);
    pinMode(26, ANALOG);
    nodac = hal.pref.getBool(hal.get_char_sha_key("使用蜂鸣器输出"), false);
    _count = hal.pref.getInt("rst_count", 20);
    gain = hal.pref.getFloat("gain", 0.3);
    _lrcoffset = hal.pref.getInt("_lrcoffset", -50);
    apll = hal.pref.getBool(hal.get_char_sha_key("audio_pll"), false);
    bits_per_chan = hal.pref.getBool("bits_per_chan", true);
    display_debug_mode = hal.pref.getBool("music_debug", false);
    exit = player_exit;
    deepsleep = player_deepsleep;
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 1;
    audioLogger = &Serial;
    audio_control_sem = xSemaphoreCreateBinary(); // 创建二进制信号量
    xSemaphoreGive(audio_control_sem);            // 初始化为可用状态

    if (music_file == NULL)
    {
        String file = buf;
        if (file.endsWith(".mp3") || file.endsWith(".wav") || 
            file.endsWith(".aac") || file.endsWith(".opus") || file.endsWith(".flac"))
        {
            music_file = buf;
        }
        else{
            sprintf(buf, "%s", hal.pref.getString("music_file").c_str());
            music_file = buf;
        }
    }
    select_file();

    player_set();

    begin_player_task();
    show_display();
    _end = false;
    unsigned long wait_time = millis();
    while (!_end && !need_deep_sleep)
    {
        if (hal.btnc.isPressing())
        {
            if (GUI::waitLongPress(PIN_BUTTONC))
            {
                player_menu();
                show_display();
            }
            else
            {
                // show_display();
                if (filelist_ok)
                {
                    if (!music_list_menu(true))
                    {
                        continue;
                    }
                    delete_playtask();
                    play_time_total = 0;
                    file_in(music_file);
                }
                else
                {
                    delete_playtask();
                    music_list_menu();
                }
                player_set();
                sem();
                begin_player_task();
            }
        }
        if (app_exit)
            return;
        if (hal.btnr.isPressing())
        {
            if (GUI::waitLongPress(hal.btnr.pin()))
            {
                next_song(true, true);
                int a = 0;
                while (hal.btnr.isPressing())
                {
                    delay(50);
                    if (a++ > 20)
                    {
                        break;
                    }
                }
            }
            else
            {
                if (gain < 0.05)
                    gain += 0.01;
                else
                    gain += 0.05;
                if (gain > 4.0)
                {
                    gain = 4.0;
                }
                if (!nodac)
                    output->SetGain(gain);
            }
        }
        if (hal.btnl.isPressing())
        {
            if (GUI::waitLongPress(hal.btnl.pin()))
            {
                next_song(false, true);
                int a = 0;
                while (hal.btnl.isPressing())
                {
                    delay(50);
                    if (a++ > 20)
                    {
                        break;
                    }
                }
            }
            else
            {
                if (gain <= 0.05)
                    gain -= 0.01;
                else
                    gain -= 0.05;
                if (gain < 0.0)
                {
                    gain = 0.0;
                }
                if (!nodac)
                    output->SetGain(gain);
            }
        }
        if ((_count > 0 && play_count > _count && _play_end) || need_deep_sleep)
        {
            need_deep_sleep = true;
            GUI::info_msgbox("提示", "出现暂未解决的BUG,将会在重启后恢复播放");
            break;
        }
        if (_play_end)
        {
            delay(10);
            next_song();
            show_display();
        }
        else
            wait_time = millis();
        if (millis() - display_time > (lrcisload ? 100 : 2000))
        { // 如果歌词加载成功，则每100ms检查一次歌词
            show_display();
        }
        if ((millis() - wait_time > 30000) && _play_end)
        {
            hal.wait_input();
            wait_time = millis();
        }
        delay(20);
    }
}
