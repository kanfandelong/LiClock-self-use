#pragma GCC optimize("O3")

#include "AppManager.h"
#include "ESP8266Audio.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <arduinoFFT.h>
#include "esp_dsp.h"
#include <atomic>

static const uint8_t APP_OnlineMusic_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0xf8, 0x03,
    0x00, 0x00, 0xfc, 0x03, 0x00, 0x00, 0xfe, 0x03, 0x00, 0x80, 0xff, 0x03,
    0x00, 0xc0, 0xff, 0x03, 0x00, 0xf0, 0xff, 0x03, 0x00, 0xf8, 0xff, 0x03,
    0x00, 0xfc, 0xff, 0x03, 0x00, 0x7e, 0xfc, 0x03, 0x00, 0x3f, 0xf8, 0x03,
    0x80, 0x1f, 0xf8, 0x03, 0x80, 0x1f, 0xf8, 0x03, 0x80, 0x1f, 0xf8, 0x03,
    0x80, 0x1f, 0xf8, 0x03, 0x80, 0x1f, 0xf8, 0x03, 0x80, 0x1f, 0xf8, 0x03,
    0x00, 0x3f, 0xfc, 0x03, 0x00, 0x7e, 0xfc, 0x03, 0x00, 0xf8, 0xff, 0x03,
    0x00, 0xf0, 0xff, 0x03, 0x00, 0xc0, 0xff, 0x03, 0x00, 0x80, 0xff, 0x03,
    0x00, 0x00, 0xfe, 0x03, 0x00, 0x00, 0xfc, 0x03, 0x00, 0x00, 0xf8, 0x03,
    0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t pause_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t play_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00,
    0x00, 0x3c, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00,
    0x00, 0x3f, 0xf0, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x3f, 0xff, 0x00,
    0x00, 0x3f, 0xff, 0xc0, 0x00, 0x3f, 0xff, 0xf0, 0x00, 0x3f, 0xff, 0xfc,
    0x00, 0x3f, 0xff, 0xff, 0x00, 0x3f, 0xff, 0xfc, 0x00, 0x3f, 0xff, 0xf0,
    0x00, 0x3f, 0xff, 0xc0, 0x00, 0x3f, 0xff, 0x00, 0x00, 0x3f, 0xfc, 0x00,
    0x00, 0x3f, 0xf0, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x3f, 0x00, 0x00,
    0x00, 0x3c, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define MAX_ONLINE_SONGS 1024
#define MAX_PLAYLISTS 20

// ==================== 数据结构 ====================

typedef struct
{
    char *url = nullptr;
    char *title = nullptr;
    char *author = nullptr;
    unsigned long duration = 0; // 歌曲时长(秒),0表示未知
} onlinesong;

typedef struct
{
    unsigned long timeMs = 0;
    String text;
} LyricLine;

typedef struct
{
    unsigned long start = 0;
    unsigned long fft_start = 0;
    unsigned long fft_end = 0;
    unsigned long display_start = 0;
    unsigned long end = 0;
} drawtime;

// ==================== 全局变量(任务/回调需要) ====================

// 音频对象(全局以便 task 和回调访问)
AudioFileSourceHTTPStream *httpStream = nullptr;
AudioGeneratorMP3 *mp3 = nullptr;
AudioOutputI2S *i2sOut = nullptr;

// 任务控制
SemaphoreHandle_t audio_control_sem = NULL;
TaskHandle_t player_loop_task_handle = NULL;
std::atomic<bool> stop_requested{false};
std::atomic<bool> _play_end{false};
std::atomic<bool> fftProcessing{false};

// FFT 环形缓冲区
static const size_t RING_BUFFER_SIZE = 8192;
float *ring_buffer = nullptr;
uint32_t write_index = 0;
volatile bool fft_data_ready = false;

// I2S 采样回调
#ifdef CONFIG_DAC_32bit
void GetSampleCB(int32_t sample[2])
{
    float left = (float)(sample[0] >> 16);
    float right = (float)(sample[1] >> 16);
    float mono = (left + right) * 0.5f;
    ring_buffer[write_index] = mono;
    write_index = (write_index + 1) & (RING_BUFFER_SIZE - 1);
}
#else
void GetSampleCB(int16_t sample[2])
{
    ring_buffer[write_index] = (float)sample[0];
    write_index = (write_index + 1) & (RING_BUFFER_SIZE - 1);
}
#endif

// ==================== 音频解码任务 ====================

static void player_loop(void *)
{
    log_i("音频解码任务已启动");
    while (!stop_requested)
    {
        if (xSemaphoreTake(audio_control_sem, 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (mp3 && mp3->isRunning())
            {
                if (!mp3->loop())
                {
                    mp3->stop();
                    _play_end = true;
                    xSemaphoreGive(audio_control_sem);
                    break;
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
            delay(5);
        }
    }
    if (mp3 && mp3->isRunning())
        mp3->stop();
    player_loop_task_handle = NULL;
    log_i("音频解码任务结束");
    vTaskDelete(NULL);
}

// ==================== 元数据回调 ====================

static void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
    (void)cbData;
    (void)isUnicode;
    char s1[32], s2[64];
    strncpy(s1, type, 31);
    s1[31] = 0;
    strncpy(s2, string, 63);
    s2[63] = 0;
    log_i("Metadata %s: %s", s1, s2);
}

static void StatusCallback(void *cbData, int code, const char *string)
{
    (void)cbData;
    log_i("Status: %d %s", code, string);
}


// ==================== 类定义 ====================

class AppOnlineMusic : public AppBase
{
private:
    onlinesong *songs = nullptr;
    int songs_size = 0;
    int onlineSongCount = 0;
    int currentSongIndex = 0;
    bool isPlaying = false;
    bool _end = false;
    int currentVolume = 15;
    unsigned long displayTime = 0;
    int currentPlaylistIndex = -1;

    String playlistNames[MAX_PLAYLISTS];
    String playlistUrls[MAX_PLAYLISTS];
    int playlistCount = 0;

    String currentSongTitle = "---";
    String currentSongAuthor = "";
    String currentSongUrl = "";

    // ---- 播放时间 ----
    unsigned long playTimeStart = 0;

    // ---- FFT ----
    uint16_t SAMPLES = 256;
    float *vReal = nullptr;
    float *vImag = nullptr;
    float *previousSpectrum = nullptr;
    float *curveScaling = nullptr;
    float *wind = nullptr;
    float *fft_data = nullptr;
    float smoothingFactor = 0.7f;
    float FFT_A_spectrum_smoothness = 2000.0f;
    float FFT_A_amplitude = 40.0f;
    float fft_gain = 1.0f;
    bool use_log = false;

    // ---- 歌词 ----
    LyricLine *lyricArray = nullptr;
    int totalLyricLines = 0;
    int currentLyricIndex = 0;
    int lastLyricIndex = -1;
    char currentLyric[3][128];
    String oldLyrics[3];
    String newLyrics[3];
    int oldLyricIndex = -1;
    int newLyricIndex = 0;
    bool scrolling = false;
    int scrollOffset = 0;
    int scrollDirection = 0;
    unsigned long scrollStartTime = 0;
    const int SCROLL_DURATION = 350;
    const int LINE_HEIGHT = 17;

    // ---- 帧时间 ----
    drawtime d_time;
    drawtime last_time;
    float fps = 0;
    TickType_t xLastWakeTime;
    TickType_t xFrequency = pdMS_TO_TICKS(25);
    BaseType_t xWasDelayed;

    // ---- 按键标志 ----
    volatile bool flag_btnc_click = false;
    volatile bool flag_btnc_long = false;
    volatile bool flag_btnr_click = false;
    volatile bool flag_btnr_long = false;
    volatile bool flag_btnl_click = false;
    volatile bool flag_btnl_long = false;

    void cleanup();
    void initFFT();
    void deinitFFT();

    // 歌词
    void loadLyrics(const char *path);
    int getLyricIndex(unsigned long currentTime);
    void getLyricLines(int index, String lyrics[]);
    void startScrollAnimation(int direction);
    bool updateScrollAnimation();
    void drawScrollingLyrics(int x, int y, int max_x = MAX_X);
    void checkAndUpdateLyrics(unsigned long currentTime);

    // 按键回调
    static void onBtnC_Click(void *param);
    static void onBtnC_LongPressStart(void *param);
    static void onBtnR_Click(void *param);
    static void onBtnR_LongPressStart(void *param);
    static void onBtnL_Click(void *param);
    static void onBtnL_LongPressStart(void *param);

public:
    AppOnlineMusic()
    {
        name = "onlinemusic";
        title = "在线音乐";
        description = "在线播放歌单音乐";
        noDefaultEvent = true;
        peripherals_requested = PERIPHERALS_SD_BIT;
        image = APP_OnlineMusic_bits;
    }

    void set();
    void setup();
    void openMenu();
    void playlistMenu();
    void addPlaylist();
    void editPlaylist();
    void deletePlaylist();
    void loadPlaylists();
    bool savePlaylists();
    void loadOnlinePlaylist(const char *url);
    bool loadPlaylistCache(int playlistIdx);
    void savePlaylistCache(int playlistIdx);
    String resolveRedirect(const String &url, int maxRedirects = 5);
    void playSong(int index);
    void stopSong();
    void beginPlayerTask();
    void deletePlaytask();
    void showPlaylist(int page);
    void showDisplay();
    void playNext();
    void playPrevious();
};

static AppOnlineMusic onlineMusicApp;

// ==================== FFT 初始化/清理 ====================

void AppOnlineMusic::initFFT()
{
    SAMPLES = hal.pref.getUInt("fft_samples", 256);
    // 确保 SAMPLES 是 2 的幂
    uint16_t p = 1;
    while (p < SAMPLES) p <<= 1;
    SAMPLES = p;
    if (SAMPLES < 64) SAMPLES = 64;
    if (SAMPLES > 1024) SAMPLES = 1024;

    vReal = new float[SAMPLES];
    vImag = new float[SAMPLES]();
    previousSpectrum = new float[SAMPLES / 2]();
    curveScaling = new float[SAMPLES / 2];
    wind = (float *)heap_caps_aligned_alloc(16, SAMPLES * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    fft_data = (float *)heap_caps_aligned_alloc(16, 2 * SAMPLES * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

    if (curveScaling && wind && fft_data)
    {
        const int lowFreqCount = 5;
        const int transitionCount = 70;
        const float lowScale = 0.00011f;
        const float highScale = 0.0009f;
        for (int i = 0; i < SAMPLES / 2; i++)
        {
            if (i < lowFreqCount)
                curveScaling[i] = lowScale;
            else if (i < lowFreqCount + transitionCount)
            {
                float ratio = (float)(i - lowFreqCount) / transitionCount;
                curveScaling[i] = lowScale + (highScale - lowScale) * ratio;
            }
            else
                curveScaling[i] = highScale;
        }
        dsps_fft2r_init_fc32(NULL, SAMPLES);
        dsps_wind_hann_f32(wind, SAMPLES);
    }

    smoothingFactor = hal.pref.getFloat("fft_smooth_val", 0.7f);
    fft_gain = hal.pref.getFloat("fft_gain", 1.2f);
    use_log = hal.pref.getBool("fft_log", false);
}

void AppOnlineMusic::deinitFFT()
{
    delete[] vReal; vReal = nullptr;
    delete[] vImag; vImag = nullptr;
    delete[] previousSpectrum; previousSpectrum = nullptr;
    delete[] curveScaling; curveScaling = nullptr;
    if (wind) { free(wind); wind = nullptr; }
    if (fft_data) { free(fft_data); fft_data = nullptr; }
}

// ==================== 歌词 ====================

void AppOnlineMusic::loadLyrics(const char *path)
{
    if (lyricArray)
    {
        delete[] lyricArray;
        lyricArray = nullptr;
    }
    totalLyricLines = 0;
    currentLyricIndex = 0;
    lastLyricIndex = -1;

    // 简单 LRC 文件加载
    if (!path || !hal.exists(path))
        return;

    File file = hal.open(path, "r");
    if (!file) return;

    // 先统计行数
    int count = 0;
    String line;
    while (file.available())
    {
        line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("["))
        {
            int rb = line.indexOf(']');
            String temp = line.substring(rb + 1);
            temp.trim();
            if (rb != -1 && temp.length() > 0)
                count++;
        }
    }
    if (count == 0)
    {
        file.close();
        return;
    }

    file.seek(0);
    lyricArray = new LyricLine[count];
    int idx = 0;
    while (file.available() && idx < count)
    {
        line = file.readStringUntil('\n');
        line.trim();
        if (!line.startsWith("[")) continue;
        int rb = line.indexOf(']');
        if (rb == -1) continue;
        String timeStr = line.substring(1, rb);
        String text = line.substring(rb + 1);
        text.trim();
        if (text.length() == 0) continue;

        int colon = timeStr.indexOf(':');
        int dot = timeStr.indexOf('.');
        if (colon == -1 || dot == -1) continue;

        int minutes = timeStr.substring(0, colon).toInt();
        int seconds = timeStr.substring(colon + 1, dot).toInt();
        String msStr = timeStr.substring(dot + 1);
        while (msStr.length() < 3) msStr += "0";
        int ms = msStr.substring(0, 3).toInt();
        lyricArray[idx].timeMs = minutes * 60000UL + seconds * 1000UL + ms;
        lyricArray[idx].text = text;
        idx++;
    }
    file.close();
    totalLyricLines = idx;
    log_i("已加载 %d 行歌词", totalLyricLines);

    // 初始化歌词显示
    currentLyricIndex = 0;
    lastLyricIndex = -1;
    for (int i = 0; i < 3; i++)
    {
        currentLyric[i][0] = '\0';
        oldLyrics[i] = "";
        newLyrics[i] = "";
    }
    if (totalLyricLines > 0)
    {
        strncpy(currentLyric[1], lyricArray[0].text.c_str(), 127);
        currentLyric[1][127] = '\0';
        if (totalLyricLines > 1)
        {
            strncpy(currentLyric[2], lyricArray[1].text.c_str(), 127);
            currentLyric[2][127] = '\0';
        }
    }
    scrolling = false;
}

int AppOnlineMusic::getLyricIndex(unsigned long currentTime)
{
    if (totalLyricLines == 0) return 0;
    if (currentLyricIndex >= totalLyricLines) currentLyricIndex = 0;

    if (currentLyricIndex > 0 && currentTime < lyricArray[currentLyricIndex].timeMs)
    {
        while (currentLyricIndex > 0 && lyricArray[currentLyricIndex].timeMs > currentTime)
            currentLyricIndex--;
    }
    else
    {
        int ti = 0;
        while (ti < totalLyricLines && lyricArray[ti].timeMs <= currentTime)
            ti++;
        currentLyricIndex = (ti > 0) ? ti - 1 : 0;
    }
    return currentLyricIndex;
}

void AppOnlineMusic::getLyricLines(int index, String lyrics[])
{
    lyrics[0] = (index > 0 && totalLyricLines > 0) ? lyricArray[index - 1].text : "-";
    lyrics[1] = (totalLyricLines > 0) ? lyricArray[index].text : "-";
    lyrics[2] = (index + 1 < totalLyricLines) ? lyricArray[index + 1].text : "-";
}

void AppOnlineMusic::startScrollAnimation(int direction)
{
    if (scrolling) return;
    for (int i = 0; i < 3; i++) oldLyrics[i] = currentLyric[i];
    oldLyricIndex = lastLyricIndex;
    getLyricLines(currentLyricIndex, newLyrics);
    newLyricIndex = currentLyricIndex;
    scrolling = true;
    scrollDirection = direction;
    scrollOffset = 0;
    scrollStartTime = millis();
}

bool AppOnlineMusic::updateScrollAnimation()
{
    if (!scrolling) return true;
    unsigned long elapsed = millis() - scrollStartTime;
    if (elapsed >= (unsigned long)SCROLL_DURATION)
    {
        scrolling = false;
        scrollOffset = LINE_HEIGHT;
        for (int i = 0; i < 3; i++)
        {
            strncpy(currentLyric[i], newLyrics[i].c_str(), 127);
            currentLyric[i][127] = '\0';
        }
        lastLyricIndex = currentLyricIndex;
        return true;
    }
    float progress = (float)elapsed / SCROLL_DURATION;
    progress = 1.0f - powf(1.0f - progress, 3);
    scrollOffset = (int)(progress * LINE_HEIGHT);
    return false;
}

void AppOnlineMusic::drawScrollingLyrics(int x, int y, int max_x)
{
    if (totalLyricLines <= 0) return;

    if (!scrolling)
    {
        int ly = y - LINE_HEIGHT;
        u8g2Fonts.setCursor(x, ly);
        u8g2Fonts.print(currentLyric[0]);

        ly = y;
        String curLine = "> " + String(currentLyric[1]);
        u8g2Fonts.setCursor(2, ly);
        u8g2Fonts.print(curLine);

        ly = y + LINE_HEIGHT;
        u8g2Fonts.setCursor(x, ly);
        u8g2Fonts.print(currentLyric[2]);
    }
    else
    {
        int oldOff = scrollDirection > 0 ? -scrollOffset : scrollOffset;
        int newOff = scrollDirection > 0 ? (LINE_HEIGHT - scrollOffset) : -(LINE_HEIGHT - scrollOffset);

        int ly = y - LINE_HEIGHT + oldOff;
        u8g2Fonts.setCursor(x, ly);
        u8g2Fonts.print(oldLyrics[0]);

        ly = y - LINE_HEIGHT + newOff;
        u8g2Fonts.setCursor(x, ly);
        u8g2Fonts.print(newLyrics[0]);

        ly = y + newOff;
        u8g2Fonts.setCursor(2, ly);
        u8g2Fonts.print("> " + newLyrics[1]);

        ly = y + LINE_HEIGHT + newOff;
        u8g2Fonts.setCursor(x, ly);
        u8g2Fonts.print(newLyrics[2]);
    }
}

void AppOnlineMusic::checkAndUpdateLyrics(unsigned long currentTime)
{
    if (totalLyricLines == 0) return;
    int newIndex = getLyricIndex(currentTime);
    if (newIndex != lastLyricIndex)
    {
        if (lastLyricIndex != -1)
        {
            unsigned long diff = lyricArray[newIndex].timeMs - lyricArray[lastLyricIndex].timeMs;
            if (!scrolling && diff >= (unsigned long)SCROLL_DURATION)
                startScrollAnimation(1);
            else
            {
                for (int i = 0; i < 3; i++) oldLyrics[i] = currentLyric[i];
                oldLyricIndex = lastLyricIndex;
                getLyricLines(currentLyricIndex, newLyrics);
                newLyricIndex = currentLyricIndex;
            }
        }
        else
        {
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
    if (scrolling) updateScrollAnimation();
}

// ==================== 按键回调 ====================

void AppOnlineMusic::onBtnC_Click(void *param)
{
    static_cast<AppOnlineMusic *>(param)->flag_btnc_click = true;
}
void AppOnlineMusic::onBtnC_LongPressStart(void *param)
{
    static_cast<AppOnlineMusic *>(param)->flag_btnc_long = true;
}
void AppOnlineMusic::onBtnR_Click(void *param)
{
    static_cast<AppOnlineMusic *>(param)->flag_btnr_click = true;
}
void AppOnlineMusic::onBtnR_LongPressStart(void *param)
{
    static_cast<AppOnlineMusic *>(param)->flag_btnr_long = true;
}
void AppOnlineMusic::onBtnL_Click(void *param)
{
    static_cast<AppOnlineMusic *>(param)->flag_btnl_click = true;
}
void AppOnlineMusic::onBtnL_LongPressStart(void *param)
{
    static_cast<AppOnlineMusic *>(param)->flag_btnl_long = true;
}

// ==================== 任务控制 ====================

void AppOnlineMusic::beginPlayerTask()
{
    _play_end = false;
    uint8_t core = xPortGetCoreID();
    uint32_t stackSize = 8192;
    if (core == 0)
        xTaskCreatePinnedToCore(player_loop, "online_play", stackSize, NULL, 8, &player_loop_task_handle, 1);
    else
        xTaskCreatePinnedToCore(player_loop, "online_play", stackSize, NULL, 8, &player_loop_task_handle, 0);
}

void AppOnlineMusic::deletePlaytask()
{
    if (_play_end || player_loop_task_handle == NULL)
        return;

    if (i2sOut)
        i2sOut->SetTimeout(0);

    stop_requested = true;
    TickType_t start = xTaskGetTickCount();
    while (player_loop_task_handle != NULL && (xTaskGetTickCount() - start) < pdMS_TO_TICKS(3000))
        vTaskDelay(pdMS_TO_TICKS(10));

    if (player_loop_task_handle != NULL)
    {
        log_w("强制删除播放任务");
        vTaskDelete(player_loop_task_handle);
        player_loop_task_handle = NULL;
        xSemaphoreTake(audio_control_sem, 0);
        xSemaphoreGive(audio_control_sem);
    }

    if (mp3 && mp3->isRunning())
        mp3->stop();

    stop_requested = false;
}

// ==================== 基础方法 ====================

void AppOnlineMusic::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
    log_i("APP %s, version: %s", name, "1.1.0");
}

void AppOnlineMusic::cleanup()
{
    stopSong();
    deletePlaytask();
    deinitFFT();

    if (ring_buffer)
    {
        free(ring_buffer);
        ring_buffer = nullptr;
    }
    if (i2sOut)
    {
        delete i2sOut;
        i2sOut = nullptr;
    }
    if (songs)
    {
        for (int i = 0; i < songs_size; i++)
        {
            if (songs[i].title) free(songs[i].title);
            if (songs[i].author) free(songs[i].author);
            if (songs[i].url) free(songs[i].url);
        }
        free(songs);
        songs = nullptr;
    }
    if (lyricArray)
    {
        delete[] lyricArray;
        lyricArray = nullptr;
    }
    totalLyricLines = 0;

    WiFi.disconnect(true);
    hal.can_light_sleep = true;
}

// ==================== 主设置 ====================

void AppOnlineMusic::setup()
{
    display.clearScreen();
    display.display();

    hal.can_light_sleep = false;
    _end = false;
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 1;

    exit = []()
    {
        onlineMusicApp.cleanup();
        player_loop_task_handle = NULL;
    };
    deepsleep = []()
    {
        digitalWrite(PIN_DAC_XSMT, 0);
        digitalWrite(PIN_DAC_EN, 0);
        onlineMusicApp.cleanup();
        player_loop_task_handle = NULL;
    };

    GUI::info_msgbox("提示", "正在连接 WIFI...");
    if (!hal.autoConnectWiFi(true))
    {
        GUI::msgbox("错误", "WIFI连接失败");
        hal.can_light_sleep = true;
        appManager.goBack();
        return;
    }
    GUI::info_msgbox("提示", "WIFI连接成功");

    // ---- 初始化音频输出 ----
    i2sOut = new AudioOutputI2S(0, 0, 8, true);
    i2sOut->SetGain(currentVolume / 21.0f);
    i2sOut->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
    i2sOut->SetMclk(false);
    i2sOut->Set_bits_per_chan(I2S_BITS_PER_CHAN_32BIT);
    i2sOut->set_ConsumeSample_CB(GetSampleCB);
    digitalWrite(PIN_DAC_EN, 1);
    hal.cheak_freq(240);
    display.setPowerMode(POWER_MODE_HPM);
    digitalWrite(PIN_DAC_XSMT, 1);

    // ---- 初始化 FFT ----
    ring_buffer = (float *)ps_malloc(sizeof(float[RING_BUFFER_SIZE]));
    if (ring_buffer)
        memset(ring_buffer, 0, sizeof(float) * RING_BUFFER_SIZE);
    initFFT();

    // ---- 创建信号量 ----
    audio_control_sem = xSemaphoreCreateBinary();
    if (audio_control_sem)
        xSemaphoreGive(audio_control_sem);

    // ---- 注册按键回调 ----
    hal.btnc.attachClick(onBtnC_Click, this);
    hal.btnc.attachLongPressStart(onBtnC_LongPressStart, this);
    hal.btnr.attachClick(onBtnR_Click, this);
    hal.btnr.attachLongPressStart(onBtnR_LongPressStart, this);
    hal.btnl.attachClick(onBtnL_Click, this);
    hal.btnl.attachLongPressStart(onBtnL_LongPressStart, this);

    // ---- 加载歌单 ----
    loadPlaylists();
    if (playlistCount == 0)
    {
        playlistNames[0] = "网易云音乐";
        playlistUrls[0] = "https://meting.xcnahida.cn/meting/api?server=netease&type=playlist&id=7031310463";
        playlistNames[1] = "本地api测试";
        playlistUrls[1] = hal.pref.getString("test_url", "https://meting.xcnahida.cn/meting/api?server=netease&type=url&id=2125045481");
        playlistCount = 2;
        savePlaylists();
    }

    // ---- 进入歌单选择 ----
    openMenu();

    if (_end)
    {
        cleanup();
        appManager.goBack();
        return;
    }

    // ---- 主循环 ----
    if (isPlaying)
        showDisplay();

    xLastWakeTime = xTaskGetTickCount();

    while (!_end)
    {
        // 播放完毕自动切歌
        if (_play_end && isPlaying)
        {
            _play_end = false;
            stopSong();
            playNext();
            showDisplay();
        }

        // 按键 C: 单击=歌单选择, 长按=主菜单
        if (flag_btnc_click)
        {
            flag_btnc_click = false;
            if (onlineSongCount > 0)
            {
                int page = (currentSongIndex / 8) + 1;
                showPlaylist(page);
                showDisplay();
            }
        }
        if (flag_btnc_long)
        {
            flag_btnc_long = false;
            openMenu();
            showDisplay();
        }

        // 按键 R: 单击=音量+, 长按=下一首
        if (flag_btnr_click)
        {
            flag_btnr_click = false;
            if (currentVolume < 21) currentVolume++;
            if (i2sOut) i2sOut->SetGain(currentVolume / 21.0f);
            showDisplay();
        }
        if (flag_btnr_long)
        {
            flag_btnr_long = false;
            playNext();
            showDisplay();
        }

        // 按键 L: 单击=音量-, 长按=上一首
        if (flag_btnl_click)
        {
            flag_btnl_click = false;
            if (currentVolume > 0) currentVolume--;
            if (i2sOut) i2sOut->SetGain(currentVolume / 21.0f);
            showDisplay();
        }
        if (flag_btnl_long)
        {
            flag_btnl_long = false;
            playPrevious();
            showDisplay();
        }

        // 定时刷新显示
        showDisplay();

        xWasDelayed = xTaskDelayUntil(&xLastWakeTime, xFrequency);
        float maxFps = 40.0f;
        if (fps > maxFps && xWasDelayed == pdFALSE)
            xLastWakeTime = xTaskGetTickCount();
    }

    cleanup();
    appManager.goBack();
}

// ==================== 主菜单 ====================

void AppOnlineMusic::openMenu()
{
    int selected = 0;
    bool end = false;

    while (!end && !_end)
    {
        static const menu_select mainOpts[] = {
            {false, "< 返回", nullptr},
            {false, "播放歌单", nullptr},
            {false, "添加歌单", nullptr},
            {false, "编辑歌单", nullptr},
            {false, "删除歌单", nullptr},
            {false, "选择歌曲", nullptr},
            {false, "音量调节", nullptr},
            {false, "停止播放", nullptr},
            {false, "退出应用", nullptr},
            {false, NULL, nullptr}};

        selected = GUI::select_menu("在线音乐", mainOpts, selected);

        switch (selected)
        {
        case 0:
            end = true;
            break;
        case 1:
            playlistMenu();
            end = true;
            break;
        case 2:
            addPlaylist();
            break;
        case 3:
            editPlaylist();
            break;
        case 4:
            deletePlaylist();
            break;
        case 5:
            if (onlineSongCount > 0)
            {
                int page = (currentSongIndex / 8) + 1;
                showPlaylist(page);
                end = true;
            }
            else
            {
                GUI::info_msgbox("提示", "请先加载歌单");
            }
            break;
        case 6:
            currentVolume = GUI::msgbox_number("音量 0-21", 2, currentVolume);
            if (currentVolume > 21) currentVolume = 21;
            if (currentVolume < 0) currentVolume = 0;
            if (i2sOut) i2sOut->SetGain(currentVolume / 21.0f);
            break;
        case 7:
            stopSong();
            GUI::info_msgbox("提示", "已停止播放");
            break;
        case 8:
            _end = true;
            end = true;
            break;
        default:
            break;
        }
    }
}

// ==================== 歌单管理 ====================

void AppOnlineMusic::playlistMenu()
{
    if (playlistCount == 0)
    {
        GUI::msgbox("提示", "暂无歌单，请先添加");
        return;
    }

    int selected = 0;
    bool end = false;

    while (!end)
    {
        static menu_item items[12];
        items[0].title = "返回";
        items[0].icon = NULL;

        for (int i = 0; i < playlistCount && i < 10; i++)
        {
            items[i + 1].title = playlistNames[i].c_str();
            items[i + 1].icon = NULL;
        }
        items[playlistCount + 1].title = NULL;
        items[playlistCount + 1].icon = NULL;

        selected = GUI::menu("选择歌单", items, 8, 8, selected);

        if (selected == 0)
        {
            end = true;
        }
        else if (selected >= 1 && selected <= playlistCount)
        {
            int plIdx = selected - 1;
            currentPlaylistIndex = plIdx;
            bool loaded = false;

            if (loadPlaylistCache(plIdx))
            {
                if (!GUI::msgbox_yn("缓存可用", "使用缓存还是重新加载?", "使用缓存", "重新加载"))
                {
                    onlineSongCount = 0;
                    GUI::info_msgbox("提示", "正在加载歌单...");
                    loadOnlinePlaylist(playlistUrls[plIdx].c_str());
                    if (onlineSongCount > 0)
                        savePlaylistCache(plIdx);
                }
                loaded = onlineSongCount > 0;
            }
            else
            {
                GUI::info_msgbox("提示", "正在加载歌单...");
                loadOnlinePlaylist(playlistUrls[plIdx].c_str());
                if (onlineSongCount > 0)
                {
                    savePlaylistCache(plIdx);
                    loaded = true;
                }
            }

            if (loaded)
            {
                currentSongIndex = 0;
                playSong(currentSongIndex);
            }
            else
            {
                GUI::msgbox("错误", "歌单加载失败");
            }
            end = true;
        }
    }
}

void AppOnlineMusic::addPlaylist()
{
    if (playlistCount >= MAX_PLAYLISTS)
    {
        GUI::msgbox("错误", "歌单已满");
        return;
    }

    int64_t playlistId = GUI::msgbox_number64("输入网易云歌单ID", 12, 7031310463LL);
    if (playlistId <= 0)
    {
        GUI::info_msgbox("提示", "未输入ID");
        return;
    }

    char urlBuf[256];
    snprintf(urlBuf, sizeof(urlBuf),
             "https://meting.xcnahida.cn/meting/api?server=netease&type=playlist&id=%lld",
             playlistId);

    char nameBuf[64];
    snprintf(nameBuf, sizeof(nameBuf), "歌单%lld", playlistId);

    playlistNames[playlistCount] = nameBuf;
    playlistUrls[playlistCount] = urlBuf;
    playlistCount++;
    if (savePlaylists())
        GUI::info_msgbox("提示", "添加成功");
}

void AppOnlineMusic::editPlaylist()
{
    if (playlistCount == 0)
    {
        GUI::msgbox("提示", "暂无歌单可编辑");
        return;
    }

    int selected = 0;
    bool end = false;

    while (!end)
    {
        static menu_item items[12];
        items[0].title = "返回";
        items[0].icon = NULL;

        for (int i = 0; i < playlistCount && i < 10; i++)
        {
            items[i + 1].title = playlistNames[i].c_str();
            items[i + 1].icon = NULL;
        }
        items[playlistCount + 1].title = NULL;
        items[playlistCount + 1].icon = NULL;

        selected = GUI::menu("编辑歌单", items, 8, 8, selected);

        if (selected == 0)
        {
            end = true;
        }
        else if (selected >= 1 && selected <= playlistCount)
        {
            int idx = selected - 1;
            int64_t playlistId = GUI::msgbox_number64("输入网易云歌单ID", 12, 7031310463ULL);
            if (playlistId <= 0)
            {
                GUI::info_msgbox("提示", "未输入ID");
                end = true;
                break;
            }

            char urlBuf[256];
            snprintf(urlBuf, sizeof(urlBuf),
                     "https://meting.xcnahida.cn/meting/api?server=netease&type=playlist&id=%lld",
                     playlistId);
            playlistUrls[idx] = urlBuf;

            char nameBuf[64];
            snprintf(nameBuf, sizeof(nameBuf), "歌单%lld", playlistId);
            playlistNames[idx] = nameBuf;

            if (savePlaylists())
                GUI::info_msgbox("提示", "修改成功");
            end = true;
        }
    }
}

void AppOnlineMusic::deletePlaylist()
{
    if (playlistCount == 0)
    {
        GUI::msgbox("提示", "暂无歌单");
        return;
    }

    int selected = 0;
    bool end = false;

    while (!end)
    {
        static menu_item items[12];
        items[0].title = "返回";
        items[0].icon = NULL;

        for (int i = 0; i < playlistCount && i < 10; i++)
        {
            items[i + 1].title = playlistNames[i].c_str();
            items[i + 1].icon = NULL;
        }
        items[playlistCount + 1].title = NULL;
        items[playlistCount + 1].icon = NULL;

        selected = GUI::menu("删除歌单", items, 8, 8, selected);

        if (selected == 0)
        {
            end = true;
        }
        else if (selected >= 1 && selected <= playlistCount)
        {
            if (GUI::msgbox_yn("确认", "确定要删除该歌单吗?", "确认", "取消"))
            {
                int idx = selected - 1;
                for (int i = idx; i < playlistCount - 1; i++)
                {
                    playlistNames[i] = playlistNames[i + 1];
                    playlistUrls[i] = playlistUrls[i + 1];
                }
                playlistCount--;
                if (savePlaylists())
                    GUI::info_msgbox("提示", "删除成功");
            }
        }
    }
}

// ==================== 歌单持久化 ====================

void AppOnlineMusic::loadPlaylists()
{
    String dataStr = hal.pref.getString(hal.get_char_sha_key("online_playlists"), "");
    if (dataStr.length() == 0) return;

    size_t jsonSize = JSON_ARRAY_SIZE(MAX_PLAYLISTS * 2) + MAX_PLAYLISTS * 2 * 128;
    DynamicJsonDocument doc(jsonSize);
    DeserializationError error = deserializeJson(doc, dataStr);
    if (error) return;

    playlistCount = 0;
    for (int i = 0; i < MAX_PLAYLISTS; i++)
    {
        if (!doc[i].containsKey("name") || !doc[i].containsKey("url")) break;
        playlistNames[i] = doc[i]["name"].as<String>();
        playlistUrls[i] = doc[i]["url"].as<String>();
        playlistCount++;
    }
}

bool AppOnlineMusic::savePlaylists()
{
    size_t jsonSize = JSON_ARRAY_SIZE(playlistCount) + playlistCount * 256;
    DynamicJsonDocument doc(jsonSize);
    for (int i = 0; i < playlistCount; i++)
    {
        doc[i]["name"] = playlistNames[i];
        doc[i]["url"] = playlistUrls[i];
    }
    String output;
    serializeJson(doc, output);
    hal.pref.putString(hal.get_char_sha_key("online_playlists"), output);
    return true;
}

// ==================== 网络歌单加载 ====================

void AppOnlineMusic::loadOnlinePlaylist(const char *url)
{
    if (!WiFi.isConnected())
    {
        GUI::msgbox("错误", "WIFI未连接");
        return;
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(60000);

    int code = http.GET();
    if (code != HTTP_CODE_OK && code != HTTP_CODE_PARTIAL_CONTENT)
    {
        log_e("HTTP request failed: %d", code);
        GUI::msgbox("错误", "网络请求失败");
        http.end();
        return;
    }

    int contentLen = http.getSize();
    log_i("Playlist data size: %d bytes", contentLen);

    size_t rawBufferSize = (contentLen > 0) ? (size_t)contentLen : 65536;
    char *rawData = nullptr;
    if (psramFound())
        rawData = (char *)ps_malloc(rawBufferSize + 1024);
    if (!rawData)
        rawData = (char *)malloc(rawBufferSize + 1024);
    if (!rawData)
    {
        log_e("Failed to allocate buffer");
        GUI::msgbox("错误", "内存不足");
        http.end();
        return;
    }

    WiFiClient *streamPtr = http.getStreamPtr();
    size_t readLen = 0;
    while (streamPtr->connected() || streamPtr->available())
    {
        if (readLen > rawBufferSize) break;
        size_t bytesRead = streamPtr->readBytes(rawData + readLen, rawBufferSize - readLen);
        if (bytesRead == 0) break;
        readLen += bytesRead;
    }
    rawData[readLen] = '\0';
    http.end();

    size_t jsonCapacity = 131072;
    if (contentLen > 0) jsonCapacity = (size_t)(contentLen * 2.5);
    if (jsonCapacity < 16384) jsonCapacity = 16384;
    if (jsonCapacity > 524288) jsonCapacity = 524288;

    DynamicJsonDocument doc(jsonCapacity);
    DeserializationError error = deserializeJson(doc, rawData);
    free(rawData);

    if (error)
    {
        log_e("JSON parsing failed: %s", error.c_str());
        GUI::msgbox("错误", "歌单解析失败");
        return;
    }

    onlineSongCount = 0;
    JsonArray jsonArray = doc.as<JsonArray>();
    int totalItems = (int)jsonArray.size();
    if (songs)
    {
        free(songs);
        songs = nullptr;
    }
    songs = (onlinesong *)ps_malloc(sizeof(onlinesong) * totalItems);
    songs_size = totalItems;

    for (int i = 0; i < totalItems; i++)
    {
        JsonObject obj = jsonArray[i];
        if (obj.containsKey("url") && (obj.containsKey("name") || obj.containsKey("title")))
        {
            String songUrl = obj["url"].as<String>();
            String songTitle = obj.containsKey("title") ? obj["title"].as<String>() : obj["name"].as<String>();
            String songAuthor = obj.containsKey("author") ? obj["author"].as<String>()
                            : obj.containsKey("artist") ? obj["artist"].as<String>() : "";

            if (songUrl.length() > 10)
            {
                songUrl.replace("\\/", "/");
                songs[i].url = strdup(songUrl.c_str());
                songs[i].title = strdup(songTitle.isEmpty() ? "未知曲目" : songTitle.c_str());
                songs[i].author = strdup(songAuthor.c_str());
                // 尝试解析 duration
                if (obj.containsKey("interval"))
                    songs[i].duration = obj["interval"].as<unsigned long>();
                else if (obj.containsKey("duration"))
                    songs[i].duration = obj["duration"].as<unsigned long>() / 1000;
                else
                    songs[i].duration = 0;
                onlineSongCount++;
            }
        }
    }
    doc.clear();
    log_i("Loaded %d songs", onlineSongCount);
}

// ==================== 歌单缓存 ====================

bool AppOnlineMusic::loadPlaylistCache(int playlistIdx)
{
    char path[64];
    snprintf(path, sizeof(path), "/sd/online_pl_cache_%d.json", playlistIdx);
    if (!hal.exists(path)) return false;

    File file = hal.open(path, FILE_READ);
    if (!file) return false;

    String content = file.readString();
    file.close();

    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, content);
    if (error) return false;

    onlineSongCount = doc["count"] | 0;
    if (onlineSongCount > MAX_ONLINE_SONGS) onlineSongCount = MAX_ONLINE_SONGS;

    if (songs)
    {
        free(songs);
        songs = nullptr;
    }
    JsonArray songArr = doc["songs"];
    songs = (onlinesong *)ps_malloc(sizeof(onlinesong) * (onlineSongCount + 1));
    songs_size = onlineSongCount;
    for (int i = 0; i < onlineSongCount && i < (int)songArr.size(); i++)
    {
        songs[i].title = strdup(songArr[i]["title"].as<const char *>());
        songs[i].author = strdup(songArr[i]["author"].as<const char *>());
        songs[i].url = strdup(songArr[i]["url"].as<const char *>());
        songs[i].duration = songArr[i]["duration"] | 0;
    }
    doc.clear();
    log_i("Loaded %d songs from cache", onlineSongCount);
    return onlineSongCount > 0;
}

void AppOnlineMusic::savePlaylistCache(int playlistIdx)
{
    char path[64];
    snprintf(path, sizeof(path), "/sd/online_pl_cache_%d.json", playlistIdx);
    File file = hal.open(path, FILE_WRITE);
    if (!file) return;

    DynamicJsonDocument doc(16384);
    doc["count"] = onlineSongCount;
    JsonArray song = doc.createNestedArray("songs");
    for (int i = 0; i < onlineSongCount; i++)
    {
        JsonObject obj = song.createNestedObject();
        obj["title"] = songs[i].title;
        obj["author"] = songs[i].author;
        obj["url"] = songs[i].url;
        obj["duration"] = songs[i].duration;
    }
    serializeJson(doc, file);
    doc.clear();
    file.close();
}

// ==================== 歌词文件获取(从API尝试) ====================

// 根据歌曲URL获取歌曲ID，用于构造歌词API请求
static String getSongIdFromUrl(const String &url)
{
    int idPos = url.indexOf("id=");
    if (idPos == -1) return "";
    int endPos = url.indexOf('&', idPos);
    if (endPos == -1) return url.substring(idPos + 3);
    return url.substring(idPos + 3, endPos);
}

static void fetchAndSaveLyrics(const String &songId, const String &savePath)
{
    if (songId.isEmpty()) return;
    String lyricUrl = "https://meting.xcnahida.cn/meting/api?server=netease&type=lyric&id=" + songId;

    HTTPClient http;
    http.begin(lyricUrl);
    http.setTimeout(10000);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        return;
    }

    WiFiClient *client = http.getStreamPtr();
    String response;
    while (client->available())
        response += (char)client->read();
    http.end();

    DynamicJsonDocument doc(32768);
    DeserializationError err = deserializeJson(doc, response);
    if (err) return;

    String lrcText;
    if (doc.containsKey("lrc") && doc["lrc"].containsKey("lyric"))
        lrcText = doc["lrc"]["lyric"].as<String>();
    else if (doc.containsKey("tlyric") && doc["tlyric"].containsKey("lyric"))
        lrcText = doc["tlyric"]["lyric"].as<String>();
    else if (doc.containsKey("lyric"))
        lrcText = doc["lyric"].as<String>();

    doc.clear();

    if (lrcText.length() > 0)
    {
        // 确保目录存在
        int slashPos = savePath.lastIndexOf('/');
        if (slashPos != -1)
        {
            String dir = savePath.substring(0, slashPos);
            if (!hal.exists(dir))
                hal.mkdir(dir);
        }
        File f = hal.open(savePath, "w");
        if (f)
        {
            f.print(lrcText);
            f.close();
            log_i("歌词已保存: %s", savePath.c_str());
        }
    }
}

// ==================== 播放控制 ====================

String AppOnlineMusic::resolveRedirect(const String &url, int maxRedirects)
{
    String currentUrl = url;
    HTTPClient http;
    WiFiClient client;

    for (int i = 0; i < maxRedirects; i++)
    {
        http.begin(client, currentUrl);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setReuse(false);
        const char *headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);

        int code = http.GET();
        if (code == HTTP_CODE_OK || code == HTTP_CODE_PARTIAL_CONTENT)
        {
            http.end();
            return currentUrl;
        }
        else if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
        {
            String location = http.header("Location");
            http.end();
            if (location.length() == 0) return url;
            currentUrl = location;
        }
        else
        {
            http.end();
            return url;
        }
    }
    return currentUrl;
}

void AppOnlineMusic::playSong(int index)
{
    if (index < 0 || index >= onlineSongCount) return;

    stopSong();
    deletePlaytask();
    delay(200);

    currentSongIndex = index;
    currentSongTitle = songs[index].title;
    currentSongAuthor = songs[index].author;
    currentSongUrl = songs[index].url;

    log_i("Playing: [%d/%d] %s", currentSongIndex + 1, onlineSongCount, currentSongTitle.c_str());

    // 尝试加载歌词
    String songId = getSongIdFromUrl(currentSongUrl);
    String lrcPath;
    if (!songId.isEmpty())
    {
        lrcPath = "/sd/lrc/online_" + songId + ".lrc";
        if (!hal.exists(lrcPath))
        {
            // 尝试从网络获取歌词
            fetchAndSaveLyrics(songId, lrcPath);
        }
    }
    loadLyrics(lrcPath.c_str());

    httpStream = new AudioFileSourceHTTPStream(currentSongUrl.c_str());
    httpStream->RegisterMetadataCB(MDCallback, (void *)"ICY");
    mp3 = new AudioGeneratorMP3();
    mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");

    if (httpStream->isOpen())
    {
        bool success = mp3->begin(httpStream, i2sOut);
        if (success)
        {
            isPlaying = true;
            playTimeStart = millis();
            if (i2sOut)
                i2sOut->SetTimeout(10);
            beginPlayerTask();
            log_i("播放已启动");
        }
        else
        {
            log_e("Failed to start playback");
            delete mp3; mp3 = nullptr;
            delete httpStream; httpStream = nullptr;
            GUI::msgbox("错误", "播放连接失败");
        }
    }
    else
    {
        log_e("Failed to open audio stream");
        delete httpStream; httpStream = nullptr;
        GUI::msgbox("错误", "无法打开音频流");
    }
}

void AppOnlineMusic::stopSong()
{
    isPlaying = false;
    deletePlaytask();

    if (mp3)
    {
        mp3->stop();
        delete mp3;
        mp3 = nullptr;
    }
    if (httpStream)
    {
        httpStream->close();
        delete httpStream;
        httpStream = nullptr;
    }
}

// ==================== 歌曲列表显示 ====================

void AppOnlineMusic::showPlaylist(int page)
{
    const int ITEMS_PER_PAGE = 8;
    int totalPages = (onlineSongCount + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (totalPages < 1) totalPages = 1;
    if (page > totalPages) page = totalPages;
    if (page < 1) page = 1;

    int selected = 0;
    bool end = false;

    while (!end)
    {
        int startIdx = (page - 1) * ITEMS_PER_PAGE;
        int endIdx = min(startIdx + ITEMS_PER_PAGE, onlineSongCount);

        static menu_item items[10];
        for (int i = 0; i < 10; i++) items[i].icon = NULL;
        items[0].title = "返回";

        static String displayNames[9];
        for (int i = startIdx; i < endIdx; i++)
        {
            String display;
            if (songs[i].author && strlen(songs[i].author) > 0)
                display = String(songs[i].title) + " - " + String(songs[i].author);
            else
                display = String(songs[i].title);
            displayNames[i - startIdx] = display;
            items[i - startIdx + 1].title = displayNames[i - startIdx].c_str();
        }

        int itemCount = endIdx - startIdx + 1;
        items[itemCount].title = NULL;
        items[itemCount].icon = NULL;

        char title[64];
        snprintf(title, sizeof(title), "歌单(%d/%d)", page, totalPages);

        selected = GUI::menu(title, items, 8, 8, selected);

        if (selected == 0)
        {
            end = true;
        }
        else if (selected >= 1 && selected <= (endIdx - startIdx))
        {
            currentSongIndex = startIdx + selected - 1;
            playSong(currentSongIndex);
            end = true;
        }
        else if (selected == (endIdx - startIdx + 1))
        {
            if (page < totalPages) page++;
            else GUI::info_msgbox("提示", "已经是最后一页");
        }
        else if (selected == (endIdx - startIdx + 2) && totalPages > 1)
        {
            if (page > 1) page--;
            else GUI::info_msgbox("提示", "已经是第一页");
        }
    }
}

// ==================== 主显示界面 ====================

void AppOnlineMusic::showDisplay()
{
    d_time.start = micros();

    constexpr int FFT_Y_TOP = 16;
    constexpr int FFT_Y_BOT = 82;
    constexpr int FFT_H = FFT_Y_BOT - FFT_Y_TOP;
    constexpr int LRC_Y1 = 90;
    constexpr int LRC_Y2 = 107;
    constexpr int LRC_Y3 = 124;
    constexpr int BOTTOM_Y = 144;

    // 计算播放时间
    uint32_t playTime = isPlaying ? (millis() - playTimeStart) : 0;

    // ==================== 静态背景绘制 ====================
    uint8_t curBuf = display.current_buffer_idx;

    // 清空并绘制基本框架
    display.clearScreen();

    // ---- 顶栏 ----
    u8g2Fonts.setCursor(2, 12);
    u8g2Fonts.printf("%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);

    // 歌曲标题(居中,滚动显示)
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setBackgroundColor(1);
    u8g2Fonts.setForegroundColor(0);

    int16_t titleW = u8g2Fonts.getUTF8Width(currentSongTitle.c_str());
    int16_t titleX = (MAX_X - titleW) / 2;
    if (titleX < 40) titleX = 40;
    if (titleX > MAX_X - titleW - 25) titleX = MAX_X - titleW - 25;
    display.setDrawWindow(35, 0, MAX_X - 60, 14);
    u8g2Fonts.setCursor(titleX, 12);
    u8g2Fonts.print(currentSongTitle);
    display.setDrawWindow();

    // 电池
    if (hal.pref.getBool(hal.get_char_sha_key("精准电量显示"), false) && hal.VCC < 4300 && !hal.isCharging)
    {
        display.drawXBitmap(MAX_X - 22, 0, getBatteryIcon(true), 20, 16, 0);
        display.fillRect(MAX_X - 19, 6, getBatterysoc(), 4, TFT_BLACK);
    }
    else
    {
        display.drawXBitmap(MAX_X - 22, 0, getBatteryIcon(), 20, 16, 0);
    }

    display.drawFastHLine(0, 14, MAX_X, 0);

    // ---- FFT 频谱 / 波形 ----
    if (hal.pref.getBool("music_fft", true) && vReal && ring_buffer)
    {
        // 从环形缓冲区读取采样数据
        uint32_t currentWrite = write_index;
        int start = (currentWrite - SAMPLES + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
        for (int i = 0; i < SAMPLES; i++)
            vReal[i] = ring_buffer[(start + i) % RING_BUFFER_SIZE];

        // 加窗 & FFT
        for (int i = 0; i < SAMPLES; i++)
        {
            fft_data[2 * i] = vReal[i] * wind[i];
            fft_data[2 * i + 1] = 0.0f;
        }
        dsps_fft2r_fc32(fft_data, SAMPLES);
        dsps_bit_rev_fc32(fft_data, SAMPLES);
        dsps_cplx2reC_fc32(fft_data, SAMPLES);

        // 取幅值
        for (int i = 0; i < SAMPLES / 2; i++)
        {
            float re = fft_data[2 * i];
            float im = fft_data[2 * i + 1];
            vReal[i] = sqrtf(re * re + im * im);
        }

        // 低频衰减
        vReal[0] *= 0.4f;
        vReal[1] *= 0.5f;
        vReal[2] *= 0.6f;

        // 频率缩放
        for (int i = 0; i < SAMPLES / 2; i++)
        {
            if (use_log)
                vReal[i] = FFT_A_amplitude * log10f(1.0f + vReal[i] / FFT_A_spectrum_smoothness);
            else
                vReal[i] *= curveScaling[i];

            vReal[i] *= fft_gain;
            // 平滑
            vReal[i] = smoothingFactor * previousSpectrum[i] + (1.0f - smoothingFactor) * vReal[i];
            // 限幅
            if (vReal[i] > 60.0f) vReal[i] = 60.0f;
            if (vReal[i] < 0.0f) vReal[i] = 0.0f;
            previousSpectrum[i] = vReal[i];
        }

        // 绘制频谱
        const int SAMPLES_HALF = SAMPLES / 2;
        for (int x = 0; x < MAX_X; x++)
        {
            float srcIdx = (float)x * (SAMPLES_HALF - 1) / (MAX_X - 1);
            int idx = (int)srcIdx;
            float frac = srcIdx - idx;
            float mag;
            if (idx < SAMPLES_HALF - 1)
                mag = previousSpectrum[idx] * (1.0f - frac) + previousSpectrum[idx + 1] * frac;
            else
                mag = previousSpectrum[idx];

            int barH = (int)(mag * FFT_H / 60.0f);
            if (barH > FFT_H) barH = FFT_H;
            if (barH < 0) barH = 0;
            if (barH > 0)
                display.drawFastVLine(x, FFT_Y_BOT - barH, barH, TFT_BLACK);
        }
    }

    // ---- 歌词显示 ----
    if (totalLyricLines > 0)
    {
        unsigned long lyricTime = playTime;
        checkAndUpdateLyrics(lyricTime);

        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setForegroundColor(0);

        if (!scrolling)
        {
            u8g2Fonts.setCursor(12, LRC_Y1);
            u8g2Fonts.print(currentLyric[0]);
            String curL = "> " + String(currentLyric[1]);
            u8g2Fonts.setCursor(4, LRC_Y2);
            u8g2Fonts.print(curL);
            u8g2Fonts.setCursor(12, LRC_Y3);
            u8g2Fonts.print(currentLyric[2]);
        }
        else
        {
            int oldOff = scrollDirection > 0 ? -scrollOffset : scrollOffset;
            int newOff = scrollDirection > 0 ? (LINE_HEIGHT - scrollOffset) : -(LINE_HEIGHT - scrollOffset);
            u8g2Fonts.setCursor(12, LRC_Y1 + oldOff);
            u8g2Fonts.print(oldLyrics[0]);
            u8g2Fonts.setCursor(12, LRC_Y1 + newOff);
            u8g2Fonts.print(newLyrics[0]);
            u8g2Fonts.setCursor(4, LRC_Y2 + newOff);
            u8g2Fonts.print("> " + newLyrics[1]);
            u8g2Fonts.setCursor(12, LRC_Y3 + newOff);
            u8g2Fonts.print(newLyrics[2]);
        }
    }
    // 无歌词时显示作者信息
    else if (currentSongAuthor.length() > 0)
    {
        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setForegroundColor(0);
        int16_t aw = u8g2Fonts.getUTF8Width(currentSongAuthor.c_str());
        int16_t ax = (MAX_X - aw) / 2;
        if (ax < 3) ax = 3;
        u8g2Fonts.setCursor(ax, LRC_Y2);
        u8g2Fonts.print(currentSongAuthor);

        // 播放/暂停图标
        if (isPlaying)
            display.drawXBitmap((MAX_X - 24) / 2, LRC_Y3, pause_bits, 12, 32, TFT_BLACK);
        else
            display.drawXBitmap((MAX_X - 24) / 2, LRC_Y3, play_bits, 12, 32, TFT_BLACK);
    }

    // ---- 底栏: 分隔线 + 歌曲信息 + 音量 ----
    display.drawFastHLine(0, BOTTOM_Y - 1, MAX_X, 0);

    // 歌曲序号
    u8g2Fonts.setCursor(3, BOTTOM_Y + 12);
    if (onlineSongCount > 0)
        u8g2Fonts.printf("[%d/%d]", currentSongIndex + 1, onlineSongCount);
    else
        u8g2Fonts.print("[--/--]");

    // 播放状态文字
    u8g2Fonts.setCursor(55, BOTTOM_Y + 12);
    u8g2Fonts.print(isPlaying ? ">" : "||");

    // 播放时间
    u8g2Fonts.setCursor(78, BOTTOM_Y + 12);
    u8g2Fonts.printf("%02d:%02d", playTime / 60000, (playTime / 1000) % 60);

    // 音量
    char volBuf[24];
    snprintf(volBuf, sizeof(volBuf), "%d", currentVolume);
    u8g2Fonts.setCursor(MAX_X - 3 - u8g2Fonts.getUTF8Width(volBuf), BOTTOM_Y + 12);
    u8g2Fonts.printf("%s", volBuf);

    d_time.fft_end = micros();

    // ==================== 刷新显示 ====================
    d_time.display_start = micros();
    display.display();
    d_time.end = micros();

    last_time = d_time;
    displayTime = millis();
}

// ==================== 歌曲切换 ====================

void AppOnlineMusic::playNext()
{
    if (onlineSongCount == 0) return;
    currentSongIndex++;
    if (currentSongIndex >= onlineSongCount)
        currentSongIndex = 0;
    playSong(currentSongIndex);
}

void AppOnlineMusic::playPrevious()
{
    if (onlineSongCount == 0) return;
    currentSongIndex--;
    if (currentSongIndex < 0)
        currentSongIndex = onlineSongCount - 1;
    playSong(currentSongIndex);
}
