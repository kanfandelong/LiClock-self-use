#include "AppManager.h"
#include "ESP8266Audio.h"
#include <arduinoFFT.h>
#include <cstdio>
#include <atomic>
#pragma GCC optimize("O3")

// 将 FreeRTOS 任务状态枚举转换为可读字符串
static const char *taskStateToString(eTaskState state)
{
    switch (state)
    {
    case eRunning:
        return "Running";
    case eReady:
        return "Ready";
    case eBlocked:
        return "Blocked";
    case eSuspended:
        return "Suspended";
    case eDeleted:
        return "Deleted";
    case eInvalid:
        return "Invalid";
    default:
        return "Unknown";
    }
}

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
} tag_info; // 标签信息结构体

typedef struct
{
    unsigned long timeMs = 0;
    String text;
} LyricLine; // 歌词行结构体

typedef struct
{
    unsigned long start = 0;
    unsigned long fft_start = 0;
    unsigned long fft_end = 0;
    unsigned long display_start = 0;
    unsigned long end = 0;
} drawtime; // 歌词行结构体

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
AudioFileSourceVorbis *vorbis = nullptr;
AudioFileSource *in = nullptr;     // 音频文件源
AudioFileSourceID3 *id3 = nullptr; // ID3信息解码处理
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

/**
 * @brief 音乐播放器应用类
 *
 * 基于ESP8266Audio库的音乐播放器应用，支持多种音频格式播放、
 * ID3标签解析、歌词同步显示和播放列表管理功能。
 */
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
    void setup();
    void set();

    // ===== 播放控制 & 列表管理 =====
    void begin_player_task();
    void player_menu();
    void player_set_menu();
    void bulid_music_list();
    bool music_list_menu(bool play = false);
    void select_file(bool user = false);
    void file_in(const char *path);
    void next_song(bool next = true, bool btn = false);
    std::atomic<bool> stop_requested{false};
    // bool stop_requested = false;
    void delete_playtask();
    void sem();
    int findSongIndexInFileList();
    bool generator_set(const char *path, AudioFileSource *source, AudioOutput *out);
    bool player_set();

    // ===== 歌词相关 =====
    String getLyricPath(const char *musicPath);
    void loadLyrics(const char *path);
    int getLyricIndex(unsigned long currentTime);
    void getLyricLines(int index, String lyrics[]);
    void startScrollAnimation(int direction);
    bool updateScrollAnimation();
    void drawScrollingLyrics(int x, int y, int max_x = SCREEN_WIDTH);
    void checkAndUpdateLyrics(unsigned long currentTime);

    // ===== UI/显示相关 =====
    void show_display();
    void show_display_debug();
    String truncateStringWithEllipsis(const char *input, int maxWidth);

    // ===== 数据成员 =====
    // 目录与文件列表
    String currentDir = "/";       // 当前歌曲目录
    String pathStr;                // 当前歌曲位于的目录
    menu_item *fileList = nullptr; // 歌曲菜单数组
    char **titles = nullptr;       // 歌曲名内存指针数组指针,存储歌曲名所在的内存位置
    char char_buf[512];            // 字符串拼接缓存
    uint16_t maxSong = 0;          // 歌曲总数
    bool filelist_ok = false;      // 歌曲列表就绪标志
    bool is_root = false;          // 是否是根目录

    // 播放进度与时间
    unsigned long play_time_start;         // 播放开始时间
    unsigned long play_time_end;           // 播放结束时间
    unsigned long play_stop_time = 0;      // 播放停止时间
    unsigned long play_time_total = 0;     // 播放总时间
    unsigned long display_time = millis(); // 屏幕上次刷新时间

    // 播放器状态
    std::atomic<bool> _play_end{false}; // 播放完成标志
    // bool _play_end = false;
    bool _end;              // 播放器主任务while循环停止标志
    bool user_stop = false; // 用户停止播放标志
    bool app_exit = false;  // 退出标志
    int play_count = 1;     // 播放歌曲数量
    int _count = 20;        // 播放歌曲上限（控制重启）
    int display_count = 0;  // 屏幕刷新次数
    std::atomic<bool> backup_buff_updata{false};
    // bool backup_buff_updata = false;
    bool loopPlay = false;
    bool autoPlay = false;
    bool randomPlay = false;

    /*     // 用于统计缓冲区3的重绘次数
        int buffer3RedrawCount = 0;           // 当前计数
        unsigned long buffer3RedrawTimer = 0; // 上一次计时起点 */

    // 音频相关设置
    tag_info info; // 歌曲ID3信息
    generator_t play_generator = UNKONWN_Generator;
    bool nodac = false;       // 无DAC标志
    bool in_littlefs = false; // 文件是否位于LittleFS
    bool bits_per_chan = false;
    float gain = 0.3; // 音频输出增益（音量）
    int apll = 0;

    // 低功耗/标志
    bool need_deep_sleep = false; // 是否需要进入deepsleep

    // 歌词显示与同步
    bool lrcisload = false;            // 歌词加载状态
    char currentLyric[3][128];         // 当前显示的歌词
    int currentLyricIndex = 0;         // 当前显示的歌词索引
    int lastLyricIndex = 0;            // 上次显示的歌词索引
    int _lrcoffset = 0;                // 歌词显示时间补偿
    int totalLyricLines = 0;           // 歌词总行数
    unsigned long lastLyricUpdate = 0; // 上次歌词更新时间
    LyricLine *lyricArray = nullptr;   // 使用动态数组存储歌词

    int lastY = 0;              // 上一次的y坐标
    bool lastScrolling = false; // 上一次的滚动状态
    int lastScrollOffset = 0;   // 上一次的滚动偏移量
    String lastCurrentLyric[3]; // 上一次的非滚动歌词（三行）
    String lastOldLyrics[3];    // 上一次的滚动旧歌词
    String lastNewLyrics[3];    // 上一次的滚动新歌词
    bool buffer3Valid = false;  // 缓冲区3是否已包含有效内容

    // 调试相关
    bool display_debug_mode = false;
    // 歌词滚动动画相关
    bool scrolling = false;            // 是否正在滚动
    int scrollOffset = 0;              // 当前滚动偏移量（像素）
    int scrollDirection = 0;           // 滚动方向：0-无，1-向上，-1-向下
    unsigned long scrollStartTime = 0; // 滚动开始时间
    const int SCROLL_DURATION = 350;   // 滚动动画持续时间（毫秒）
    const int LINE_HEIGHT = 17;        // 每行歌词高度（像素）

    // 歌词缓冲区
    String oldLyrics[3] = {"", "", ""}; // 旧的歌词行
    String newLyrics[3] = {"", "", ""}; // 新的歌词行
    int oldLyricIndex = -1;             // 旧的歌词索引
    int newLyricIndex = 0;              // 新的歌词索引

    // FFT 部分
    uint16_t SAMPLES = 512;
    float smoothingFactor = 0.7f; // 平滑控制, 0~1，越大越平滑
    float FFT_A_spectrum_smoothness = 2000.0f;
    float FFT_A_amplitude = 40.0f;
    float fft_gain = 1.0;
    ArduinoFFT<float> FFT = ArduinoFFT<float>();
    static const size_t RING_BUFFER_SIZE = 8192; // 足够容纳 96kHz 下 40ms 的数据
    float *ring_buffer = nullptr;
    uint32_t write_index = 0; // 写指针（中断中更新）
    float *curveScaling;
    float *vReal;
    float *vImag;
    float *previousSpectrum;
    float fps = 0, max_fps = 50.0;
    bool use_log = false;
    TickType_t xLastWakeTime;
    TickType_t xFrequency; // 运行周期
    BaseType_t xWasDelayed;
    void show_display_fft();
    drawtime last_time;
    drawtime d_time;
};
static AppMusicPlayer app; // 创建App对象

/**
 * @brief UTF-16到UTF-8编码转换函数
 * @param utf16Str UTF-16编码的输入数据指针
 * @param length 输入数据长度
 * @param hasBOM 是否包含字节顺序标记（BOM），默认为true
 * @return 转换后的UTF-8字符串
 * @note 支持UTF-16大端序和小端序，能处理代理对（surrogate pairs）
 */
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

/**
 * @brief 检测字符串是否为有效的UTF-8编码
 * @param str 待检测的字符串指针
 * @return 如果是有效的UTF-8编码返回true，否则返回false
 * @note 支持检测1-4字节的UTF-8字符
 */
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

/**
 * @brief 标签元数据回调处理函数
 * @param cbData 回调数据指针
 * @param type 标签类型（如"title"、"album"等）
 * @param isUnicode 是否为Unicode编码
 * @param string 标签内容字符串
 * @note 将标签信息存储到app对象的结构体中
 */
// 用于记录是否已经通过 ID3TAG 获得了总时长
static bool id3_tlen_received = false;

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
    String outputString;
    String id3_type = type;
    const char *src = static_cast<const char *>(cbData);

    if (id3_type.equalsIgnoreCase("APIC"))
    {
        info("%s callback for: %s = '%s'", cbData, type, string);
        return;
    }

    // ----- 文字编码处理 -----
    if (isUnicode)
    {
        // 计算 UTF‑16 字符串长度（字节数）
        size_t length = 0;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(string);
        while (ptr[length] != 0 || ptr[length + 1] != 0)
        {
            length += 2;
            if (length > 1024 * 10)
                break; // 防止缓冲区溢出
        }
        outputString = utf16ToUtf8(reinterpret_cast<const uint8_t *>(string), length);
    }
    else
    {
        // 已经是 UTF‑8 ?
        if (isUtf8(string))
        {
            outputString = string;
        }
        else
        {
            // 假设 ISO‑8859‑1，手动转为 UTF‑8
            const uint8_t *ptr = reinterpret_cast<const uint8_t *>(string);
            while (*ptr)
            {
                if (*ptr < 0x80)
                {
                    outputString += static_cast<char>(*ptr);
                }
                else
                {
                    outputString += static_cast<char>(0xC0 | (*ptr >> 6));
                    outputString += static_cast<char>(0x80 | (*ptr & 0x3F));
                }
                ++ptr;
            }
        }
    }

    // ----- 保存标签信息 -----
    if (id3_type.equalsIgnoreCase("title"))
    {
        app.info.title = outputString;
    }
    else if (id3_type.equalsIgnoreCase("album"))
    {
        app.info.album = outputString;
    }
    else if (id3_type.equalsIgnoreCase("performer") || id3_type.equalsIgnoreCase("artist"))
    {
        app.info.performer = outputString;
    }
    else if (id3_type.equalsIgnoreCase("tlen"))
    {
        unsigned long newLen = strtoul(outputString.c_str(), nullptr, 10);
        // 当回调来源是 ID3TAG 时，始终写入并标记已收到
        if (strcmp(src, "ID3TAG") == 0 || strcmp(src, "FLACTAG") == 0)
        {
            app.info.tlen = newLen;
            id3_tlen_received = true;
        }
        // 当来源是 MP3INFO 且尚未收到 ID3TAG，则更新（每次回调均可更新）
        else if (strcmp(src, "MP3INFO") == 0 && !id3_tlen_received)
        {
            app.info.tlen = newLen;
        }
        // 其它来源保持原值
    }
    else if (id3_type.equalsIgnoreCase("LYRICS") || id3_type.equalsIgnoreCase("USLT"))
    {
        String lyricPath = app.getLyricPath(music_file);
        if (!hal.exists(lyricPath))
        {
            File f = hal.open(lyricPath, "w");
            if (f)
            {
                info("%s callback for: %s = '%s'", cbData, type, outputString.c_str());
                f.write((const uint8_t *)outputString.c_str(), strlen(outputString.c_str()));
                f.close();
            }
            else
            {
                log_e("无法创建歌词文件: %s", lyricPath.c_str());
            }
            info("已写入歌词文件用做缓存: %s", lyricPath.c_str());
            // 在写入歌词文件后立即加载歌词，因为默认歌词加载位置位于解码开始之前，此时可能还没有歌词文件，导致无法加载到歌词(哪怕回调获取到了歌词)。
            if (hal.pref.getBool(hal.get_char_sha_key("lrc歌词"), false))
            {
                app.loadLyrics(music_file);
            }
            app.backup_buff_updata = true;
            return;
        }
        else
            info("歌词文件已存在: %s", lyricPath.c_str());
    }

    info("%s callback for: %s = '%s'", cbData, type, outputString.c_str());
    // info("%s callback for: %s 的原始值 = '%s'", cbData, type, string);
    app.backup_buff_updata = true;
}
#ifdef CONFIG_DAC_32bit
void GetSampleCB(int32_t sample[2])
{
    // 将左右声道分别转换为浮点数
    float left = (float)(sample[0] >> 16);
    float right = (float)(sample[1] >> 16);
    // 混合
    float mono = (left + right) * 0.5f;
    app.ring_buffer[app.write_index] = mono;
    app.write_index = (app.write_index + 1) & (app.RING_BUFFER_SIZE - 1); // 快速取模
}
#else
void GetSampleCB(int16_t sample[2])
{
    if (app.fftProcessing)
    {
        // FFT还没处理完，跳过本次采样点获取，避免覆盖
        return;
    }
    app.vReal[app.sampleIndex] = (float)sample[0];
    app.vImag[app.sampleIndex] = 0.0f;
    app.sampleIndex++;
    // When sampleIndex reaches the number of samples, wrap around.
    // Use >= to avoid writing past the allocated buffers (valid indices: 0..SAMPLES-1).
    if (app.sampleIndex >= app.SAMPLES)
    {
        app.sampleIndex = 0;
        app.fftProcessing = true; // 标记FFT开始处理
    }
}
#endif

/**
 * @brief 播放器退出清理函数
 * @note 释放所有动态分配的内存，重置引脚状态，保存音量设置到Preferences
 */
static void player_exit()
{
    if (hal.bat_info.current.avg < 0)
        hal.pref.putInt("player_power", hal.bat_info.current.avg);
    // is_ran = true;
    digitalWrite(PIN_DAC_XSMT, 0);
    digitalWrite(PIN_DAC_EN, 0);
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

    delete[] app.curveScaling;
    delete[] app.vReal;
    delete[] app.vImag;
    delete[] app.previousSpectrum;
    free(app.ring_buffer);
}
/**
 * @brief 播放器深度睡眠前处理函数
 * @note 保存当前音量设置到Preferences
 */
static void player_deepsleep()
{
    hal.pref.putFloat("gain", app.gain);
    // is_ran = true;
}

/**
 * @brief 删除音频解码器对象
 * @note 安全删除所有类型的音频解码器指针
 */
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

/**
 * @brief 删除音频输出对象
 * @note 安全删除所有类型的音频输出指针
 */
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
 * @brief 音频解码任务主循环函数
 * @param parameter 任务参数（未使用）
 * @note 运行在独立任务中，负责音频解码循环，使用信号量保证线程安全
 */
void player_loop(void *)
{
    log_i("开始音频解码任务");
    log_i("目标音频文件：%s", music_file);
    log_i("目标解码器：%s", play_generator_str[app.play_generator]);
    log_i("当前栈的历史剩余最小值：%ld", uxTaskGetStackHighWaterMark(NULL));
    app.play_time_start = millis();
    while (!app.stop_requested)
    { // 检查退出标志
        if (xSemaphoreTake(audio_control_sem, 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (generator->isRunning())
            {
                if (!generator->loop())
                {
                    generator->stop();
                    app._play_end = true;
                    app.play_time_end = millis();
                    app.play_time_total = app.play_time_end - app.play_time_start;
                    log_i("解码器已停止");
                    xSemaphoreGive(audio_control_sem); // 确保释放信号量
                    break;                             // 退出循环
                }
            }
            else
            {
                delay(5);
            }
            xSemaphoreGive(audio_control_sem);
        }
        else
        {
            warn("信号量获取失败");
            if (!app.user_stop)
            {
                xSemaphoreGive(audio_control_sem); // 确保释放信号量
                warn("释放信号量");
            }
            delay(5);
        }
    }
    // 退出前清理（如果尚未停止）
    if (generator->isRunning())
    {
        generator->stop();
    }
    player_loop_task_handle = NULL;
    log_i("解码器任务栈的历史剩余最小值：%ld", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}

/**
 * @brief 设置应用在列表中的显示状态
 * @note 从Preferences读取应用显示设置，并打印版本信息
 */
void AppMusicPlayer::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
    log_i("APP %s,版本:%s  构建日期:%s %s", name, "0.1.6", __DATE__, __TIME__);
}
/**
 * @brief 字符串截断函数，添加省略号
 * @param input 原始字符串
 * @param maxWidth 最大显示宽度（像素）
 * @return 截断后的字符串
 * @note 考虑UTF-8字符宽度，中文等宽字符为14像素，ASCII字符为7像素
 */
String AppMusicPlayer::truncateStringWithEllipsis(const char *input, int maxWidth)
{
    String result = "";
    int currentWidth = 0;
    const char *ptr = input;
    int ellipsisWidth = 21; // "..." 三个点，每个点宽度7

    if (u8g2Fonts.getUTF8Width(input) <= maxWidth)
    {
        return String(input); // 字符串宽度不超过最大宽度，直接返回
    }

    // 计算实际可用于显示内容的宽度（减去省略号宽度）
    int availableWidth = maxWidth - ellipsisWidth;

    while (*ptr != '\0' && currentWidth < availableWidth)
    {
        unsigned char c = *ptr;
        int charWidth;
        int charLen = 1; // 字符字节数

        // UTF-8字符检测
        if ((c & 0x80) == 0)
        {
            // ASCII字符（0-127）
            charWidth = 7;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            // 2字节UTF-8字符
            charWidth = 14;
            charLen = 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            // 3字节UTF-8字符（通常包含中文字符）
            charWidth = 14;
            charLen = 3;
        }
        else
        {
            // 其他情况，默认为中文宽度
            charWidth = 14;
            // 跳过剩余字节
            while ((*ptr & 0xC0) == 0x80)
            {
                ptr++;
                charLen++;
            }
        }

        // 检查添加当前字符后是否会超过可用宽度
        if (currentWidth + charWidth > availableWidth)
        {
            break; // 超过宽度，停止添加字符
        }

        // 添加当前字符
        for (int i = 0; i < charLen && *ptr != '\0'; i++)
        {
            result += *ptr;
            ptr++;
        }
        currentWidth += charWidth;
    }

    // 添加省略号
    result += "...";
    return result;
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
 * @brief 根据音乐文件路径生成对应的歌词文件路径
 * @param musicPath 音乐文件完整路径
 * @return 歌词文件完整路径
 * @note 将音频文件扩展名替换为.lrc
 */
String AppMusicPlayer::getLyricPath(const char *musicPath)
{
    log_i("生成歌词文件路径");
    if (musicPath == NULL)
        return String();
    String basePath(musicPath);
    int lastSlash = basePath.lastIndexOf('/');
    String filename = basePath.substring(lastSlash + 1);
    // 替换扩展名为.lrc
    int dotIndex = filename.lastIndexOf('.');
    if (dotIndex != -1)
        filename = filename.substring(0, dotIndex) + ".lrc";
    else
        filename += ".lrc";

    // 生成两个歌词路径
    static const String sdLrcDir = "/sd/lrc";
    static const String littlefsLrcDir = "/littlefs/lrc";
    String sdLrcPath = sdLrcDir + "/" + filename;
    String littlefsLrcPath = littlefsLrcDir + "/" + filename;

    // 检查并创建lrc文件夹
    if (!hal.exists(sdLrcDir))
    {
        hal.mkdir(sdLrcDir);
    }
    if (!hal.exists(littlefsLrcDir))
    {
        hal.mkdir(littlefsLrcDir);
    }

    // 优先返回SD卡路径，如果需要可以切换为littlefs
    if (basePath.startsWith("/sd/"))
    {
        return sdLrcPath;
    }
    else if (basePath.startsWith("/littlefs/"))
    {
        return littlefsLrcPath;
    }
    else
    {
        // 默认返回SD卡路径
        return sdLrcPath;
    }
}

/**
 * @brief 统计歌词文件中的有效歌词行数
 * @param path 音乐文件路径（用于生成歌词文件路径）
 * @return 有效歌词行数，文件无法打开返回-1
 * @note 仅统计以'['开头的歌词行，支持UTF-8 BOM标记检测
 */
int countLyricLines(const char *path)
{
    log_i("开始获取歌词行数");
    int count = 0;
    File file = hal.open(path, "r");
    if (!file)
        return -1;

    bool debug = hal.pref.getBool("lrc_debug");

    if (file.available() >= 3)
    {
        char bom[3];
        file.readBytes(bom, 3);
        if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)
        {
            // 跳过BOM
            file.seek(3);
            if (debug)
                log_i("检测到并跳过UTF-8 BOM标记");
        }
        else
        {
            // 回到文件开头
            file.seek(0);
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
            GUI::msgbox("错误", "歌词行数获取超时", 5);
            file.close();
            return -1;
        }
        line = file.readStringUntil('\n');
        line.trim();
        // 只统计包含实际歌词文本的行，排除仅有元数据的标签行（如 [ti:...], [ar:...], [al:...]）
        if (line.startsWith("["))
        {
            // 查找首个右方括号的位置
            int rightBracketPos = line.indexOf(']');
            if (rightBracketPos != -1)
            {
                // 检查右方括号后是否还有非空白字符，若有则视为歌词行
                String afterBracket = line.substring(rightBracketPos + 1);
                afterBracket.trim();
                if (afterBracket.length() > 0)
                {
                    count++;
                }
            }
        }
    }
    file.close();
    return count;
}

/**
 * @brief 加载并解析歌词文件
 * @param path 音乐文件路径（用于生成歌词文件路径）
 * @note 解析时间戳和歌词文本，存储到lyricArray数组中，设置lrcisload状态标志
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
        warn("歌词文件 \"%s\" 不存在,中止加载操作", lrcPath.c_str());
        return;
    }
    else if (totalLyricLines == 0)
    {
        warn("未在歌词文件 \"%s\" 中识别到有效的歌词,中止加载操作", lrcPath.c_str());
        return;
    }

    // 预先分配内存
    lyricArray = new LyricLine[totalLyricLines];

    if (lyricArray == nullptr)
    {
        error("内存分配失败,中止加载操作");
        return;
    }

    File file = hal.open(lrcPath, "r");

    if (!file)
    {
        error("歌词文件打开发生意外错误,中止加载操作");
        return;
    }
    log_i("开始加载歌词，歌词行数：%d", totalLyricLines);

    bool debug = hal.pref.getBool("lrc_debug");

    if (file.available() >= 3)
    {
        char bom[3];
        file.readBytes(bom, 3);
        if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)
        {
            // 跳过BOM
            file.seek(3);
            if (debug)
                log_i("检测到并跳过UTF-8 BOM标记");
        }
        else
        {
            // 回到文件开头
            file.seek(0);
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
            warn("歌词加载超时");
            GUI::msgbox("错误", "歌词加载超时", 5);
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

                // 去除两端空白并检查是否有实际歌词文本，若无则视为元数据行跳过
                text.trim();
                if (text.length() == 0)
                    continue;

                text.replace(String((char)0xE3) + String((char)0x80) + String((char)0x80), "  ");
                // text.replace(String("—"), "--");

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

    // 清空歌词滚动数据
    scrollOffset = 0;
    for (uint8_t i = 0; i < 3; i++)
    {
        currentLyric[i][0] = '\0';
        oldLyrics[i] = "";
        newLyrics[i] = "";
    }
    oldLyricIndex = -1;
    newLyricIndex = 0;

    sprintf(currentLyric[0], "---");
    strncpy(currentLyric[1], lyricArray[0].text.c_str(), 127);
    currentLyric[1][127] = '\0';
    strncpy(currentLyric[2], lyricArray[1].text.c_str(), 127);
    currentLyric[2][127] = '\0';

    newLyrics[0] = "---";
    oldLyrics[0] = "---";
    newLyrics[1] = lyricArray[0].text;
    oldLyrics[1] = "---";
    newLyrics[2] = lyricArray[1].text;
    oldLyrics[2] = lyricArray[0].text;

    scrolling = false;
}
/**
 * @brief 根据当前播放时间获取对应的歌词索引
 * @param currentTime 当前播放时间（毫秒）
 * @return 当前歌词行的索引
 * @note 不再直接更新显示，仅返回索引
 */
int AppMusicPlayer::getLyricIndex(unsigned long currentTime)
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
        int tempIndex = 0;
        while (tempIndex < totalLyricLines && lyricArray[tempIndex].timeMs <= currentTime)
        {
            tempIndex++;
        }
        currentLyricIndex = (tempIndex > 0) ? tempIndex - 1 : 0;
    }

    return currentLyricIndex;
}

/**
 * @brief 获取指定索引的三行歌词
 * @param index 中心行索引
 * @param lyrics 输出的歌词数组（大小为3）
 */
void AppMusicPlayer::getLyricLines(int index, String lyrics[])
{
    // 前一行
    if (index == 0 || totalLyricLines == 0)
    {
        lyrics[0] = "-";
    }
    else
    {
        lyrics[0] = lyricArray[index - 1].text;
    }

    // 当前行
    lyrics[1] = lyricArray[index].text;

    // 后一行
    if (index + 1 >= totalLyricLines)
    {
        lyrics[2] = "-";
    }
    else
    {
        lyrics[2] = lyricArray[index + 1].text;
    }
}

/**
 * @brief 启动歌词滚动动画
 * @param direction 滚动方向：1-向上，-1-向下
 * @note 设置滚动动画状态和参数
 */
void AppMusicPlayer::startScrollAnimation(int direction)
{
    if (scrolling)
        return; // 如果已经在滚动，不重复启动

    // 保存旧歌词
    for (int i = 0; i < 3; i++)
    {
        oldLyrics[i] = currentLyric[i];
    }
    oldLyricIndex = lastLyricIndex;

    // 获取新歌词
    getLyricLines(currentLyricIndex, newLyrics);
    newLyricIndex = currentLyricIndex;

    // 设置滚动参数
    scrolling = true;
    scrollDirection = direction;
    scrollOffset = 0;
    scrollStartTime = millis();

    // log_i("方向：%d，索引：%d -> %d", direction, oldLyricIndex, newLyricIndex);
}

/**
 * @brief 更新滚动动画
 * @return 是否完成滚动
 */
bool AppMusicPlayer::updateScrollAnimation()
{
    if (!scrolling)
        return true;

    unsigned long elapsed = millis() - scrollStartTime;
    if (elapsed >= SCROLL_DURATION)
    {
        // 动画完成
        scrolling = false;
        scrollOffset = LINE_HEIGHT;

        // 更新显示歌词
        for (int i = 0; i < 3; i++)
        {
            strncpy(currentLyric[i], newLyrics[i].c_str(), 127);
            currentLyric[i][127] = '\0';
        }
        lastLyricIndex = currentLyricIndex;

        // log_i("歌词滚动动画完成");
        return true;
    }

    // 计算当前偏移量（缓动函数：easeOutCubic）
    float progress = (float)elapsed / SCROLL_DURATION;
    progress = 1.0 - pow(1.0 - progress, 3); // 缓动效果
    scrollOffset = (int)(progress * LINE_HEIGHT);

    return false;
}

/**
 * @brief 显示带滚动动画的歌词
 * @param x 起始X坐标
 * @param y 起始Y坐标（第二行位置）
 */
void AppMusicPlayer::drawScrollingLyrics(int x, int y, int max_x)
{
    // 判断是否需要更新缓冲区3
    bool needRedraw = false;

    // 检查坐标变化
    if (y != lastY)
        needRedraw = true;

    // 检查滚动状态变化
    if (scrolling != lastScrolling)
        needRedraw = true;

    if (!scrolling)
    {
        // 非滚动模式：检查当前歌词是否变化
        for (int i = 0; i < 3; i++)
        {
            if (strcmp(currentLyric[i], lastCurrentLyric[i].c_str()) != 0)
            {
                needRedraw = true;
                break;
            }
        }
    }
    else
    {
        // 滚动模式：检查偏移量和歌词内容是否变化
        if (scrollOffset != lastScrollOffset)
            needRedraw = true;
        for (int i = 0; i < 3; i++)
        {
            if (oldLyrics[i] != lastOldLyrics[i] || newLyrics[i] != lastNewLyrics[i])
            {
                needRedraw = true;
                break;
            }
        }
    }

    // 缓冲区3从未有效（首次绘制）也需要重绘
    if (!buffer3Valid)
        needRedraw = true;

    int x1 = 14, x2 = 2;
    uint8_t current_buffer = display.current_buffer_idx;

    if (needRedraw)
    {
        // 需要更新缓冲区3
        display.swapBuffer(3); // 切换到缓冲区3
        display.clearScreen();

        if (max_x != SCREEN_WIDTH)
            display.setDrawWindow(0, y - (LINE_HEIGHT * 2) + 2, max_x - 9, (LINE_HEIGHT * 3));
        else
            display.setDrawWindow(0, y - (LINE_HEIGHT * 2) + 2, 383, (LINE_HEIGHT * 3));
        if (!scrolling)
        {

            // 正常显示
            int lineY = y - LINE_HEIGHT; // 第一行
            u8g2Fonts.setCursor(x1, lineY);
            u8g2Fonts.print(currentLyric[0]);
            if (u8g2Fonts.getCursorX() > max_x)
                u8g2Fonts.drawStr(max_x - 9, lineY, "...");

            lineY = y; // 第二行（当前行）
            String currentLine = "> " + String(currentLyric[1]);
            u8g2Fonts.setCursor(x2, lineY);
            u8g2Fonts.print(currentLine);
            if (u8g2Fonts.getCursorX() > max_x)
                u8g2Fonts.drawStr(max_x - 9, lineY, "...");

            lineY = y + LINE_HEIGHT; // 第三行
            u8g2Fonts.setCursor(x1, lineY);
            u8g2Fonts.print(currentLyric[2]);
            if (u8g2Fonts.getCursorX() > max_x)
                u8g2Fonts.drawStr(max_x - 9, lineY, "...");
        }
        else
        {
            int oldOffset = scrollDirection > 0 ? -scrollOffset : scrollOffset;
            int newOffset = scrollDirection > 0 ? (LINE_HEIGHT - scrollOffset) : -(LINE_HEIGHT - scrollOffset);
            // 设置绘制窗口
            // 滚动显示

            // 绘制旧歌词（向上移动）
            int lineY;
            lineY = y - LINE_HEIGHT + oldOffset;
            u8g2Fonts.setCursor(x1, lineY);
            u8g2Fonts.print(oldLyrics[0]);
            if (u8g2Fonts.getCursorX() > max_x)
                u8g2Fonts.drawStr(max_x - 9, lineY, "...");

            // 绘制新歌词（从下方进入）
            lineY = y - LINE_HEIGHT + newOffset;
            u8g2Fonts.setCursor(x1, lineY);
            u8g2Fonts.print(newLyrics[0]);
            if (u8g2Fonts.getCursorX() > max_x)
                u8g2Fonts.drawStr(max_x - 9, lineY, "...");

            lineY = y + newOffset;
            String newCurrentLine = "> " + newLyrics[1];
            u8g2Fonts.setCursor(x2, lineY);
            u8g2Fonts.print(newCurrentLine);
            if (u8g2Fonts.getCursorX() > max_x)
                u8g2Fonts.drawStr(max_x - 9, lineY, "...");

            lineY = y + LINE_HEIGHT + newOffset;
            u8g2Fonts.setCursor(x1, lineY);
            u8g2Fonts.print(newLyrics[2]);
            if (u8g2Fonts.getCursorX() > max_x)
                u8g2Fonts.drawStr(max_x - 9, lineY, "...");

            // 恢复全屏绘制窗口
        }
        display.setDrawWindow();

        display.swapBuffer(current_buffer); // 恢复到原缓冲区

        // 更新记录的状态
        lastY = y;
        lastScrolling = scrolling;
        if (!scrolling)
        {
            for (int i = 0; i < 3; i++)
            {
                lastCurrentLyric[i] = currentLyric[i];
            }
        }
        else
        {
            lastScrollOffset = scrollOffset;
            for (int i = 0; i < 3; i++)
            {
                lastOldLyrics[i] = oldLyrics[i];
                lastNewLyrics[i] = newLyrics[i];
            }
        }
        buffer3Valid = true;

        // 统计本次重绘
        // buffer3RedrawCount++;
    }

    // 无论是否重绘，都将缓冲区3的内容混合到当前显示缓冲区
    display.blendBuffers(current_buffer, 3, OR);
    // 每秒输出一次统计
    // if (millis() - buffer3RedrawTimer >= 1000)
    // {
    //     log_i("缓冲区3重绘次数: %d", buffer3RedrawCount);
    //     buffer3RedrawCount = 0;
    //     buffer3RedrawTimer = millis();
    // }
}

/**
 * @brief 检查并处理歌词滚动
 * @param currentTime 当前播放时间
 */
void AppMusicPlayer::checkAndUpdateLyrics(unsigned long currentTime)
{
    int newIndex = getLyricIndex(currentTime);

    if (newIndex != lastLyricIndex)
    {
        if (lastLyricIndex != -1) // 不是第一次
        {
            if (!scrolling && ((lyricArray[newIndex].timeMs - lyricArray[lastLyricIndex].timeMs) >= SCROLL_DURATION))
            {
                // 启动滚动动画（向上滚动）
                startScrollAnimation(1);
            }
            else
            {
                // 保存旧歌词
                for (int i = 0; i < 3; i++)
                {
                    oldLyrics[i] = currentLyric[i];
                }
                oldLyricIndex = lastLyricIndex;

                // 获取新歌词
                getLyricLines(currentLyricIndex, newLyrics);
                newLyricIndex = currentLyricIndex;
            }
        }
        else
        {
            // 第一次显示，直接设置
            getLyricLines(newIndex, newLyrics);
            for (int i = 0; i < 3; i++)
            {
                strncpy(currentLyric[i], newLyrics[i].c_str(), 127);
                currentLyric[i][127] = '\0';
            }
        }

        lastLyricIndex = newIndex;
        currentLyricIndex = newIndex;
    }

    // 更新滚动动画
    if (scrolling)
    {
        updateScrollAnimation();
    }
}

/**
 * @brief 在当前歌曲列表中查找歌曲索引
 * @return 找到返回歌曲索引，未找到返回-1
 * @note 通过比较文件名在titles数组中查找匹配项
 */
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
    for (int i = 0; titles[i] != nullptr; ++i)
    {
        if (filename.equals(String(titles[i])))
        {
            return i; // 找到匹配项，返回索引
        }
    }

    warn("未在歌曲列表中找到“%s”的找到匹配项", filename);
    return -1; // 未找到匹配项
}

/**
 * @brief 选择播放文件
 * @param user 是否为用户手动选择（true：用户选择，false：自动恢复上次播放）
 * @note 支持从文件对话框选择或恢复上次播放的文件，会自动创建音乐列表
 */
void AppMusicPlayer::select_file(bool user)
{
    if (music_file != NULL && !user) // is_ran &&
    {
        String name = music_file;
        if (name.endsWith(".mp3") || name.endsWith(".wav") || name.endsWith(".aac") || name.endsWith(".opus") || name.endsWith(".flac"))
        {
            if (!hal.exists(music_file))
                goto select;
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
 * @brief 初始化音频文件源
 * @param path 音频文件路径
 * @note 根据路径判断文件系统类型，创建对应的AudioFileSource对象，自动处理文件系统挂载失败
 */
void AppMusicPlayer::file_in(const char *path)
{
    need_deep_sleep = false;
    if (in != nullptr)
    {
        delete in;
        in = nullptr;
    }
    lrcisload = false;
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
        error("无法打开指定的文件（%s）以供播放,正在重试", path);
        if (file_sd)
            in = new AudioFileSourceSD(_path);
        else
            in = new AudioFileSourceLittleFS(_path);
        if (!in->isOpen() && file_sd)
        {
            error("无法打开指定的文件（%s）以供播放，尝试重新挂载文件系统后播放", path);
            peripherals.tf_unload();
            GUI::info_msgbox("提示", "SD卡重新挂载中...");
            delay(50);
            peripherals.load(PERIPHERALS_SD_BIT);
            in = new AudioFileSourceSD(_path);
            if (!in->isOpen())
            {
                error("无法打开指定的文件（%s）以供播放", path);
                need_deep_sleep = true;
                appManager.noDeepSleep = false;
                appManager.nextWakeup = 1;
            }
        }
    }
}
/**
 * @brief 切换上一首/下一首歌曲
 * @param next true：下一首，false：上一首
 * @param btn 是否为按钮触发
 * @note 根据播放模式（单曲循环、顺序播放、随机播放）决定切换逻辑
 */
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
        if (player_set())
        {
            begin_player_task();
        }
        else
            _play_end = true;
        return;
    }
    else if (randomPlay)
    {
        uint32_t range = maxSong;
        uint32_t r;

        // 拒绝采样确保均匀分布
        do
        {
            r = esp_random();
        } while (r >= (UINT32_MAX - (UINT32_MAX % range)));

        currentSongIndex = r % range;
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
    if (!need_deep_sleep)
    { // 确认文件打开成功
        if (player_set())
        {
            begin_player_task();
        }
        else
            _play_end = true;
    }
}
/**
 * @brief 音频控制信号量操作函数
 * @note 获取或释放音频控制信号量，用于播放/暂停控制
 */
void AppMusicPlayer::sem()
{
    if (xSemaphoreTake(audio_control_sem, 1000 / portTICK_PERIOD_MS) == pdTRUE)
    {
        log_i("获取信号量");
    }
    else
    {
        xSemaphoreGive(audio_control_sem);
        log_i("释放信号量");
    }
}
/**
 * @brief 删除音频播放任务
 * @note 停止解码器并删除播放任务，确保线程安全
 */
void AppMusicPlayer::delete_playtask()
{
    if (_play_end || player_loop_task_handle == NULL)
        return;

    if (i2s_output != nullptr)
        i2s_output->SetTimeout(0); // 设置i2s写无超时
    // 请求任务停止
    stop_requested = true;

    // 等待任务自然结束（超时机制防止死等）
    TickType_t start = xTaskGetTickCount();
    while (player_loop_task_handle != NULL && (xTaskGetTickCount() - start) < pdMS_TO_TICKS(5000))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 如果任务仍未退出，再考虑强制手段（极少发生）
    if (player_loop_task_handle != NULL)
    {
        warn("任务未响应，强制删除");
        vTaskDelete(player_loop_task_handle);
        player_loop_task_handle = NULL;
        if (xSemaphoreTake(audio_control_sem, 1000 / portTICK_PERIOD_MS) != pdTRUE)
            xSemaphoreGive(audio_control_sem); // 确保释放信号量
    }

    // 确保 generator 被停止（如果还没停）
    if (generator != nullptr && generator->isRunning())
    {
        generator->stop();
    }

    // 重置标志
    stop_requested = false;
}
/**
 * @brief 创建音乐播放列表
 * @note 扫描当前目录下的音频文件，构建titles数组和fileList菜单
 */
void AppMusicPlayer::bulid_music_list()
{
    if (!filelist_ok)
    {
        bool debug = hal.pref.getBool("lrc_debug");
        uint16_t song_count = 0;
        File root;
        uint64_t start = millis(), end;
        // 定义链表节点结构
        struct MusicNode
        {
            char *name;
            MusicNode *next;
        };

        MusicNode *head = nullptr;
        MusicNode *tail = nullptr;

        // ===== 新增：检查预生成播放列表文件 =====
        String folderName = "";
        String playlistPath = "";
        bool usePregenList = false;

        // 从currentDir提取文件夹名（最后一个'/'之后的部分）
        int lastSlashIdx = currentDir.lastIndexOf('/');
        if (lastSlashIdx != -1)
        {
            folderName = currentDir.substring(lastSlashIdx + 1);
            // 构建预生成列表文件路径：/sd/playlist/文件夹名.npl
            playlistPath = "/sd/playlist/" + folderName + ".npl";
            log_i("检查预生成播放列表: %s", playlistPath.c_str());

            if (hal.exists(playlistPath.c_str()))
            {
                log_i("找到预生成播放列表文件");
                File listFile = hal.open(playlistPath, "r");
                listFile.setBufferSize(8192);
                if (listFile)
                {
                    usePregenList = true;
                    // 记录开始时间，用于检测超时
                    unsigned long startTime = millis();
                    const unsigned long timeout = 5000; // 5秒超时
                    String line;
                    String fullPath;
                    while (listFile.available() && millis() - startTime < timeout)
                    {
                        line = listFile.readStringUntil('\n');
                        line.trim();

                        // 跳过空行和注释行（以#开头的行）
                        if (line.length() == 0 || line.startsWith("#"))
                            continue;

                        // 检查文件是否存在于当前目录
                        // fullPath = currentDir + "/" + line;
                        // if (in_littlefs)
                        //     fullPath = fullPath; // LittleFS路径保持不变
                        // else
                        //     fullPath = "/sd" + fullPath; // SD卡路径添加前缀
                        // if (hal.exists(fullPath.c_str()) &&
                        //     (line.endsWith(".mp3") || line.endsWith(".wav") ||
                        //      line.endsWith(".aac") || line.endsWith(".opus") || line.endsWith(".flac")))
                        // {
                        // 创建新节点
                        MusicNode *newNode = new MusicNode;
                        newNode->name = strdup(line.c_str()); // 复制文件名
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
                        if (debug)
                            log_d("从预生成列表添加: %s", line.c_str());
                        // }
                    }
                    listFile.close();
                    log_i("从预生成列表加载了 %d 首歌曲", song_count);
                }
                else
                {
                    log_w("无法打开预生成列表文件: %s", playlistPath.c_str());
                }
            }
        }

        // 如果没有使用预生成列表或预生成列表为空，则扫描目录
        if (!usePregenList || song_count == 0)
        {
            log_i("未找到预生成列表，开始扫描目录...");
            if (is_root)
            {
                if (!in_littlefs)
                    root = SD_MMC.open("/");
                else
                    root = LittleFS.open("/");
                log_i("创建音乐列表,从根目录");
            }
            else
            {
                if (!in_littlefs)
                    root = SD_MMC.open(currentDir);
                else
                    root = LittleFS.open(currentDir);
                // root = hal.open(currentDir);
                log_i("创建音乐列表,从文件夹:%s", currentDir.c_str());
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
                    MusicNode *newNode = new MusicNode;
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
                    if (debug)
                        log_d("添加: %s", dir.name());
                }
                dir.close();
                dir = root.openNextFile();
            }
            dir.close();
            root.close();
        }
        // ===== 新增代码结束 =====

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
        titles = new char *[song_count];
        memset(titles, 0, sizeof(char *[song_count]));

        // 设置返回项
        fileList[0].title = "返回";
        fileList[0].icon = NULL;

        // 将链表数据转移到数组
        MusicNode *current = head;
        int i = 1;
        while (current != nullptr && i <= song_count)
        {
            titles[i - 1] = current->name; // 直接使用链表中的字符串指针
            fileList[i].title = titles[i - 1];
            fileList[i].icon = NULL;

            MusicNode *temp = current;
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
 * @brief 显示音乐列表菜单并选择歌曲
 * @param play 是否在选择后立即播放（播放任务是否运行，如果传入true，则需要调用此函数后调用file_in函数）
 * @return true：选择了歌曲，false：未选择（返回）
 * @note 显示当前目录下的音乐文件列表供用户选择
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
        {true, "FFT频谱", "music_fft"},
        {true, "lrc歌词", nullptr},
        {false, "其他设置", nullptr},
        {false, NULL, nullptr},
}; // 音乐播放器菜单
static const menu_select menu_set_player[] =
    {
        {false, "< 返回", nullptr},
        {false, "歌词显示补偿", nullptr},
        {true, "使用25/26/0输出", nullptr},
        {true, "32bit通道宽度", "bits_per_chan"},
        {true, "使用蜂鸣器输出", nullptr},
        {true, "audio_pll", nullptr},
        {false, "重启间隔", nullptr},
        {false, "FFT平滑控制", nullptr},
        {false, "FFT参数1", nullptr},
        {false, "FFT参数2", nullptr},
        {false, "线性缩放增益", nullptr},
        {false, "xFrequency", nullptr},
        {false, "FFT采样点数", nullptr},
        {true, "FFT对数缩放", "fft_log"},
        {true, "显示歌词时关闭歌曲信息显示", "lrc_off_info"},
        {true, "显示debug界面", "display_debug"},
        {true, "显示debug信息", "music_debug"},
        {true, "打印歌词debug信息", "lrc_debug"},
        {false, NULL, nullptr},
}; // 音乐播放器菜单
/**
 * @brief 显示播放器主菜单
 * @note 提供播放控制、列表选择、设置等功能的交互菜单
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
                    if (i2s_output != nullptr)
                        i2s_output->SetTimeout(10);
                }
                else
                {
                    user_stop = true;
                    play_stop_time = millis();
                    if (i2s_output != nullptr)
                        i2s_output->SetTimeout(0);
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
            if (player_set())
            {
                // sem();
                begin_player_task();
            }
            else
                _play_end = true;
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
            if (player_set())
            {
                // sem();
                begin_player_task();
            }
            else
                _play_end = true;
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

        case 11:
            player_set_menu();
            break;
        default:
            GUI::info_msgbox("警告", "非法的输入值");
            break;
        }
    }
    loopPlay = hal.pref.getBool(hal.get_char_sha_key("单曲循环"), false);
    autoPlay = hal.pref.getBool(hal.get_char_sha_key("顺序播放"), false);
    randomPlay = hal.pref.getBool(hal.get_char_sha_key("随机播放"), false);
    hal.can_light_sleep = true;
    backup_buff_updata = true;
}
/**
 * @brief 显示播放器设置菜单
 * @note 配置歌词显示、音频输出、调试信息等设置选项
 */
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
        case 1:
            _lrcoffset = GUI::msgbox_number("单位ms", 4, _lrcoffset);
            hal.pref.putInt("_lrcoffset", _lrcoffset);
            break;
        case 6:
            _count = GUI::msgbox_number("重启间隔 0-999", 3, _count);
            hal.pref.putInt("rst_count", _count);
            break;
        case 7:
            smoothingFactor = (float)GUI::msgbox_number("平滑控制 0-100", 3, (int)(smoothingFactor * 100.0)) / 100.0;
            hal.pref.putFloat("fft_smooth_val", smoothingFactor);
            break;
        case 8:
            FFT_A_spectrum_smoothness = (float)GUI::msgbox_number("FFT参数1", 4, (int)(FFT_A_spectrum_smoothness));
            break;
        case 9:
            FFT_A_amplitude = (float)GUI::msgbox_number("FFT参数2", 3, (int)(FFT_A_amplitude));
            break;
        case 10:
            fft_gain = (float)GUI::msgbox_number("线性缩放增益", 3, (int)(fft_gain * 100.0)) / 100.0;
            hal.pref.putFloat("fft_gain", fft_gain);
            break;
        case 11:
            xFrequency = pdMS_TO_TICKS(GUI::msgbox_number("xFrequency", 2, xFrequency));
            hal.pref.putInt("xFrequency", (int)xFrequency);
            break;
        case 12:
            hal.pref.putUInt("fft_samples", GUI::msgbox_number("FFT采样点数", 3, hal.pref.getUInt("fft_samples", 256)));
            break;
        default:
            GUI::info_msgbox("警告", "非法的输入值");
            break;
        }
    }
    use_log = hal.pref.getBool("fft_log", false);
    display_debug_mode = hal.pref.getBool("music_debug", false);
    max_fps = 1000.0 / (float)xFrequency;
}

/**
 * @brief 启动音频播放任务
 * @note 创建独立任务运行音频解码循环，根据解码器类型分配不同栈空间
 */
void AppMusicPlayer::begin_player_task()
{
    _play_end = false;
    play_stop_time = 0;
    // play_time_start = millis();
    uint8_t core = xPortGetCoreID();
    uint32_t stack_size = 8192;
    if (play_generator == OPUS_Generator)
        stack_size = 16384;
    log_i("将为解码任务分配%ld字节堆栈", stack_size);
    log_i("app运行在: core%d", core);
    if (core == 0)
        xTaskCreatePinnedToCore(player_loop, "play_task", stack_size, NULL, 8, &player_loop_task_handle, 1);
    else
        xTaskCreatePinnedToCore(player_loop, "play_task", stack_size, NULL, 8, &player_loop_task_handle, 0);
}
/**
 * @brief 显示播放器界面
 * @note 根据显示模式调用对应的显示函数
 */
void AppMusicPlayer::show_display()
{
    if (hal.pref.getBool("display_debug", false))
        show_display_debug();
    else
        show_display_fft();
}
/**
 * @brief 显示调试模式界面
 * @note 显示详细调试信息，包括内存状态、电池信息、播放状态等
 */
void AppMusicPlayer::show_display_debug()
{
    display.clearScreen();
    bool lrcupdate = false;
    if (lrcisload) //  && !user_stop
    {
        unsigned long currentTime = millis() - play_time_start - _lrcoffset - play_stop_time;
        checkAndUpdateLyrics(currentTime);

        // 绘制歌词（带滚动效果）
        // 计算居中位置
        String displayText = scrolling ? newLyrics[1] : String(currentLyric[1]);
        int textWidth = u8g2Fonts.getUTF8Width(displayText.c_str());
        int xPos = (296 - textWidth) / 2;
        if (xPos < 2)
            xPos = 2;

        drawScrollingLyrics(xPos, 45); // 45是第二行的Y坐标

        char buf[80];
        snprintf(buf, 80, "%s  %s", info.title.c_str(), info.performer.c_str());
        int x = u8g2Fonts.getUTF8Width(buf);
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
        display.fillRect(277, 6, getBatterysoc(), 4, TFT_BLACK);
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
    display.fillCircle(48 + (int16_t)w, 83, 3, TFT_BLACK);
    display.drawRoundRect(48, 82, 200, 3, 1, TFT_BLACK);
    display.drawLine(48, 83, 48 + (int16_t)w, 83, TFT_BLACK);

    u8g2Fonts.setCursor(18, 87);
    u8g2Fonts.printf("%02d:%02d", play_time / 60, play_time % 60);
    u8g2Fonts.setCursor(253, 87);
    u8g2Fonts.printf("%02d:%02d", total_time / 60, total_time % 60);

    u8g2Fonts.setCursor(3, 112);
    u8g2Fonts.printf("Gain:%.2f vcc:%dmV bat:%.3fV soc:%d%% soh:%d%%", gain, hal.VCC, hal.bat_info.voltage, hal.bat_info.soc, hal.bat_info.soh);
    u8g2Fonts.setCursor(3, 125);
    u8g2Fonts.printf("剩余堆内存：%.2fKB I:%dmA P:%dmW %dmAh", (float)ESP.getFreeHeap() / 1024.0, hal.bat_info.current.avg, hal.bat_info.power, hal.bat_info.capacity.remain);
    display.display();
}

void AppMusicPlayer::show_display_fft()
{
    d_time.start = micros();
    constexpr int y = 146;  // 设置进度条的Y坐标
    constexpr int x = 61;   // 设置进度条的X起始坐标
    constexpr int w1 = 262; // 设置进度条的宽度
    static int lrc_max_x = SCREEN_WIDTH;
    uint32_t play_time = (millis() - play_time_start - play_stop_time);
    static uint32_t total_time = 0;
    if (backup_buff_updata)
    {
        uint8_t current_buffer = display.current_buffer_idx;
        display.swapBuffer(1); // 切换到缓冲区1
        display.clearScreen();

        if (lrcisload) //  && !user_stop
        {
            lrc_max_x = SCREEN_WIDTH;

            if (!hal.pref.getBool("lrc_off_info"))
            {
                int16_t wchar;
                int x = 0;

                String titleStr = info.title;
                int spaceIdx = titleStr.indexOf(' ');
                String titleToDraw = (spaceIdx > 0) ? titleStr.substring(0, spaceIdx) : titleStr;
                x = u8g2Fonts.getUTF8Width(titleToDraw.c_str());
                {
                    int startX = SCREEN_WIDTH - x - 5;
                    // ensure start position is at least half the screen width
                    if (startX < SCREEN_WIDTH / 2)
                        startX = SCREEN_WIDTH / 2;
                    u8g2Fonts.setCursor(startX, 93);
                    if (startX < lrc_max_x)
                        lrc_max_x = startX;
                }
                u8g2Fonts.print(titleToDraw);

                String performerStr = info.performer;
                spaceIdx = performerStr.indexOf(' ');
                String performerToDraw = (spaceIdx > 0) ? performerStr.substring(0, spaceIdx) : performerStr;
                x = u8g2Fonts.getUTF8Width(performerToDraw.c_str());
                {
                    int startX = SCREEN_WIDTH - x - 5;
                    if (startX < SCREEN_WIDTH / 2)
                        startX = SCREEN_WIDTH / 2;
                    u8g2Fonts.setCursor(startX, 110);
                    if (startX < lrc_max_x)
                        lrc_max_x = startX;
                }
                u8g2Fonts.print(performerToDraw);

                String albumStr = info.album;
                spaceIdx = albumStr.indexOf(' ');
                String albumToDraw = (spaceIdx > 0) ? albumStr.substring(0, spaceIdx) : albumStr;
                x = u8g2Fonts.getUTF8Width(albumToDraw.c_str());
                {
                    int startX = SCREEN_WIDTH - x - 5;
                    if (startX < SCREEN_WIDTH / 2)
                        startX = SCREEN_WIDTH / 2;
                    u8g2Fonts.setCursor(startX, 127);
                    if (startX < lrc_max_x)
                        lrc_max_x = startX;
                }
                u8g2Fonts.print(albumToDraw);
            }
        }
        else
        {
            int x = 0;
            x = u8g2Fonts.getUTF8Width(info.title.c_str());
            u8g2Fonts.setCursor(SCREEN_WIDTH - x - 5, 93);
            u8g2Fonts.print(info.title);

            x = u8g2Fonts.getUTF8Width(info.performer.c_str());
            u8g2Fonts.setCursor(SCREEN_WIDTH - x - 5, 110);
            u8g2Fonts.print(info.performer);

            x = u8g2Fonts.getUTF8Width(info.album.c_str());
            u8g2Fonts.setCursor(SCREEN_WIDTH - x - 5, 127);
            u8g2Fonts.print(info.album);

            if (_play_end || user_stop)
                display.drawXBitmap(22, 97, pause_bits, 24, 24, TFT_BLACK);
            else
                display.drawXBitmap(22, 97, play_bits, 24, 24, TFT_BLACK);
        }
        int16_t wchar;
        display.drawFastHLine(0, 14, SCREEN_WIDTH, 0);
        if (titles[currentSongIndex] != nullptr)
        {
            // 标题栏
            if (titles[currentSongIndex])
            {
                wchar = u8g2Fonts.getUTF8Width(titles[currentSongIndex]);
                int x = (SCREEN_WIDTH - wchar) / 2;
                if (x < 40)
                    x = 40;
                u8g2Fonts.setCursor(x, 12);
                display.setDrawWindow(40, 0, 304, 13);
                u8g2Fonts.print(titles[currentSongIndex]);
                display.setDrawWindow();
            }
        }
        else
        {
            wchar = u8g2Fonts.getUTF8Width("音乐播放器");
            u8g2Fonts.setCursor((SCREEN_WIDTH - wchar) / 2, 12);
            u8g2Fonts.print("音乐播放器");
        }

        if (!loopPlay)
            play_time_total = 0;
        if (info.tlen != 0)
            total_time = info.tlen;
        else
            total_time = play_time_total;

        if (info.tlen != 0 && play_time > info.tlen)
            play_time = info.tlen;

        display.drawRoundRect(x, y - 6, w1, 5, 2, TFT_BLACK);
        // display.drawLine(x, y - 4, x + (int16_t)w, y - 4, TFT_BLACK);
        u8g2Fonts.setCursor(341, y);
        u8g2Fonts.printf("%02d:%02d", total_time / 1000 / 60, total_time / 1000 % 60);
        display.swapBuffer(current_buffer);
        backup_buff_updata = false;
    }

    display.copyBuffer(display.current_buffer_idx, 1); // 从缓冲区1复制显示内容到当前缓冲区
    int w = 0;
    if (play_time > total_time)
        play_time = total_time;
    if (total_time > 0)
    {
        w = (play_time * w1 + total_time / 2) / total_time;
    }
    if (w > w1)
        w = w1;
    display.fillCircle(x + (int16_t)w, y - 4, 5, TFT_BLACK);
    display.fillRoundRect(x, y - 6, (int16_t)w, 5, 2, TFT_BLACK);
    u8g2Fonts.setCursor(18, y);
    u8g2Fonts.printf("%02d:%02d", play_time / 1000 / 60, play_time / 1000 % 60);

    d_time.fft_start = micros();

    if (hal.pref.getBool("music_fft"))
    {
        // 获取当前写指针快照（保证原子性）
        uint32_t current_write = write_index;
        // 计算最新SAMPLES个样本的起始索引
        int start = (current_write - SAMPLES + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
        for (int i = 0; i < SAMPLES; i++)
        {
            vReal[i] = ring_buffer[(start + i) % RING_BUFFER_SIZE];
            vImag[i] = 0.0f;
        }

        // 1. 采样数据加窗（汉明窗）
        FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HANN, FFT_FORWARD);

        // 2. FFT计算
        FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);

        // 3. 复数转幅度
        FFT.complexToMagnitude(vReal, vImag, SAMPLES);

        if (!use_log)
        {
            vReal[0] = vReal[0] * 0.4f;
            vReal[1] = vReal[1] * 0.5f;
            vReal[2] = vReal[2] * 0.6f;
            vReal[3] = vReal[3] * 0.7f;
            vReal[4] = vReal[4] * 0.8f;
            vReal[5] = vReal[5] * 0.9f;
        }
        // 4. 幅度谱对数变换和归一化（可调参数）
        for (int i = 0; i < SAMPLES / 2; i++)
        {
            // 缩放
            if (use_log)
                vReal[i] = FFT_A_amplitude * log10f(1.0f + vReal[i] / FFT_A_spectrum_smoothness);
            else
                vReal[i] = vReal[i] * curveScaling[i];

            // 平滑处理
            vReal[i] = smoothingFactor * previousSpectrum[i] + (1 - smoothingFactor) * vReal[i];

            vReal[i] = vReal[i] * fft_gain;

            // 限幅处理
            if (vReal[i] > 60)
                vReal[i] = 60;
            if (vReal[i] < 0)
                vReal[i] = 0;
            // 保存数据用于显示和平滑
            previousSpectrum[i] = vReal[i];
        }

        if (SAMPLES == 256)
            for (int i = 0; i < 128; i++)
            {
                // display.fillRect(i * 3, 75 - previousSpectrum[i], 2, previousSpectrum[i] + 1, TFT_BLACK);
                display.drawFastVLine(i * 3 + 1, 75 - previousSpectrum[i], previousSpectrum[i] + 1, TFT_BLACK);
                display.drawFastVLine(i * 3 + 2, 75 - previousSpectrum[i], previousSpectrum[i] + 1, TFT_BLACK);
            }
        else if (SAMPLES == 512)
        {
            uint8_t index = SAMPLES / 256;
            for (int i = 0; i < 128; i++)
            {
                // display.fillRect(i * 3, 75 - previousSpectrum[i], 2, previousSpectrum[i] + 1, TFT_BLACK);
                int value = (previousSpectrum[i * index] + previousSpectrum[(i * index) + 1]) * 0.5f;
                display.drawFastVLine(i * 3 + 1, 75 - value, value + 1, TFT_BLACK);
                display.drawFastVLine(i * 3 + 2, 75 - value, value + 1, TFT_BLACK);
            }
        }
        else
        {
            uint8_t index = SAMPLES / 256;
            for (int i = 0; i < 128; i++)
            {
                // display.fillRect(i * 3, 75 - previousSpectrum[i], 2, previousSpectrum[i] + 1, TFT_BLACK);
                display.drawFastVLine(i * 3 + 1, 75 - previousSpectrum[i * index], previousSpectrum[i * index] + 1, TFT_BLACK);
                display.drawFastVLine(i * 3 + 2, 75 - previousSpectrum[i * index], previousSpectrum[i * index] + 1, TFT_BLACK);
            }
        }
    }
    else
    {
        // 获取采样率（假设 output 是有效的音频解码器输出）
        int rate = 48000;
        if (output != nullptr)
            rate = output->GetRate();
        const float frame_time = 1.0f / 40.0f;                 // 每帧25ms
        int samples_per_col = (int)(rate * frame_time + 0.5f); // 每列对应的采样点数（四舍五入）
        if (hal.pref.getInt("line_samples", 0) > 0)
            samples_per_col = hal.pref.getInt("line_samples", 0);

        if (samples_per_col <= 0)
            samples_per_col = 1; // 防御处理

        // 静态变量：列缓冲和滚动索引（跨帧保持）
        static uint8_t min_vals[384];
        static uint8_t max_vals[384];
        static int start_col = 0;
        static bool first_frame = true;

        // 第一帧初始化所有列为基线（屏幕中间 y=45）
        if (first_frame)
        {
            for (int i = 0; i < 384; i++)
            {
                min_vals[i] = 45;
                max_vals[i] = 45;
            }
            first_frame = false;
        }

        // 原子读取当前写指针
        uint32_t current_write = write_index;

        // 计算新列对应采样点的起始索引（环形缓冲区）
        int start_sample = (current_write - samples_per_col + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;

        // 遍历采样点，找出该列的最小值和最大值
        int16_t min_sample = INT16_MAX;
        int16_t max_sample = INT16_MIN;
        for (int i = 0; i < samples_per_col; i++)
        {
            int idx = (start_sample + i) % RING_BUFFER_SIZE;
            int16_t val = ring_buffer[idx];
            if (val < min_sample)
                min_sample = val;
            if (val > max_sample)
                max_sample = val;
        }

        // 将采样值映射到屏幕Y坐标（0~60范围，基线偏移15）
        // 映射公式: y = 15 + (sample + 32768) * 60 / 65535
        int min_y = 15 + ((int32_t)(min_sample + 32768) * 60) / 65535;
        int max_y = 15 + ((int32_t)(max_sample + 32768) * 60) / 65535;

        // 限制在有效范围（15~75）
        if (min_y < 15)
            min_y = 15;
        if (min_y > 75)
            min_y = 75;
        if (max_y < 15)
            max_y = 15;
        if (max_y > 75)
            max_y = 75;
        if (min_y > max_y)
        {
            int tmp = min_y;
            min_y = max_y;
            max_y = tmp;
        } // 确保 min <= max

        // 将新列数据存入当前最左侧的位置（该位置即将被覆盖）
        min_vals[start_col] = min_y;
        max_vals[start_col] = max_y;

        // 更新起始列，使新列成为最右侧（滚动）
        start_col = (start_col + 1) % 384;

        // 绘制整个波形（从左到右）
        for (int x = 0; x < 384; x++)
        {
            int col = (start_col + x) % 384; // 当前x对应的列索引
            uint8_t y1 = min_vals[col];
            uint8_t y2 = max_vals[col];
            if (y1 == y2)
            {
                display.drawPixel(x, y1, TFT_BLACK);
            }
            else
            {
                display.drawFastVLine(x, y1, y2 - y1 + 1, TFT_BLACK);
            }
        }
    }

    d_time.fft_end = micros();

    if (lrcisload) //  && !user_stop
    {
        unsigned long currentTime = millis() - play_time_start - _lrcoffset - play_stop_time;
        checkAndUpdateLyrics(currentTime);

        drawScrollingLyrics(14, 110, lrc_max_x); // 第二行的Y坐标
    }

    // 电池
    if ((hal.pref.getBool(hal.get_char_sha_key("精准电量显示"), false) || hal.bat_info.soc != 255) && hal.VCC < 4300 && !hal.isCharging)
    {
        display.drawXBitmap(362, 0, getBatteryIcon(true), 20, 16, 0);
        display.fillRect(365, 6, getBatterysoc(), 4, TFT_BLACK);
        // int soc = (hal.VCC - 3200) * 13 / 1000;
        // for (int x = 365; x < 365 + soc; x++)
        // {
        //     display.drawFastVLine(x, 6, 4, TFT_BLACK);
        // }
    }
    else
        display.drawXBitmap(362, 0, getBatteryIcon(), 20, 16, 0);
    u8g2Fonts.setCursor(2, 12);
    u8g2Fonts.printf("%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);

    u8g2Fonts.setCursor(3, 165);
    if (loopPlay)
    {
        u8g2Fonts.printf("单曲");
    }
    else if (autoPlay)
    {
        if (randomPlay)
            u8g2Fonts.printf("随机");
        else
            u8g2Fonts.printf("顺序");
    }
    if (output != nullptr)
    {
        int rate = output->GetRate();
        int khz = rate / 1000;
        int frac = (rate % 1000) / 100; // 只取一位小数
        if (frac > 0)
            u8g2Fonts.printf(" %d.%dKHz|%dbit", khz, frac, output->GetBitsPerSample());
        else
            u8g2Fonts.printf(" %dKHz|%dbit", khz, output->GetBitsPerSample());
    }

    if (display_debug_mode)
    {
        static uint32_t last_update_time = 0;
        uint32_t now = millis();
        static char _buf[64];
        static int x;
        if (now - last_update_time > 200)
        { // 200ms更新一次
            last_update_time = now;
            float all = (float)(d_time.start - last_time.start) / 1000.0;
            fps = 1000.0 / all;
            float fft_time = (float)(last_time.fft_end - last_time.fft_start) / 1000.0;
            float other_time = ((float)(last_time.display_start - last_time.start) / 1000.0) - fft_time;
            float display_time = (float)(last_time.end - last_time.display_start) / 1000.0;
            sprintf(_buf, "%.1f|%.1f|%.1f|%d fps: %.1f", fft_time, other_time, display_time, xWasDelayed, fps);
            x = (SCREEN_WIDTH - u8g2Fonts.getUTF8Width(_buf)) / 2;
        }
        u8g2Fonts.setCursor(x, 165);
        u8g2Fonts.print(_buf);
    }

    char gain_buf[16];
    sprintf(gain_buf, "音量:%d", (uint16_t)(gain * 100.0));
    u8g2Fonts.setCursor(381 - u8g2Fonts.getUTF8Width(gain_buf), 165);
    u8g2Fonts.printf(gain_buf);

    d_time.display_start = micros();
    display.display();
    d_time.end = micros();
    last_time = d_time;
}

/**
 * @brief 根据文件类型设置对应的音频解码器
 * @param path 音频文件路径
 * @param source 音频文件源对象
 * @param out 音频输出对象
 * @return true：设置成功，false：设置失败
 * @note 根据文件扩展名选择MP3/FLAC/AAC/WAV/OPUS解码器
 */
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
        mp3_generator->RegisterMetadataCB(MDCallback, (void *)"MP3INFO");
        generator = mp3_generator;
        break;
    case Flac_Generator:
        flac_generator = new AudioGeneratorFLAC();
        flac_generator->RegisterMetadataCB(MDCallback, (void *)"FLACTAG");
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
                error("未能初始化音频解码器！");
                GUI::msgbox("错误", "未能初始化音频解码器！", 5);
                return false;
            }
        }
        else
        {
            if (!generator->begin(id3, out))
            {
                error("未能初始化音频解码器！");
                GUI::msgbox("错误", "未能初始化音频解码器！", 5);
                return false;
            }
        }
    }
    else
    {
        log_e("未能实例化音频解码器！");
        GUI::msgbox("错误", "未能实例化音频解码器！这可能是音乐文件格式不受支持导致的。", 5);
        return false;
    }
    return true;
}

/**
 * @brief 初始化音频播放器
 * @return true：初始化成功，false：初始化失败
 * @note 配置音频输出和解码器，重置ID3信息，更新播放计数
 */
bool AppMusicPlayer::player_set()
{
    play_count++;
    info.album = "---";
    info.performer = "---";
    info.title = "---";
    info.tlen = 0;
    backup_buff_updata = true;
    id3_tlen_received = false;
    app.play_time_start = millis();                           // 提前重置一次`,确保歌词正常显示
    memset(ring_buffer, 0, sizeof(float) * RING_BUFFER_SIZE); // 清环形缓冲区
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
        if (hal.pref.getBool(hal.get_char_sha_key("使用25/26/0输出"), true))
        {
            i2s_output = new AudioOutputI2S(0, 0, 8, apll);
            output = i2s_output;
            i2s_output->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
            i2s_output->SetMclk(false);
            i2s_output->set_ConsumeSample_CB(GetSampleCB);
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

// 初始化曲线缩放数组（在setup或初始化函数中调用）
void initCurveScaling()
{
    const int lowFreqCount = 5;     // 低频点数量
    const int transitionCount = 70; // 过渡区数量
    const float lowScale = 0.00011; // 低频压缩值
    const float highScale = 0.0009; // 高频压缩值

    for (int i = 0; i < app.SAMPLES / 2; i++)
    {
        if (i < lowFreqCount)
        {
            // 低频部分 - 强压缩
            app.curveScaling[i] = lowScale;
        }
        else if (i < lowFreqCount + transitionCount)
        {
            // 过渡区域 - 线性插值
            float ratio = (float)(i - lowFreqCount) / transitionCount;
            app.curveScaling[i] = lowScale + (highScale - lowScale) * ratio;
        }
        else
        {
            // 高频部分 - 弱压缩
            app.curveScaling[i] = highScale;
        }
    }
}

/**
 * @brief 音乐播放器主函数
 * @note 初始化播放器硬件和软件环境，启动播放任务，处理用户交互事件
 */
void AppMusicPlayer::setup()
{
    digitalWrite(PIN_DAC_EN, 1);
    hal.cheak_freq(240);
    display.setPowerMode(POWER_MODE_HPM);
    display.clearScreen();
    display.display();
    digitalWrite(PIN_DAC_XSMT, 1);

    SAMPLES = hal.pref.getUInt("fft_samples", 256);
    vReal = new float[SAMPLES];
    vImag = new float[SAMPLES];
    curveScaling = new float[SAMPLES / 2];
    previousSpectrum = new float[SAMPLES / 2];
    memset(previousSpectrum, 0, sizeof(float[SAMPLES / 2]));
    initCurveScaling();
    ring_buffer = (float *)ps_malloc(sizeof(float[RING_BUFFER_SIZE]));
    if (ring_buffer)
        memset(ring_buffer, 0, sizeof(float) * RING_BUFFER_SIZE);

    nodac = hal.pref.getBool(hal.get_char_sha_key("使用蜂鸣器输出"), false);
    _count = hal.pref.getInt("rst_count", -1);
    gain = hal.pref.getFloat("gain", 0.3);
    _lrcoffset = hal.pref.getInt("_lrcoffset", -50);
    apll = hal.pref.getBool(hal.get_char_sha_key("audio_pll"), true);
    bits_per_chan = hal.pref.getBool("bits_per_chan", true);
    display_debug_mode = hal.pref.getBool("music_debug", true);
    smoothingFactor = hal.pref.getFloat("fft_smooth_val", 0.7f);
    fft_gain = hal.pref.getFloat("fft_gain", 1.2f);

    loopPlay = hal.pref.getBool(hal.get_char_sha_key("单曲循环"), false);
    autoPlay = hal.pref.getBool(hal.get_char_sha_key("顺序播放"), true);
    randomPlay = hal.pref.getBool(hal.get_char_sha_key("随机播放"), false);

    exit = player_exit;
    deepsleep = player_deepsleep;
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 1;
    audioLogger = &Serial0;
    audio_control_sem = xSemaphoreCreateBinary(); // 创建二进制信号量
    xSemaphoreGive(audio_control_sem);            // 初始化为可用状态
    uint8_t run_index = 0;
    if (music_file == NULL)
    {
        String file = buf;
        if (file.endsWith(".mp3") || file.endsWith(".wav") ||
            file.endsWith(".aac") || file.endsWith(".opus") || file.endsWith(".flac"))
        {
            music_file = buf;
        }
        else
        {
            sprintf(buf, "%s", hal.pref.getString("music_file").c_str());
            music_file = buf;
        }
    }
    select_file();

    if (player_set())
        begin_player_task();
    else
        _play_end = true;
    show_display();
    _end = false;
    xLastWakeTime = xTaskGetTickCount();
    xFrequency = pdMS_TO_TICKS(hal.pref.getInt("xFrequency", 20)); // 20ms周期
    max_fps = 1000.0 / (float)xFrequency;
    unsigned long wait_time = millis();
    while (!_end && !need_deep_sleep)
    {
        if (hal.btnc.isPressing())
        {
            if (GUI::waitLongPress(PIN_BUTTONC))
            {
                player_menu();
            }
            else
            {
                if (filelist_ok)
                {
                    if (!music_list_menu(true))
                    {
                        while (hal.btnc.isPressing())
                        {
                            delay(1);
                        }
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
                if (player_set())
                {
                    // sem();
                    begin_player_task();
                }
                else
                    _play_end = true;
            }
            while (hal.btnc.isPressing())
            {
                delay(1);
            }
            show_display();
        }
        if (app_exit)
            return;
        if (hal.btnr.isPressing())
        {
            if (GUI::waitLongPress(hal.btnr.pin()))
            {
                next_song(true, true);
                backup_buff_updata = true;
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
                backup_buff_updata = true;
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
                if (gain < 0.06)
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
            backup_buff_updata = true;
            show_display();
        }
        else
            wait_time = millis();
        show_display();
        if ((millis() - wait_time > 30000) && _play_end)
        {
            hal.wait_input();
            wait_time = millis();
        }
        xWasDelayed = xTaskDelayUntil(&xLastWakeTime, xFrequency);
        if (fps > max_fps && xWasDelayed == pdFALSE)
        { // 如果是超帧丢步,则重置xLastWakeTime
            xLastWakeTime = xTaskGetTickCount();
        }
    }
    hal.cheak_freq(160, true);
}
