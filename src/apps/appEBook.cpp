#include "AppManager.h"
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
    bool indexFile();
    bool openFile(const char *filename = NULL);
    bool gotoPage(uint32_t page);
    bool draw_page1();
    bool draw_page2();
    bool draw_page3();
    void drawCurrentPage();
    int getTotalPages();
    void openMenu();
    void ebooksettings();
    FILE *indexFileHandle = NULL;
    FILE *currentFileHandle = NULL;
    File txtFile, indexesFile;
    char indexesName[256];
    size_t currentFileOffset = 0;
    char currentFilename[256];
    bool __eof = false;
    bool file_fs_sd = false;    //文件是否位于TF卡上
    bool exit_app = false;      //是否退出app
    bool need_deepsleep = false;//在启用lightsleep时是否需要间歇性deepsleep（不进行此操作似乎会导致看门狗复位）
};
RTC_DATA_ATTR uint32_t currentPage = -1; // 0:第一页
RTC_DATA_ATTR bool ebook_run = false;    //电子书运行标志 
RTC_DATA_ATTR bool gotonextpage = false; //特殊情况自动下一页标志
RTC_DATA_ATTR u8_t lightsleep_count = 0; //lightsleep次数
static AppEBook app;
static void appebook_exit()
{
    display.clearScreen();
    display.display(true);
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        if (app.txtFile){
            app.txtFile.close();
        }
        if (app.indexesFile){
            app.indexesFile.close();
        }
    }else{
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
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示"))){
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    }
    hal.pref.putInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, currentPage);
    Serial.printf("退出电子书，当前页：%d\n", currentPage);
    ebook_run = false;
}
static void appebook_deepsleep()
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        if (app.txtFile){
            app.txtFile.close();
        }
        if (app.indexesFile){
            app.indexesFile.close();
        }
    }else{
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
}
void AppEBook::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
// 左键：上一页
// 右键：下一页
// 长按右键：打开菜单

void AppEBook::setup()
{
    bool page_changed = false;
    app.exit = appebook_exit;
    app.deepsleep = appebook_deepsleep;
    app.currentFilename[0] = 0;
    display.clearScreen();
    size_t s = hal.pref.getBytes(SETTINGS_PARAM_LAST_EBOOK, app.currentFilename, 256);
    if (hal.wakeUpFromDeepSleep == false || currentPage == -1)
    {
        currentPage = hal.pref.getInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, 0);
        if (s == 0)
        {
            openFile();
        }
        else
        {
            Serial.printf("电子书：上次打开的文件：%s，上次打开的页：%d\n", app.currentFilename, currentPage);
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
        Serial.println("从DeepSleep唤醒");
        if (s == 0)
            appManager.goBack();
        if (app.openFile(app.currentFilename) == false)
            appManager.goBack();
    }
    gotoPage(currentPage);
    if ((!file_fs_sd) && hal.pref.getBool(hal.get_char_sha_key("使用lightsleep"))){
        peripherals.tf_unload();
    }
    //if (hal.btnl.isPressing())
    bool while_run = true;
    while (while_run)
    {
        if (hal.btnc.isPressing())
        {   
            openMenu();
            display.display(true);
        }
        if (hal.btnl.isPressing() || ((hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) && ebook_run == true))
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
                Serial.println("上一页");
                page_changed = true;
            }
        }
        if (hal.btnr.isPressing() || ((hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) && ebook_run == true || (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER && hal.pref.getBool(hal.get_char_sha_key("自动翻页")))))
        {
            if (GUI::waitLongPress(PIN_BUTTONR)){
                Serial.println("打开菜单");
                // 打开菜单
                openMenu();
                display.display(true);
                while (hal.btnl.isPressing() || hal.btnr.isPressing())
                {
                    delay(10);
                }
                //return;
            }else{
                if (gotoPage(currentPage + 1)){
                    Serial.println("下一页");
                    page_changed = true;
                }else
                {
                    GUI::msgbox("提示", "已经是最后一页了");
                    Serial.println("已经是最后一页了");
                    display.display(true);
                }
            }
        }
        if (page_changed == true)
        {
            page_changed = false;
            drawCurrentPage();
        }
        if (GUI::waitLongPress(PIN_BUTTONR))
        {
            Serial.println("打开菜单");
            // 打开菜单
            openMenu();
            display.display(true);
        }
        yield();
        if ((hal.pref.getBool(hal.get_char_sha_key("使用lightsleep")) || hal.pref.getBool(hal.get_char_sha_key("自动翻页"))) && exit_app == false){
            if (hal.pref.getBool(hal.get_char_sha_key("自动翻页"))){
                esp_sleep_enable_timer_wakeup(hal.pref.getInt("auto_page", 10) * 1000000UL);
            }
            if (hal.btn_activelow){
                esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 0);
                esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ALL_LOW);
                gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_LOW_LEVEL);
            }else{
                if (hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) == true){
                    esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 1);
                    esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ANY_HIGH);
                    gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_HIGH_LEVEL);
                }else
                    esp_sleep_enable_ext1_wakeup((1ULL << PIN_BUTTONC) | (1ULL << PIN_BUTTONL) | (1ULL << PIN_BUTTONR), ESP_EXT1_WAKEUP_ANY_HIGH);
            }
            esp_sleep_enable_gpio_wakeup();
            log_i("进入lightsleep");
            esp_light_sleep_start();
            log_i("退出lightsleep");
            lightsleep_count++;
            if (lightsleep_count > hal.pref.getInt("max_lightsleep", 20)){
                need_deepsleep = true;
                hal.pref.putInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, currentPage);
                lightsleep_count = 0;
            }
        }
        while_run = hal.pref.getBool(hal.get_char_sha_key("使用lightsleep"));
        if (exit_app || need_deepsleep){
            need_deepsleep = false;
            break;
        }
        ebook_run = true;
    }
    if (exit_app){
        appManager.goBack();
    }
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 61 - hal.timeinfo.tm_sec;
}

// 索引格式
// 每4字节代表一页在某个文件中的起始位置
// 文件全部采用UTF 8编码
// 字符宽度：默认英文7,中文14
bool AppEBook::indexcode_1(){
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
    if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        Serial.println("File starts with UTF-8 BOM");
        fseek(currentFileHandle, 3, SEEK_SET); // 移动到 BOM 头之后的位置
    } else {
        fseek(currentFileHandle, 0, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    } 
    long begin = millis(), last;
    while (true)
    {
    start:
        if (!r_flag){
        c = fgetc(currentFileHandle);
        }else{
            r_flag = false;
        }
        if (c == EOF)
        {
            break;
        }
        offset++;
        if (c == '\r'){
            goto start;
        }
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
                            {
                                utf_bytes = 5;
                            }
                            else
                            {
                                utf_bytes = 4;
                            }
                        }
                        else
                        {
                            utf_bytes = 3;
                        }
                    }
                    else
                    {
                        utf_bytes = 2;
                    }
                }
                else
                {
                    utf_bytes = 1;
                }
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
            x = 0;
            add_pending = 0;
            c = fgetc(currentFileHandle);
            if (c == '\r'){
                offset++;
            }else{
                r_flag = true;
            }
            uint32_t offsetall = 0;
            offsetall = ftell(currentFileHandle);
            char r = fgetc(currentFileHandle);
            if (r == '\r'){
                offset++;
            }else{
                fseek(currentFileHandle, offsetall, SEEK_SET);
            }
        }
        else if (x + add_pending >= 294)
        {
            x = add_pending;
            y += 14;
        }
        else
        {
            x += add_pending;
        }
        if (y >= 128 - 14)
        {
            page++;
            x = 0;
            y = 0;
            uint32_t pageOffset = offset - utf_bytes - 1;
            //uint32_t pageOffset = offset - utf_bytes;
            fwrite(&pageOffset, 4, 1, indexFileHandle);
        }
        if(millis() - last > 1000)
        {
            display.clearScreen();
            u8g2Fonts.setCursor(0,15);
            u8g2Fonts.printf("文件名称：%s", currentFilename);
            u8g2Fonts.setCursor(0,30);
            u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall,offsetall/1024);
            u8g2Fonts.setCursor(0,45);
            u8g2Fonts.printf("剩余大小：%d(%dKB)",offsetall-offset,(offsetall-offset)/1024);
            u8g2Fonts.setCursor(0,60);
            u8g2Fonts.printf("索引进度：%d%%",offset*100/offsetall);
            display.display(true);
            last = millis();
        }
    }
    fclose(indexFileHandle);
    indexFileHandle = NULL;
    char *tmp = (char *)malloc(256);
    int h = (millis() - begin) / 1000;
    display.clearScreen();
    u8g2Fonts.setCursor(0,15);
    u8g2Fonts.printf("文件名称：%s",currentFilename);
    u8g2Fonts.setCursor(0,30);
    u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall,offsetall/1024);
    u8g2Fonts.setCursor(0,45);
    u8g2Fonts.printf("剩余大小：0(0KB)");
    u8g2Fonts.setCursor(0,60);
    u8g2Fonts.printf("索引进度：100%%\n");
    u8g2Fonts.setCursor(0,75);
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
bool AppEBook::indexcode_2(){
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
    if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        Serial.println("File starts with UTF-8 BOM");
        fseek(currentFileHandle, 3, SEEK_SET); // 移动到 BOM 头之后的位置
    } else {
        fseek(currentFileHandle, 0, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    }
    long begin = millis(), last;
    while (true)
    {
    start:
        if (!r_flag){
        c = fgetc(currentFileHandle);
        }else{
            r_flag = false;
        }
        if (c == EOF)
        {
            break;
        }
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
                            {
                                utf_bytes = 5;
                            }
                            else
                            {
                                utf_bytes = 4;
                            }
                        }
                        else
                        {
                            utf_bytes = 3;
                        }
                    }
                    else
                    {
                        utf_bytes = 2;
                    }
                }
                else
                {
                    utf_bytes = 1;
                }
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
            x = 0;
            add_pending = 0;
            c = fgetc(currentFileHandle);
            if (c == '\r'){
                offset++;
            }else{
                r_flag = true;
            }
        }
        else if (x + add_pending >= 294)
        {
            x = add_pending;
            y += 14;
        }
        else
        {
            x += add_pending;
        }
        if (y >= 128 - 14)
        {
            page++;
            x = 0;
            y = 0;
            uint32_t pageOffset = offset - utf_bytes - 1;
            fwrite(&pageOffset, 4, 1, indexFileHandle);
        }
        if(millis() - last > 1000)
        {
            display.clearScreen();
            u8g2Fonts.setCursor(0,15);
            u8g2Fonts.printf("文件名称：%s", currentFilename);
            u8g2Fonts.setCursor(0,30);
            u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall,offsetall/1024);
            u8g2Fonts.setCursor(0,45);
            u8g2Fonts.printf("剩余大小：%d(%dKB)",offsetall-offset,(offsetall-offset)/1024);
            u8g2Fonts.setCursor(0,60);
            u8g2Fonts.printf("索引进度：%d%%",offset*100/offsetall);
            display.display(true);
            last = millis();
        }
    }
    fclose(indexFileHandle);
    indexFileHandle = NULL;
    char *tmp = (char *)malloc(256);
    int h = (millis() - begin) / 1000;
    display.clearScreen();
    u8g2Fonts.setCursor(0,15);
    u8g2Fonts.printf("文件名称：%s",currentFilename);
    u8g2Fonts.setCursor(0,30);
    u8g2Fonts.printf("文件大小：%u(%dKB)", offsetall,offsetall/1024);
    u8g2Fonts.setCursor(0,45);
    u8g2Fonts.printf("剩余大小：0(0KB)");
    u8g2Fonts.setCursor(0,60);
    u8g2Fonts.printf("索引进度：100%%\n");
    u8g2Fonts.setCursor(0,75);
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
const char* remove_path_prefix(const char* path, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t path_len = strlen(path);

    // 检查路径是否以指定前缀开头
    if (strncmp(path, prefix, prefix_len) == 0) {
        // 返回去除前缀后的路径
        return path + prefix_len;
    }
    // 如果路径不以指定前缀开头，则返回原始路径
    return path;
}
int8_t getCharLength(char zf) //获取ascii字符的长度
{
  if (zf == 0x20) return 4;      //空格
  else if (zf == '!') return 4;
  else if (zf == '"') return 5;
  else if (zf == '#') return 5;
  else if (zf == '$') return 6;
  else if (zf == '%') return 7;
  else if (zf == '&') return 7;
  else if (zf == '\'') return 3;
  else if (zf == '(') return 5;
  else if (zf == ')') return 5;
  else if (zf == '*') return 7;
  else if (zf == '+') return 7;
  else if (zf == ',') return 3;
  else if (zf == '.') return 3;

  else if (zf == '1') return 5;
  else if (zf == ':') return 4;
  else if (zf == ';') return 4;
  else if (zf == '@') return 9;

  else if (zf == 'A') return 8;
  else if (zf == 'D') return 7;
  else if (zf == 'G') return 7;
  else if (zf == 'H') return 7;
  else if (zf == 'I') return 3;
  else if (zf == 'J') return 3;
  else if (zf == 'M') return 8;
  else if (zf == 'N') return 7;
  else if (zf == 'O') return 7;
  else if (zf == 'Q') return 7;
  else if (zf == 'T') return 7;
  else if (zf == 'U') return 7;
  else if (zf == 'V') return 7;
  else if (zf == 'W') return 11;
  else if (zf == 'X') return 7;
  else if (zf == 'Y') return 7;
  else if (zf == 'Z') return 7;

  else if (zf == '[') return 5;
  else if (zf == ']') return 5;
  else if (zf == '`') return 5;

  else if (zf == 'c') return 5;
  else if (zf == 'f') return 5;
  else if (zf == 'i') return 1;
  else if (zf == 'j') return 2;
  else if (zf == 'k') return 5;
  else if (zf == 'l') return 2;
  else if (zf == 'm') return 9;
  else if (zf == 'o') return 7;
  else if (zf == 'r') return 4;
  else if (zf == 's') return 5;
  else if (zf == 't') return 4;
  else if (zf == 'v') return 7;
  else if (zf == 'w') return 9;
  else if (zf == 'x') return 5;
  else if (zf == 'y') return 7;
  else if (zf == 'z') return 5;

  else if (zf == '{') return 5;
  else if (zf == '|') return 4;
  else if (zf == '}') return 5;

  else if ((zf >= 0 && zf <= 31) || zf == 127) return -1; //没有实际显示功能的字符

  else return 6;
}
bool AppEBook::indexcode_3(){
  String txt[9 + 1] = {};  // 0-7行为一页 共8行
  int8_t line = 0;         // 当前行
  char c;                  // 中间数据
  uint16_t en_count = 0;   // 统计ascii和ascii扩展字符 1-2个字节
  uint16_t ch_count = 0;   // 统计中文等 3个字节的字符
  uint8_t line_old = 0;    //记录旧行位置
  boolean hskgState = 0;   //行首4个空格检测 0-检测过 1-未检测

  uint32_t pageCount = 1;      // 页数计数
  boolean line0_state = 1;     // 每页页首记录状态位
  uint32_t yswz_count = 0;      // 待写入文件统计
  String yswz_str = "";        // 待写入的文件
  uint32_t txtTotalSize = txtFile.size();//记录该TXT文件的大小，插入到索引的倒数14-8位
  long begin = millis(), last;
  if (indexesFile){
    indexesFile.close();
    if (file_fs_sd){
        SD.remove(indexesName);
        indexesFile = SD.open(indexesName, "a");
    }else{
        LittleFS.remove(indexesName);
        indexesFile = LittleFS.open(indexesName, "a");
    }
  }
  while (txtFile.available())
  {
    if (line_old != line) //行首4个空格检测状态重置
    {
      line_old = line;
      hskgState = 1;
    }

    if (line0_state == 1 && line == 0 && pageCount > 1)
    {
      line0_state = 0;
      uint32_t yswz_uint32 = txtFile.position(); //获取当前位置 yswz=页数位置
      //页数位置编码处理
      if (yswz_uint32 >= 1000000) yswz_str += String(yswz_uint32);
      else if (yswz_uint32 >= 100000)yswz_str += "0" + String(yswz_uint32);
      else if (yswz_uint32 >= 10000)yswz_str += "00" + String(yswz_uint32);
      else if (yswz_uint32 >= 1000)yswz_str += "000" + String(yswz_uint32);
      else if (yswz_uint32 >= 100)yswz_str += "0000" + String(yswz_uint32);
      else if (yswz_uint32 >= 10)yswz_str += "00000" + String(yswz_uint32);
      else yswz_str += "000000" + String(yswz_uint32);
      yswz_count++;
      if (yswz_count == 200) //每500页控制屏幕显示一下当前进度
      {
        if (!indexesFile){
            if (file_fs_sd){
                indexesFile = SD.open(indexesName, FILE_APPEND);
            }else{
                indexesFile = LittleFS.open(indexesName, "a"); //在索引文件末尾追加内容
            }
        }else{
            indexesFile.close();
            if (file_fs_sd){
                indexesFile = SD.open(indexesName, FILE_APPEND);
            }else{
                indexesFile = LittleFS.open(indexesName, "a"); //在索引文件末尾追加内容
            }
        }
        indexesFile.print(yswz_str); //将待写入的缓存 写入索引文件中
        indexesFile.close();

        yswz_str = "";   // 待写入文件清空
        yswz_count = 0;  // 待写入计数清空

        //计算剩余量,进度条
        if(millis() - last > 1000){
            uint32_t shengyu_int = txtTotalSize - txtFile.available();
            float shengyu_float = (float(shengyu_int) / float(txtTotalSize)) * 100.0;
            display.clearScreen();
            u8g2Fonts.setCursor(0,15);
            u8g2Fonts.printf("文件名称：%s", currentFilename);
            u8g2Fonts.setCursor(0,30);
            u8g2Fonts.printf("文件大小：%0.2fKB", float(txtTotalSize) / 1024.0);
            u8g2Fonts.setCursor(0,45);
            u8g2Fonts.printf("剩余大小：%0.2fKB", float(txtFile.available()) / 1024.0);
            u8g2Fonts.setCursor(0,60);
            u8g2Fonts.printf("索引进度：%0.2f%%", shengyu_float);
            display.display(true);
            last = millis();
        }
        //Serial.println("写入索引文件");
      }
      //Serial.print("第"); Serial.print(pageCount); Serial.print("页，页首位置："); Serial.println(yswz_uint32);
    }

    c = txtFile.read();   // 读取一个字节
    while (c == '\n' && line <= 8) // 检查换行符,并将多个连续空白的换行合并成一个
    {
      // 检测到首行并且为空白则不需要插入换行
      if (line == 0) //等于首行，并且首行不为空，才插入换行
      {
        if (txt[line].length() > 0) line++; //换行
        else txt[line].clear();
      }
      else //非首行的换行检测
      {
        //连续空白的换行合并成一个
        if (txt[line].length() > 0) line++;
        else if (txt[line].length() == 0 && txt[line - 1].length() > 0) line++;
        /*else if (txt[line].length() == 1 && txt[line - 1].length() == 1) hh = 0;*/
      }
      if (line <= 8)c = txtFile.read();
      en_count = 0;
      ch_count = 0;
    }
    if (c == '\t') //检查水平制表符 tab
    {
      if (txt[line].length() == 0) txt[line] += "    "; //行首的一个水平制表符 替换成4个空格
      else                         txt[line] += "       ";//非行首的一个水平制表符 替换成7个空格
    }
    else if ((c >= 0 && c <= 31) || c == 127) //检查没有实际显示功能的字符
    {
      //ESP.wdtFeed();  // 喂狗
    }
    else txt[line] += c;

    //检查字符的格式 + 数据处理 + 长度计算
    boolean asciiState = 0;
    byte a = B11100000;
    byte b = c & a;

    if (b == B11100000) //中文等 3个字节
    {
      ch_count ++;
      c = txtFile.read();
      txt[line] += c;
      c = txtFile.read();
      txt[line] += c;
    }
    else if (b == B11000000) //ascii扩展 2个字节
    {
      en_count += 12;
      c = txtFile.read();
      txt[line] += c;
    }
    else if (c == '\t') //水平制表符，代替两个中文位置，12*2
    {
      if (txt[line] == "    ") en_count += 20; //行首，因为后面会检测4个空格再加4所以这里是20
      else en_count += 24; //非行首
    }
    else if (c >= 0 && c <= 255)
    {
      en_count += getCharLength(c) + 1; //getCharLength=获取ascii字符的像素长度
      asciiState = 1;
    }

    uint16_t StringLength = en_count + (ch_count  * 12); //一个中文12个像素长度

    if (StringLength >= 260 && hskgState) //检测到行首的4个空格预计的长度再加长一点
    {
      if (txt[line][0] == ' ' && txt[line][1] == ' ' &&
          txt[line][2] == ' ' && txt[line][3] == ' ') {
        en_count += 4;
      }
      hskgState = 0;
    }

    if (StringLength >= 283) //283个像素检查是否已填满屏幕 ，填满一行
    {
      if (asciiState == 0)
      {
        line++;
        en_count = 0;
        ch_count = 0;
      }
      else if (StringLength >= 286)
      {
        char t = txtFile.read();
        txtFile.seek(-1, SeekCur); //往回移
        int8_t cz =  294 - StringLength;
        int8_t t_length = getCharLength(t);
        byte a = B11100000;
        byte b = t & a;
        if (b == B11100000 || b == B11000000) //中文 ascii扩展
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
    if (line == 9)
    {
      line0_state = 1;
      pageCount++;
      line = 0;
      en_count = 0;
      ch_count = 0;
      for (uint8_t i = 0; i < 9; i++) txt[i].clear();
    }
  }

  //剩余的字节写入索引文件，并在末尾加入文件大小校验位14-8 页数记录位7-1
  uint32_t size_uint32 = txtTotalSize; //获取当前TXT文件的大小
  String size_str = "";
  //TXT文件大小编码处理
  if (size_uint32 >= 1000000) size_str += String(size_uint32);
  else if (size_uint32 >= 100000)size_str += String("0") + String(size_uint32);
  else if (size_uint32 >= 10000)size_str += String("00") + String(size_uint32);
  else if (size_uint32 >= 1000)size_str += String("000") + String(size_uint32);
  else if (size_uint32 >= 100)size_str += String("0000") + String(size_uint32);
  else if (size_uint32 >= 10)size_str += String("00000") + String(size_uint32);
  else size_str += String("000000") + String(size_uint32);

  if (yswz_count != 0)  //还有剩余页数就在末尾加入 剩余的页数+文件大小位+当前位置位（初始0）
  {
    if (!indexesFile){
        if (file_fs_sd){
            indexesFile = SD.open(indexesName, FILE_APPEND);
        }else{
            indexesFile = LittleFS.open(indexesName, "a");
        }
    }else{
        indexesFile.close();
        if (file_fs_sd){
            indexesFile = SD.open(indexesName, FILE_APPEND);
        }else{
            indexesFile = LittleFS.open(indexesName, "a");
        }
    }
    indexesFile.print(yswz_str + size_str + "0000000");
  }
  else  //没有剩余页数了就在末尾加入文件大小位+当前位置位
  {
    if (!indexesFile){
        if (file_fs_sd){
            indexesFile = SD.open(indexesName, FILE_APPEND);
        }else{
            indexesFile = LittleFS.open(indexesName, "a");
        }
    }else{
        indexesFile.close();
        if (file_fs_sd){
            indexesFile = SD.open(indexesName, FILE_APPEND);
        }else{
            indexesFile = LittleFS.open(indexesName, "a");
        }
    }
    indexesFile.print(size_str + "0000000");
  }
  indexesFile.close();
  if (file_fs_sd){
    indexesFile = SD.open(indexesName, "r");
  }else{
    indexesFile = LittleFS.open(indexesName, "r");
  }
  uint32_t indexes_size = indexesFile.size();
  Serial.print("索引文件大小："); Serial.println(indexes_size);
  Serial.print("yswz_count："); Serial.println(yswz_count);
  Serial.print("pageCount："); Serial.println(pageCount);

  // 校验索引是否正确建立
  // 算法：一页为7个字节（从第二页开始记录所以要总页数-1），加上文件大小位7个字节，加上当前页数位7个字节
  // 所以为：7*((总页数-1)+1+1))
  if (indexes_size == 7 * ((pageCount - 1) + 1 + 1))
  {     
    display.clearScreen();
    u8g2Fonts.setCursor(0,15);
    u8g2Fonts.printf("文件名称：%s", currentFilename);
    u8g2Fonts.setCursor(0,30);
    u8g2Fonts.printf("文件大小：%0.2fKB", float(txtTotalSize) / 1024.0);
    u8g2Fonts.setCursor(0,45);
    u8g2Fonts.printf("剩余大小：%0KB");
    u8g2Fonts.setCursor(0,60);
    u8g2Fonts.printf("索引进度：100%%");
    display.display();  
  }
  else
  {
    Serial.println("校验失败，索引文件无效，请重新创建");
    if (strncmp(currentFilename, "/littlefs/", 10) == 0){
        LittleFS.remove(indexesName); 
    }else if(strncmp(currentFilename, "/sd/", 4) == 0){
        SD.remove(indexesName); 
    } 
    GUI::msgbox("提示", "文件索引失败");
    return false;
  }


  indexesFile.close();

  yswz_str = "";
  yswz_count = 0;

  txtFile.close();

  uint32_t need = millis() - begin;
  Serial.print("计算完毕："); Serial.print(pageCount); Serial.println("页");
  
  char *tmp = (char *)malloc(256);
  sprintf(tmp, "耗时：%0.2fS,共%d页", (float)need / 1000.0, pageCount);
  GUI::msgbox("索引成功", tmp);
  free(tmp);
  if (strncmp(currentFilename, "/littlefs/", 10) == 0){
    indexesFile = LittleFS.open(indexesName); 
  }else if(strncmp(currentFilename, "/sd/", 4) == 0){
    indexesFile = SD.open(indexesName); 
  } 
  if (!indexesFile){
    GUI::msgbox("索引失败", "索引文件打开失败");
    return false;
  }
  delay(500);
  line = 0;
  en_count = 0;
  ch_count = 0;
  for (uint8_t i = 0; i < 9; i++) txt[i].clear();
  return true;
}
bool AppEBook::indexFile(){
    if (hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")) == false){
        return indexcode_2();
    }else if(hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) == false && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        return indexcode_3();
    }else{
        return indexcode_1();
    }
}

bool AppEBook::openFile(const char *filename)
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        if (app.txtFile){
            app.txtFile.close();
        }
        if (app.indexesFile){
            app.indexesFile.close();
        }
    }else{
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
        strcpy(currentFilename, GUI::fileDialog("请选择文件"));
    }
    else
    {
        strcpy(currentFilename, filename);
    }
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        if (strncmp(currentFilename, "/littlefs/", 10) == 0){
            file_fs_sd = false;
            txtFile= LittleFS.open(remove_path_prefix(currentFilename, "/littlefs"));
            sprintf(indexesName, "%s.i", remove_path_prefix(currentFilename, "/littlefs"));
            indexesFile = LittleFS.open(indexesName);
        }else if(strncmp(currentFilename, "/sd/", 4) == 0){
            file_fs_sd = true;
            txtFile= SD.open(remove_path_prefix(currentFilename, "/sd")); 
            sprintf(indexesName, "%s.i", remove_path_prefix(currentFilename, "/sd"));
            indexesFile = SD.open(indexesName);
        }  
        if (!txtFile){
            return false;
        }if (!indexesFile)
            if (!indexFile())
                return false;
    }else{
        currentFileHandle = fopen(currentFilename, "rb");
        if (currentFileHandle == NULL)
        {
            return false;
        }
        indexFileHandle = fopen((String(currentFilename) + ".i").c_str(), "rb");
        if (indexFileHandle == NULL)
        {
            if (!indexFile())
            {
                return false;
            }
        }
    }
    hal.pref.putBytes(SETTINGS_PARAM_LAST_EBOOK, app.currentFilename, strlen(app.currentFilename));
    return true;
}

bool AppEBook::gotoPage(uint32_t page)
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        if (!indexesFile){
            if (file_fs_sd){
                indexesFile = SD.open(indexesName, "r");
            }else{
                indexesFile = LittleFS.open(indexesName, "r");
            }
        }
        if (page == 0)
        {
            currentPage = 0;
            currentFileOffset = 0;
            txtFile.seek(currentFileOffset, SeekSet);
            return true;
        }
        else{
            uint32_t gbwz = 0;    //计算上一页的页首位置
            String gbwz_str = ""; //光标位置String
            //Serial.print("当前页1："); Serial.println(pageCurrent);
            //计算上一页的页首位置
            //因为第一页不需要记录所以要减1，因为我要的是上一页所以再减1
            gbwz = (page + 1) * 7 - 7;
            //Serial.print("gbwz："); Serial.println(gbwz);
            //打开索引，寻找上一页的页首位置
            indexesFile.seek(gbwz, SeekSet);
            //获取索引的数据
            for (uint8_t i = 0; i < 7; i++)
            {
                char c = indexesFile.read();
                gbwz_str += c;
            }
            uint32_t gbwz_uint32 = atol(gbwz_str.c_str()); //装换成int格式
            //indexesFile.close();
            currentFileOffset = gbwz_uint32;
            txtFile.seek(currentFileOffset, SeekSet);
            currentPage = page;
            return true;
            }
    }else{
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
bool AppEBook::draw_page1(){
    uint32_t offsetall = 0;
    offsetall = ftell(currentFileHandle);
    if (offsetall == 0){
    uint8_t buffer[3];
    buffer[0] = fgetc(currentFileHandle);
    buffer[1] = fgetc(currentFileHandle);
    buffer[2] = fgetc(currentFileHandle);
    // 检查是否为 BOM 头
    if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        Serial.println("File starts with UTF-8 BOM");
        fseek(currentFileHandle, offsetall + 3, SEEK_SET); // 移动到 BOM 头之后的位置
    } else {
        fseek(currentFileHandle, offsetall, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    } }
    int16_t x = 1, y = 0;
    int c;
    bool r_flag = false;
    // 窗口
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示"))){
        display.clearScreen(GxEPD_BLACK);
        u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    }else{
        display.clearScreen();
    }
    // 自动换行
    while (true)
    {
    start:
        if (!r_flag){
            c = fgetc(currentFileHandle); 
        }else{
            r_flag = false;
        }
        if (c == EOF)
        {
            __eof = true;
            break;
        }
        if (c == '\r'){
            goto start;
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
                            {
                                utf_bytes = 5;
                            }
                            else
                            {
                                utf_bytes = 4;
                            }
                        }
                        else
                        {
                            utf_bytes = 3;
                        }
                    }
                    else
                    {
                        utf_bytes = 2;
                    }
                }
                else
                {
                    utf_bytes = 1;
                }
            }
            else
            {
                GUI::info_msgbox("错误", "绘制文本时索引错误，可能文件非UTF-8编码");
                F_LOG("绘制文本时索引错误，可能文件非UTF-8编码");
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
            if (c != '\r'){
                r_flag = true;
            }
            uint32_t offsetall = 0;
            offsetall = ftell(currentFileHandle);
            char r = fgetc(currentFileHandle);
            if (r != '\r'){
                fseek(currentFileHandle, offsetall, SEEK_SET);
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
bool AppEBook::draw_page2(){
    uint32_t offsetall = 0;
    offsetall = ftell(currentFileHandle);
    uint8_t buffer[3];
    buffer[0] = fgetc(currentFileHandle);
    buffer[1] = fgetc(currentFileHandle);
    buffer[2] = fgetc(currentFileHandle);
    // 检查是否为 BOM 头
    if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        Serial.println("File starts with UTF-8 BOM");
        fseek(currentFileHandle, offsetall + 3, SEEK_SET); // 移动到 BOM 头之后的位置
    } else {
        fseek(currentFileHandle, offsetall, SEEK_SET); // 如果不是 BOM 头，回到文件开头
    }
    int16_t x = 1, y = 0;
    int c;
    bool r_flag = false;
    // 窗口
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示"))){
        display.clearScreen(GxEPD_BLACK);
        u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    }else{
        display.clearScreen();
    }
    // 自动换行
    while (true)
    {
    start:
        if (!r_flag){
            c = fgetc(currentFileHandle); 
        }else{
            r_flag = false;
        }
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
                            {
                                utf_bytes = 5;
                            }
                            else
                            {
                                utf_bytes = 4;
                            }
                        }
                        else
                        {
                            utf_bytes = 3;
                        }
                    }
                    else
                    {
                        utf_bytes = 2;
                    }
                }
                else
                {
                    utf_bytes = 1;
                }
            }
            else
            {
                Serial.println("非预期的UTF8编码");
                F_LOG("绘制文本时索引错误，可能文件非UTF-8编码");
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
            if (c != '\r'){
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
bool AppEBook::draw_page3(){
    String txt[9 + 1] = {};  // 0-8行为一页 共9行
    int8_t line = 0;         // 当前行
    char c;                  // 中间数据
    uint16_t en_count = 0;   // 统计ascii和ascii扩展字符 1-2个字节
    uint16_t ch_count = 0;   // 统计中文等 3个字节的字符
    uint8_t line_old = 0;    //记录旧行位置
    boolean hskgState = 1;   //行首4个空格检测 0-检测过 1-未检测
    if (!txtFile){
        if (file_fs_sd){
            txtFile = SD.open(remove_path_prefix(currentFilename, "/sd")); 
        }else{
            txtFile = LittleFS.open(remove_path_prefix(currentFilename, "/littlefs"));
        }
        gotoPage(currentPage);
    }
    while (line < 9)
    {
        if (line_old != line) //行首4个空格检测状态重置
        {
            line_old = line;
            hskgState = 1;
        }

        c = txtFile.read();   // 读取一个字节

        while (c == '\n' && line <= 8) // 检查换行符,并将多个连续空白的换行合并成一个
        {
            // 检测到首行并且为空白则不需要插入换行
            if (line == 0) //等于首行，并且首行不为空，才插入换行
            {
                if (txt[line].length() > 0) line++; //换行
                else txt[line].clear();
            }
            else //非首行的换行检测
            {
                //连续空白的换行合并成一个
                if (txt[line].length() > 0) line++;
                else if (txt[line].length() == 0 && txt[line - 1].length() > 0) line++;
                /*else if (txt[line].length() == 1 && txt[line - 1].length() == 1) hh = 0;*/
            }
            if (line <= 8)c = txtFile.read();
            en_count = 0;
            ch_count = 0;
        }

        if (c == '\t') //检查水平制表符 tab
        {
            if (txt[line].length() == 0) txt[line] += "    "; //行首的一个水平制表符 替换成4个空格
            else                         txt[line] += "       ";//非行首的一个水平制表符 替换成7个空格
        }
        else if ((c >= 0 && c <= 31) || c == 127) //检查没有实际显示功能的字符
        {
            //ESP.wdtFeed();  // 喂狗
        }
        else txt[line] += c;
        //检查字符的格式 + 数据处理 + 长度计算
        boolean asciiState = 0;
        byte a = B11100000;
        byte b = c & a;

        if (b == B11100000) //中文等 3个字节
        {
            ch_count ++;
            c = txtFile.read();
            txt[line] += c;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (b == B11000000) //ascii扩展 2个字节
        {
            en_count += 12;
            c = txtFile.read();
            txt[line] += c;
        }
        else if (c == '\t') //水平制表符，代替两个中文位置，12*2
        {
            if (txt[line] == "    ") en_count += 20; //行首，因为后面会检测4个空格再加4所以这里是20
            else en_count += 24; //非行首
        }
        else if (c >= 0 && c <= 255)
        {
            en_count += getCharLength(c) + 1;
            asciiState = 1;
        }

        uint16_t StringLength = en_count + (ch_count  * 12);

        if (StringLength >= 260 && hskgState) //检测到行首的4个空格预计的长度再加长一点
        {
            if (txt[line][0] == ' ' && txt[line][1] == ' ' &&
                txt[line][2] == ' ' && txt[line][3] == ' ') {
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

        if (StringLength >= 283) //检查是否已填满屏幕 283
        {
            //Serial.println("");
            //Serial.print("行"); Serial.print(line); Serial.print(" 预计像素长度:"); Serial.println(StringLength);
            //Serial.print("行"); Serial.print(line); Serial.print(" 实际像素长度:"); Serial.println(u8g2Fonts.getUTF8Width(txt[line].c_str()));
            if (asciiState == 0) //最后一个字符是中文，直接换行
            {
                line++;
                en_count = 0;
                ch_count = 0;
            }
            else if (StringLength >= 286) //286 最后一个字符不是中文，在继续检测
            {
                char t = txtFile.read();
                txtFile.seek(-1, SeekCur); //往回移
                int8_t cz =  294 - StringLength;
                int8_t t_length = getCharLength(t);
                /*Serial.print("字符t:"); Serial.println(t);
                  Serial.print("字符t:"); Serial.println(t, HEX);
                  Serial.print("t长度:"); Serial.println(t_length);
                  Serial.print("差值:"); Serial.println(cz);*/
                byte a = B11100000;
                byte b = t & a;
                if (b == B11100000 || b == B11000000) //中文 ascii扩展
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    //Serial.println("测试2");
                }
                else if (t_length > cz)
                {
                    line++;
                    en_count = 0;
                    ch_count = 0;
                    //Serial.println("测试3");
                }
            }
        }
    }
      //for (uint8_t i = 0; i < 8; i++) Serial.println(txt[i]); //串口输出内容
    if (hal.pref.getBool(hal.get_char_sha_key("反色显示"))){
        display.clearScreen(GxEPD_BLACK);
        u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    }else{
        display.clearScreen();
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    }
    for (uint8_t i = 0; i < 9; i++)
    {
      uint8_t offset = 0; //缩减偏移量
      if (txt[i][0] == 0x20) //检查首行是否为半角空格 0x20
      {
        //继续检测后3位是否为半角空格，检测到连续的4个半角空格，偏移12个像素
        if (txt[i][1] == 0x20 && txt[i][2] == 0x20 && txt[i][3] == 0x20)
          //offset = 12;
          offset = 0;
      }
      else if (txt[i][0] == 0xE3 && txt[i][1] == 0x80 && txt[i][2] == 0x80) //检查首行是否为全角空格 0x3000 = E3 80 80
      {
        //继续检测后2位是否为全角空格，检测到连续的2个全角空格，偏移2个像素
        if (txt[i][3] == 0xE3 && txt[i][4] == 0x80 && txt[i][5] == 0x80)
          offset = 2;
      }

      u8g2Fonts.setCursor(1 + offset, i * 14 + 13);
      u8g2Fonts.print(txt[i]);
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
void AppEBook::drawCurrentPage(){
    bool state;
    if (hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序")) == false){
        state = draw_page2();
    }else if(hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1")) == false && hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        state = draw_page3();
    }else{
        state = draw_page1();
    }
    if (state){
    }else{
        GUI::info_msgbox("错误", "绘制文本中出现错误");
    }
}

int AppEBook::getTotalPages()
{
    if (hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        int indexesFileSize = indexesFile.size();
        return (indexesFileSize / 7) - 1; 
    }else{
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
    int moth=hal.timeinfo.tm_mon + 1,d=hal.timeinfo.tm_mday,dw=hal.timeinfo.tm_wday,h=hal.timeinfo.tm_hour,m=hal.timeinfo.tm_min,s=hal.timeinfo.tm_sec;
    char buf[64], vbat[64];
    sprintf(buf,"当前时间:%d月%d日 星期%s %d:%d:%d",moth,d,dayOfWeek[dw],h,m,s);
    sprintf(vbat, "电源电压：%.2fV", (float)hal.VCC / 1000.0);
    char *title = (char *)malloc(128);
    int totalPages = getTotalPages();
    sprintf(title, "%d/%d %d%%", currentPage + 1, totalPages, (currentPage + 1) * 100 / totalPages);
    const menu_item items[] = {
        {NULL, "返回"},
        {NULL, "退出"},
        {NULL, "换文件.."},
        {NULL, "重建当前文件索引"},
        {NULL, "跳转到.."},
        {NULL, buf},
        {NULL, "设置"},
        {NULL, vbat},
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
        peripherals.load(PERIPHERALS_SD_BIT);
        if (openFile() == false)
        {
            GUI::msgbox("打开文件失败", currentFilename);
            F_LOG("文件%s打开失败", currentFilename);
        }
        gotoPage(0);
        drawCurrentPage();
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
    {
        int page = GUI::msgbox_number("跳转到..", get_digits(totalPages), currentPage + 1);
        if (page > 0)
        {
            if (gotoPage(page - 1) == false)
            {
                GUI::msgbox("跳转失败", "页码超出范围");
                F_LOG("跳转失败，%d超出范围" ,page - 1);
                gotoPage(currentPage);
            }
            drawCurrentPage();
        }
    }
    break;
    case 6:
        ebooksettings();
        break;
    default:
        GUI::msgbox("错误", "无效的选项,或此选项为空");
        break;
    }
}

void AppEBook::ebooksettings(){
    static const menu_select ebook_set[] = {
        {false, "返回"},
        {true,  "根据唤醒源翻页"},          
        {true,  "自动翻页"},
        {false, "自动翻页延时"},        //3
        {true,  "使用lightsleep"},   
        {false, "最大lightsleep次数"}, //5
        {true,  "反色显示"},            
        {true,  "使用备选txt解析程序1"},
        {true,  "甘草索引程序"},
        {false, "屏幕全刷间隔"},        //9
        {false, NULL},
    };
    bool code = hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1"));
    bool code2 = hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"));
    int res = 0;
    bool end = false;
    while (!end){
        res = GUI::select_menu("电子书设置", ebook_set);
        switch (res)
        {
            case 0:
                end = true;
                break;
            case 3:
                hal.pref.putInt("auto_page", GUI::msgbox_number("输入时长s", 5, hal.pref.getInt("auto_page", 10)));
                break;
            case 5:
                hal.pref.putInt("max_lightsleep", GUI::msgbox_number("输入次数", 3, hal.pref.getInt("max_lightsleep", 20)));
                break;
            case 9:
                hal.pref.putInt("display_count", GUI::msgbox_number("输入全刷间隔", 2, hal.pref.getInt("display_count", 15)));
                break;
            default:
                GUI::info_msgbox("错误", "无效的选项");
                break;
        }
    }
    if (code != hal.pref.getBool(hal.get_char_sha_key("使用备选txt解析程序1"))){
        indexFile();
        gotoPage(0);
        drawCurrentPage();
    }if (code2 != hal.pref.getBool(hal.get_char_sha_key("甘草索引程序"))){
        indexFile();
        gotoPage(0);
        drawCurrentPage();
    }
}