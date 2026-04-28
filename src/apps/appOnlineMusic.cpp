#include "AppManager.h"
#include "ESP8266Audio.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>

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

typedef struct
{
    char *url = nullptr;
    char *title = nullptr;
    char *author = nullptr;
} onlinesong; // 歌曲信息结构体

class AppOnlineMusic : public AppBase
{
private:
    AudioFileSourceHTTPStream *httpStream = nullptr;
    AudioGeneratorMP3 *mp3 = nullptr;
    AudioOutputI2S *out = nullptr;

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

    void cleanup();

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
    void showPlaylist(int page);
    void showDisplay();
    void playNext();
    void playPrevious();

    static void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string);
    static void StatusCallback(void *cbData, int code, const char *string);
};

static AppOnlineMusic onlineMusicApp;

void AppOnlineMusic::MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
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

void AppOnlineMusic::StatusCallback(void *cbData, int code, const char *string)
{
    (void)cbData;
    log_i("Status: %d %s", code, string);
}

void AppOnlineMusic::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
    log_i("APP %s, version: %s", name, "1.0.0");
}

void AppOnlineMusic::cleanup()
{
    stopSong();
    if (out)
    {
        delete out;
        out = nullptr;
    }
    if (songs)
    {
        for (int i = 0; i < songs_size; i++)
        {
            if (songs[i].title != nullptr)
                free(songs[i].title);
            if (songs[i].author != nullptr)
                free(songs[i].author);
            if (songs[i].url != nullptr)
                free(songs[i].url);
        }
        free(songs);
    }
    WiFi.disconnect(true);
    hal.can_light_sleep = true;
}

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
    };
    deepsleep = []()
    {
        onlineMusicApp.cleanup();
    };

    GUI::info_msgbox("提示", "正在连接 WIFI...");
    if (!hal.autoConnectWiFi(true))
    {
        GUI::msgbox("错误", "WIFI连接失败");
        hal.can_light_sleep = true;
        return;
    }
    GUI::info_msgbox("提示", "WIFI连接成功");

    out = new AudioOutputI2S(0, 0, 4, true);
    out->SetGain(currentVolume / 21.0f);
    out->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
    out->SetMclk(false);
    out->Set_bits_per_chan(I2S_BITS_PER_CHAN_32BIT);
    digitalWrite(PIN_DAC_EN, 1); // 使能DAC电源
    hal.cheak_freq(240);
    display.setPowerMode(POWER_MODE_HPM); // 屏幕切换至高性能模式
    digitalWrite(PIN_DAC_XSMT, 1);        // 解除DAC静音

    // 添加一个测试,感觉测试应该是没有问题了
    /*     char testUrl[] = "https://meting.xcnahida.cn:443/meting/api?server=netease&type=url&id=2125045481";
        httpStream = new AudioFileSourceHTTPStream(testUrl);
        mp3 = new AudioGeneratorMP3();
        mp3->begin(httpStream, out);
        while (1)
        {
            if (mp3->isRunning())
            {
                if (!mp3->loop())
                    mp3->stop();
            }
            else
            {
                log_i("MP3 done");
                delay(1000);
                appManager.goBack();
                _end = true;
                return;
            }
        } */

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

    openMenu();

    if (_end)
    {
        cleanup();
        appManager.goBack();
        return;
    }

    if (isPlaying)
        showDisplay();

    while (!_end)
    {
        if (isPlaying && mp3 && mp3->isRunning())
        {
            if (!mp3->loop())
            {
                // 播放结束，切歌
                stopSong();
                playNext();
                showDisplay();
            }
        }

        if (hal.btnc.isPressing())
        {
            if (GUI::waitLongPress(PIN_BUTTONC))
            {
                _end = true;
                break;
            }
            else
            {
                openMenu();
                if (isPlaying)
                    showDisplay();
            }
        }

        if (hal.btnr.isPressing())
        {
            if (GUI::waitLongPress(hal.btnr.pin()))
            {
                playNext();
                showDisplay();
                while (hal.btnr.isPressing())
                    delay(20);
            }
            else
            {
                if (currentVolume < 21)
                    currentVolume++;
                if (out)
                    out->SetGain(currentVolume / 21.0f);
                showDisplay();
            }
        }

        if (hal.btnl.isPressing())
        {
            if (GUI::waitLongPress(hal.btnl.pin()))
            {
                playPrevious();
                showDisplay();
                while (hal.btnl.isPressing())
                    delay(20);
            }
            else
            {
                if (currentVolume > 0)
                    currentVolume--;
                if (out)
                    out->SetGain(currentVolume / 21.0f);
                showDisplay();
            }
        }

        if (millis() - displayTime > 2000 && isPlaying)
        {
            showDisplay();
        }

        delay(10);
    }

    cleanup();
    appManager.goBack();
}

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
            if (currentVolume > 21)
                currentVolume = 21;
            if (currentVolume < 0)
                currentVolume = 0;
            if (out)
            {
                out->SetGain(currentVolume / 21.0f);
            }
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
    {
        GUI::info_msgbox("提示", "添加成功");
    }
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
            {
                GUI::info_msgbox("提示", "修改成功");
            }
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
                {
                    GUI::info_msgbox("提示", "删除成功");
                }
            }
        }
    }
}

void AppOnlineMusic::loadPlaylists()
{
    String dataStr = hal.pref.getString(hal.get_char_sha_key("online_playlists"), "");
    if (dataStr.length() == 0)
        return;

    size_t jsonSize = JSON_ARRAY_SIZE(MAX_PLAYLISTS * 2) + MAX_PLAYLISTS * 2 * 128;
    DynamicJsonDocument doc(jsonSize);
    DeserializationError error = deserializeJson(doc, dataStr);
    if (error)
        return;

    playlistCount = 0;
    for (int i = 0; i < MAX_PLAYLISTS; i++)
    {
        if (!doc[i].containsKey("name") || !doc[i].containsKey("url"))
            break;
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

    int contentLen = http.getSize(); // 可能为 -1（分块传输）
    log_i("Playlist data size: %d bytes", contentLen);

    // --- 1. 确定原始响应数据缓冲区大小 ---
    size_t rawBufferSize = 0;
    if (contentLen > 0)
    {
        rawBufferSize = contentLen;
    }
    else
    {
        // 未知长度时，分配一个较大值（或可动态扩展，这里固定 64KB）
        rawBufferSize = 65536;
    }

    // --- 2. 在 PSRAM 中分配缓冲区 ---
    char *rawData = nullptr;
    if (psramFound())
    {
        rawData = (char *)ps_malloc(rawBufferSize + 1024); // +1 用于结尾 '\0'
    }
    if (!rawData)
    {
        // 回退到普通 malloc
        rawData = (char *)malloc(rawBufferSize + 1024);
    }
    if (!rawData)
    {
        log_e("Failed to allocate buffer for HTTP response");
        GUI::msgbox("错误", "内存不足");
        http.end();
        return;
    }

    // --- 3. 从流中读取完整响应到缓冲区 ---
    WiFiClient *streamPtr = http.getStreamPtr();
    size_t readLen = 0;
    while (streamPtr->connected() || streamPtr->available())
    {
        if (readLen > rawBufferSize)
        {
            // 缓冲区不足，可以尝试扩大（为简化代码，这里直接报错）
            log_e("Response too large for buffer (%u bytes)", rawBufferSize);
            free(rawData);
            http.end();
            GUI::msgbox("错误", "响应数据过大");
            return;
        }
        size_t bytesRead = streamPtr->readBytes(rawData + readLen, rawBufferSize - readLen);
        if (bytesRead == 0)
            break;
        readLen += bytesRead;
    }
    rawData[readLen] = '\0';

    http.end(); // 关闭连接，数据已完全读入缓冲区

    // --- 4. 准备 JSON 文档容量（沿用原逻辑）---
    size_t jsonCapacity = 131072;
    if (contentLen > 0)
        jsonCapacity = (size_t)(contentLen * 2.5);
    if (jsonCapacity < 16384)
        jsonCapacity = 16384;
    if (jsonCapacity > 524288)
        jsonCapacity = 524288;

    DynamicJsonDocument doc(jsonCapacity);
    DeserializationError error = deserializeJson(doc, rawData);

    free(rawData); // 释放缓冲区

    if (error)
    {
        log_e("JSON parsing failed: %s (capacity: %u)", error.c_str(), jsonCapacity);
        GUI::msgbox("错误", "歌单解析失败");
        return;
    }

    onlineSongCount = 0;

    JsonArray jsonArray = doc.as<JsonArray>();
    int totalItems = (int)jsonArray.size();
    if (songs != nullptr)
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
            String songTitle = "";
            String songAuthor = "";

            if (obj.containsKey("title"))
                songTitle = obj["title"].as<String>();
            else
                songTitle = obj["name"].as<String>();

            if (obj.containsKey("author"))
                songAuthor = obj["author"].as<String>();
            else if (obj.containsKey("artist"))
                songAuthor = obj["artist"].as<String>();

            if (songUrl.length() > 10)
            {
                songUrl.replace("\\/", "/");
                songs[i].url = strdup(songUrl.c_str());
                songTitle = songTitle.isEmpty() ? "未知曲目" : songTitle;
                songs[i].title = strdup(songTitle.c_str());
                songs[i].author = strdup(songAuthor.c_str());
                onlineSongCount++;
            }
        }
    }

    doc.clear();
    log_i("Loaded %d songs", onlineSongCount);
}

bool AppOnlineMusic::loadPlaylistCache(int playlistIdx)
{
    char path[64];
    snprintf(path, sizeof(path), "/sd/online_pl_cache_%d.json", playlistIdx);

    if (!hal.exists(path))
        return false;

    File file = hal.open(path, FILE_READ);
    if (!file)
        return false;

    String content = file.readString();
    file.close();

    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, content);
    if (error)
        return false;

    onlineSongCount = doc["count"] | 0;
    if (onlineSongCount > MAX_ONLINE_SONGS)
        onlineSongCount = MAX_ONLINE_SONGS;

    if (songs != nullptr)
    {
        free(songs);
        songs = nullptr;
    }
    JsonArray song = doc["songs"];
    songs = (onlinesong *)ps_malloc(sizeof(onlinesong) * (onlineSongCount + 1));
    songs_size = onlineSongCount;
    for (int i = 0; i < onlineSongCount && i < (int)song.size(); i++)
    {
        songs[i].title = strdup(song[i]["title"].as<const char *>());
        songs[i].author = strdup(song[i]["author"].as<const char *>());
        songs[i].url = strdup(song[i]["url"].as<const char *>());
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
    if (!file)
        return;

    DynamicJsonDocument doc(16384);
    doc["count"] = onlineSongCount;
    JsonArray song = doc.createNestedArray("songs");
    for (int i = 0; i < onlineSongCount; i++)
    {
        JsonObject obj = song.createNestedObject();
        obj["title"] = songs[i].title;
        obj["author"] = songs[i].author;
        obj["url"] = songs[i].url;
    }

    serializeJson(doc, file);
    doc.clear();
    file.close();
}

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
            log_i("Final URL: %s", currentUrl.c_str());
            return currentUrl;
        }
        else if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
        {
            String location = http.header("Location");
            http.end();
            if (location.length() == 0)
            {
                log_e("Redirect with no Location header");
                return url;
            }
            log_i("Redirect %d -> %s", code, location.c_str());
            currentUrl = location;
        }
        else
        {
            log_e("resolveRedirect HTTP %d", code);
            http.end();
            return url;
        }
    }

    log_w("Exceeded max redirect count");
    return currentUrl;
}

void AppOnlineMusic::playSong(int index)
{
    if (index < 0 || index >= onlineSongCount)
    {
        return;
    }

    stopSong();
    delay(200);

    currentSongIndex = index;
    currentSongTitle = songs[index].title;
    currentSongAuthor = songs[index].author;
    currentSongUrl = songs[index].url;

    log_i("Playing: [%d/%d] %s", currentSongIndex + 1, onlineSongCount, currentSongTitle.c_str());
    log_i("URL: %s", currentSongUrl.c_str());

    httpStream = new AudioFileSourceHTTPStream(currentSongUrl.c_str());
    httpStream->RegisterMetadataCB(MDCallback, (void *)"ICY");
    mp3 = new AudioGeneratorMP3();
    mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");

    if (httpStream->isOpen())
    {
        bool success = mp3->begin(httpStream, out);
        if (success)
        {
            isPlaying = true;
            log_i("Play started successfully");
        }
        else
        {
            log_e("Failed to start playback");
            delete mp3;
            mp3 = nullptr;
            delete httpStream;
            httpStream = nullptr;
            GUI::msgbox("错误", "播放连接失败");
        }
    }
    else
    {
        log_e("Failed to open audio stream");
        delete httpStream;
        httpStream = nullptr;
        GUI::msgbox("错误", "无法打开音频流");
    }
}

void AppOnlineMusic::stopSong()
{
    isPlaying = false; // 先标记，这样主循环就不会再访问 mp3 了

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

void AppOnlineMusic::showPlaylist(int page)
{
    const int ITEMS_PER_PAGE = 8;
    int totalPages = (onlineSongCount + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (totalPages < 1)
        totalPages = 1;
    if (page > totalPages)
        page = totalPages;
    if (page < 1)
        page = 1;

    int selected = 0;
    bool end = false;

    while (!end)
    {
        int startIdx = (page - 1) * ITEMS_PER_PAGE;
        int endIdx = min(startIdx + ITEMS_PER_PAGE, onlineSongCount);

        static menu_item items[10];
        for (int i = 0; i < 10; i++)
        {
            items[i].icon = NULL;
        }

        items[0].title = "返回";

        static String displayNames[9];
        for (int i = startIdx; i < endIdx; i++)
        {
            String display;
            if (songs[i].author && strlen(songs[i].author) > 0)
                display = String(songs[i].title) + " - " + String(songs[i].author);
            else
                display = String(songs[i].title);

            displayNames[i - startIdx] = display; // 存到静态数组中
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
            if (page < totalPages)
            {
                page++;
            }
            else
            {
                GUI::info_msgbox("提示", "已经是最后一页");
            }
        }
        else if (selected == (endIdx - startIdx + 2) && totalPages > 1)
        {
            if (page > 1)
            {
                page--;
            }
            else
            {
                GUI::info_msgbox("提示", "已经是第一页");
            }
        }
    }
}

void AppOnlineMusic::showDisplay()
{
    display.clearScreen();

    GUI::drawWindowsWithTitle("在线音乐播放器");

    if (hal.pref.getBool(hal.get_char_sha_key("精准电量显示"), false) && hal.VCC < 4300 && !hal.isCharging)
    {
        display.drawXBitmap(274, 0, getBatteryIcon(true), 20, 16, 0);
        display.fillRect(277, 6, getBatterysoc(), 4, TFT_BLACK);
    }
    else
    {
        display.drawXBitmap(274, 0, getBatteryIcon(), 20, 16, 0);
    }

    u8g2Fonts.setCursor(2, 12);
    u8g2Fonts.printf("%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);

    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setBackgroundColor(1);
    u8g2Fonts.setForegroundColor(0);

    int16_t titleW = u8g2Fonts.getUTF8Width(currentSongTitle.c_str());
    int16_t titleX = (MAX_X - titleW) / 2;
    if (titleX < 3)
        titleX = 3;
    u8g2Fonts.setCursor(titleX, 40);
    u8g2Fonts.print(currentSongTitle);

    if (currentSongAuthor.length() > 0)
    {
        int16_t authorW = u8g2Fonts.getUTF8Width(currentSongAuthor.c_str());
        int16_t authorX = (MAX_X - authorW) / 2;
        if (authorX < 3)
            authorX = 3;
        u8g2Fonts.setCursor(authorX, 58);
        u8g2Fonts.print(currentSongAuthor);
    }

    if (mp3 && mp3->isRunning())
    {
        display.drawXBitmap(136, 77, pause_bits, 24, 24, TFT_BLACK);
    }
    else
    {
        display.drawXBitmap(136, 77, play_bits, 24, 24, TFT_BLACK);
    }

    if (onlineSongCount > 0)
    {
        u8g2Fonts.setCursor(3, 125);
        u8g2Fonts.printf("[%d/%d]", currentSongIndex + 1, onlineSongCount);
    }

    char volBuf[16];
    snprintf(volBuf, sizeof(volBuf), "音量:%d", currentVolume);
    u8g2Fonts.setCursor(MAX_X - 3 - u8g2Fonts.getUTF8Width(volBuf), 125);
    u8g2Fonts.printf("%s", volBuf);

    display.display(false);
    displayTime = millis();
}

void AppOnlineMusic::playNext()
{
    if (onlineSongCount == 0)
        return;

    currentSongIndex++;
    if (currentSongIndex >= onlineSongCount)
    {
        currentSongIndex = 0;
    }
    playSong(currentSongIndex);
}

void AppOnlineMusic::playPrevious()
{
    if (onlineSongCount == 0)
        return;

    currentSongIndex--;
    if (currentSongIndex < 0)
    {
        currentSongIndex = onlineSongCount - 1;
    }
    playSong(currentSongIndex);
}
