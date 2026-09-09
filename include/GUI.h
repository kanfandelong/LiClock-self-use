#pragma once

enum MenuItemType
{
    MENU_ITEM_ACTION = 0, // 普通菜单项，点击返回索引
    MENU_ITEM_CHECKBOX,   // 多选框，切换 bool 值
    MENU_ITEM_RADIO,      // 单选框，同一 group 互斥
    MENU_ITEM_VALUE       // 数值项，点击进入数字输入
};

typedef struct
{
    const uint8_t *icon; // 图标数据，必须是单色位图，宽高由ico_w和ico_h指定
    const char *title;
    const char *key;       // Preferences 存储键
    int *value;            // 仅 MENU_ITEM_VALUE 有效
    int min, max;            // 仅 MENU_ITEM_VALUE 有效
    uint8_t group;               // 仅 MENU_ITEM_RADIO 有效（同组互斥）
    uint8_t type; // MenuItemType 枚举值
} menu_item_mix;

typedef struct
{
    const uint8_t *icon; // 12*12图标,XBM格式
    const char *title;   // 标题
} menu_item;

typedef struct
{
    bool select;   // 是否显示复选框
    const char *title;   // 标题
    const char *key;
} menu_select;

typedef struct
{
    uint8_t scan;
    uint8_t gray;
    uint16_t w;
    uint16_t h;
} HEADGRAY;

typedef struct
{
    uint8_t scan;
    uint8_t gray;
    uint16_t w;
    uint16_t h;
    uint16_t frametime;
} LBM_V_HEAD;

#define FRAME_MAGIC 0x524C4500 // "REL\0"表示Raw/Encoded LBM

enum frameType
{
    FRAME_TYPE_LBM = 0,
    FRAME_TYPE_LBM_RLE,
    FRAME_TYPE_RAW,
    FRAME_TYPE_RAW_RLE
};

typedef struct
{
    uint32_t signature;
    uint16_t data_len;
    uint8_t data_type;
    uint8_t reserved;
} frame_HEAD;

namespace GUI
{
    extern int last_buffer_idx;
    bool waitLongPress(int btn); // 检查长按，如果是长按则返回true
    void autoIndentDraw(const char *str, int max_x, int start_x = 2, int fontsize = 13);
    void drawWindowsWithTitle(const char *title = NULL, int16_t x = 0, int16_t y = 0, int16_t w = MAX_X, int16_t h = MAX_Y);
    void msgbox(const char *title, const char *msg, uint16_t timeout = 30);
    void info_msgbox(const char *title, const char *msg, int start_x = -1, int start_y = -1);
    bool msgbox_yn(const char *title, const char *msg, const char *yes = NULL, const char *no = NULL);
    int msgbox_number(const char *title, uint16_t digits, int pre_value); // 注意digits，1表示一位，2表示两位，程序中减一
    int64_t msgbox_number64(const char *title, uint16_t digits, int64_t pre_value); // int64数字输入，支持大ID
    uint32_t msgbox_hex(const char *title, uint16_t digits, uint32_t pre_value);
    int64_t msgbox_number64(const char *title, uint16_t digits, int64_t pre_value);
    char* englishInput(const char *name = "");
    int msgbox_time(const char *title, int pre_value);
    int menu(const char *title, const menu_item options[], int16_t ico_w = 8, int16_t ico_h = 8, int default_selected = 0);
    int menu(const char *title, const menu_item_mix options[], int16_t ico_w, int16_t ico_h, int default_selected);
    int select_menu(const char *title, const menu_select options[], int default_selected = 0);
    int rle_decompress(const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_max);
    void PlayLBM_V(int16_t x, int16_t y, const char *filename, uint16_t color);
    void drawLBM(int16_t x, int16_t y, const char *filename, uint16_t color);
    void drawGrayScaleImage(bool is4Bit, int x, int y, int w, int h, const uint8_t *bitmap);
    void drawbitmap(int16_t x, int16_t y, const uint8_t bitmap[],int16_t w, int16_t h, uint16_t color);
    void drawBMP(const char *filename, bool partial_update = 1, bool overwrite = 0, int16_t x = 0, int16_t y = 0, bool with_color  = 1);
    void drawJPG(String name, FS fs);
    bool epd_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t* bitmap);
    // fileManager.cpp
    /**
     * @brief 文件选择器，注意，这个函数完全没有考虑线程安全，no reentrent!!!
     * @param title 标题
     * @param isApp 是否为lua应用选择
     * @param endsWidth 文件尾缀筛选，默认不筛选，如果输入了文件尾缀，则只显示以该字符串结尾的文件
     * @param gotoendsWidth 文件尾缀筛选，默认筛选".i"后缀文件，文件列表不显示以该字符串结尾的文件
     * @param cwd 打开的目录，默认为根目录
     * @param file_system 文件系统，默认NULL(会提示用户选择文件系统),传入"TF"或"LittleFS"字符串以选择打开的文件系统
     * @note  传入NULL以禁用，endsWidth,gotoendsWidth,gotoendsWidth,file_system
     * @return 用户选择的文件
     */
    const char *fileDialog(const char *title, bool isApp = false, const char *endsWidth = NULL, const char *gotoendsWidth = ".i", String cwd = "/", const char *file_system = NULL, bool cleardepth = true);
} // namespace GUI
