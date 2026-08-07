#ifndef LYRICS_H
#define LYRICS_H

#include "A_Config.h"

#define LYRICS_API_BASE_URL "https://lrclib.net/api/"


typedef struct
{
    String album = "---";     // 专辑
    String performer = "---"; // 歌手
    String title = "---";     // 标题
    uint32_t tlen = 0;
} tag_info; // 标签信息结构体

class Lyrics
{
public:
    explicit Lyrics(const String &baseUrl = "https://lrclib.net/api/");
    char * get(const tag_info info, bool debug);
    char * search(const tag_info info, bool debug);
    char * getLyrics(const tag_info info, bool debug = false);

private:
    const char *User_Agent = "LRCGET v0.2.0 (https://github.com/tranxuanthang/lrcget)";
    String _baseUrl;
};

extern Lyrics lyrics; // 全局歌词对象

#endif // LYRICS_H
