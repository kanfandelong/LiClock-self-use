#include "AppManager.h"
#include <rtc_wdt.h>
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
    bool indexcode_1();
    bool indexcode_2();
    bool indexcode_3();
    bool indexcode_ttf();
    bool indexFile();
    bool openFile(const char *filename = NULL);
    bool gotoPage(uint32_t page);
    bool draw_page1();
    bool draw_page2();
    bool draw_page3();
    void drawCurrentPage();
    int findchapterTitle(bool up = false);
    int getTotalPages();
    void openMenu();
    void ebooksettings();
    Preferences ebook_nvs;
    FILE *indexFileHandle = NULL;
    FILE *currentFileHandle = NULL;
    File txtFile, indexesFile;
    char indexesName[256];
    size_t currentFileOffset = 0;
    char currentFilename[256];
    bool __eof = false;
    bool exit_app = false;       // 是否退出app
    bool need_deepsleep = false; // 在启用lightsleep时是否需要间歇性deepsleep（不进行此操作似乎会导致看门狗复位）
};
RTC_DATA_ATTR uint32_t currentPage = -1; // 0:第一页
RTC_DATA_ATTR bool ebook_run = false;    // 电子书运行标志
RTC_DATA_ATTR bool gotonextpage = false; // 特殊情况自动下一页标志
RTC_DATA_ATTR u8_t lightsleep_count = 0; // lightsleep次数
static AppEBook app;
static void appebook_exit()
{
    hal.cheak_freq(hal.pref.getInt("CpuFreq", 80));
    display.clearScreen();
    display.display(true);
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
        if (app.txtFile)
        {
            app.txtFile.close();
        }
        if (app.indexesFile)
        {
            app.indexesFile.close();
        }
    }
    else
    {
        if (app.currentFileHandle != NULL)
        {
            fclose(app.currentFileHandle);
            app.currentFileHandle = NULL;
        }
        if (app.indexFileHandle != NULL)
        {
            fclose(app.indexFileHandle);
            app.indexFileHandle = NULL;
        }
    }
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示")))
    {
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    }
    // hal.pref.putInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, currentPage);
    app.ebook_nvs.putUInt(hal.get_char_sha_key(app.currentFilename, true), currentPage);
    Serial.printf("退出电子书，当前页：%d\n", currentPage);
    currentPage = -1;
    ebook_run = false;
}
static void appebook_deepsleep()
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
        if (app.txtFile)
        {
            app.txtFile.close();
        }
        if (app.indexesFile)
        {
            app.indexesFile.close();
        }
    }
    else
    {
        if (app.currentFileHandle != NULL)
        {
            fclose(app.currentFileHandle);
            app.currentFileHandle = NULL;
        }
        if (app.indexFileHandle != NULL)
        {
            fclose(app.indexFileHandle);
            app.indexFileHandle = NULL;
        }
    }
    display.powerOff();
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
    esp_task_wdt_init(portMAX_DELAY, false);
    rtc_wdt_protect_off();
    // rtc_wdt_set_length_of_reset_signal(RTC_WDT_SYS_RESET_SIG, RTC_WDT_LENGTH_3_2us);
    // rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_INTERRUPT);
    // rtc_wdt_set_time(RTC_WDT_STAGE0, 30000); // 30秒超时
    // rtc_wdt_enable();
    rtc_wdt_disable();
    rtc_wdt_protect_on();
    bool page_changed = false;
    ebook_nvs.begin("ebook");
    app.exit = appebook_exit;
    app.deepsleep = appebook_deepsleep;
    app.currentFilename[0] = 0;
    if (hal.pref.getBool(hal.get_char_sha_key("禁用休眠")))
        if (hal.pref.getBool(hal.get_char_sha_key("降频运行Ebook")))
            hal.cheak_freq(80, true);
    display.clearScreen();
    size_t s = hal.pref.getBytes(SETTINGS_PARAM_LAST_EBOOK, app.currentFilename, 256);
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
            log_i("电子书：上次打开的文件：%s，上次打开的页：%d\n", app.currentFilename, currentPage);
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
            display.display(true);
        }
        if (hal.btnl.isPressing() || ((hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) && ebook_run == true && (!hal.pref.getBool(hal.get_char_sha_key("禁用休眠")))))
        {
            if (currentPage == 0)
            {
                GUI::msgbox("提示", "已经是第一页了");
                display.display(true);
            }
            else if (gotoPage(currentPage - 1) == false)
            {
                GUI::msgbox("提示", "翻页发生错误");
                display.display(true);
            }
            else
            {
                log_i("上一页 当期页:%ld\n", currentPage);
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
                display.display(true);
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
                    log_i("下一页 当期页:%ld\n", currentPage);
                    page_changed = true;
                }
                else
                {
                    GUI::msgbox("提示", "已经是最后一页了");
                    log_i("已经是最后一页了");
                    display.display(true);
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
            display.powerOff();
            if (hal.pref.getBool(hal.get_char_sha_key("自动翻页")))
            {
                esp_sleep_enable_timer_wakeup(hal.pref.getInt("auto_page", 10) * 1000000UL);
            }
            if (hal.btn_activelow)
            {
                esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 0);
                esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ALL_LOW);
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
bool AppEBook::indexcode_1()
{
    if (currentFileHandle == NULL)
    {
        GUI::msgbox("索引错误", "请先打开文件");
        return false;
    }
    uint32_t offsetall = 0;
    fseek(currentFileHandle, 0L, SEEK_END);
    offsetall = ftell(currentFileHandle);
    rewind(currentFileHandle);
    fseek(currentFileHandle, 0, SEEK_SET);
    uint32_t page = 0;   // 页码
    uint32_t offset = 0; // 主文件偏移量
    int16_t x = 0;       // 当前页中字符的x坐标
    int16_t y = 0;       // 当前页中字符的y坐标
    int c;
    bool r_flag = false;
    String indexname = String(currentFilename) + ".i";
    indexFileHandle = fopen(indexname.c_str(), "wb");
    if (indexFileHandle == NULL)
    {
        Serial.println("打开索引文件失败");
        GUI::msgbox("打开索引文件失败", indexname.c_str());
        return false;
    }
    uint8_t buffer[3];
    buffer[0] = fgetc(currentFileHandle);
    buffer[1] = fgetc(currentFileHandle);
    buffer[2] = fgetc(currentFileHandle);

    // 检查是否为 BOM 头
    if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
    {
        Serial.println("File starts with UTF-8 BOM");
        fseek(currentFileHandle, 3, SEEK_SET); // 移动到 BOM 头之后的位置
    }
    else
        fseek(currentFileHandle, 0, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    long begin = millis(), last;
    while (true)
    {
    start:
        if (!r_flag)
            c = fgetc(currentFileHandle);
        else
            r_flag = false;
        if (c == EOF)
            break;
        offset++;
        if (c == '\r')
            goto start;
        while (c == '\n' && x == 0 && y == 0)
            goto start;
        int utf_bytes = 0;
        if (c & 0x80)
        {
            if (c & 0x40)
            {
                if (c & 0x20)
                {
                    if (c & 0x10)
                    {
                        if (c & 0x08)
                        {
                            if (c & 0x04)
                                utf_bytes = 5;
                            else
                                utf_bytes = 4;
                        }
                        else
                            utf_bytes = 3;
                    }
                    else
                        utf_bytes = 2;
                }
                else
                    utf_bytes = 1;
            }
            else
            {
                GUI::msgbox("索引错误", "非预期的UTF8编码");
                fclose(indexFileHandle);
                indexFileHandle = NULL;
                return false;
            }
        }
        if (utf_bytes != 0)
        {
            for (int i = 0; i < utf_bytes; i++)
            {
                fgetc(currentFileHandle);
                offset++;
            }
        }
        int add_pending;
        if (utf_bytes == 0)
            add_pending = 6;
        else
            add_pending = 12;
        if (c == '\n')
        {
            y += 14;
            x = 0;
            add_pending = 0;
            c = fgetc(currentFileHandle);
            if (c == '\r')
                offset++;
            else
                r_flag = true;
            uint32_t offsetall = 0;
            offsetall = ftell(currentFileHandle);
            char r = fgetc(currentFileHandle);
            if (r == '\r')
                offset++;
            else
                fseek(currentFileHandle, offsetall, SEEK_SET);
        }
        else if (x + add_pending >= 294)
        {
            x = add_pending;
            y += 14;
        }
        else
            x += add_pending;
        if (y >= 128 - 14)
        {
            page++;
            x = 0;
            y = 0;
            uint32_t pageOffset = offset - utf_bytes - 1;
            // uint32_t pageOffset = offset - utf_bytes;
            fwrite(&pageOffset, 4, 1, indexFileHandle);
        }
        if (millis() - last > 1000)
        {
            display.clearScreen();
            u8g2Fonts.setCursor(0, 15);
            u8g2Fonts.printf("文件名称：%s", currentFilename);
            u8g2Fonts.setCursor(0, 30);
            u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall, offsetall / 1024);
            u8g2Fonts.setCursor(0, 45);
            u8g2Fonts.printf("剩余大小：%d(%dKB)", offsetall - offset, (offsetall - offset) / 1024);
            u8g2Fonts.setCursor(0, 60);
            u8g2Fonts.printf("索引进度：%d%%", offset * 100 / offsetall);
            display.display(true);
            last = millis();
        }
        delay(1);
    }
    fclose(indexFileHandle);
    indexFileHandle = NULL;
    char *tmp = (char *)malloc(256);
    int h = (millis() - begin) / 1000;
    display.clearScreen();
    u8g2Fonts.setCursor(0, 15);
    u8g2Fonts.printf("文件名称：%s", currentFilename);
    u8g2Fonts.setCursor(0, 30);
    u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall, offsetall / 1024);
    u8g2Fonts.setCursor(0, 45);
    u8g2Fonts.printf("剩余大小：0(0KB)");
    u8g2Fonts.setCursor(0, 60);
    u8g2Fonts.printf("索引进度：100%%\n");
    u8g2Fonts.setCursor(0, 75);
    u8g2Fonts.printf("耗时：%ds", h);
    display.display(true);
    sprintf(tmp, "共%d页, 最后一页偏移量：%d", page + 1, offset);
    GUI::msgbox("索引成功", tmp);
    free(tmp);
    indexFileHandle = fopen((String(currentFilename) + ".i").c_str(), "rb");
    if (indexFileHandle == NULL)
    {
        GUI::msgbox("索引失败", "无法再次打开索引文件");
        return false;
    }
    return true;
}
bool AppEBook::indexcode_2()
{
    if (currentFileHandle == NULL)
    {
        GUI::msgbox("索引错误", "请先打开文件");
        return false;
    }
    uint32_t offsetall = 0;
    fseek(currentFileHandle, 0L, SEEK_END);
    offsetall = ftell(currentFileHandle);
    rewind(currentFileHandle);
    fseek(currentFileHandle, 0, SEEK_SET);
    uint32_t page = 0;   // 页码
    uint32_t offset = 0; // 主文件偏移量
    int16_t x = 0;       // 当前页中字符的x坐标
    int16_t y = 0;       // 当前页中字符的y坐标
    int c;
    bool r_flag = false;
    String indexname = String(currentFilename) + ".i";
    indexFileHandle = fopen(indexname.c_str(), "wb");
    if (indexFileHandle == NULL)
    {
        Serial.println("打开索引文件失败");
        GUI::msgbox("打开索引文件失败", indexname.c_str());
        return false;
    }
    uint8_t buffer[3];
    buffer[0] = fgetc(currentFileHandle);
    buffer[1] = fgetc(currentFileHandle);
    buffer[2] = fgetc(currentFileHandle);

    // 检查是否为 BOM 头
    if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
    {
        Serial.println("File starts with UTF-8 BOM");
        fseek(currentFileHandle, 3, SEEK_SET); // 移动到 BOM 头之后的位置
    }
    else
        fseek(currentFileHandle, 0, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    long begin = millis(), last;
    while (true)
    {
    start:
        if (!r_flag)
            c = fgetc(currentFileHandle);
        else
            r_flag = false;
        if (c == EOF)
            break;
        offset++;
        while (c == '\n' && x == 0 && y == 0)
            goto start;
        int utf_bytes = 0;
        if (c & 0x80)
        {
            if (c & 0x40)
            {
                if (c & 0x20)
                {
                    if (c & 0x10)
                    {
                        if (c & 0x08)
                        {
                            if (c & 0x04)
                                utf_bytes = 5;
                            else
                                utf_bytes = 4;
                        }
                        else
                            utf_bytes = 3;
                    }
                    else
                        utf_bytes = 2;
                }
                else
                    utf_bytes = 1;
            }
            else
            {
                GUI::msgbox("索引错误", "非预期的UTF8编码");
                fclose(indexFileHandle);
                indexFileHandle = NULL;
                return false;
            }
        }
        if (utf_bytes != 0)
        {
            for (int i = 0; i < utf_bytes; i++)
            {
                fgetc(currentFileHandle);
                offset++;
            }
        }
        int add_pending;
        if (utf_bytes == 0)
            add_pending = 6;
        else
            add_pending = 12;
        if (c == '\n')
        {
            y += 14;
            x = 0;
            add_pending = 0;
            c = fgetc(currentFileHandle);
            if (c == '\r')
                offset++;
            else
                r_flag = true;
        }
        else if (x + add_pending >= 294)
        {
            x = add_pending;
            y += 14;
        }
        else
            x += add_pending;
        if (y >= 128 - 14)
        {
            page++;
            x = 0;
            y = 0;
            uint32_t pageOffset = offset - utf_bytes - 1;
            fwrite(&pageOffset, 4, 1, indexFileHandle);
        }
        if (millis() - last > 1000)
        {
            display.clearScreen();
            u8g2Fonts.setCursor(0, 15);
            u8g2Fonts.printf("文件名称：%s", currentFilename);
            u8g2Fonts.setCursor(0, 30);
            u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall, offsetall / 1024);
            u8g2Fonts.setCursor(0, 45);
            u8g2Fonts.printf("剩余大小：%d(%dKB)", offsetall - offset, (offsetall - offset) / 1024);
            u8g2Fonts.setCursor(0, 60);
            u8g2Fonts.printf("索引进度：%d%%", offset * 100 / offsetall);
            display.display(true);
            last = millis();
        }
        delay(1);
    }
    fclose(indexFileHandle);
    indexFileHandle = NULL;
    char *tmp = (char *)malloc(256);
    int h = (millis() - begin) / 1000;
    display.clearScreen();
    u8g2Fonts.setCursor(0, 15);
    u8g2Fonts.printf("文件名称：%s", currentFilename);
    u8g2Fonts.setCursor(0, 30);
    u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall, offsetall / 1024);
    u8g2Fonts.setCursor(0, 45);
    u8g2Fonts.printf("剩余大小：0(0KB)");
    u8g2Fonts.setCursor(0, 60);
    u8g2Fonts.printf("索引进度：100%%\n");
    u8g2Fonts.setCursor(0, 75);
    u8g2Fonts.printf("耗时：%ds", h);
    display.display(true);
    sprintf(tmp, "共%d页, 最后一页偏移量：%d", page + 1, offset);
    GUI::msgbox("索引成功", tmp);
    free(tmp);
    indexFileHandle = fopen((String(currentFilename) + ".i").c_str(), "rb");
    if (indexFileHandle == NULL)
    {
        GUI::msgbox("索引失败", "无法再次打开索引文件");
        return false;
    }
    return true;
}
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
int8_t getCharLength(char zf) // 获取ascii字符的长度
{
    // 控制字符（0-31和127）返回0
    if ((zf >= 0 && zf <= 31) || zf == 127)
        return 0;

    // 可打印字符
    switch (zf)
    {
    case ' ':
        return 6; // 空格 (32)
    case '!':
        return 3; // 33
    case '"':
        return 5; // 34
    case '#':
        return 6; // 35
    case '$':
        return 5; // 36
    case '%':
        return 6; // 37
    case '&':
        return 5; // 38
    case '\'':
        return 3; // 39
    case '(':
        return 4; // 40
    case ')':
        return 4; // 41
    case '*':
        return 5; // 42
    case '+':
        return 7; // 43
    case ',':
        return 4; // 44
    case '-':
        return 5; // 45
    case '.':
        return 3; // 46
    case '/':
        return 4; // 47

    // 数字 0-9
    case '0':
        return 5; // 48
    case '1':
        return 5; // 49
    case '2':
        return 5; // 50
    case '3':
        return 5; // 51
    case '4':
        return 6; // 52
    case '5':
        return 5; // 53
    case '6':
        return 5; // 54
    case '7':
        return 5; // 55
    case '8':
        return 5; // 56
    case '9':
        return 5; // 57

    case ':':
        return 1; // 58
    case ';':
        return 4; // 59
    case '<':
        return 5; // 60
    case '=':
        return 5; // 61
    case '>':
        return 5; // 62
    case '?':
        return 5; // 63
    case '@':
        return 7; // 64

    // 大写字母 A-Z
    case 'A':
        return 7; // 65
    case 'B':
        return 6; // 66
    case 'C':
        return 6; // 67
    case 'D':
        return 7; // 68
    case 'E':
        return 5; // 69
    case 'F':
        return 5; // 70
    case 'G':
        return 6; // 71
    case 'H':
        return 6; // 72
    case 'I':
        return 3; // 73
    case 'J':
        return 3; // 74
    case 'K':
        return 5; // 75
    case 'L':
        return 5; // 76
    case 'M':
        return 7; // 77
    case 'N':
        return 6; // 78
    case 'O':
        return 7; // 79
    case 'P':
        return 5; // 80
    case 'Q':
        return 7; // 81
    case 'R':
        return 6; // 82
    case 'S':
        return 5; // 83
    case 'T':
        return 7; // 84
    case 'U':
        return 6; // 85
    case 'V':
        return 7; // 86
    case 'W':
        return 9; // 87
    case 'X':
        return 6; // 88
    case 'Y':
        return 7; // 89
    case 'Z':
        return 7; // 90

    case '[':
        return 5; // 91
    case '\\':
        return 5; // 92
    case ']':
        return 4; // 93
    case '^':
        return 5; // 94
    case '_':
        return 5; // 95
    case '`':
        return 3; // 96

    // 小写字母 a-z
    case 'a':
        return 5; // 97
    case 'b':
        return 5; // 98
    case 'c':
        return 4; // 99
    case 'd':
        return 5; // 100
    case 'e':
        return 5; // 101
    case 'f':
        return 3; // 102
    case 'g':
        return 5; // 103
    case 'h':
        return 5; // 104
    case 'i':
        return 1; // 105
    case 'j':
        return 2; // 106
    case 'k':
        return 5; // 107
    case 'l':
        return 1; // 108
    case 'm':
        return 7; // 109
    case 'n':
        return 5; // 110
    case 'o':
        return 5; // 111
    case 'p':
        return 5; // 112
    case 'q':
        return 5; // 113
    case 'r':
        return 3; // 114
    case 's':
        return 5; // 115
    case 't':
        return 3; // 116
    case 'u':
        return 5; // 117
    case 'v':
        return 5; // 118
    case 'w':
        return 7; // 119
    case 'x':
        return 5; // 120
    case 'y':
        return 5; // 121
    case 'z':
        return 5; // 122

    case '{':
        return 3; // 123
    case '|':
        return 3; // 124
    case '}':
        return 3; // 125
    case '~':
        return 6; // 126

    default:
        return 6; // 其他字符（非ASCII或未覆盖的字符）
    }
}
bool AppEBook::indexcode_3()
{
    bool mode = hal.pref.getBool("Vertical");
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
                                    indexesFile = SD.open(indexesName, FILE_APPEND);
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
            if (millis() - last > 2000)
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
                display.display(true);
                log_i("文件名称：%s 索引进度：%0.2f%%", currentFilename, shengyu_float);
                last = millis();
            }
            // Serial.println("写入索引文件");
            // }
            // Serial.print("第"); Serial.print(pageCount); Serial.print("页，页首位置："); Serial.println(yswz_uint32);
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
    //             indexesFile = SD.open(indexesName, FILE_APPEND);
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
    //             indexesFile = SD.open(indexesName, FILE_APPEND);
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
    // Serial.print("yswz_count：");
    // Serial.println(yswz_count);
    info("pageCount：%lu 预期大小：%lu", pageCount, 4 * ((pageCount - 1) + 1));
    info("索引文件大小：%lu", indexes_size);

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
        display.display(true);
        log_i("文件名称：%s 索引进度：100%%", currentFilename);
    }
    else
    {
        indexesFile.close();
        warn("校验失败，索引文件大小与预期大小不符");
        // if (strncmp(currentFilename, "/littlefs/", 10) == 0)
        //     LittleFS.remove(indexesName);
        // else if (strncmp(currentFilename, "/sd/", 4) == 0)
        //     SD.remove(indexesName);
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
                                    indexesFile = SD.open(indexesName, FILE_APPEND);
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
            if (millis() - last > 2000)
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
                display.display(true);
                log_i("文件名称：%s 索引进度：%0.2f%%", currentFilename, shengyu_float);
                last = millis();
            }
            // Serial.println("写入索引文件");
            // }
            // Serial.print("第"); Serial.print(pageCount); Serial.print("页，页首位置："); Serial.println(yswz_uint32);
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
    //             indexesFile = SD.open(indexesName, FILE_APPEND);
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
    //             indexesFile = SD.open(indexesName, FILE_APPEND);
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
    // Serial.print("yswz_count：");
    // Serial.println(yswz_count);
    info("pageCount：%lu 预期大小：%lu", pageCount, 4 * ((pageCount - 1) + 1));
    info("索引文件大小：%lu", indexes_size);

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
        display.display(true);
        log_i("文件名称：%s 索引进度：100%%", currentFilename);
    }
    else
    {
        indexesFile.close();
        warn("校验失败，索引文件大小与预期大小不符");
        // if (strncmp(currentFilename, "/littlefs/", 10) == 0)
        //     LittleFS.remove(indexesName);
        // else if (strncmp(currentFilename, "/sd/", 4) == 0)
        //     SD.remove(indexesName);
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
    if (hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")) == false)
        return indexcode_2();
    else if (hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) == false && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
        return indexcode_3();
    }
    else
        return indexcode_1();
}

bool AppEBook::openFile(const char *filename)
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
        if (app.txtFile)
            app.txtFile.close();
        if (app.indexesFile)
            app.indexesFile.close();
    }
    else
    {
        if (app.currentFileHandle != NULL)
        {
            fclose(app.currentFileHandle);
            app.currentFileHandle = NULL;
        }
        if (app.indexFileHandle != NULL)
        {
            fclose(app.indexFileHandle);
            app.indexFileHandle = NULL;
        }
    }
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
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
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
        indexesFile.seek(indexesFile.size() - 4, SeekSet);
        indexesFile.readBytes((char *)&lasttxtsize, 4);
        indexesFile.seek(0, SeekSet);
        info("lasttxtsize: %lu nowtxtsize: %lu", lasttxtsize, nowtxtsize);
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
    }
    else
    {
        currentFileHandle = fopen(currentFilename, "rb");
        if (currentFileHandle == NULL)
            return false;
        indexFileHandle = fopen((String(currentFilename) + ".i").c_str(), "rb");
        if (indexFileHandle == NULL)
        {
            bool index_ok = indexFile();
            hal.cheak_freq(hal.pref.getInt("CpuFreq", 80));
            if (!index_ok)
                return false;
        }
    }
    hal.pref.putBytes(SETTINGS_PARAM_LAST_EBOOK, app.currentFilename, strlen(app.currentFilename));
    return true;
}

bool AppEBook::gotoPage(uint32_t page)
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
        if (!indexesFile)
        {
            indexesFile = hal.open(indexesName);
            if (!indexesFile)
                error("indexesFile not open");
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
            // Serial.print("当前页1："); Serial.println(pageCurrent);
            // 计算上一页的页首位置
            // 因为第一页不需要记录所以要减1，因为我要的是上一页所以再减1
            // gbwz = (page + 1) * 7 - 7;
            // gbwz = (page + 1) * 4 - 4;
            // Serial.print("gbwz："); Serial.println(gbwz);
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
            info("seekset %lu", currentFileOffset);
            txtFile.seek(currentFileOffset, SeekSet);
            currentPage = page;
            return true;
        }
    }
    else
    {
        if (page == 0)
        {
            currentPage = 0;
            currentFileOffset = 0;
            fseek(currentFileHandle, 0, SEEK_SET);
            return true;
        }
        else if (indexFileHandle != NULL)
        {
            fseek(indexFileHandle, (page - 1) * 4, SEEK_SET);
            if (fread(&currentFileOffset, sizeof(currentFileOffset), 1, indexFileHandle) == 1)
            {
                currentPage = page;
                info("seekset %lu", currentFileOffset);
                fseek(currentFileHandle, currentFileOffset, SEEK_SET);
                return true;
            }
            else
                __eof = true;
        }
    }
    return false;
}
uint8_t RTC_DATA_ATTR partcount = 100;
bool AppEBook::draw_page1()
{
    uint32_t offsetall = 0;
    offsetall = ftell(currentFileHandle);
    if (offsetall == 0)
    {
        uint8_t buffer[3];
        buffer[0] = fgetc(currentFileHandle);
        buffer[1] = fgetc(currentFileHandle);
        buffer[2] = fgetc(currentFileHandle);
        // 检查是否为 BOM 头
        if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
        {
            Serial.println("File starts with UTF-8 BOM");
            fseek(currentFileHandle, offsetall + 3, SEEK_SET); // 移动到 BOM 头之后的位置
        }
        else
            fseek(currentFileHandle, offsetall, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    }
    int16_t x = 1, y = 0;
    int c;
    bool r_flag = false;
    // 窗口
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示")))
    {
        display.clearScreen(GxEPD_BLACK);
        u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    }
    else
        display.clearScreen();
    // 自动换行
    while (true)
    {
    start:
        if (!r_flag)
            c = fgetc(currentFileHandle);
        else
            r_flag = false;
        if (c == EOF)
        {
            __eof = true;
            break;
        }
        if (c == '\r')
            goto start;
        while (c == '\n' && x == 1 && y == 0)
            goto start;
        int utf_bytes = 0;
        if (c & 0x80)
        {
            if (c & 0x40)
            {
                if (c & 0x20)
                {
                    if (c & 0x10)
                    {
                        if (c & 0x08)
                        {
                            if (c & 0x04)
                                utf_bytes = 5;
                            else
                                utf_bytes = 4;
                        }
                        else
                            utf_bytes = 3;
                    }
                    else
                        utf_bytes = 2;
                }
                else
                    utf_bytes = 1;
            }
            else
            {
                GUI::info_msgbox("错误", "绘制文本时索引错误，可能文件非UTF-8编码");
                warn("绘制文本时索引错误，可能文件非UTF-8编码");
                return false;
                break;
            }
        }
        int add_pending;
        if (utf_bytes == 0)
        {
            add_pending = 6;
        }
        else
        {
            add_pending = 12;
        }
        if (c == '\n')
        {
            y += 14;
            x = 1;
            add_pending = 0;
            c = fgetc(currentFileHandle);
            if (c != '\r')
                r_flag = true;
            uint32_t offsetall = 0;
            offsetall = ftell(currentFileHandle);
            char r = fgetc(currentFileHandle);
            if (r != '\r')
                fseek(currentFileHandle, offsetall, SEEK_SET);
        }
        else if (x + add_pending >= 294 + 1)
        {
            x = 1;
            y += 14;
        }
        if (y >= 128 - 14)
        {
            if (utf_bytes != 0)
            {
                for (int i = 0; i < utf_bytes; i++)
                    fgetc(currentFileHandle);
                break;
            }
        }
        if (c != '\n' && c != '\r')
        {
            u8g2Fonts.setCursor(x, y + 13);
            x += add_pending;
            u8g2Fonts.write((uint8_t)c);
            if (utf_bytes != 0)
            {
                for (int i = 0; i < utf_bytes; i++)
                {
                    int c1 = fgetc(currentFileHandle);
                    if (c1 == EOF)
                        break;
                    u8g2Fonts.write((uint8_t)c1);
                }
            }
        }
    }
    if (partcount < hal.pref.getInt("display_count", 15))
    {
        display.display(true);
        ++partcount;
    }
    else
    {
        partcount = 0;
        display.display(false);
    }
    return true;
}
bool AppEBook::draw_page2()
{
    uint32_t offsetall = 0;
    offsetall = ftell(currentFileHandle);
    uint8_t buffer[3];
    buffer[0] = fgetc(currentFileHandle);
    buffer[1] = fgetc(currentFileHandle);
    buffer[2] = fgetc(currentFileHandle);
    // 检查是否为 BOM 头
    if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
    {
        Serial.println("File starts with UTF-8 BOM");
        fseek(currentFileHandle, offsetall + 3, SEEK_SET); // 移动到 BOM 头之后的位置
    }
    else
        fseek(currentFileHandle, offsetall, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    int16_t x = 1, y = 0;
    int c;
    bool r_flag = false;
    // 窗口
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示")))
    {
        display.clearScreen(GxEPD_BLACK);
        u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    }
    else
        display.clearScreen();
    // 自动换行
    while (true)
    {
    start:
        if (!r_flag)
            c = fgetc(currentFileHandle);
        else
            r_flag = false;
        if (c == EOF)
        {
            __eof = true;
            break;
        }
        while (c == '\n' && x == 1 && y == 0)
            goto start;
        int utf_bytes = 0;
        if (c & 0x80)
        {
            if (c & 0x40)
            {
                if (c & 0x20)
                {
                    if (c & 0x10)
                    {
                        if (c & 0x08)
                        {
                            if (c & 0x04)
                                utf_bytes = 5;
                            else
                                utf_bytes = 4;
                        }
                        else
                            utf_bytes = 3;
                    }
                    else
                        utf_bytes = 2;
                }
                else
                    utf_bytes = 1;
            }
            else
            {
                Serial.println("非预期的UTF8编码");
                warn("绘制文本时索引错误，可能文件非UTF-8编码");
                return false;
                break;
            }
        }
        int add_pending;
        if (utf_bytes == 0)
        {
            add_pending = 6;
        }
        else
        {
            add_pending = 12;
        }
        if (c == '\n')
        {
            y += 14;
            x = 1;
            add_pending = 0;
            c = fgetc(currentFileHandle);
            if (c != '\r')
            {
                r_flag = true;
            }
        }
        else if (x + add_pending >= 294 + 1)
        {
            x = 1;
            y += 14;
        }
        if (y >= 128 - 14)
        {
            if (utf_bytes != 0)
            {
                for (int i = 0; i < utf_bytes; i++)
                {
                    fgetc(currentFileHandle);
                }
                break;
            }
        }
        if (c != '\n')
        {
            u8g2Fonts.setCursor(x, y + 13);
            x += add_pending;
            u8g2Fonts.write((uint8_t)c);
            if (utf_bytes != 0)
            {
                for (int i = 0; i < utf_bytes; i++)
                {
                    int c1 = fgetc(currentFileHandle);
                    if (c1 == EOF)
                    {
                        break;
                    }
                    u8g2Fonts.write((uint8_t)c1);
                }
            }
        }
    }
    if (partcount < hal.pref.getInt("display_count", 15))
    {
        display.display(true);
        ++partcount;
    }
    else
    {
        partcount = 0;
        display.display(false);
    }
    return true;
}
bool AppEBook::draw_page3()
{
    bool mode = hal.pref.getBool("Vertical");
    int8_t maxline = mode ? 21 : 9;
    String txt[mode ? 22 : 10] = {}; // 0-8行为一页 共9行
    int8_t line = 0;                 // 当前行
    char c;                          // 中间数据
    uint16_t en_count = 0;           // 统计ascii和ascii扩展字符 1-2个字节
    uint16_t ch_count = 0;           // 统计中文等 3个字节的字符
    uint8_t line_old = 0;            // 记录旧行位置
    boolean hskgState = 1;           // 行首4个空格检测 0-检测过 1-未检测
    if (!txtFile)
    {
        txtFile = hal.open(currentFilename);

        if (!txtFile)
            error("%s 打开失败", currentFilename);
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
            en_count += getCharLength(c) + 1;
            asciiState = 1;
        }

        uint16_t StringLength = en_count + (ch_count * 12);

        if (StringLength >= (mode ? 96 : 260) && hskgState) // 检测到行首的4个空格预计的长度再加长一点
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
          Serial.println("");
          Serial.println(txt[line]);
          Serial.print("ch_count:"); Serial.println(ch_count);
          Serial.print("en_count:"); Serial.println(en_count);
          Serial.print("预计像素长度:"); Serial.println(StringLength);
          Serial.print("实际像素长度:"); Serial.println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
          }*/

        if (StringLength >= (mode ? 115 : 283)) // 检查是否已填满屏幕 283
        {
            // Serial.println("");
            // Serial.print("行"); Serial.print(line); Serial.print(" 预计像素长度:"); Serial.println(StringLength);
            // Serial.print("行"); Serial.print(line); Serial.print(" 实际像素长度:"); Serial.println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
            if (asciiState == 0) // 最后一个字符是中文，直接换行
            {
                line++;
                en_count = 0;
                ch_count = 0;
            }
            else if (StringLength >= (mode ? 118 : 286)) // 286 最后一个字符不是中文，在继续检测
            {
                char t = txtFile.read();
                txtFile.seek(-1, SeekCur); // 往回移
                int8_t cz = (mode ? 126 : 294) - StringLength;
                int8_t t_length = getCharLength(t);
                /*Serial.print("字符t:"); Serial.println(t);
                  Serial.print("字符t:"); Serial.println(t, HEX);
                  Serial.print("t长度:"); Serial.println(t_length);
                  Serial.print("差值:"); Serial.println(cz);*/
                byte a = B11100000;
                byte b = t & a;
                if (b == B11100000 || b == B11000000) // 中文 ascii扩展
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    // Serial.println("测试2");
                }
                else if (t_length > cz)
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    // Serial.println("测试3");
                }
            }
        }
    }
    // for (uint8_t i = 0; i < 8; i++) Serial.println(txt[i]); //串口输出内容
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示")))
    {
        display.clearScreen(GxEPD_BLACK);
        u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    }
    else
    {
        display.clearScreen();
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    }
    if (mode)
    {
        info("设置方向 ==> 2");
        display.setRotation(2);
    }
    for (uint8_t i = 0; i < (mode ? 21 : 9); i++)
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
        txt[i].replace(String("—"), "--");
        u8g2Fonts.setCursor(4 + offset, i * 14 + 13);
        u8g2Fonts.print(txt[i]);
        log_i("%s", txt[i].c_str());
    }
    if (partcount < hal.pref.getInt("display_count", 15))
    {
        display.display(true);
        ++partcount;
    }
    else
    {
        partcount = 0;
        display.display(false);
    }
    if (mode)
    {
        display.setRotation(hal.pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3));
    }
    return true;
}
void AppEBook::drawCurrentPage()
{
    if (hal.pref.getBool(hal.get_char_sha_key("快速显示")))
        display.epd2.PLL_set(0x3A);
    else
        display.epd2.PLL_set(hal.pref.getUInt("ebook_pllset", 0x3C));
    bool state;
    if (hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")) == false)
    {
        state = draw_page2();
    }
    else if (hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) == false && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
        state = draw_page3();
    }
    else
    {
        state = draw_page1();
    }
    if (!state)
        GUI::info_msgbox("错误", "绘制文本中出现错误");
    display.epd2.PLL_set(hal.pref.getUInt("pllset", 0x3C));
}

/**
 * 检查字符串是否包含章节标题
 * @param str 输入的字符串
 * @return 如果符合章节规则返回true，否则返回false
 */
bool isChapterTitle(const String &str)
{
    // 快速预检查：必须同时包含"第"和"章"
    // log_i("%s", str.c_str());
    int diIndex = str.indexOf("第");
    int zhangIndex = str.indexOf("章");

    // log_i("%d %d", diIndex, zhangIndex);
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
    String txt[9] = {};    // 0-8行为一页 共9行
    char c;                // 中间数据
    int8_t line = 0;       // 当前行
    uint16_t en_count = 0; // 统计ascii和ascii扩展字符 1-2个字节
    uint16_t ch_count = 0; // 统计中文等 3个字节的字符
    uint8_t line_old = 0;  // 记录旧行位置
    boolean hskgState = 1; // 行首4个空格检测 0-检测过 1-未检测
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
    for (uint8_t a = 0; a < 9; a++)
    {
        txt[a] = "";
    }
    line = 0;      // 当前行
    en_count = 0;  // 统计ascii和ascii扩展字符 1-2个字节
    ch_count = 0;  // 统计中文等 3个字节的字符
    line_old = 0;  // 记录旧行位置
    hskgState = 1; // 行首4个空格检测 0-检测过 1-未检测
    while (line < 9)
    {
        if (line_old != line) // 行首4个空格检测状态重置
        {
            line_old = line;
            hskgState = 1;
        }

        c = txtFile.read(); // 读取一个字节

        while (c == '\n' && line <= 8) // 检查换行符,并将多个连续空白的换行合并成一个
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
            if (line <= 8)
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
            en_count += getCharLength(c) + 1;
            asciiState = 1;
        }

        uint16_t StringLength = en_count + (ch_count * 12);

        if (StringLength >= 260 && hskgState) // 检测到行首的4个空格预计的长度再加长一点
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
          Serial.println("");
          Serial.println(txt[line]);
          Serial.print("ch_count:"); Serial.println(ch_count);
          Serial.print("en_count:"); Serial.println(en_count);
          Serial.print("预计像素长度:"); Serial.println(StringLength);
          Serial.print("实际像素长度:"); Serial.println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
          }*/

        if (StringLength >= 283) // 检查是否已填满屏幕 283
        {
            // Serial.println("");
            // Serial.print("行"); Serial.print(line); Serial.print(" 预计像素长度:"); Serial.println(StringLength);
            // Serial.print("行"); Serial.print(line); Serial.print(" 实际像素长度:"); Serial.println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
            if (asciiState == 0) // 最后一个字符是中文，直接换行
            {
                line++;
                en_count = 0;
                ch_count = 0;
            }
            else if (StringLength >= 286) // 286 最后一个字符不是中文，在继续检测
            {
                char t = txtFile.read();
                txtFile.seek(-1, SeekCur); // 往回移
                int8_t cz = 294 - StringLength;
                int8_t t_length = getCharLength(t);
                /*Serial.print("字符t:"); Serial.println(t);
                  Serial.print("字符t:"); Serial.println(t, HEX);
                  Serial.print("t长度:"); Serial.println(t_length);
                  Serial.print("差值:"); Serial.println(cz);*/
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
    }
    for (uint8_t i = 0; i < 9; i++)
    {
        if (isChapterTitle(txt[i]))
        {
            is_find = true;
            log_i("%s", txt[i].c_str());
            return currentPage + 1;
        }
    }
    if (!is_find)
    {
        goto begin;
    }
    return currentPage + 1;
}

int AppEBook::getTotalPages()
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")))
    {
        int indexesFileSize = indexesFile.size();
        // return (indexesFileSize / 4) + 1;
        return (indexesFileSize / 4);
    }
    else
    {
        struct stat fileStat;
        fstat(fileno(indexFileHandle), &fileStat);
        return fileStat.st_size / 4 + 1;
    }
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
    const char *dayOfWeek[] = {"日", "一", "二", "三", "四", "五", "六"};
    int moth = hal.timeinfo.tm_mon + 1, d = hal.timeinfo.tm_mday, dw = hal.timeinfo.tm_wday, h = hal.timeinfo.tm_hour, m = hal.timeinfo.tm_min, s = hal.timeinfo.tm_sec;
    char buf[64], vbat[64];
    sprintf(buf, "当前时间:%d月%d日 星期%s %d:%d:%d", moth, d, dayOfWeek[dw], h, m, s);
    sprintf(vbat, "电源电压：%.2fV (%d%%)", hal.bat_info.voltage, hal.bat_info.soc);
    char *title = (char *)malloc(128);
    int totalPages = getTotalPages();
    sprintf(title, "%d/%d %.1f%%", currentPage + 1, totalPages, (float)((currentPage + 1) * 100) / (float)(totalPages));
    const menu_item items[] = {
        {NULL, "< 返回"},
        {NULL, "退出"},
        {NULL, "跳转到.."},
        {NULL, "重建当前文件索引"},
        {NULL, "换文件.."},
        {NULL, buf},
        {NULL, "设置"},
        {NULL, vbat},
        {NULL, NULL},
    };
    const menu_item page_goto_items[] = {
        {NULL, "< 返回"},
        {NULL, "跳转至页"},
        {NULL, "跳转至下一章"},
        {NULL, "跳转至上一章"},
        {NULL, NULL},
    };
    int ret = GUI::menu(title, items);
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
        int page;
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
        default:
            GUI::msgbox("错误", "无效的选项,或此选项为空");
            break;
        }
        if (page > 0)
        {
            if (gotoPage(page - 1) == false)
            {
                GUI::msgbox("跳转失败", "页码超出范围");
                error("跳转失败，%d超出范围", page - 1);
                gotoPage(currentPage);
            }
            drawCurrentPage();
        }
    }
    break;
    case 3:
        display.clearScreen();
        display.setCursor(20, 20);
        display.setFont(&FreeSans9pt7b);
        display.print("Rebuilding index...");
        display.display(true);
        indexFile();
        gotoPage(0);
        drawCurrentPage();
        need_deepsleep = true;
        break;
    case 4:
        ebook_nvs.putUInt(hal.get_char_sha_key(app.currentFilename, true), currentPage);
        peripherals.load(PERIPHERALS_SD_BIT);
        if (openFile() == false)
        {
            GUI::msgbox("打开文件失败", currentFilename);
            error("文件%s打开失败", currentFilename);
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
}

#include <nvs.h>

void AppEBook::ebooksettings()
{
    char nvs_info[48];
    nvs_stats_t stats = ebook_nvs.getStats();
    sprintf(nvs_info, "NVS使用 %d/%d", stats.used_entries, stats.total_entries);
    static const menu_select ebook_set[] = {
        {false, "< 返回", nullptr},
        {true, "根据唤醒源翻页", nullptr},
        {true, "自动翻页", nullptr},
        {false, "自动翻页延时", nullptr}, // 3
        {true, "使用lightsleep", nullptr},
        {true, "禁用休眠", nullptr},
        {false, "最大lightsleep次数", nullptr}, // 6
        {true, "反色显示", nullptr},
        {true, "快速显示", nullptr},
        {false, "屏幕全刷间隔", nullptr}, // 9
        {true, "使用备选txt解析程序1", nullptr},
        {true, "甘草索引程序", nullptr},
        {true, "甘草索引竖屏模式", "Vertical"},
        {true, "降频运行Ebook", nullptr},
        {false, "保存页数", nullptr},                // 14
        {false, "Ebook单独Pll设置", nullptr},        // 15
        {false, "生成进度/读取进度(.sav)", nullptr}, // 16
        {false, nvs_info, nullptr},
        {false, NULL, nullptr},
    };
    bool code = hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1"));
    bool code2 = hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"));
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
            hal.pref.putInt("display_count", GUI::msgbox_number("输入全刷间隔", 2, hal.pref.getInt("display_count", 15)));
            break;
        case 14:
            app.ebook_nvs.putUInt(hal.get_char_sha_key(app.currentFilename, true), currentPage);
            GUI::info_msgbox("提示", "已保存当前页到nvs");
            break;
        case 15:
        {
            uint val = GUI::msgbox_hex("set_PLL", 2, hal.pref.getUInt("ebook_pllset", 0x3C));
            hal.pref.putUInt("ebook_pllset", val);
        }
        break;
        case 16:
        {
            if (GUI::msgbox_yn("提示", "生成进度/读取进度(.sav)", "生成", "读取"))
            {
                if (!indexesFile)
                {
                    indexesFile = hal.open(indexesName, "r");
                    if (!indexesFile)
                        error("indexesFile not open");
                }
                indexesFile.seek((currentPage - 1) * 4, SeekSet);
                uint32_t gbwz_uint32;
                uint32_t indexeOffset = 0;
                if (indexesFile.read((uint8_t *)&gbwz_uint32, 4) != 0)
                    indexeOffset = gbwz_uint32;
                info("%lu", indexeOffset);
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
                    error("%s not open", savname.c_str());
                }
            }
            else
            {
                GUI::info_msgbox("查找中......", "正在根据sav查找页...");
                if (!indexesFile)
                {
                    indexesFile = hal.open(indexesName, "r");
                    if (!indexesFile)
                        error("indexesFile not open");
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
                    error("not open %s", savname.c_str());
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
                info("page %d", page - 1);
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
                        error("跳转失败，%d超出范围", page - 1);
                        gotoPage(currentPage);
                    }
                    drawCurrentPage();
                }
            }
        }
        break;
        case 17:
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
    if (!(code == hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) && code2 == hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))))
    {
        indexFile();
        gotoPage(0);
        drawCurrentPage();
    }
    if (hal.pref.getBool(hal.get_char_sha_key("快速显示")))
        display.epd2.PLL_set(0x3A);
    else
        display.epd2.PLL_set(hal.pref.getUInt("ebook_pllset", 0x3C));
}