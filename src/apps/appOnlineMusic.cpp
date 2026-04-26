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
    0x80, 0x1f, 0xf8, 0x03, 0x00, 0x3f, 0xfc, 0x03, 0x00, 0x7e, 0xfc, 0x03,
    0x00, 0xf8, 0xff, 0x03, 0x00, 0xf0, 0xff, 0x03, 0x00, 0xc0, 0xff, 0x03,
    0x00, 0x80, 0xff, 0x03, 0x00, 0x00, 0xfe, 0x03, 0x00, 0x00, 0xfc, 0x03,
    0x00, 0x00, 0xf8, 0x03, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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

#define MAX_ONLINE_SONGS 256
#define MAX_PLAYLISTS 10
#define PLAYLIST_FILE "/online_playlists.json"

class AppOnlineMusic : public AppBase
{
private:
    AudioFileSourceHTTPStream *httpStream = nullptr;
    AudioGeneratorMP3 *mp3 = nullptr;
    AudioOutputI2S *out = nullptr;

    String onlineSongUrls[MAX_ONLINE_SONGS];
    String onlineSongTitles[MAX_ONLINE_SONGS];
    String onlineSongAuthors[MAX_ONLINE_SONGS];
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
    strncpy_P(s1, type, sizeof(s1));
    s1[sizeof(s1)-1] = 0;
    strncpy_P(s2, string, sizeof(s2));
    s2[sizeof(s2)-1] = 0;
    log_i("METADATA '%s' = '%s'", s1, s2);
}

void AppOnlineMusic::StatusCallback(void *cbData, int code, const char *string)
{
    (void)cbData;
    char s1[64];
    strncpy_P(s1, string, sizeof(s1));
    s1[sizeof(s1)-1] = 0;
    log_i("STATUS %d = '%s'", code, s1);
}

void AppOnlineMusic::cleanup()
{
    stopSong();
    if (out)
    {
        delete out;
        out = nullptr;
    }
    WiFi.disconnect(true);
    hal.can_light_sleep = true;
}

void AppOnlineMusic::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
    log_i("APP %s, 版本: %s", name, "1.0.0");
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
        GUI::msgbox("错误", "WIFI 连接失败");
        hal.can_light_sleep = true;
        return;
    }
    GUI::info_msgbox("提示", "WIFI 连接成功");

    out = new AudioOutputI2S(0, 1);
    out->SetOutputModeMono(true);
    out->SetGain(currentVolume / 21.0f);
    out->SetRate(48000);

    loadPlaylists();

    if (playlistCount == 0)
    {
        playlistNames[0] = "网易云音乐";
        playlistUrls[0] = "https://meting.xcnahida.cn/meting/api?server=netease&type=playlist&id=7031310463";
        playlistCount = 1;
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
            deletePlaylist();
            break;
        case 4:
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
        case 5:
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
        case 6:
            stopSong();
            GUI::info_msgbox("提示", "已停止播放");
            break;
        case 7:
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
    int selected = 0;

    static const menu_select addOpts[] = {
        {false, "< 返回", nullptr},
        {false, "通过 ID 添加", nullptr},
        {false, "通过 URL 添加", nullptr},
        {false, NULL, nullptr}};

    selected = GUI::select_menu("添加歌单", addOpts, 0);

    if (selected == 0)
        return;

    String playlistUrl;
    const char *name;

    if (selected == 1)
    {
        static const menu_select platformOpts[] = {
            {false, "< 返回", nullptr},
            {false, "网易云音乐", nullptr},
            {false, "QQ 音乐(实验性)", nullptr},
            {false, NULL, nullptr}};

        int platform = GUI::select_menu("选择平台", platformOpts, 1);
        if (platform == 0)
            return;

        int64_t playlistId = GUI::msgbox_number64("输入歌单 ID", 12, 0);
        if (playlistId <= 0)
        {
            GUI::msgbox("提示", "ID 无效");
            return;
        }

        if (platform == 1)
        {
            playlistUrl = "https://meting.xcnahida.cn/meting/api?server=netease&type=playlist&id=";
        }
        else
        {
            playlistUrl = "https://meting.xcnahida.cn/meting/api?server=tencent&type=playlist&id=";
        }
        playlistUrl += String(playlistId);

        name = GUI::englishInput("歌单名称");
    }
    else
    {
        name = GUI::englishInput("歌单名称");
        const char *url = GUI::englishInput("歌单 URL");
        if (url == NULL || strlen(url) == 0)
        {
            GUI::msgbox("提示", "URL 不能为空");
            return;
        }
        playlistUrl = url;
    }

    if (name == NULL || strlen(name) == 0)
    {
        GUI::msgbox("提示", "名称不能为空");
        return;
    }

    if (playlistCount >= MAX_PLAYLISTS)
    {
        GUI::msgbox("提示", "歌单数量已达上限");
        return;
    }

    playlistNames[playlistCount] = name;
    playlistUrls[playlistCount] = playlistUrl;
    playlistCount++;

    if (savePlaylists())
    {
        GUI::msgbox("提示", "添加成功");
    }
    else
    {
        GUI::msgbox("错误", "保存失败");
    }
}

void AppOnlineMusic::deletePlaylist()
{
    if (playlistCount == 0)
    {
        GUI::msgbox("提示", "没有可删除的歌单");
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
            if (GUI::msgbox_yn("确认", "确定要删除这个歌单吗?"))
            {
                for (int i = selected - 1; i < playlistCount - 1; i++)
                {
                    playlistNames[i] = playlistNames[i + 1];
                    playlistUrls[i] = playlistUrls[i + 1];
                }
                playlistCount--;

                if (savePlaylists())
                {
                    GUI::info_msgbox("提示", "删除成功");
                }
                else
                {
                    GUI::msgbox("错误", "保存失败");
                }
            }
        }
    }
}

void AppOnlineMusic::loadPlaylists()
{
    if (!hal.exists(PLAYLIST_FILE))
    {
        playlistCount = 0;
        return;
    }

    File file = hal.open(PLAYLIST_FILE, FILE_READ);
    if (!file)
    {
        playlistCount = 0;
        return;
    }

    String content = file.readString();
    file.close();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, content);

    if (error)
    {
        playlistCount = 0;
        return;
    }

    playlistCount = doc["count"] | 0;
    if (playlistCount > MAX_PLAYLISTS)
        playlistCount = MAX_PLAYLISTS;

    JsonArray playlists = doc["playlists"];
    for (int i = 0; i < playlistCount && i < playlists.size(); i++)
    {
        playlistNames[i] = playlists[i]["name"].as<String>();
        playlistUrls[i] = playlists[i]["url"].as<String>();
    }

    doc.clear();
}

bool AppOnlineMusic::savePlaylists()
{
    File file = hal.open(PLAYLIST_FILE, FILE_WRITE);
    if (!file)
    {
        return false;
    }

    DynamicJsonDocument doc(4096);
    doc["count"] = playlistCount;

    JsonArray playlists = doc.createNestedArray("playlists");
    for (int i = 0; i < playlistCount; i++)
    {
        JsonObject obj = playlists.createNestedObject();
        obj["name"] = playlistNames[i];
        obj["url"] = playlistUrls[i];
    }

    serializeJson(doc, file);
    doc.clear();
    file.close();

    return true;
}

void AppOnlineMusic::loadOnlinePlaylist(const char *url)
{
    if (!WiFi.isConnected())
    {
        GUI::msgbox("错误", "WiFi未连接");
        return;
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(30000);

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        log_e("HTTP请求失败: %d", code);
        GUI::msgbox("错误", "网络请求失败");
        http.end();
        return;
    }

    int contentLen = http.getSize();
    log_i("歌单数据大小: %d bytes", contentLen);

    size_t jsonCapacity = 131072;
    if (contentLen > 0)
        jsonCapacity = (size_t)(contentLen * 2.5);
    if (jsonCapacity < 16384)
        jsonCapacity = 16384;
    if (jsonCapacity > 524288)
        jsonCapacity = 524288;

    WiFiClient *stream = http.getStreamPtr();
    DynamicJsonDocument doc(jsonCapacity);
    DeserializationError error = deserializeJson(doc, *stream);
    http.end();

    if (error)
    {
        log_e("JSON解析失败: %s (capacity: %u)", error.c_str(), jsonCapacity);
        GUI::msgbox("错误", "歌单解析失败");
        return;
    }

    onlineSongCount = 0;

    JsonArray jsonArray = doc.as<JsonArray>();
    for (int i = 0; i < (int)jsonArray.size() && onlineSongCount < MAX_ONLINE_SONGS; i++)
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
                onlineSongUrls[onlineSongCount] = songUrl;
                onlineSongTitles[onlineSongCount] = songTitle.isEmpty() ? "未知曲目" : songTitle;
                onlineSongAuthors[onlineSongCount] = songAuthor;
                onlineSongCount++;
            }
        }
    }

    doc.clear();
    log_i("加载了 %d 首歌曲", onlineSongCount);
}

void AppOnlineMusic::savePlaylistCache(int playlistIdx)
{
    char path[64];
    snprintf(path, sizeof(path), "/online_pl_cache_%d.json", playlistIdx);

    File file = hal.open(path, FILE_WRITE);
    if (!file)
        return;

    DynamicJsonDocument doc(16384);
    doc["count"] = onlineSongCount;
    JsonArray songs = doc.createNestedArray("songs");
    for (int i = 0; i < onlineSongCount; i++)
    {
        JsonObject obj = songs.createNestedObject();
        obj["title"] = onlineSongTitles[i];
        obj["author"] = onlineSongAuthors[i];
        obj["url"] = onlineSongUrls[i];
    }

    serializeJson(doc, file);
    doc.clear();
    file.close();
}

bool AppOnlineMusic::loadPlaylistCache(int playlistIdx)
{
    char path[64];
    snprintf(path, sizeof(path), "/online_pl_cache_%d.json", playlistIdx);

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

    JsonArray songs = doc["songs"];
    for (int i = 0; i < onlineSongCount && i < (int)songs.size(); i++)
    {
        onlineSongTitles[i] = songs[i]["title"].as<String>();
        onlineSongAuthors[i] = songs[i]["author"].as<String>();
        onlineSongUrls[i] = songs[i]["url"].as<String>();
    }

    doc.clear();
    log_i("从缓存加载了 %d 首歌曲", onlineSongCount);
    return onlineSongCount > 0;
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
            log_i("最终URL: %s", currentUrl.c_str());
            return currentUrl;
        }
        else if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
        {
            String location = http.header("Location");
            http.end();
            if (location.length() == 0)
            {
                log_e("重定向无Location头");
                return url;
            }
            log_i("重定向 %d -> %s", code, location.c_str());
            currentUrl = location;
        }
        else
        {
            log_e("resolveRedirect HTTP %d", code);
            http.end();
            return url;
        }
    }

    log_w("超过最大重定向次数");
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
    currentSongTitle = onlineSongTitles[index];
    currentSongAuthor = onlineSongAuthors[index];
    currentSongUrl = onlineSongUrls[index];

    log_i("播放: [%d/%d] %s", currentSongIndex + 1, onlineSongCount, currentSongTitle.c_str());
    log_i("原始URL: %s", currentSongUrl.c_str());

    String finalUrl = resolveRedirect(currentSongUrl);

    httpStream = new AudioFileSourceHTTPStream(finalUrl.c_str());
    httpStream->RegisterMetadataCB(MDCallback, (void*)"ICY");
    mp3 = new AudioGeneratorMP3();
    mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");

    if (httpStream->isOpen())
    {
        bool success = mp3->begin(httpStream, out);
        if (success)
        {
            isPlaying = true;
            log_i("播放成功");
        }
        else
        {
            log_e("播放启动失败");
            delete mp3;
            mp3 = nullptr;
            delete httpStream;
            httpStream = nullptr;
            GUI::msgbox("错误", "播放连接失败");
        }
    }
    else
    {
        log_e("无法打开流");
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
        for (int i = 0; i < 9; i++)
        {
            items[i].icon = NULL;
        }

        items[0].title = "返回";

        static String displayNames[9];
        for (int i = startIdx; i < endIdx; i++)
        {
            if (onlineSongAuthors[i].length() > 0)
                displayNames[i - startIdx] = onlineSongTitles[i] + " - " + onlineSongAuthors[i];
            else
                displayNames[i - startIdx] = onlineSongTitles[i];
            items[i - startIdx + 1].title = displayNames[i - startIdx].c_str();
        }

        int itemCount = endIdx - startIdx + 1;
        items[itemCount].title = NULL;
        items[itemCount].icon = NULL;

        char title[32];
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
    int16_t titleX = (296 - titleW) / 2;
    if (titleX < 3)
        titleX = 3;
    u8g2Fonts.setCursor(titleX, 40);
    u8g2Fonts.print(currentSongTitle);

    if (currentSongAuthor.length() > 0)
    {
        int16_t authorW = u8g2Fonts.getUTF8Width(currentSongAuthor.c_str());
        int16_t authorX = (296 - authorW) / 2;
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
    u8g2Fonts.setCursor(293 - u8g2Fonts.getUTF8Width(volBuf), 125);
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
