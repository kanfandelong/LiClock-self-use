#pragma once

typedef struct
{
    const uint8_t *icon; // 12*12图标,XBM格式
    const char *title;   // 标题
} menu_item;

typedef struct
{
    const bool select; // 是否显示复选框
    const char *title;   // 标题
} menu_select;

namespace GUI
{
    extern int last_buffer_idx;
    bool waitLongPress(int btn); // 检查长按，如果是长按则返回true
    void autoIndentDraw(const char *str, int max_x, int start_x = 2, int fontsize = 13);
    void drawWindowsWithTitle(const char *title = NULL, int16_t x = 0, int16_t y = 0, int16_t w = 296, int16_t h = 128);
    void msgbox(const char *title, const char *msg);
    void info_msgbox(const char *title, const char *msg, int start_x = 68, int start_y = 16);
    bool msgbox_yn(const char *title, const char *msg, const char *yes = NULL, const char *no = NULL);
    int msgbox_number(const char *title, uint16_t digits, int pre_value); // 注意digits，1表示一位，2表示两位，程序中减一
    void drawKeyboard(int selectedRow, int selectedCol);
    const char* englishInput(const char *name = "");
    int msgbox_time(const char *title, int pre_value);
    int menu(const char *title, const menu_item options[], int16_t ico_w = 8, int16_t ico_h = 8);
    int select_menu(const char *title, const menu_select options[]);
    void drawLBM(int16_t x, int16_t y,const char *filename, uint16_t color);
    void drawGrayScaleImage(bool is4Bit, int x, int y, int w, int h, const uint8_t *bitmap);
    void drawgraybitmap(int16_t x, int16_t y, const char *filename, uint16_t color);
    void drawBMP(FS *fs, const char *filename, bool partial_update = 1, bool overwrite = 0, int16_t x = 0, int16_t y = 0, bool with_color  = 1);
    void drawJPG(String name, FS fs);
    bool epd_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t* bitmap);
    // fileManager.cpp
    /**
     * @brief 文件选择器，注意，这个函数完全没有考虑线程安全，no reentrent!!!
     * @param title 标题
     * @param isApp 是否为lua应用选择
     * @param endsWidth 文件尾缀筛选，默认不筛选，如果输入了文件尾缀，则只显示以该字符串结尾的文件
     * @param gotoendsWidth 文件尾缀筛选，默认不筛选，如果输入了文件尾缀，则忽略以该字符串结尾的文件
     * @param cwd 打开的目录，默认为根目录
     * @return 返回文件名，如果返回NULL，则用户取消选择
     */
    const char *fileDialog(const char *title, bool isApp = false, const char *endsWidth = NULL, const char *gotoendsWidth = ".i", String cwd = "/");
} // namespace GUI
