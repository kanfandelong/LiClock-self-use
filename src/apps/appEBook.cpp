#include "AppManager.h"
// #include <rtc_wdt.h>
static const uint8_t ebook_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0x03,
    0x00, 0x00, 0x00, 0x06, 0x00, 0xfc, 0xff, 0x04, 0x00, 0x00, 0x80, 0x05,
    0x00, 0xff, 0x3f, 0x05, 0x80, 0x00, 0x40, 0x05, 0x80, 0x00, 0x40, 0x05,
    0x80, 0xf0, 0x4f, 0x05, 0x80, 0x00, 0x40, 0x05, 0x80, 0xfe, 0x4f, 0x05,
    0x80, 0x00, 0x40, 0x05, 0x80, 0x3e, 0x40, 0x05, 0x80, 0x00, 0x40, 0x05,
    0x80, 0x00, 0x40, 0x05, 0x80, 0xf0, 0x4f, 0x05, 0x80, 0x00, 0x40, 0x05,
    0x80, 0xfe, 0x4f, 0x05, 0x80, 0x00, 0x40, 0x05, 0x80, 0x3e, 0x40, 0x01,
    0x80, 0x00, 0x40, 0x01, 0x80, 0x00, 0x40, 0x00, 0x80, 0x01, 0x60, 0x00,
    0x00, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
class AppEBook : public AppBase
{
private:
    /* data */
public:
    AppEBook()
    {
        name = "ebook";
        title = "电子书";
        description = "简易电子书";
        image = ebook_bits;
        peripherals_requested = PERIPHERALS_SD_BIT;
        wakeupIO[0] = PIN_BUTTONL;
        wakeupIO[1] = PIN_BUTTONR;
        noDefaultEvent = true;
    }
    void set();
    void setup();
    //////////////////////////
    bool indexcode_3();
    bool indexcode_ttf();
    bool indexFile();
    bool openFile(const char *filename = NULL);
    bool gotoPage(uint32_t page);
    bool draw_page3();
    void drawCurrentPage();
    int findchapterTitle(bool up = false);
    int getTotalPages();
    void openMenu();
    void ebooksettings();
    Preferences ebook_nvs;
    File txtFile, indexesFile;
    char *indexesName;
    char *chapterTitle;
    char *currentFilename;
    size_t currentFileOffset = 0;
    bool page_next = true;
    bool __eof = false;
    bool exit_app = false;       // 是否退出app
    bool need_deepsleep = false; // 在启用lightsleep时是否需要间歇性deepsleep（不进行此操作似乎会导致看门狗复位）
};
RTC_DATA_ATTR uint32_t currentPage = -1; // 0:第一页
RTC_DATA_ATTR bool ebook_run = false;    // 电子书运行标志
RTC_DATA_ATTR bool gotonextpage = false; // 特殊情况自动下一页标志
RTC_DATA_ATTR u8_t lightsleep_count = 0; // lightsleep次数

typedef union
{
    struct
    {
        uint8_t font_h : 8;
        uint8_t font_w : 8;
        bool mode : 1;          // 是否为竖屏
        uint16_t reserved : 15; // 保留位
    };
    uint32_t value; // 整体封装为uint32_t
} index_info;

static AppEBook app;

static void appebook_exit()
{
    hal.cheak_freq(hal.pref.getInt("CpuFreq", 80));
    display.clearScreen();
    display.display();
    if (app.txtFile)
    {
        app.txtFile.close();
    }
    if (app.indexesFile)
    {
        app.indexesFile.close();
    }
    free(app.indexesName);
    free(app.chapterTitle);
    free(app.currentFilename);
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示")))
    {
        u8g2Fonts.setBackgroundColor(TFT_WHITE);
        u8g2Fonts.setForegroundColor(TFT_BLACK);
    }
    // hal.pref.putInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, currentPage);
    app.ebook_nvs.putUInt(hal.get_char_sha_key(app.currentFilename, true), currentPage);
    log_printf("退出电子书，当前页：%d\n", currentPage);
    currentPage = -1;
    ebook_run = false;
}
static void appebook_deepsleep()
{
    if (app.txtFile)
    {
        app.txtFile.close();
    }
    if (app.indexesFile)
    {
        app.indexesFile.close();
    }
    // display.powerOff();
}
void AppEBook::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
// 左键：上一页
// 右键：下一页
// 长按右键：打开菜单

void AppEBook::setup()
{
    display.setPowerMode(POWER_MODE_HPM);
    // esp_task_wdt_init(portMAX_DELAY, false);
    // rtc_wdt_protect_off();
    // // rtc_wdt_set_length_of_reset_signal(RTC_WDT_SYS_RESET_SIG, RTC_WDT_LENGTH_3_2us);
    // // rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_INTERRUPT);
    // // rtc_wdt_set_time(RTC_WDT_STAGE0, 30000); // 30秒超时
    // // rtc_wdt_enable();
    // rtc_wdt_disable();
    // rtc_wdt_protect_on();

    indexesName = (char *)malloc(512);
    chapterTitle = (char *)malloc(512);
    currentFilename = (char *)malloc(512);
    memset(indexesName, 0, 512);
    memset(chapterTitle, 0, 512);
    memset(currentFilename, 0, 512);
    bool page_changed = false;
    ebook_nvs.begin("ebook");
    app.exit = appebook_exit;
    app.deepsleep = appebook_deepsleep;
    app.currentFilename[0] = 0;
    if (hal.pref.getBool(hal.get_char_sha_key("禁用休眠")))
        if (hal.pref.getBool(hal.get_char_sha_key("降频运行Ebook")))
            hal.cheak_freq(80, true);

    if (!(hal.pref.getString("ebook_font", "default") == "default"))
        u8g2Fonts.setFont(hal.pref.getString("ebook_font", "default").c_str());
    else
        u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());

    display.clearScreen();
    size_t s = hal.pref.getBytes(SETTINGS_PARAM_LAST_EBOOK, app.currentFilename, 512);
    if (hal.wakeUpFromDeepSleep == false || currentPage == -1)
    {
        // currentPage = hal.pref.getInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, 0);
        if (s == 0)
        {
            currentPage = 0;
            openFile();
        }
        else
        {
            currentPage = ebook_nvs.getUInt(hal.get_char_sha_key(app.currentFilename, true), 0);
            log_i("电子书：上次打开的文件：%s，上次打开的页：%d", app.currentFilename, currentPage);
            if (openFile(app.currentFilename) == false)
            {
                if (openFile() == false)
                {
                    GUI::msgbox("打开文件失败", currentFilename);
                    hal.pref.remove(SETTINGS_PARAM_LAST_EBOOK);
                    hal.pref.remove(SETTINGS_PARAM_LAST_EBOOK_PAGE);
                    appManager.goBack();
                }
            }
        }
        page_changed = true;
    }
    else
    {
        log_i("从DeepSleep唤醒");
        if (s == 0)
            appManager.goBack();
        if (app.openFile(app.currentFilename) == false)
            appManager.goBack();
    }
    gotoPage(currentPage);
    if ((strncmp(currentFilename, "/littlefs/", 10) == 0) && hal.pref.getBool(hal.get_char_sha_key("使用lightsleep")))
    {
        peripherals.tf_unload();
    }
    // if (hal.btnl.isPressing())
    bool while_run = true;
    while (while_run)
    {
        if (hal.btnc.isPressing())
        {
            if (GUI::waitLongPress(hal.btnc.pin()))
            {
                app.ebook_nvs.putUInt(hal.get_char_sha_key(app.currentFilename, true), currentPage);
                GUI::info_msgbox("提示", "已保存当前页到nvs");
                if (hal.btnc.isPressing())
                {
                    while (hal.btnc.isPressing())
                        delay(10);
                }
            }
            else
            {
                openMenu();
            }
            display.display();
        }
        if (hal.btnl.isPressing() || ((hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) && ebook_run == true && (!hal.pref.getBool(hal.get_char_sha_key("禁用休眠")))))
        {
            if (currentPage == 0)
            {
                GUI::msgbox("提示", "已经是第一页了");
                display.display();
            }
            else if (gotoPage(currentPage - 1) == false)
            {
                GUI::msgbox("提示", "翻页发生错误");
                display.display();
            }
            else
            {
                log_i("上一页 ==> %ld", currentPage);
                page_changed = true;
            }
        }
        if (hal.btnr.isPressing() || ((hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) && ebook_run == true && (!hal.pref.getBool(hal.get_char_sha_key("禁用休眠"))) || (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER && hal.pref.getBool(hal.get_char_sha_key("自动翻页")))))
        {
            if ((!(hal.pref.getBool(hal.get_char_sha_key("禁用休眠")) || hal.pref.getBool(hal.get_char_sha_key("使用lightsleep")))) && GUI::waitLongPress(hal.btnr.pin()))
            {
                log_i("打开菜单");
                // 打开菜单
                openMenu();
                display.display();
                while (hal.btnl.isPressing() || hal.btnr.isPressing())
                {
                    delay(10);
                }
                // return;
            }
            else
            {
                if (gotoPage(currentPage + 1))
                {
                    log_i("下一页 ==> %ld", currentPage);
                    page_changed = true;
                }
                else
                {
                    GUI::msgbox("提示", "已经是最后一页了");
                    log_i("已经是最后一页了");
                    display.display();
                }
            }
        }
        if (page_changed == true)
        {
            page_changed = false;
            drawCurrentPage();
        }
        yield();
        if ((hal.pref.getBool(hal.get_char_sha_key("使用lightsleep")) || hal.pref.getBool(hal.get_char_sha_key("自动翻页"))) && exit_app == false)
        {
            // display.powerOff();
            if (hal.pref.getBool(hal.get_char_sha_key("自动翻页")))
            {
                esp_sleep_enable_timer_wakeup(hal.pref.getInt("auto_page", 10) * 1000000UL);
            }
            if (hal.btn_activelow)
            {
                esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 0);
                esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ANY_LOW);
                gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_LOW_LEVEL);
            }
            else
            {
                if (hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) == true)
                {
                    esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 1);
                    esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ANY_HIGH);
                    gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_HIGH_LEVEL);
                }
                else
                    esp_sleep_enable_ext1_wakeup((1ULL << PIN_BUTTONC) | (1ULL << PIN_BUTTONL) | (1ULL << PIN_BUTTONR), ESP_EXT1_WAKEUP_ANY_HIGH);
            }

            esp_sleep_enable_gpio_wakeup();
            log_i("进入lightsleep");
            // rtc_wdt_disable();
            esp_light_sleep_start();
            // rtc_wdt_enable();
            // rtc_wdt_feed();
            log_i("退出lightsleep");
            lightsleep_count++;
            if (hal.pref.getInt("max_lightsleep", 20) != -1 && lightsleep_count > hal.pref.getInt("max_lightsleep", 20))
            {
                need_deepsleep = true;
                hal.pref.putInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, currentPage);
                lightsleep_count = 0;
            }
        }
        while_run = hal.pref.getBool(hal.get_char_sha_key("使用lightsleep"));
        if (hal.pref.getBool(hal.get_char_sha_key("禁用休眠")))
        {
            if (hal.pref.getBool(hal.get_char_sha_key("降频运行Ebook")))
                hal.cheak_freq(80, true);
            while_run = true;
            while (!(hal.btnc.isPressing() || hal.btnl.isPressing() || hal.btnr.isPressing()) && (!exit_app))
            {
                delay(50);
            }
        }
        if (exit_app || need_deepsleep)
        {
            while_run = false;
            if (need_deepsleep)
            {
                appManager.noDeepSleep = false;
                appManager.nextWakeup = 1;
                need_deepsleep = false;
                return;
            }
            break;
        }
        ebook_run = true;
    }
    if (exit_app)
    {
        appManager.goBack();
    }
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 61 - hal.timeinfo.tm_sec;
}

// 索引格式
// 每4字节代表一页在某个文件中的起始位置
// 文件全部采用UTF 8编码
// 字符宽度：默认英文7,中文14

/* const char *remove_path_prefix(const char *path, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    size_t path_len = strlen(path);

    // 检查路径是否以指定前缀开头
    if (strncmp(path, prefix, prefix_len) == 0)
    {
        // 返回去除前缀后的路径
        return path + prefix_len;
    }
    // 如果路径不以指定前缀开头，则返回原始路径
    return path;
} */
int8_t getCharLength(char zf, bool rebuild = false)
{
    // 控制字符（0-31 和 127）宽度一律为 0
    if ((zf >= 0 && zf <= 31) || zf == 127)
        return 0;

    // 静态查找表，只构建一次
    static int8_t widthTable[128] = {0};
    static bool tableBuilt = false;

    if (!tableBuilt || rebuild)
    {
        // 仅对可打印字符 32-126 进行测量
        for (int c = 32; c <= 126; c++)
        {
            char str[2] = {(char)c, '\0'};
            // u8g2 的 getUTF8Width 返回的是 u8g2_uint_t，转成 int8_t
            widthTable[c] = (int8_t)u8g2Fonts.getUTF8Width(str);
        }
        tableBuilt = true;
    }

    return widthTable[(unsigned char)zf];
}

bool AppEBook::indexcode_3()
{
    bool mode = hal.pref.getBool("Vertical");
    // ========== 1. 获取当前字体的动态参数 ==========
    int8_t fontWidth = u8g2Fonts.getUTF8Width("中") + 1;
    int8_t fontHeight = u8g2Fonts.getFontHeight() + 3; // 字体高度（像素）
    int8_t spaceWidth = u8g2Fonts.getUTF8Width(" ");   // 空格宽度（像素）
    if (fontHeight <= 0)
        fontHeight = 14; // 防止异常
    if (spaceWidth <= 0)
        spaceWidth = fontHeight / 2; // 保底

    // 根据屏幕方向和字体高度计算每页最大行数
    const int screenHeight = mode ? 384 : 168; // 竖屏 384，横屏 168
    int maxline = screenHeight / fontHeight;   // 动态行数
    if (maxline < 1)
        maxline = 1;

    // 文本绘制区域宽度（保持原有边距概念，可随字体调整，此处沿用原值，亦可动态）
    const int textWidthHoriz = 380;
    const int textWidthVert = 165;
    int textWidth = mode ? textWidthVert : textWidthHoriz;

    // ========== 2. 初始化变量 ==========
    String txt[maxline + 1]; // 动态行缓冲
    int8_t line = 0;
    uint16_t currentWidth = 0;     // 当前行累积像素宽度
    bool extraIndentAdded = false; // 行首四空格额外缩进是否已加

    char c;
    uint16_t en_count = 0; // 保留用于旧逻辑兼容（实际未用）
    uint16_t ch_count = 0;
    uint8_t line_old = 0;
    boolean hskgState = 0;

    uint32_t pageCount = 1;
    boolean line0_state = 1;
    uint32_t txtTotalSize = txtFile.size();

    // ========== 3. 打开索引文件 ==========
    txtFile.seek(0, SeekSet);
    long begin = millis(), last = 0;
    if (indexesFile)
    {
        indexesFile.close();
        hal.remove(indexesName);
        indexesFile = hal.open(indexesName, "a", true);
    }
    if (!indexesFile)
    {
        indexesFile = hal.open(indexesName, FILE_APPEND, true);
    }
    indexesFile.setBufferSize(8192);
    txtFile.setBufferSize(8192);

    while (txtFile.available())
    {
        if (line_old != line) // 行首4个空格检测状态重置
        {
            line_old = line;
            hskgState = 1;
        }

        if (line0_state == 1 && line == 0 && pageCount > 1)
        {
            line0_state = 0;
            uint32_t position = txtFile.position(); // 获取当前位置 yswz=页数位置
            // 页数位置编码处理

            indexesFile.write((uint8_t *)&position, 4);

            // 计算剩余量,进度条
            if (millis() - last > 1000)
            {
                uint32_t shengyu_int = txtTotalSize - txtFile.available();
                float shengyu_float = (float(shengyu_int) / float(txtTotalSize)) * 100.0;
                display.clearScreen();
                u8g2Fonts.setCursor(0, 15);
                u8g2Fonts.printf("文件名称：%s", currentFilename);
                u8g2Fonts.setCursor(0, 30);
                u8g2Fonts.printf("文件大小：%0.2fKB", float(txtTotalSize) / 1024.0);
                u8g2Fonts.setCursor(0, 45);
                u8g2Fonts.printf("剩余大小：%0.2fKB", float(txtFile.available()) / 1024.0);
                u8g2Fonts.setCursor(0, 60);
                u8g2Fonts.printf("索引进度：%0.2f%%", shengyu_float);
                display.display();
                log_i("文件名称：%s 索引进度：%0.2f%%", currentFilename, shengyu_float);
                last = millis();
                esp_task_wdt_reset();
            }
        }

        c = txtFile.read();                  // 读取一个字节
        while (c == '\n' && line <= maxline) // 检查换行符,并将多个连续空白的换行合并成一个
        {
            // 检测到首行并且为空白则不需要插入换行
            if (line == 0) // 等于首行，并且首行不为空，才插入换行
            {
                if (txt[line].length() > 0)
                    line++; // 换行
                else
                    txt[line].clear();
            }
            else // 非首行的换行检测
            {
                // 连续空白的换行合并成一个
                if (txt[line].length() > 0)
                    line++;
                else if (txt[line].length() == 0 && txt[line - 1].length() > 0)
                    line++;
                /*else if (txt[line].length() == 1 && txt[line - 1].length() == 1) hh = 0;*/
            }
            if (line <= maxline)
                c = txtFile.read();
            en_count = 0;
            ch_count = 0;
        }
        if (c == '\t') // 检查水平制表符 tab
        {
            if (txt[line].length() == 0)
                txt[line] += "    "; // 行首的一个水平制表符 替换成4个空格
            else
                txt[line] += "       "; // 非行首的一个水平制表符 替换成7个空格
        }
        else if ((c >= 0 && c <= 31) || c == 127) // 检查没有实际显示功能的字符
        {
            // ESP.wdtFeed();  // 喂狗
        }
        else
            txt[line] += c;

        // 检查字符的格式 + 数据处理 + 长度计算
        boolean asciiState = 0;
        byte a = B11100000;
        byte b = c & a;

        if (b == B11100000) // 中文等 3个字节
        {
            ch_count++;
            c = txtFile.read();
            txt[line] += c;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (b == B11000000) // ascii扩展 2个字节
        {
            en_count += fontWidth;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (c == '\t') // 水平制表符，代替两个中文位置，12*2
        {
            if (txt[line] == "    ")
                en_count += spaceWidth * 4; // 行首，因为后面会检测4个空格再加4所以这里是20
            else
                en_count += fontWidth * 2; // 非行首
        }
        else if (c >= 0 && c <= 255)
        {
            en_count += getCharLength(c) + 1; // getCharLength=获取ascii字符的像素长度
            asciiState = 1;
        }

        uint16_t StringLength = en_count + (ch_count * fontWidth); // 一个中文12个像素长度

        if (StringLength >= textWidth - fontWidth * 2 && hskgState) // 检测到行首的4个空格预计的长度再加长一点
        {
            if (txt[line][0] == ' ' && txt[line][1] == ' ' &&
                txt[line][2] == ' ' && txt[line][3] == ' ')
            {
                en_count += 4;
            }
            hskgState = 0;
        }

        if (StringLength >= textWidth - fontWidth - 1) // 283个像素检查是否已填满屏幕 ，填满一行
        {
            if (asciiState == 0)
            {
                line++;
                en_count = 0;
                ch_count = 0;
            }
            else if (StringLength >= textWidth - (fontWidth / 3) * 2)
            {
                char t = txtFile.read();
                txtFile.seek(-1, SeekCur); // 往回移
                int8_t cz = (mode ? 168 : 378) - StringLength;
                int8_t t_length = getCharLength(t);
                byte a = B11100000;
                byte b = t & a;
                if (b == B11100000 || b == B11000000) // 中文 ascii扩展
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                }
                else if (t_length > cz)
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                }
            }
        }
        if (line == maxline)
        {
            line0_state = 1;
            pageCount++;
            line = 0;
            en_count = 0;
            ch_count = 0;
            for (uint8_t i = 0; i < maxline; i++)
                txt[i].clear();
        }
    }

    indexesFile.write((uint8_t *)&txtTotalSize, 4); // 写入索引时txt的文件大小
    index_info info;
    info.mode = mode;
    info.font_h = fontHeight;
    info.font_w = fontWidth;
    indexesFile.write((uint8_t *)&info, 4); // 写入索引时的相关信息
    indexesFile.flush();
    indexesFile.close();

    indexesFile = hal.open(indexesName, "r", true);
    uint32_t indexes_size = indexesFile.size();
    // log_print("yswz_count：");
    // log_println(yswz_count);
    log_i("pageCount：%lu 预期大小：%lu", pageCount, 4 * ((pageCount - 1) + 2));
    log_i("索引文件大小：%lu", indexes_size);

    // 校验索引是否正确建立
    // 算法：一页为4个字节（从第二页开始记录所以要总页数-1），加上文件大小位4个字节
    // 所以为：4*((总页数-1)+1))
    // if (indexes_size == 7 * ((pageCount - 1) + 1 + 1))
    // 算法：一页为4个字节（从第二页开始记录所以要总页数-1）
    if (indexes_size == 4 * ((pageCount - 1) + 2))
    {
        indexesFile.close();
        display.clearScreen();
        u8g2Fonts.setCursor(0, 15);
        u8g2Fonts.printf("文件名称：%s", currentFilename);
        u8g2Fonts.setCursor(0, 30);
        u8g2Fonts.printf("文件大小：%0.2fKB", float(txtTotalSize) / 1024.0);
        u8g2Fonts.setCursor(0, 45);
        u8g2Fonts.printf("剩余大小：0KB");
        u8g2Fonts.setCursor(0, 60);
        u8g2Fonts.printf("索引进度：100%%");
        display.display();
        log_i("文件名称：%s 索引进度：100%%", currentFilename);
    }
    else
    {
        indexesFile.close();
        log_w("校验失败，索引文件大小与预期大小不符");
        // if (strncmp(currentFilename, "/littlefs/", 10) == 0)
        //     LittleFS.remove(indexesName);
        // else if (strncmp(currentFilename, "/sd/", 4) == 0)
        //     SD_MMC.remove(indexesName);
        GUI::msgbox("警告", "索引文件大小与预期大小不符", 5);
    }

    // yswz_str = "";
    // yswz_count = 0;

    txtFile.close();

    uint32_t need = millis() - begin;

    char *tmp = (char *)malloc(256);
    sprintf(tmp, "耗时：%0.2fS,共%d页", (float)need / 1000.0, pageCount);
    GUI::msgbox("索引结束", tmp);
    free(tmp);
    indexesFile = hal.open(indexesName);
    if (!indexesFile)
    {
        GUI::msgbox("索引失败", "索引文件打开失败");
        return false;
    }
    delay(500);
    line = 0;
    en_count = 0;
    ch_count = 0;
    for (uint8_t i = 0; i < maxline; i++)
        txt[i].clear();
    return true;
}

bool AppEBook::indexcode_ttf()
{
    bool mode = hal.pref.getBool("Vertical");
    uint8_t font_size = hal.pref.getUChar("FontSize", 16);
    uint8_t font_width = font_size + 1;
    int8_t maxline = mode ? 21 : 9;
    String txt[mode ? 22 : 10] = {}; // 0-7行为一页 共8行
    int8_t line = 0;                 // 当前行
    char c;                          // 中间数据
    uint16_t en_count = 0;           // 统计ascii和ascii扩展字符 1-2个字节
    uint16_t ch_count = 0;           // 统计中文等 3个字节的字符
    uint8_t line_old = 0;            // 记录旧行位置
    boolean hskgState = 0;           // 行首4个空格检测 0-检测过 1-未检测

    uint32_t pageCount = 1;  // 页数计数
    boolean line0_state = 1; // 每页页首记录状态位
    // uint32_t yswz_count = 0;                // 待写入文件统计
    // String yswz_str = "";                   // 待写入的文件
    uint32_t txtTotalSize = txtFile.size(); // 记录该TXT文件的大小，插入到索引的倒数14-8位
    txtFile.seek(0, SeekSet);
    long begin = millis(), last;
    if (indexesFile)
    {
        indexesFile.close();
        hal.remove(indexesName);
        indexesFile = hal.open(indexesName, "a", true);
    }
    if (!indexesFile)
    {
        indexesFile = hal.open(indexesName, FILE_APPEND, true);
    }
    indexesFile.setBufferSize(8192);
    while (txtFile.available())
    {
        if (line_old != line) // 行首4个空格检测状态重置
        {
            line_old = line;
            hskgState = 1;
        }

        if (line0_state == 1 && line == 0 && pageCount > 1)
        {
            line0_state = 0;
            uint32_t yswz_uint32 = txtFile.position(); // 获取当前位置 yswz=页数位置
            // 页数位置编码处理

            indexesFile.write((uint8_t *)&yswz_uint32, 4);

            /*             if (yswz_uint32 >= 1000000)
                            yswz_str += String(yswz_uint32);
                        else if (yswz_uint32 >= 100000)
                            yswz_str += "0" + String(yswz_uint32);
                        else if (yswz_uint32 >= 10000)
                            yswz_str += "00" + String(yswz_uint32);
                        else if (yswz_uint32 >= 1000)
                            yswz_str += "000" + String(yswz_uint32);
                        else if (yswz_uint32 >= 100)
                            yswz_str += "0000" + String(yswz_uint32);
                        else if (yswz_uint32 >= 10)
                            yswz_str += "00000" + String(yswz_uint32);
                        else
                            yswz_str += "000000" + String(yswz_uint32); */
            /*             yswz_count++;
                        if (yswz_count == 500)
                        {
                            if (!indexesFile)
                            {
                                if (file_fs_sd)
                                {
                                    indexesFile = SD_MMC.open(indexesName, FILE_APPEND);
                                }
                                else
                                {
                                    indexesFile = LittleFS.open(indexesName, "a"); // 在索引文件末尾追加内容
                                }
                            }
                            indexesFile.print(yswz_str); // 将待写入的缓存 写入索引文件中
                            indexesFile.flush();

                            yswz_str = "";  // 待写入文件清空
                            yswz_count = 0; // 待写入计数清空 */

            // 计算剩余量,进度条
            if (millis() - last > 200)
            {
                uint32_t shengyu_int = txtTotalSize - txtFile.available();
                float shengyu_float = (float(shengyu_int) / float(txtTotalSize)) * 100.0;
                display.clearScreen();
                u8g2Fonts.setCursor(0, 15);
                u8g2Fonts.printf("文件名称：%s", currentFilename);
                u8g2Fonts.setCursor(0, 30);
                u8g2Fonts.printf("文件大小：%0.2fKB", float(txtTotalSize) / 1024.0);
                u8g2Fonts.setCursor(0, 45);
                u8g2Fonts.printf("剩余大小：%0.2fKB", float(txtFile.available()) / 1024.0);
                u8g2Fonts.setCursor(0, 60);
                u8g2Fonts.printf("索引进度：%0.2f%%", shengyu_float);
                display.display();
                log_i("文件名称：%s 索引进度：%0.2f%%", currentFilename, shengyu_float);
                last = millis();
            }
            // log_println("写入索引文件");
            // }
            // log_print("第"); log_print(pageCount); log_print("页，页首位置："); log_println(yswz_uint32);
        }

        c = txtFile.read();                          // 读取一个字节
        while (c == '\n' && line <= (mode ? 20 : 8)) // 检查换行符,并将多个连续空白的换行合并成一个
        {
            // 检测到首行并且为空白则不需要插入换行
            if (line == 0) // 等于首行，并且首行不为空，才插入换行
            {
                if (txt[line].length() > 0)
                    line++; // 换行
                else
                    txt[line].clear();
            }
            else // 非首行的换行检测
            {
                // 连续空白的换行合并成一个
                if (txt[line].length() > 0)
                    line++;
                else if (txt[line].length() == 0 && txt[line - 1].length() > 0)
                    line++;
                /*else if (txt[line].length() == 1 && txt[line - 1].length() == 1) hh = 0;*/
            }
            if (line <= (mode ? 20 : 8))
                c = txtFile.read();
            en_count = 0;
            ch_count = 0;
        }
        if (c == '\t') // 检查水平制表符 tab
        {
            if (txt[line].length() == 0)
                txt[line] += "    "; // 行首的一个水平制表符 替换成4个空格
            else
                txt[line] += "       "; // 非行首的一个水平制表符 替换成7个空格
        }
        else if ((c >= 0 && c <= 31) || c == 127) // 检查没有实际显示功能的字符
        {
            // ESP.wdtFeed();  // 喂狗
        }
        else
            txt[line] += c;

        // 检查字符的格式 + 数据处理 + 长度计算
        boolean asciiState = 0;
        byte a = B11100000;
        byte b = c & a;

        if (b == B11100000) // 中文等 3个字节
        {
            ch_count++;
            c = txtFile.read();
            txt[line] += c;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (b == B11000000) // ascii扩展 2个字节
        {
            en_count += 12;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (c == '\t') // 水平制表符，代替两个中文位置，12*2
        {
            if (txt[line] == "    ")
                en_count += 20; // 行首，因为后面会检测4个空格再加4所以这里是20
            else
                en_count += 24; // 非行首
        }
        else if (c >= 0 && c <= 255)
        {
            en_count += getCharLength(c) + 1; // getCharLength=获取ascii字符的像素长度
            asciiState = 1;
        }

        uint16_t StringLength = en_count + (ch_count * 12); // 一个中文12个像素长度

        if (StringLength >= (mode ? 96 : 260) && hskgState) // 检测到行首的4个空格预计的长度再加长一点
        {
            if (txt[line][0] == ' ' && txt[line][1] == ' ' &&
                txt[line][2] == ' ' && txt[line][3] == ' ')
            {
                en_count += 4;
            }
            hskgState = 0;
        }

        if (StringLength >= (mode ? 115 : 283)) // 283个像素检查是否已填满屏幕 ，填满一行
        {
            if (asciiState == 0)
            {
                line++;
                en_count = 0;
                ch_count = 0;
            }
            else if (StringLength >= (mode ? 118 : 286))
            {
                char t = txtFile.read();
                txtFile.seek(-1, SeekCur); // 往回移
                int8_t cz = (mode ? 126 : 294) - StringLength;
                int8_t t_length = getCharLength(t);
                byte a = B11100000;
                byte b = t & a;
                if (b == B11100000 || b == B11000000) // 中文 ascii扩展
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                }
                else if (t_length > cz)
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                }
            }
        }
        if (line == maxline)
        {
            line0_state = 1;
            pageCount++;
            line = 0;
            en_count = 0;
            ch_count = 0;
            for (uint8_t i = 0; i < maxline; i++)
                txt[i].clear();
        }
    }

    // 剩余的字节写入索引文件，并在末尾加入文件大小校验位14-8 页数记录位7-1
    /*     uint32_t size_uint32 = txtTotalSize; // 获取当前TXT文件的大小
        String size_str = "";
        // TXT文件大小编码处理
        if (size_uint32 >= 1000000)
            size_str += String(size_uint32);
        else if (size_uint32 >= 100000)
            size_str += String("0") + String(size_uint32);
        else if (size_uint32 >= 10000)
            size_str += String("00") + String(size_uint32);
        else if (size_uint32 >= 1000)
            size_str += String("000") + String(size_uint32);
        else if (size_uint32 >= 100)
            size_str += String("0000") + String(size_uint32);
        else if (size_uint32 >= 10)
            size_str += String("00000") + String(size_uint32);
        else
            size_str += String("000000") + String(size_uint32); */

    // if (yswz_count != 0) // 还有剩余页数就在末尾加入 剩余的页数+文件大小位+当前位置位（初始0）
    // {
    //     if (!indexesFile)
    //     {
    //         if (file_fs_sd)
    //         {
    //             indexesFile = SD_MMC.open(indexesName, FILE_APPEND);
    //         }
    //         else
    //         {
    //             indexesFile = LittleFS.open(indexesName, "a");
    //         }
    //     }
    //     // indexesFile.print(yswz_str + size_str + "0000000");
    //     indexesFile.print(yswz_str);
    //     indexesFile.flush();
    //     indexesFile.close();
    // }
    // else // 没有剩余页数了就在末尾加入文件大小位+当前位置位
    // {
    //     if (!indexesFile)
    //     {
    //         if (file_fs_sd)
    //             indexesFile = SD_MMC.open(indexesName, FILE_APPEND);
    //         else
    //             indexesFile = LittleFS.open(indexesName, "a");
    //     }
    //     indexesFile.print(size_str + "0000000");
    //     indexesFile.flush();
    //     indexesFile.close();
    // }
    indexesFile.write((uint8_t *)&txtTotalSize, 4);
    indexesFile.flush();
    indexesFile.close();

    indexesFile = hal.open(indexesName, "r", true);
    uint32_t indexes_size = indexesFile.size();
    // log_print("yswz_count：");
    // log_println(yswz_count);
    log_i("pageCount：%lu 预期大小：%lu", pageCount, 4 * ((pageCount - 1) + 1));
    log_i("索引文件大小：%lu", indexes_size);

    // 校验索引是否正确建立
    // 算法：一页为4个字节（从第二页开始记录所以要总页数-1），加上文件大小位4个字节
    // 所以为：4*((总页数-1)+1))
    // if (indexes_size == 7 * ((pageCount - 1) + 1 + 1))
    // 算法：一页为4个字节（从第二页开始记录所以要总页数-1）
    if (indexes_size == 4 * ((pageCount - 1) + 1))
    {
        indexesFile.close();
        display.clearScreen();
        u8g2Fonts.setCursor(0, 15);
        u8g2Fonts.printf("文件名称：%s", currentFilename);
        u8g2Fonts.setCursor(0, 30);
        u8g2Fonts.printf("文件大小：%0.2fKB", float(txtTotalSize) / 1024.0);
        u8g2Fonts.setCursor(0, 45);
        u8g2Fonts.printf("剩余大小：0KB");
        u8g2Fonts.setCursor(0, 60);
        u8g2Fonts.printf("索引进度：100%%");
        display.display();
        log_i("文件名称：%s 索引进度：100%%", currentFilename);
    }
    else
    {
        indexesFile.close();
        log_w("校验失败，索引文件大小与预期大小不符");
        // if (strncmp(currentFilename, "/littlefs/", 10) == 0)
        //     LittleFS.remove(indexesName);
        // else if (strncmp(currentFilename, "/sd/", 4) == 0)
        //     SD_MMC.remove(indexesName);
        GUI::msgbox("警告", "索引文件大小与预期大小不符", 5);
    }

    // yswz_str = "";
    // yswz_count = 0;

    txtFile.close();

    uint32_t need = millis() - begin;

    char *tmp = (char *)malloc(256);
    sprintf(tmp, "耗时：%0.2fS,共%d页", (float)need / 1000.0, pageCount);
    GUI::msgbox("索引结束", tmp);
    free(tmp);
    indexesFile = hal.open(indexesName);
    if (!indexesFile)
    {
        GUI::msgbox("索引失败", "索引文件打开失败");
        return false;
    }
    delay(500);
    line = 0;
    en_count = 0;
    ch_count = 0;
    for (uint8_t i = 0; i < maxline; i++)
        txt[i].clear();
    return true;
}

bool AppEBook::indexFile()
{
    if (hal.pref.getBool(hal.get_char_sha_key("降频运行Ebook")))
        hal.cheak_freq(80, true);
    else
        hal.cheak_freq(hal.pref.getInt("CpuFreq", 240));

    if (!(hal.pref.getString("ebook_font", "default") == "default"))
        u8g2Fonts.setFont(hal.pref.getString("ebook_font", "default").c_str());
    else
        u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());

    return indexcode_3();
}

bool AppEBook::openFile(const char *filename)
{
    if (app.txtFile)
        app.txtFile.close();
    if (app.indexesFile)
        app.indexesFile.close();
    if (filename == NULL)
    {
        const char *name = NULL;
        while (name == NULL)
        {
            name = GUI::fileDialog("请选择文件");
        }
        strcpy(currentFilename, name);
    }
    else
        strcpy(currentFilename, filename);

    sprintf(indexesName, "%s.i", currentFilename);
    txtFile = hal.open(currentFilename);
    indexesFile = hal.open(indexesName);

    if (!txtFile)
        return false;
    if (!indexesFile)
    {
        bool index_ok = indexFile();
        hal.cheak_freq(hal.pref.getInt("CpuFreq", 80));
        if (!index_ok)
            return false;
    }
    uint32_t lasttxtsize, nowtxtsize = txtFile.size();
    index_info info;
    indexesFile.seek(indexesFile.size() - 8, SeekSet);
    indexesFile.readBytes((char *)&lasttxtsize, 4);
    indexesFile.readBytes((char *)&info, 4);
    indexesFile.seek(0, SeekSet);
    log_i("lasttxtsize: %lu nowtxtsize: %lu", lasttxtsize, nowtxtsize);
    if (lasttxtsize != nowtxtsize)
    {
        if (GUI::msgbox_yn("提示", "txt文件大小与创建索引时不同，是否重建索引", "重建", "忽略"))
        {
            bool index_ok = indexFile();
            hal.cheak_freq(hal.pref.getInt("CpuFreq", 80));
            if (!index_ok)
                return false;
        }
    }
    if (hal.pref.getBool("Vertical") != info.mode)
    {
        if (GUI::msgbox_yn("提示", "当前屏幕方向与创建索引时不同，是否重建索引", "重建", "忽略"))
        {
            bool index_ok = indexFile();
            hal.cheak_freq(hal.pref.getInt("CpuFreq", 80));
            if (!index_ok)
                return false;
        }
    }
    else
    {
        log_i("索引时的屏幕方向与当前一致");
    }

    hal.pref.putBytes(SETTINGS_PARAM_LAST_EBOOK, app.currentFilename, strlen(app.currentFilename));
    return true;
}

bool AppEBook::gotoPage(uint32_t page)
{
    static uint32_t last_page = 0;
    if (page > last_page)
        page_next = true;
    else
        page_next = false;
    last_page = page;

    if (!indexesFile)
    {
        indexesFile = hal.open(indexesName);
        if (!indexesFile)
            log_e("indexesFile not open");
    }
    if (page == 0)
    {
        currentPage = 0;
        currentFileOffset = 0;
        txtFile.seek(currentFileOffset, SeekSet);
        return true;
    }
    else
    {
        // uint32_t gbwz = 0;    // 计算上一页的页首位置
        // String gbwz_str = ""; // 光标位置String
        // log_print("当前页1："); log_println(pageCurrent);
        // 计算上一页的页首位置
        // 因为第一页不需要记录所以要减1，因为我要的是上一页所以再减1
        // gbwz = (page + 1) * 7 - 7;
        // gbwz = (page + 1) * 4 - 4;
        // log_print("gbwz："); log_println(gbwz);
        // 打开索引，寻找上一页的页首位置
        indexesFile.seek((page - 1) * 4, SeekSet);
        // 获取索引的数据
        /*             for (uint8_t i = 0; i < 7; i++)
                    {
                        char c = indexesFile.read();
                        gbwz_str += c;
                    }
                    uint32_t gbwz_uint32 = atol(gbwz_str.c_str()); // 装换成int格式 */
        uint32_t gbwz_uint32;
        if (indexesFile.read((uint8_t *)&gbwz_uint32, 4) == 0)
            return false;
        // indexesFile.close();
        currentFileOffset = gbwz_uint32;
        // log_i("%ld", gbwz_uint32);
        // if (currentFileOffset > txtFile.size())
        //     return false;
        log_i("go to page %lu", page);
        log_i("seekset ==> %lu", currentFileOffset);
        txtFile.seek(currentFileOffset, SeekSet);
        currentPage = page;
        return true;
    }

    return false;
}

bool AppEBook::draw_page3()
{
    bool mode = hal.pref.getBool("Vertical");
    // ========== 1. 获取当前字体的动态参数 ==========
    int8_t fontWidth = u8g2Fonts.getUTF8Width("中") + 1;
    int8_t fontHeight = u8g2Fonts.getFontHeight() + 3; // 字体高度（像素）
    int8_t spaceWidth = u8g2Fonts.getUTF8Width(" ");   // 空格宽度（像素）
    if (fontHeight <= 0)
        fontHeight = 14; // 防止异常
    if (spaceWidth <= 0)
        spaceWidth = fontHeight / 2; // 保底

    // 根据屏幕方向和字体高度计算每页最大行数
    const int screenHeight = mode ? 384 : 168;   // 竖屏 384，横屏 168
    int maxline = screenHeight / fontHeight; // 动态行数
    if (maxline < 1)
        maxline = 1;

    // 文本绘制区域宽度（保持原有边距概念，可随字体调整，此处沿用原值，亦可动态）
    const int textWidthHoriz = 380;
    const int textWidthVert = 165;
    int textWidth = mode ? textWidthVert : textWidthHoriz;

    // log_i("fontHeight: %d, fontWidth: %d, spaceWidth: %d, maxline: %d, textWidth: %d", fontHeight, fontWidth, spaceWidth, maxline, textWidth);

    // ========== 2. 初始化变量 ==========
    String txt[maxline + 1]; // 动态行缓冲
    int8_t line = 0;
    uint16_t currentWidth = 0;     // 当前行累积像素宽度
    bool extraIndentAdded = false; // 行首四空格额外缩进是否已加

    char c;
    uint16_t en_count = 0; // 保留用于旧逻辑兼容（实际未用）
    uint16_t ch_count = 0;
    uint8_t line_old = 0;
    boolean hskgState = 0;

    uint32_t pageCount = 1;
    boolean line0_state = 1;

    if (!txtFile)
    {
        txtFile = hal.open(currentFilename);

        if (!txtFile)
            log_e("%s 打开失败", currentFilename);
        if (!gotoPage(currentPage))
            return false;
    }
    while (line < maxline)
    {
        if (line_old != line) // 行首4个空格检测状态重置
        {
            line_old = line;
            hskgState = 1;
        }

        c = txtFile.read(); // 读取一个字节

        while (c == '\n' && line <= maxline) // 检查换行符,并将多个连续空白的换行合并成一个
        {
            // 检测到首行并且为空白则不需要插入换行
            if (line == 0) // 等于首行，并且首行不为空，才插入换行
            {
                if (txt[line].length() > 0)
                    line++; // 换行
                else
                    txt[line].clear();
            }
            else // 非首行的换行检测
            {
                // 连续空白的换行合并成一个
                if (txt[line].length() > 0)
                    line++;
                else if (txt[line].length() == 0 && txt[line - 1].length() > 0)
                    line++;
                /*else if (txt[line].length() == 1 && txt[line - 1].length() == 1) hh = 0;*/
            }
            if (line <= maxline)
                c = txtFile.read();
            en_count = 0;
            ch_count = 0;
        }

        if (c == '\t') // 检查水平制表符 tab
        {
            if (txt[line].length() == 0)
                txt[line] += "    "; // 行首的一个水平制表符 替换成4个空格
            else
                txt[line] += "       "; // 非行首的一个水平制表符 替换成7个空格
        }
        else if ((c >= 0 && c <= 31) || c == 127) // 检查没有实际显示功能的字符
        {
            // ESP.wdtFeed();  // 喂狗
        }
        else
            txt[line] += c;
        // 检查字符的格式 + 数据处理 + 长度计算
        boolean asciiState = 0;
        byte a = B11100000;
        byte b = c & a;

        if (b == B11100000) // 中文等 3个字节
        {
            ch_count++;
            c = txtFile.read();
            txt[line] += c;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (b == B11000000) // ascii扩展 2个字节
        {
            en_count += fontWidth;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (c == '\t') // 水平制表符，代替两个中文位置，12*2
        {
            if (txt[line] == "    ")
                en_count += spaceWidth * 4; // 行首，因为后面会检测4个空格再加4所以这里是20
            else
                en_count += fontWidth * 2; // 非行首
        }
        else if (c >= 0 && c <= 255)
        {
            en_count += getCharLength(c) + 1;
            asciiState = 1;
        }

        uint16_t StringLength = en_count + (ch_count * fontWidth);

        if (StringLength >= textWidth - fontWidth * 2 && hskgState) // 检测到行首的4个空格预计的长度再加长一点
        {
            if (txt[line][0] == ' ' && txt[line][1] == ' ' &&
                txt[line][2] == ' ' && txt[line][3] == ' ')
            {
                en_count += 4;
            }
            hskgState = 0;
        }

        /*if (line == 4)
          {
          log_println("");
          log_println(txt[line]);
          log_print("ch_count:"); log_println(ch_count);
          log_print("en_count:"); log_println(en_count);
          log_print("预计像素长度:"); log_println(StringLength);
          log_print("实际像素长度:"); log_printf(u8g2Fonts.getUTF8Width(txt[line].c_str()));
          }*/

        if (StringLength >= textWidth - fontWidth - 1) // 检查是否已填满屏幕 283
        {
            // log_println("");
            // log_print("行"); log_print(line); log_print(" 预计像素长度:"); log_println(StringLength);
            // log_print("行"); log_print(line); log_print(" 实际像素长度:"); log_println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
            if (asciiState == 0) // 最后一个字符是中文，直接换行
            {
                line++;
                en_count = 0;
                ch_count = 0;
            }
            else if (StringLength >= textWidth - (fontWidth / 3) * 2) // 286 最后一个字符不是中文，在继续检测
            {
                char t = txtFile.read();
                txtFile.seek(-1, SeekCur); // 往回移
                int8_t cz = (mode ? 168 : 378) - StringLength;
                int8_t t_length = getCharLength(t);
                /*log_print("字符t:"); log_println(t);
                  log_print("字符t:"); log_println(t, HEX);
                  log_print("t长度:"); log_println(t_length);
                  log_print("差值:"); log_println(cz);*/
                byte a = B11100000;
                byte b = t & a;
                if (b == B11100000 || b == B11000000) // 中文 ascii扩展
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    // log_println("测试2");
                }
                else if (t_length > cz)
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    // log_println("测试3");
                }
            }
        }
    }
    // for (uint8_t i = 0; i < 8; i++) log_println(txt[i]); //串口输出内容
    display.swapBuffer(3);

    if (mode)
    {
        if (hal.pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3) == 3)
        {
            log_i("设置方向 ==> 2");
            display.setRotation(2);
        }
        if (hal.pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3) == 1)
        {
            log_i("设置方向 ==> 0");
            display.setRotation(0);
        }
        display.setDrawWindow(0, 0, 168, 384);
    }
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示")))
    {
        display.clearScreen(TFT_BLACK);
        u8g2Fonts.setBackgroundColor(TFT_BLACK);
        u8g2Fonts.setForegroundColor(TFT_WHITE);
    }
    else
    {
        display.clearScreen();
        u8g2Fonts.setBackgroundColor(TFT_WHITE);
        u8g2Fonts.setForegroundColor(TFT_BLACK);
    }
    for (uint8_t i = 0; i < maxline; i++)
    {
        uint8_t offset = 0;    // 缩减偏移量
        if (txt[i][0] == 0x20) // 检查首行是否为半角空格 0x20
        {
            // 继续检测后3位是否为半角空格，检测到连续的4个半角空格，偏移12个像素
            if (txt[i][1] == 0x20 && txt[i][2] == 0x20 && txt[i][3] == 0x20)
                // offset = 12;
                offset = 0;
        }
        else if (txt[i][0] == 0xE3 && txt[i][1] == 0x80 && txt[i][2] == 0x80) // 检查首行是否为全角空格 0x3000 = E3 80 80
        {
            // 继续检测后2位是否为全角空格，检测到连续的2个全角空格，偏移2个像素
            // if (txt[i][3] == 0xE3 && txt[i][4] == 0x80 && txt[i][5] == 0x80)
            //     offset = 2;
            txt[i].replace(String((char)0xE3) + String((char)0x80) + String((char)0x80), "  ");
            offset = 0;
        }
        // txt[i].replace(String("—"), "--");
        u8g2Fonts.setCursor(4 + offset, i * fontHeight + (fontHeight - 1));
        u8g2Fonts.print(txt[i]);
        // log_i("%s", txt[i].c_str());
    }
    display.swapBuffer(0);

    int orientation = hal.pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3);
    int duration = mode ? 250 : 500;
    SlideDirection direction;

    if (mode)
    {
        if (orientation == 3)
        {
            direction = page_next ? SLIDE_DOWN : SLIDE_UP;
        }
        else if (orientation == 1)
        {
            direction = page_next ? SLIDE_UP : SLIDE_DOWN;
        }
    }
    else
    {
        if (orientation == 3)
        {
            direction = page_next ? SLIDE_LEFT : SLIDE_RIGHT;
        }
        else if (orientation == 1)
        {
            direction = page_next ? SLIDE_RIGHT : SLIDE_LEFT;
        }
    }

    display.slideScreenFull(direction, duration, 3);

    display.copyBuffer(0, 3);
    display.display(true);

    if (mode)
    {
        display.setRotation(hal.pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3));
        display.setDrawWindow();
    }
    return true;
}
void AppEBook::drawCurrentPage()
{
    // if (hal.pref.getBool(hal.get_char_sha_key("快速显示")))
    // display.epd2.PLL_set(0x3A);
    // else
    // display.epd2.PLL_set(hal.pref.getUInt("ebook_pllset", 0x3C));
    bool state;

    if (!(hal.pref.getString("ebook_font", "default") == "default"))
        u8g2Fonts.setFont(hal.pref.getString("ebook_font", "default").c_str());
    else
        u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());

    state = draw_page3();

    if (!state)
        GUI::info_msgbox("错误", "绘制文本中出现错误");
    // display.epd2.PLL_set(hal.pref.getUInt("pllset", 0x3C));
}

/**
 * 检查字符串是否包含章节标题
 * @param str 输入的字符串
 * @return 如果符合章节规则返回true，否则返回false
 */
bool isChapterTitle(const String &str)
{
    // 快速预检查：必须同时包含"第"和"章"
    int diIndex = str.indexOf("第");
    int zhangIndex = str.indexOf("章");

    // log_i("%s %d %d", str.c_str(), diIndex, zhangIndex);
    if (diIndex == -1 || zhangIndex == -1)
    {
        return false;
    }
    // return true;
    // 检查"第"必须在"章"之前
    if (diIndex >= zhangIndex)
    {
        return false;
    }

    // 检查中间部分长度（字节数）
    // int contentStart = diIndex + 3; // "第"在UTF-8中占3字节
    // int contentLength = zhangIndex - contentStart;

    // if (contentLength <= 0 || contentLength > 30)
    // {
    //     return false;
    // }

    // return isValidChapterContent(str.substring(contentStart, zhangIndex));
    return true;
}

int AppEBook::findchapterTitle(bool up)
{
    bool mode = hal.pref.getBool("Vertical");
    // ========== 1. 获取当前字体的动态参数 ==========
    int8_t fontWidth = u8g2Fonts.getUTF8Width("中") + 1;
    int8_t fontHeight = u8g2Fonts.getFontHeight() + 3; // 字体高度（像素）
    int8_t spaceWidth = u8g2Fonts.getUTF8Width(" ");   // 空格宽度（像素）
    if (fontHeight <= 0)
        fontHeight = 14; // 防止异常
    if (spaceWidth <= 0)
        spaceWidth = fontHeight / 2; // 保底

    // 根据屏幕方向和字体高度计算每页最大行数
    const int screenHeight = mode ? 384 : 168;   // 竖屏 384，横屏 168
    int maxline = screenHeight / fontHeight + 3; // 动态行数
    if (maxline < 1)
        maxline = 1;

    // 文本绘制区域宽度（保持原有边距概念，可随字体调整，此处沿用原值，亦可动态）
    const int textWidthHoriz = 380;
    const int textWidthVert = 165;
    int textWidth = mode ? textWidthVert : textWidthHoriz;

    // log_i("fontHeight: %d, fontWidth: %d, spaceWidth: %d, maxline: %d, textWidth: %d", fontHeight, fontWidth, spaceWidth, maxline, textWidth);

    // ========== 2. 初始化变量 ==========
    String txt[maxline + 1]; // 动态行缓冲
    int8_t line = 0;
    uint16_t currentWidth = 0;     // 当前行累积像素宽度
    bool extraIndentAdded = false; // 行首四空格额外缩进是否已加

    char c;
    uint16_t en_count = 0; // 保留用于旧逻辑兼容（实际未用）
    uint16_t ch_count = 0;
    uint8_t line_old = 0;
    boolean hskgState = 0;

    uint32_t pageCount = 1;
    boolean line0_state = 1;

    uint32_t save_page = currentPage;
    bool is_find = false;
    if (!txtFile)
    {

        txtFile = hal.open(currentFilename);
        gotoPage(currentPage);
    }
begin:
    if (hal.btnl.isPressing())
    {
        if (GUI::waitLongPress(hal.btnl.pin()))
        {
            return save_page + 1;
        }
    }
    if (up)
    {
        if (!gotoPage(currentPage - 1))
            return save_page + 1;
    }
    else
    {
        if (!gotoPage(currentPage + 1))
            return save_page + 1;
    }
    log_i("查找%Ld页", currentPage);
    for (uint8_t a = 0; a < maxline; a++)
    {
        txt[a].clear();
    }
    line = 0;      // 当前行
    en_count = 0;  // 统计ascii和ascii扩展字符 1-2个字节
    ch_count = 0;  // 统计中文等 3个字节的字符
    line_old = 0;  // 记录旧行位置
    hskgState = 1; // 行首4个空格检测 0-检测过 1-未检测
    while (line < maxline)
    {
        if (line_old != line) // 行首4个空格检测状态重置
        {
            line_old = line;
            hskgState = 1;
        }

        c = txtFile.read(); // 读取一个字节

        while (c == '\n' && line <= maxline) // 检查换行符,并将多个连续空白的换行合并成一个
        {
            // 检测到首行并且为空白则不需要插入换行
            if (line == 0) // 等于首行，并且首行不为空，才插入换行
            {
                if (txt[line].length() > 0)
                    line++; // 换行
                else
                    txt[line].clear();
            }
            else // 非首行的换行检测
            {
                // 连续空白的换行合并成一个
                if (txt[line].length() > 0)
                    line++;
                else if (txt[line].length() == 0 && txt[line - 1].length() > 0)
                    line++;
                /*else if (txt[line].length() == 1 && txt[line - 1].length() == 1) hh = 0;*/
            }
            if (line <= maxline)
                c = txtFile.read();
            en_count = 0;
            ch_count = 0;
        }

        if (c == '\t') // 检查水平制表符 tab
        {
            if (txt[line].length() == 0)
                txt[line] += "    "; // 行首的一个水平制表符 替换成4个空格
            else
                txt[line] += "       "; // 非行首的一个水平制表符 替换成7个空格
        }
        else if ((c >= 0 && c <= 31) || c == 127) // 检查没有实际显示功能的字符
        {
            // ESP.wdtFeed();  // 喂狗
        }
        else
            txt[line] += c;
        // 检查字符的格式 + 数据处理 + 长度计算
        boolean asciiState = 0;
        byte a = B11100000;
        byte b = c & a;

        if (b == B11100000) // 中文等 3个字节
        {
            ch_count++;
            c = txtFile.read();
            txt[line] += c;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (b == B11000000) // ascii扩展 2个字节
        {
            en_count += 12;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (c == '\t') // 水平制表符，代替两个中文位置，12*2
        {
            if (txt[line] == "    ")
                en_count += spaceWidth * 4; // 行首，因为后面会检测4个空格再加4所以这里是20
            else
                en_count += fontWidth * 2; // 非行首
        }
        else if (c >= 0 && c <= 255)
        {
            en_count += getCharLength(c) + 1;
            asciiState = 1;
        }

        uint16_t StringLength = en_count + (ch_count * fontWidth);

        if (StringLength >= textWidth - fontWidth * 2 && hskgState) // 检测到行首的4个空格预计的长度再加长一点
        {
            if (txt[line][0] == ' ' && txt[line][1] == ' ' &&
                txt[line][2] == ' ' && txt[line][3] == ' ')
            {
                en_count += 4;
            }
            hskgState = 0;
        }

        /*if (line == 4)
          {
          log_println("");
          log_println(txt[line]);
          log_print("ch_count:"); log_println(ch_count);
          log_print("en_count:"); log_println(en_count);
          log_print("预计像素长度:"); log_println(StringLength);
          log_print("实际像素长度:"); log_println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
          }*/

        if (StringLength >= textWidth - fontWidth - 1) // 检查是否已填满屏幕 283
        {
            // log_println("");
            // log_print("行"); log_print(line); log_print(" 预计像素长度:"); log_println(StringLength);
            // log_print("行"); log_print(line); log_print(" 实际像素长度:"); log_println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
            if (asciiState == 0) // 最后一个字符是中文，直接换行
            {
                line++;
                en_count = 0;
                ch_count = 0;
            }
            else if (StringLength >= textWidth - (fontWidth / 3) * 2) // 286 最后一个字符不是中文，在继续检测
            {
                char t = txtFile.read();
                txtFile.seek(-1, SeekCur); // 往回移
                int8_t cz = (mode ? 168 : 378) - StringLength;
                int8_t t_length = getCharLength(t);
                /*log_print("字符t:"); log_println(t);
                  log_print("字符t:"); log_println(t, HEX);
                  log_print("t长度:"); log_println(t_length);
                  log_print("差值:"); log_println(cz);*/
                byte a = B11100000;
                byte b = t & a;
                if (b == B11100000 || b == B11000000) // 中文 ascii扩展
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    // log_println("测试2");
                }
                else if (t_length > cz)
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    // log_println("测试3");
                }
            }
        }
    }
    for (uint8_t i = 0; i < maxline + 1; i++)
    {
        if (isChapterTitle(txt[i]))
        {
            is_find = true;
            log_i("在%d行找到 \"%s\"", i, txt[i].c_str());
            sprintf(chapterTitle, "%s", txt[i].c_str());
            return currentPage + 1;
        }
    }
    if (!is_find)
    {
        delay(1);
        goto begin;
    }
    return currentPage + 1;
}

int AppEBook::getTotalPages()
{
    int indexesFileSize = indexesFile.size();
    // return (indexesFileSize / 4) + 1;
    return (indexesFileSize / 4) + 1 - 2;
}
static int get_digits(int val)
{
    int digits = 0;
    while (val > 0)
    {
        val /= 10;
        digits++;
    }
    return digits;
}
void AppEBook::openMenu()
{
    if (!(hal.pref.getString("system_font", "default") == "default"))
        u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());

    bool mode = hal.pref.getBool("Vertical");
    const char *dayOfWeek[] = {"日", "一", "二", "三", "四", "五", "六"};
    int moth = hal.timeinfo.tm_mon + 1, d = hal.timeinfo.tm_mday, dw = hal.timeinfo.tm_wday, h = hal.timeinfo.tm_hour, m = hal.timeinfo.tm_min, s = hal.timeinfo.tm_sec;
    char buf[128];
    sprintf(buf, "%d月%d日 星期%s %d:%d:%d    %.2fV (%d%%)", moth, d, dayOfWeek[dw], h, m, s, hal.bat_info.voltage, hal.bat_info.soc);
    char *title = (char *)malloc(128);
    int totalPages = getTotalPages();
    sprintf(title, "%d/%d %.1f%%", currentPage + 1, totalPages, (float)((currentPage + 1) * 100) / (float)(totalPages));
    const menu_select items[] = {
        {false, "< 返回", nullptr},           // 0
        {false, "退出", nullptr},             // 1
        {false, "跳转到..", nullptr},         // 2
        {false, "重建当前文件索引", nullptr}, // 3
        {true, "竖屏模式", "Vertical"},       // 4
        {false, "换文件..", nullptr},         // 5
        {false, "设置", nullptr},             // 6
        {false, buf},
        {false, nullptr, nullptr},
    };
    const menu_item page_goto_items[] = {
        {NULL, "< 返回"},
        {NULL, "跳转至页"},
        {NULL, "跳转至下一章"},
        {NULL, "跳转至上一章"},
        {NULL, "跳转至上/下X章"},
        {NULL, NULL},
    };
    int ret = GUI::select_menu(title, items);
    free(title);
    switch (ret)
    {
    case 0:
        break;
    case 1:
        exit_app = true;
        break;
    case 2:
    {
        int ret = GUI::menu(title, page_goto_items, 8, 8, 1);
        int page = 0;
        switch (ret)
        {
        case 0:
            break;
        case 1:
            page = GUI::msgbox_number("跳转到..", get_digits(totalPages), currentPage + 1);
            break;
        case 2:
            GUI::info_msgbox("跳转至下一章", "正在查找......\n请稍候...\n长按左键以终止查找");
            page = findchapterTitle();
            break;
        case 3:
            GUI::info_msgbox("跳转至上一章", "正在查找......\n请稍候...\n长按左键以终止查找");
            page = findchapterTitle(true);
            break;
        case 4:
        {
            char buf[2][256];
            int i = GUI::msgbox_number("跳转至上/下X章", 3, 0);
            sprintf(buf[0], "跳转至%s%d章", (1 > 0) ? "下" : "上", i);
            GUI::info_msgbox(buf[0], "正在查找......\n请稍候...\n长按左键以终止查找");
            chapterTitle[0] = '\0';
            if (i > 0)
            {
                for (int a = 0; a < abs(i); a++)
                {
                    page = findchapterTitle();
                    sprintf(buf[0], "查找进度 %d / %d", a, abs(i));
                    sprintf(buf[1], "找到章节行\n\" %s \"", chapterTitle);
                    GUI::info_msgbox(buf[0], buf[1]);
                }
            }
            if (i < 0)
            {
                for (int a = 0; a < abs(i); a++)
                {
                    page = findchapterTitle(true);
                    sprintf(buf[0], "查找进度 %d / %d", a, abs(i));
                    sprintf(buf[1], "找到章节行\n\" %s \"", chapterTitle);
                    GUI::info_msgbox(buf[0], buf[1]);
                }
            }
        }
        break;
        default:
            GUI::msgbox("错误", "无效的选项,或此选项为空");
            break;
        }
        if (page > 0)
        {
            if (gotoPage(page - 1) == false)
            {
                GUI::msgbox("跳转失败", "页码超出范围");
                log_e("跳转失败，%d超出范围", page - 1);
                gotoPage(currentPage);
            }
            delay(250);
            GUI::info_msgbox("章节跳转", "查找结束,即将跳转...");
            delay(250);
            drawCurrentPage();
        }
    }
    break;
    case 3:
    {
        display.clearScreen();
        display.setCursor(20, 20);
        display.setFont(&FreeSans9pt7b);
        display.print("Rebuilding index...");
        display.display();
        if (!indexesFile)
        {
            indexesFile = hal.open(indexesName, "r");
            if (!indexesFile)
                log_e("indexesFile not open");
        }
        indexesFile.seek((currentPage - 1) * 4, SeekSet);
        uint32_t indexeOffset = 0, page;
        if (indexesFile.read((uint8_t *)&indexeOffset, 4) < 4)
            return;

        indexFile();

        size_t fileSize = indexesFile.size();

        // 计算索引总数（每个索引4字节）
        uint32_t indexCount = fileSize / 4 - 1;

        // 二分查找
        int32_t left = 0;               // 左边界（索引位置，对应第二页）
        int32_t right = indexCount - 1; // 右边界
        int32_t mid;
        uint32_t midValue;
        uint32_t foundPage = 1; // 默认第一页
        if (!(indexeOffset == 0 || fileSize == 0))
        {
            while (left <= right)
            {
                int mid = left + (right - left) / 2;

                // 读取中间的索引值
                indexesFile.seek(mid * sizeof(uint32_t));
                uint32_t midValue;
                indexesFile.read((uint8_t *)&midValue, sizeof(uint32_t));

                if (midValue <= indexeOffset)
                {
                    // 目标偏移量在中间索引之后或相等
                    page = mid + 1; // 加1因为索引是从第二页开始的
                    left = mid + 1;
                }
                else
                {
                    // 目标偏移量在中间索引之前
                    right = mid - 1;
                }
            }
        }
        else
        {
            page = 1;
        }
        log_i("page %d", page - 1);
        // 如果没有精确匹配，返回找到的页数
        // 如果targetOffset小于第一个索引，foundPage保持为1
        if (page > 0)
        {
            if (gotoPage(page - 1) == false)
            {
                String info = "页码";
                info += page;
                info += "超出范围";
                GUI::msgbox("跳转失败", info.c_str());
                log_e("跳转失败，%d超出范围", page - 1);
                gotoPage(0);
            }
            drawCurrentPage();
        }
    }
    break;
    case 5:
        ebook_nvs.putUInt(hal.get_char_sha_key(app.currentFilename, true), currentPage);
        peripherals.load(PERIPHERALS_SD_BIT);
        if (openFile() == false)
        {
            GUI::msgbox("打开文件失败", currentFilename);
            log_e("文件%s打开失败", currentFilename);
        }
        currentPage = ebook_nvs.getUInt(hal.get_char_sha_key(app.currentFilename, true), 0);
        gotoPage(currentPage);
        drawCurrentPage();
        break;
    case 6:
        ebooksettings();
        break;
    default:
        GUI::msgbox("错误", "无效的选项,或此选项为空");
        break;
    }

    if (!(hal.pref.getString("ebook_font", "default") == "default"))
        u8g2Fonts.setFont(hal.pref.getString("ebook_font", "default").c_str());
    else
        u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());

    if (hal.pref.getBool("Vertical") != mode)
    {
        if (!indexesFile)
        {
            indexesFile = hal.open(indexesName, "r");
            if (!indexesFile)
                log_e("indexesFile not open");
        }
        indexesFile.seek((currentPage - 1) * 4, SeekSet);
        uint32_t indexeOffset = 0, page;
        if (indexesFile.read((uint8_t *)&indexeOffset, 4) < 4)
            return;

        indexFile();

        size_t fileSize = indexesFile.size();

        // 计算索引总数（每个索引4字节）
        uint32_t indexCount = fileSize / 4 - 1;

        // 二分查找
        int32_t left = 0;               // 左边界（索引位置，对应第二页）
        int32_t right = indexCount - 1; // 右边界
        int32_t mid;
        uint32_t midValue;
        uint32_t foundPage = 1; // 默认第一页
        if (!(indexeOffset == 0 || fileSize == 0))
        {
            while (left <= right)
            {
                int mid = left + (right - left) / 2;

                // 读取中间的索引值
                indexesFile.seek(mid * sizeof(uint32_t));
                uint32_t midValue;
                indexesFile.read((uint8_t *)&midValue, sizeof(uint32_t));

                if (midValue <= indexeOffset)
                {
                    // 目标偏移量在中间索引之后或相等
                    page = mid + 1; // 加1因为索引是从第二页开始的
                    left = mid + 1;
                }
                else
                {
                    // 目标偏移量在中间索引之前
                    right = mid - 1;
                }
            }
        }
        else
        {
            page = 1;
        }
        log_i("page %d", page - 1);
        // 如果没有精确匹配，返回找到的页数
        // 如果targetOffset小于第一个索引，foundPage保持为1
        if (page > 0)
        {
            if (gotoPage(page - 1) == false)
            {
                String info = "页码";
                info += page;
                info += "超出范围";
                GUI::msgbox("跳转失败", info.c_str());
                log_e("跳转失败，%d超出范围", page - 1);
                gotoPage(0);
            }
            drawCurrentPage();
        }
    }
}

#include <nvs.h>

void AppEBook::ebooksettings()
{
    char nvs_info[48];
    nvs_stats_t stats = ebook_nvs.getStats();
    sprintf(nvs_info, "NVS使用 %d/%d", stats.used_entries, stats.total_entries);
    menu_select ebook_set[] = {
        {false, "< 返回", nullptr},                  // 0
        {true, "根据唤醒源翻页", nullptr},           // 1
        {true, "自动翻页", nullptr},                 // 2
        {false, "自动翻页延时", nullptr},            // 3
        {true, "使用lightsleep", nullptr},           // 4
        {true, "禁用休眠", nullptr},                 // 5
        {false, "最大lightsleep次数", nullptr},      // 6
        {true, "反色显示", nullptr},                 // 7
        {true, "降频运行Ebook", nullptr},            // 8
        {false, "设置自定义阅读字体", nullptr},      // 9
        {false, "保存页数", nullptr},                // 10
        {false, "生成进度/读取进度(.sav)", nullptr}, // 11
        {false, nvs_info, nullptr},                  // 12
        {false, NULL, nullptr},
    };
    bool code = hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1"));
    bool code2 = hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"), true);
    int res = 0;
    bool end = false;
    while (!end)
    {
        res = GUI::select_menu("电子书设置", ebook_set, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 3:
            hal.pref.putInt("auto_page", GUI::msgbox_number("输入时长s", 5, hal.pref.getInt("auto_page", 10)));
            break;
        case 6:
            hal.pref.putInt("max_lightsleep", GUI::msgbox_number("输入次数", 3, hal.pref.getInt("max_lightsleep", 20)));
            break;
        case 9:
        {
            const char *str = GUI::fileDialog("请选择文本绘制字体文件", false, NULL, NULL);
            if (str == NULL)
            {
                hal.pref.putString("ebook_font", hal.pref.getString("system_font", "default"));
            }
            else
            {
                hal.pref.putString("ebook_font", String(str));
                u8g2Fonts.setFont(hal.pref.getString("ebook_font", "default").c_str());
            }
        }
        break;
        case 10:
            app.ebook_nvs.putUInt(hal.get_char_sha_key(app.currentFilename, true), currentPage);
            GUI::info_msgbox("提示", "已保存当前页到nvs");
            break;
        case 11:
        {
            if (GUI::msgbox_yn("提示", "生成进度/读取进度(.sav)", "生成", "读取"))
            {
                if (!indexesFile)
                {
                    indexesFile = hal.open(indexesName, "r");
                    if (!indexesFile)
                        log_e("indexesFile not open");
                }
                indexesFile.seek((currentPage - 1) * 4, SeekSet);
                uint32_t gbwz_uint32;
                uint32_t indexeOffset = 0;
                if (indexesFile.read((uint8_t *)&gbwz_uint32, 4) != 0)
                    indexeOffset = gbwz_uint32;
                log_i("%lu", indexeOffset);
                String savname;
                savname = "/userdat/";
                savname += txtFile.name();
                savname.replace(".txt", ".sav");
                File sav = LittleFS.open(savname, "w");
                if (sav)
                {
                    sav.write((uint8_t *)&indexeOffset, 4);
                    sav.close();
                }
                else
                {
                    log_e("%s not open", savname.c_str());
                }
            }
            else
            {
                GUI::info_msgbox("查找中......", "正在根据sav查找页...");
                if (!indexesFile)
                {
                    indexesFile = hal.open(indexesName, "r");
                    if (!indexesFile)
                        log_e("indexesFile not open");
                }
                // indexesFile.seek((currentPage - 1) * 4, SeekSet);
                uint32_t indexeOffset = 0;
                uint32_t gbwz_uint32;
                uint32_t page = 0;
                String savname;
                savname = "/userdat/";
                savname += txtFile.name();
                savname.replace(".txt", ".sav");
                File sav = LittleFS.open(savname, "r");
                if (!sav)
                {
                    log_e("not open %s", savname.c_str());
                }
                sav.read((uint8_t *)&gbwz_uint32, 4);
                sav.close();
                size_t fileSize = indexesFile.size();

                // 计算索引总数（每个索引4字节）
                uint32_t indexCount = fileSize / 4;

                // 二分查找
                int32_t left = 0;               // 左边界（索引位置，对应第二页）
                int32_t right = indexCount - 1; // 右边界
                int32_t mid;
                uint32_t midValue;
                uint32_t foundPage = 1; // 默认第一页
                if (!(gbwz_uint32 == 0 || fileSize == 0))
                {
                    while (left <= right)
                    {
                        int mid = left + (right - left) / 2;

                        // 读取中间的索引值
                        indexesFile.seek(mid * sizeof(uint32_t));
                        uint32_t midValue;
                        indexesFile.read((uint8_t *)&midValue, sizeof(uint32_t));

                        if (midValue <= gbwz_uint32)
                        {
                            // 目标偏移量在中间索引之后或相等
                            page = mid + 1; // 加1因为索引是从第二页开始的
                            left = mid + 1;
                        }
                        else
                        {
                            // 目标偏移量在中间索引之前
                            right = mid - 1;
                        }
                    }
                }
                else
                {
                    page = 1;
                }
                log_i("page %d", page - 1);
                // 如果没有精确匹配，返回找到的页数
                // 如果targetOffset小于第一个索引，foundPage保持为1
                if (page > 0)
                {
                    if (gotoPage(page - 1) == false)
                    {
                        String info = "页码";
                        info += page;
                        info += "超出范围";
                        GUI::msgbox("跳转失败", info.c_str());
                        log_e("跳转失败，%d超出范围", page - 1);
                        gotoPage(currentPage);
                    }
                    drawCurrentPage();
                }
            }
        }
        break;
        case 12:
            char buf[64];
            sprintf(buf, "%s\n清除所有阅读进度或返回", nvs_info);
            if (!GUI::msgbox_yn("提示", buf, "返回", "清除"))
            {
                if (ebook_nvs.clear())
                    GUI::info_msgbox("清除所有记录", "OK");
                else
                    GUI::info_msgbox("清除所有记录", "error");
            }
            break;
        default:
            GUI::info_msgbox("错误", "无效的选项");
            break;
        }
    }
}