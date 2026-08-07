#include "lyrics.h"

Lyrics::Lyrics(const String &baseUrl)
{
    _baseUrl = baseUrl;
    if (!_baseUrl.endsWith("/"))
    {
        _baseUrl += "/";
    }
}

String urlEncode(const String &str)
{
    String encoded = "";
    char hex[] = "0123456789ABCDEF";

    for (size_t i = 0; i < str.length(); i++)
    {
        unsigned char c = str[i]; // 取出每个字节

        // 保留无需编码的字符：字母、数字、-_.~
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += (char)c;
        }
        else
        {
            // 其他字符转换为 %XX 格式
            encoded += '%';
            encoded += hex[(c >> 4) & 0xF];
            encoded += hex[c & 0xF];
        }
    }

    return encoded;
}

// 辅助函数：判断标签字段是否有效（非空且不为"---"）
static bool isValidField(const String &s)
{
    return s.length() > 0 && s != "---";
}

// 辅助函数：解析 JSON 响应并提取 syncedLyrics 字段
// 如果成功则返回动态分配的字符串，否则返回 nullptr
static char *extractSyncedLyrics(JsonDocument &doc)
{
    if (!doc["syncedLyrics"].is<const char *>())
    {
        return nullptr;
    }
    const char *lyrics = doc["syncedLyrics"];
    if (lyrics == nullptr || strlen(lyrics) == 0)
    {
        return nullptr;
    }
    // 复制字符串，使用 strdup 或 new
    char *result = strdup(lyrics);
    return result;
}

void debugPrintResponse(const String &url, int httpCode, const char * result)
{
    File file = hal.open("/littlefs/lyrics_debug.txt", "a");
    if (file)
    {
        file.println("Search URL: " + url);
        file.println("HTTP Code: " + String(httpCode));
        if (httpCode == HTTP_CODE_OK)
        {
            file.println("Response:");
            if (result)
            {
                file.write((uint8_t *)result, strlen(result));
            }
            else
            {
                file.println("No synced lyrics found in search results.");
            }
        }
    }
    file.close();
}

char *Lyrics::get(const tag_info info, bool debug)
{
    // 检查get所需参数是否有效
    if (!isValidField(info.title) || !isValidField(info.performer) ||
        !isValidField(info.album) || ((info.tlen / 1000) <= 0))
    {
        return nullptr;
    }

    // 构建 URL
    String url = _baseUrl + "get?track_name=" + urlEncode(info.title) +
                 "&artist_name=" + urlEncode(info.performer) +
                 "&album_name=" + urlEncode(info.album) +
                 "&duration=" + String(info.tlen / 1000);

    String ca_cert = hal.get_CAcert("/littlefs/System/ISRG Root X1.crt");
    HTTPClient http;
    http.begin(url, ca_cert.c_str());
    http.addHeader("User-Agent", User_Agent);

    int httpCode = http.GET();
    char *result = nullptr;

    if (httpCode == HTTP_CODE_OK)
    {
        // 解析 JSON
        DynamicJsonDocument doc(8192); // 足够容纳歌词响应
        DeserializationError error = deserializeJson(doc, http.getStream());
        if (!error)
        {
            result = extractSyncedLyrics(doc);
        }
    }

    if (debug)
    {
        debugPrintResponse(url, httpCode, result);
    }

    http.end();
    return result; // 可能为 nullptr
}

char *Lyrics::search(const tag_info info, bool debug)
{
    String url = _baseUrl + "search?";

    bool hasTrackName = isValidField(info.title);
    bool hasArtistName = isValidField(info.performer);
    bool hasAlbumName = isValidField(info.album);

    if (hasTrackName)
    {
        // 有 track_name，优先使用结构化搜索
        url += "track_name=" + urlEncode(info.title);
        if (hasArtistName)
        {
            url += "&artist_name=" + urlEncode(info.performer);
        }
        if (hasAlbumName)
        {
            url += "&album_name=" + urlEncode(info.album);
        }
    }
    else
    {
        // 无有效 track_name，必须使用 q 参数
        String q;
        if (hasArtistName)
        {
            q += info.performer;
        }
        if (hasAlbumName)
        {
            if (!q.isEmpty())
                q += " ";
            q += info.album;
        }
        // 如果 q 仍为空，则无法搜索
        if (q.isEmpty())
        {
            return nullptr;
        }
        url += "q=" + urlEncode(q);
    }

    String ca_cert = hal.get_CAcert("/littlefs/System/ISRG Root X1.crt");
    HTTPClient http;
    http.begin(url, ca_cert.c_str());
    http.addHeader("User-Agent", User_Agent);

    int httpCode = http.GET();
    char *result = nullptr;

    if (httpCode == HTTP_CODE_OK)
    {
        // 解析 JSON 数组
        int responseLen = http.getSize();
        DynamicJsonDocument doc(responseLen + 1024);
        DeserializationError error = deserializeJson(doc, http.getStream());
        if (!error && doc.is<JsonArray>())
        {
            JsonArray arr = doc.as<JsonArray>();
            float target = (float)info.tlen / 1000.0f;
            bool durationValid = (info.tlen > 0);

            for (JsonObject item : arr)
            {
                if (durationValid)
                {
                    // 时长有效，需在 ±2 秒内匹配
                    float duration = item["duration"];
                    const float tolerance = 2.0f;
                    if (fabs(duration - target) <= tolerance)
                    {
                        if (item["syncedLyrics"].is<const char *>())
                        {
                            const char *lyrics = item["syncedLyrics"];
                            if (lyrics != nullptr && strlen(lyrics) > 0)
                            {
                                result = strdup(lyrics);
                                break;
                            }
                        }
                    }
                }
                else
                {
                    // 时长无效，直接返回第一个有同步歌词的结果
                    if (item["syncedLyrics"].is<const char *>())
                    {
                        const char *lyrics = item["syncedLyrics"];
                        if (lyrics != nullptr && strlen(lyrics) > 0)
                        {
                            result = strdup(lyrics);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (debug)
    {
        debugPrintResponse(url, httpCode, result);
    }
    http.end();

    return result;
}

char *Lyrics::getLyrics(const tag_info info, bool debug)
{
    // 首先尝试精确获取
    bool canUseGet = isValidField(info.title) &&
                     isValidField(info.performer) &&
                     isValidField(info.album) &&
                     info.tlen > 0;

    if (canUseGet)
    {
        char *lyrics = get(info, debug);
        if (lyrics != nullptr)
        {
            return lyrics;
        }
    }

    // 精确获取失败或条件不足，尝试搜索
    return search(info, debug);
}

Lyrics lyrics; // 全局歌词对象