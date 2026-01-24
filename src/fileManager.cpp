#include <A_Config.h>
#include <GUI.h>
// 9 * 12
static const uint8_t imgfile_bits[] = {
    0x7e, 0x00, 0xa1, 0x00, 0x21, 0x01, 0xc9, 0x01, 0x15, 0x01, 0x09, 0x01,
    0x01, 0x01, 0x41, 0x01, 0xe1, 0x01, 0xf1, 0x01, 0xfd, 0x01, 0xff, 0x01};
static const uint8_t luafile_bits[] = {
    0x7e, 0x00, 0xa1, 0x00, 0x21, 0x01, 0xc1, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x21, 0x01, 0x09, 0x01, 0x15, 0x01, 0x09, 0x01, 0x01, 0x01, 0xff, 0x01};
static const uint8_t musicfile_bits[] = {
    0x7e, 0x00, 0xa1, 0x00, 0x21, 0x01, 0xc1, 0x01, 0x01, 0x01, 0x61, 0x01,
    0x59, 0x01, 0x49, 0x01, 0x69, 0x01, 0x0d, 0x01, 0x01, 0x01, 0xff, 0x01};
static const uint8_t otherfile_bits[] = {
    0x7e, 0x00, 0xa1, 0x00, 0x21, 0x01, 0xc1, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 0x01};
static const uint8_t sysfile_bits[] = {
    0x7e, 0x00, 0xa1, 0x00, 0x21, 0x01, 0xc1, 0x01, 0x01, 0x01, 0x11, 0x01,
    0x11, 0x01, 0x11, 0x01, 0x01, 0x01, 0x11, 0x01, 0x01, 0x01, 0xff, 0x01};
static const uint8_t textfile_bits[] = {
    0x7e, 0x00, 0xa1, 0x00, 0x21, 0x01, 0xc1, 0x01, 0x01, 0x01, 0x7d, 0x01,
    0x01, 0x01, 0x7d, 0x01, 0x01, 0x01, 0x7d, 0x01, 0x01, 0x01, 0xff, 0x01};
// 12*12
static const uint8_t folder_bits[] = {
    0x00, 0x00, 0x1e, 0x00, 0x21, 0x00, 0xc1, 0x0f, 0x01, 0x08, 0x01, 0x08,
    0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0xfe, 0x07, 0x00, 0x00};

struct s_fileicondict
{
    const char *extension;
    const uint8_t *ico;
};
static const struct s_fileicondict fileicondict[] = {
    {"bmp", imgfile_bits},
    {"jpg", imgfile_bits},
    {"png", imgfile_bits},
    {"xbm", imgfile_bits},
    {"lua", luafile_bits},
    {"buz", musicfile_bits},
    {"mp3", musicfile_bits},
    {"flac", musicfile_bits},
    {"aac", musicfile_bits},
    {"wav", musicfile_bits},
    {"opus", musicfile_bits}, // 添加opus格式支持
    {"i", sysfile_bits},
    {"json", sysfile_bits},
    {"bin", sysfile_bits},
    {"txt", textfile_bits},
    {"lbm", imgfile_bits}, // 自定义的图片格式（实际上是XBM编码）
    {NULL, NULL},
};

static const menu_item root_menu[] =
    {
        {folder_bits, ".."},
        {folder_bits, "littlefs"},
        {folder_bits, "sd"},
        {NULL, NULL},
};

/**
 *
“lbm”格式定义：
扩展名.lbm
单色位图
小端
2字节宽度
2字节高度
之后是数据
*/
static const uint8_t *getFileIcon(const char *extension)
{
    for (int i = 0; fileicondict[i].extension != NULL; i++)
    {
        if (strcmp(fileicondict[i].extension, extension) == 0)
        {
            return fileicondict[i].ico;
        }
    }
    return otherfile_bits;
}

String truncatePath(const String& cwd, U8G2_FOR_ADAFRUIT_GFX& u8g2) {
    const int16_t maxWidth = 200;
    
    // 1. 根目录直接返回
    if (cwd == "/") {
        return cwd;
    }
    
    // 2. 检查完整路径是否合适
    if (u8g2.getUTF8Width(cwd.c_str()) <= maxWidth) {
        return cwd;
    }
    
    // 3. 分割路径
    std::vector<String> parts;
    int start = 0;
    int end = cwd.indexOf('/');
    
    while (end != -1) {
        if (end > start) { // 忽略空的部分
            parts.push_back(cwd.substring(start, end));
        }
        start = end + 1;
        end = cwd.indexOf('/', start);
    }
    // 添加最后一部分
    if (start < cwd.length()) {
        parts.push_back(cwd.substring(start));
    }
    
    // 4. 逐步截断路径
    // 从后向前保留更多目录
    for (int keepParts = parts.size(); keepParts >= 1; keepParts--) {
        // 构建路径
        String path;
        if (keepParts == parts.size()) {
            // 完整路径
            path = "/";
            for (int i = 0; i < parts.size(); i++) {
                path += parts[i];
                if (i < parts.size() - 1) path += "/";
            }
        } else if (keepParts == 1) {
            // 只剩下最后一部分
            path = ".../" + parts.back();
        } else {
            // 保留最后几部分
            path = "...";
            for (int i = parts.size() - keepParts; i < parts.size(); i++) {
                path += "/" + parts[i];
            }
        }
        
        // 检查宽度
        if (u8g2.getUTF8Width(path.c_str()) <= maxWidth) {
            return path;
        }
    }
    
    // 5. 如果连".../dir_name"都超过200，直接返回它
    return ".../" + parts.back();
}

namespace GUI
{
    char filedialog_buffer[300];
    void push_buffer();
    void pop_buffer();
    const char *fileDialog(const char *title, bool isApp, const char *endsWidth, const char *gotoendsWidth, String cwd, const char *file_system, bool cleardepth)
    {
        // 注意，这个函数完全没有考虑线程安全，no reentrent!!!
        // 处理多扩展名过滤
        char **extensionList = NULL;
        int extensionCount = 0;

        // 如果endsWidth不为NULL且包含换行符，则分割字符串
        if (endsWidth != NULL && strchr(endsWidth, '\n') != NULL)
        {
            // 计算扩展名数量
            const char *ptr = endsWidth;
            extensionCount = 1; // 至少有一个扩展名
            while (*ptr)
            {
                if (*ptr == '\n')
                    extensionCount++;
                ptr++;
            }

            // 分配内存存储扩展名
            extensionList = (char **)malloc(extensionCount * sizeof(char *));
            if (extensionList != NULL)
            {
                // 复制字符串并分割
                char *copy = strdup(endsWidth);
                if (copy != NULL)
                {
                    int idx = 0;
                    char *token = strtok(copy, "\n");
                    while (token != NULL && idx < extensionCount)
                    {
                        extensionList[idx++] = strdup(token);
                        token = strtok(NULL, "\n");
                    }
                    free(copy);
                }
            }
        }
        // 首先选择文件系统
        bool useSD = false;
        if (isApp == false)
        {
            if (digitalRead(PIN_SD_CARDDETECT) == LOW)
            {
                if (file_system != NULL)
                {
                    if (strcmp(file_system, "TF") == 0)
                    {
                        useSD = true;
                    }
                    else if (strcmp(file_system, "LittleFS") == 0)
                    {
                        useSD = false;
                    }
                }
                else
                {
                    // if (msgbox_yn("请选择文件系统", "左：LittleFS\n右：TF 卡", "TF 卡", "LittleFS"))
                    // {
                    //     useSD = true;
                    // }
                    int select_ = 1;
                select_fs:
                    int selected = menu(title, root_menu, 12, 12, 1);
                    switch (selected)
                    {
                    case 0:
                        GUI::msgbox("警告", "没有上级目录");
                        if (select_ < 4)
                            goto select_fs;
                        else
                            useSD = false;
                        break;
                    case 1:
                        useSD = false;
                        break;
                    case 2:
                        useSD = true;
                        break;

                    default:
                        break;
                    }
                }
            }
        }
        else
        {
            useSD = true;
        }
        if ((!peripherals.isSDLoaded()) && useSD && digitalRead(PIN_SD_CARDDETECT) == LOW)
            peripherals.load(PERIPHERALS_SD_BIT);
        // String cwd = "/";
        File root;
        File file;
        int16_t total_entries = 0;
        menu_item entries[256];
        char *titles[256];
        static uint8_t selectedStack[64] = {1}; // 假设最多64层目录
        static int8_t depth = 0;
        if (cleardepth){
            depth = 0;
            memset(selectedStack, 1, sizeof(selectedStack));
        }
        memset(entries, 0, sizeof(entries));
        memset(titles, 0, sizeof(titles));
        entries[0].icon = folder_bits;
        entries[0].title = "..";
        while (1)
        {
            Serial.print("[文件浏览] 当前工作目录：");
            Serial.println(cwd);
            // 首先清除
            total_entries = 1;
            while (titles[total_entries] != NULL)
            {
                free(titles[total_entries]);
                titles[total_entries] = NULL;
                entries[total_entries].icon = NULL;
                entries[total_entries].title = NULL;
                ++total_entries;
            }
            total_entries = 1;
            int try_open = 0;
        open_root:
            if (useSD)
            {
                root = SD.open(cwd);
            }
            else
            {
                root = LittleFS.open(cwd);
            }
            if (cwd != "/")
                cwd += "/";
            if (!root)
            {
                error("[文件] root未打开");
                cwd = "/";
                try_open++;
                if (try_open > 10)
                    return NULL;
                goto open_root;
            }
            unsigned long start_time = millis(), end_time;
            String full_cwd;
            if (useSD)
            {
                full_cwd = "/sd" + cwd;
            }
            else
            {
                full_cwd = "/littlefs" + cwd;
            }
            GUI::info_msgbox(truncatePath(full_cwd, u8g2Fonts).c_str(), "正在构建文件列表...");
            file = root.openNextFile();
            String ext, tmp;
            while (file)
            {
                tmp = file.name();
                if (tmp.lastIndexOf('.') != -1)
                {
                    ext = tmp.substring(tmp.lastIndexOf('.') + 1);
                }
                else
                {
                    ext = "";
                }
                if (file.isDirectory())
                    Serial.printf("\033[36m%s\033[0m\n", tmp.c_str());
                else
                {
                    if (tmp.endsWith(".lua"))
                        Serial.printf("\033[32m%s\033[0m\n", tmp.c_str());
                    else
                        Serial.println(tmp);
                }
                if (gotoendsWidth != NULL)
                {
                    if (tmp.endsWith(gotoendsWidth))
                    {
                        file.close();
                        file = root.openNextFile();
                        continue;
                    }
                }
                if (isApp == false)
                {
                    if (file.isDirectory())
                    {
                        entries[total_entries].icon = folder_bits;
                    }
                    else
                    {
                        if (endsWidth != NULL)
                        {
                            // if (strcmp(endsWidth, ext.c_str()) != 0)
                            // {
                            //     file.close();
                            //     file = root.openNextFile();
                            //     continue;
                            // }
                            bool match = false;
                            if (extensionList != NULL)
                            {
                                // 多扩展名匹配
                                for (int i = 0; i < extensionCount; i++)
                                {
                                    if (extensionList[i] != NULL && strcmp(extensionList[i], ext.c_str()) == 0)
                                    {
                                        match = true;
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                // 单扩展名匹配 (保持原有逻辑)
                                match = (strcmp(endsWidth, ext.c_str()) == 0);
                            }
                            if (!match)
                            {
                                file.close();
                                file = root.openNextFile();
                                continue;
                            }
                        }
                        entries[total_entries].icon = getFileIcon(ext.c_str());
                    }
                    titles[total_entries] = (char *)malloc(strlen(file.name()) + 1);
                    if (titles[total_entries] == NULL)
                    {
                        GUI::msgbox("严重错误", "[文件管理] 动态内存不足");
                        error("堆内存不足");
                        ESP.restart();
                    }
                    strcpy(titles[total_entries], file.name());
                    entries[total_entries].title = titles[total_entries];
                    ++total_entries;
                }
                else
                {
                    if (file.isDirectory())
                    {
                        if (ext == "app")
                        {
                            entries[total_entries].icon = luafile_bits;
                            titles[total_entries] = (char *)malloc(strlen(file.name()) + 1);
                            if (titles[total_entries] == NULL)
                            {
                                GUI::msgbox("严重错误", "[文件管理] 动态内存不足");
                                error("堆内存不足");
                                ESP.restart();
                            }
                            strcpy(titles[total_entries], file.name());
                            entries[total_entries].title = titles[total_entries];
                            ++total_entries;
                        }
                    }
                }
                if (total_entries >= 256)
                {
                    GUI::msgbox("严重错误", "文件数超过256个，将导致内存溢出，即将重启");
                    ESP.restart();
                }
                file.close();
                file = root.openNextFile();
            }
            end_time = millis() - start_time;
            log_i(" [文件] 创建文件列表，耗时%.2fs", (float)end_time / 1000.0);
            int selected = menu(truncatePath(full_cwd, u8g2Fonts).c_str(), entries, 12, 12, selectedStack[depth]);
            if (selected == 0)
            {
                depth--;
                if (depth < 0)
                    depth = 0;
                // 上一级目录
                // display.display(true); // 局部刷新一次
                if (cwd == "/")
                {
                    total_entries = 1;
                    while (titles[total_entries] != NULL)
                    {
                        free(titles[total_entries]);
                        titles[total_entries] = NULL;
                        ++total_entries;
                    }
                    if (extensionList != NULL)
                    {
                        for (int i = 0; i < extensionCount; i++)
                        {
                            if (extensionList[i] != NULL)
                            {
                                free(extensionList[i]);
                            }
                        }
                        free(extensionList);
                    }
                    return NULL;
                }
                else
                {
                    if (cwd.lastIndexOf('/', cwd.length() - 2) == 0)
                    {
                        // 下一个是根目录
                        cwd = "/";
                        root.close();
                    }
                    else
                    {
                        cwd = cwd.substring(0, cwd.lastIndexOf('/', cwd.length() - 2));
                        root.close();
                    }
                    continue;
                }
            }
            else if (entries[selected].icon == folder_bits)
            {
                selectedStack[depth] = selected;
                depth++;
                if (depth >= 63){
                    depth = 62;
                    log_w("目录深度已达上限,目录选择历史将会发生错乱");
                }
                selectedStack[depth] = 1; // 重置之前可能相同深度的选择
                // 是文件夹
                cwd += entries[selected].title;
                root.close();
                continue;
            }
            else
            {
                selectedStack[depth] = selected;
                sprintf(filedialog_buffer, "%s%s%s", useSD ? "/sd" : "/littlefs", cwd.c_str(), entries[selected].title);
                break;
            }
        }
        total_entries = 1;
        while (titles[total_entries] != NULL)
        {
            free(titles[total_entries]);
            titles[total_entries] = NULL;
            ++total_entries;
        }
        if (extensionList != NULL)
        {
            for (int i = 0; i < extensionCount; i++)
            {
                if (extensionList[i] != NULL)
                {
                    free(extensionList[i]);
                }
            }
            free(extensionList);
        }
        return filedialog_buffer;
    }
}
