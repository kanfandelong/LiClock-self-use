#include <A_Config.h>
#include <GUI.h>
#include <dirent.h>
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
static const uint8_t fontfile_bits[] = {
    0x7e, 0x00, 0xa1, 0x00, 0x21, 0x01, 0xc1, 0x01, 0x1d, 0x01, 0x09, 0x01,
    0x09, 0x01, 0x09, 0x01, 0x09, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 0x01};
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
    {"m4a", musicfile_bits},
    {"ogg", musicfile_bits},
    {"ttf", fontfile_bits},
    {"TTF", fontfile_bits},
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

String truncatePath(const String &cwd, U8G2_FOR_ADAFRUIT_GFX &u8g2)
{
    const int16_t maxWidth = 200;

    // 1. 根目录直接返回
    if (cwd == "/")
    {
        return cwd;
    }

    // 2. 检查完整路径是否合适
    if (u8g2.getUTF8Width(cwd.c_str()) <= maxWidth)
    {
        return cwd;
    }

    // 3. 分割路径
    std::vector<String> parts;
    int start = 0;
    int end = cwd.indexOf('/');

    while (end != -1)
    {
        if (end > start)
        { // 忽略空的部分
            parts.push_back(cwd.substring(start, end));
        }
        start = end + 1;
        end = cwd.indexOf('/', start);
    }
    // 添加最后一部分
    if (start < cwd.length())
    {
        parts.push_back(cwd.substring(start));
    }

    // 4. 逐步截断路径
    // 从后向前保留更多目录
    for (int keepParts = parts.size(); keepParts >= 1; keepParts--)
    {
        // 构建路径
        String path;
        if (keepParts == parts.size())
        {
            // 完整路径
            path = "/";
            for (int i = 0; i < parts.size(); i++)
            {
                path += parts[i];
                if (i < parts.size() - 1)
                    path += "/";
            }
        }
        else if (keepParts == 1)
        {
            // 只剩下最后一部分
            path = ".../" + parts.back();
        }
        else
        {
            // 保留最后几部分
            path = "...";
            for (int i = parts.size() - keepParts; i < parts.size(); i++)
            {
                path += "/" + parts[i];
            }
        }

        // 检查宽度
        if (u8g2.getUTF8Width(path.c_str()) <= maxWidth)
        {
            return path;
        }
    }

    // 5. 如果连".../dir_name"都超过200，直接返回它
    return ".../" + parts.back();
}

namespace GUI
{
    char filedialog_buffer[512];
    void push_buffer();
    void pop_buffer();
    const char *fileDialog(const char *title, bool isApp, const char *endsWidth, const char *gotoendsWidth, String cwd, const char *file_system, bool cleardepth)
    {
        display.setPowerMode(POWER_MODE_HPM);
        // 注意，这个函数完全没有考虑线程安全，no reentrent!!!
        bool extListAllocated = false;
        bool entriesAllocated = false;
        bool useSD = false;
        DIR *dir = NULL;
        menu_item *entries = NULL;
        char **extensionList = NULL;
        const char *result = NULL;
        int extensionCount = 0;
        int total_entries = 0;
        int max_entries = 256;
        static uint8_t selectedStack[64] = {1}; // 假设最多64层目录
        static int8_t depth = 0;
        // 处理多扩展名过滤

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
            extListAllocated = true;
        }
        // 首先选择文件系统
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

        if (cleardepth)
        {
            depth = 0;
            memset(selectedStack, 1, sizeof(selectedStack));
        }
        entries = (menu_item *)malloc(max_entries * sizeof(menu_item));
        if (entries == NULL)
        {
            GUI::msgbox("严重错误", "[文件管理] 动态内存不足");
            result = NULL;
            goto clean;
        }
        entriesAllocated = true;

        for (int i = 0; i < max_entries; i++)
        {
            entries[i].icon = NULL;
            entries[i].title = NULL;
        }

        entries[0].icon = folder_bits;
        entries[0].title = "..";
        while (1)
        {
            log_printf("[文件浏览] 当前工作目录：");
            log_printf("%s\n", cwd.c_str());
            // 首先清除
            for (int i = 1; i < total_entries; ++i)
            {
                if (entries[i].title)
                {
                    free((void *)entries[i].title);
                    entries[i].title = NULL;
                }
                entries[i].icon = NULL;
            }
            total_entries = 1;
            int try_open = 0;
        open_root:
            String dir_name;
            String full_cwd = useSD ? "/sd" : "/littlefs";
            if (cwd != "/")
                cwd += "/";
            if (!cwd.startsWith("/"))
                full_cwd += "/";
            full_cwd += cwd;
            if (!full_cwd.endsWith("/") && cwd != "/")
                full_cwd += "/"; // 可选
            dir_name = full_cwd;
            // 移除末尾多余的 '/'
            if (dir_name.endsWith("/") && dir_name != "/")
            {
                dir_name.remove(dir_name.length() - 1);
            }
            if (dir){
                closedir(dir);
                dir = NULL;
            }
            dir = opendir(dir_name.c_str());
            if (!dir)
            {
                log_e("[文件] dir未打开");
                cwd = "/";
                try_open++;
                if (try_open > 10)
                {
                    result = NULL;
                    goto clean;
                }
                goto open_root;
            }
            unsigned long start_time = millis(), end_time;
            // GUI::info_msgbox(truncatePath(full_cwd, u8g2Fonts).c_str(), "正在构建文件列表...");
            bool show_files = hal.pref.getBool("show_files", false);

            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL)
            {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                {
                    continue;
                }
                // 获取文件名 C 字符串
                const char *name = entry->d_name;
                bool isDir = (entry->d_type == DT_DIR);
                size_t nameLen = strlen(name);

                // 查找扩展名（最后一个点）
                const char *dot = strrchr(name, '.');
                const char *ext = dot ? (dot + 1) : "";
                size_t extLen = dot ? (nameLen - (dot - name) - 1) : 0;

                // 调试输出（避免重复调用 endsWith）
                if (show_files)
                {
                    if (isDir)
                    {
                        log_printf("\033[36m%s\033[0m\n", name);
                    }
                    else
                    {
                        // 检查扩展名是否为 ".lua"
                        if (extLen == 3 && strcmp(ext, "lua") == 0)
                        {
                            log_printf("\033[32m%s\033[0m\n", name);
                        }
                        else
                        {
                            log_printf("%s\n", name);
                        }
                    }
                }

                // 跳过 gotoendsWidth 的文件（后缀匹配）
                bool skip = false;
                if (gotoendsWidth)
                {
                    size_t suffixLen = strlen(gotoendsWidth);
                    if (nameLen >= suffixLen && strcmp(name + nameLen - suffixLen, gotoendsWidth) == 0)
                    {
                        skip = true;
                    }
                }
                if (skip)
                {
                    continue;
                }

                // 检查是否需要扩容（写入前）
                if (total_entries >= max_entries)
                {
                    int new_capacity = max_entries + 64;
                    menu_item *new_entries = (menu_item *)realloc(entries, (new_capacity + 1) * sizeof(menu_item));
                    if (!new_entries)
                    {
                        GUI::msgbox("内存不足", "无法扩展菜单数组");
                        result = NULL;
                        goto clean;
                    }
                    entries = new_entries;
                    // 清零新增部分（从 max_entries 到 new_capacity-1）
                    for (int i = max_entries; i < new_capacity; ++i)
                    {
                        entries[i].icon = NULL;
                        entries[i].title = NULL;
                    }
                    entries[new_capacity].icon = NULL; // 哨兵
                    entries[new_capacity].title = NULL;
                    max_entries = new_capacity;
                }

                // 根据模式处理条目
                if (!isApp)
                {
                    if (isDir)
                    {
                        entries[total_entries].icon = folder_bits;
                    }
                    else
                    {
                        // 扩展名过滤
                        if (endsWidth)
                        {
                            bool match = false;
                            if (extensionList)
                            {
                                for (int i = 0; i < extensionCount; ++i)
                                {
                                    if (extensionList[i] && strcmp(extensionList[i], ext) == 0)
                                    {
                                        match = true;
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                match = (strcmp(endsWidth, ext) == 0);
                            }
                            if (!match)
                            {
                                continue;
                            }
                        }
                        entries[total_entries].icon = getFileIcon(ext);
                    }
                    // 分配文件名内存
                    char *titleCopy = strdup(name);
                    if (!titleCopy)
                    {
                        // 内存不足
                        GUI::msgbox("内存不足", "无法分配文件名");
                        result = NULL;
                        goto clean;
                    }
                    entries[total_entries].title = titleCopy;
                    ++total_entries;
                }
                else
                {
                    // App 模式：只显示扩展名为 "app" 的目录
                    if (isDir && extLen == 3 && strcmp(ext, "app") == 0)
                    {
                        entries[total_entries].icon = luafile_bits;
                        char *titleCopy = strdup(name);
                        if (!titleCopy)
                        {
                            GUI::msgbox("内存不足", "无法分配文件名");
                            result = NULL;
                            goto clean;
                        }
                        entries[total_entries].title = titleCopy;
                        ++total_entries;
                    }
                }
            }
            closedir(dir);
            dir = NULL;
            // 设置哨兵
            entries[total_entries].icon = NULL;
            entries[total_entries].title = NULL;
            end_time = millis() - start_time;
            log_i(" [文件] 创建文件列表，耗时%.2fs, 共%d个列表项", (float)end_time / 1000.0, total_entries);
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
                    result = NULL;
                    goto clean; // 直接退出
                }
                else
                {
                    if (cwd.lastIndexOf('/', cwd.length() - 2) == 0)
                    {
                        // 下一个是根目录
                        cwd = "/";
                    }
                    else
                    {
                        cwd = cwd.substring(0, cwd.lastIndexOf('/', cwd.length() - 2));
                    }
                    continue;
                }
            }
            else if (entries[selected].icon == folder_bits)
            {
                selectedStack[depth] = selected;
                depth++;
                if (depth >= 63)
                {
                    depth = 62;
                    log_w("目录深度已达上限,目录选择历史将会发生错乱");
                }
                selectedStack[depth] = 1; // 重置之前可能相同深度的选择
                // 是文件夹
                cwd += entries[selected].title;
                continue;
            }
            else
            {
                selectedStack[depth] = selected;
                // 避免缓冲区溢出：使用 snprintf 并限制长度
                snprintf(filedialog_buffer, sizeof(filedialog_buffer), "%s%s%s", useSD ? "/sd" : "/littlefs", cwd.c_str(), entries[selected].title);
                result = filedialog_buffer;
                break;
            }
        }
    clean: // 统一清理入口
        // 关闭目录流
        if (dir)
            closedir(dir);
        // 释放 entries 及其中的 title
        if (entriesAllocated)
        {
            for (int i = 1; i < max_entries; ++i) // 第一个title字符串常量 ".."，不需要释放
            {
                if (entries[i].title != NULL)
                    free((void *)entries[i].title);
            }
            free(entries);
        }
        // 释放 extensionList
        if (extListAllocated)
        {
            for (int i = 0; i < extensionCount; ++i)
            {
                if (extensionList[i])
                    free(extensionList[i]);
            }
            free(extensionList);
        }
        return result;
    }
}
