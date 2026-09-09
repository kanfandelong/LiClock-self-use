#include <AppManager.h>
#include <algorithm> // for std::max
AppManager appManager;
#define MAX_APP_COUNT 128
AppBase *appList[MAX_APP_COUNT];
int tail = 0;
char RTC_DATA_ATTR latest_appname[36] = "";
void appList_push_back(AppBase *app)
{
    appList[tail++] = app;
}

void printAppList() {
    log_printf("\033[96m======= App List =======\033[0m\r\n");
    // 表头（对齐参考）
    log_printf("%-3s %-20s | %-20s | %-5s | %-4s\r\n", 
               "Idx", "Name", "Title", "ID", "Show");
    log_printf("---- -------------------- | -------------------- | ----- | ----\r\n");

    for (int i = 0; i < MAX_APP_COUNT; ++i) {
        if (appList[i] != nullptr) {
            // [%2u] 两位索引右对齐
            // %-20.20s 名字左对齐，最多显示20个字符（超出截断，不足补空格）
            // %-20.20s 标题同理
            // ID:%3u   ID 右对齐3位
            // %-3s     show 左对齐3字符
            log_printf("[%2u] %-20.20s | %-20.20s | ID:%3u | %-3s\r\n",
                       i,
                       appList[i]->name,
                       appList[i]->title,
                       appList[i]->appID,
                       appList[i]->_showInList ? "yes" : "no");
        }
    }
    log_printf("\033[96m========================\033[0m\r\n");
}

AppBase::AppBase()
{
    lightsleep = NULL;
    wakeup = NULL;
    exit = NULL;
    deepsleep = NULL;
    appID = appManager.getAValidAppID();
    appManager.increaseValidAppID();
    appList_push_back(this);
}
AppBase::~AppBase()
{
}

void AppBase::set() {
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

AppBase *AppManager::getPtrByName(const char *appName)
{
    for (int16_t i = 0; i < tail; i++)
    {
        if (strcmp(appList[i]->name, appName) == 0)
            return appList[i];
    }
    return NULL;
}

AppBase *AppManager::getRealClock()
{
    bootapp = hal.pref.getString(SETTINGS_PARAM_HOME_APP, "");
    if (bootapp == "")
    {
        hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
        bootapp = "clock";
    }
    if (bootapp == "clock")
    {
        if (hal.pref.getBool(hal.get_char_sha_key("离线模式")))
        {
            bootapp = "clockonly";
        }
    }
    if (appManager.getPtrByName(bootapp.c_str()) == NULL)
    {
        log_w("严重错误 之前设置的App不存在，使用默认时钟App");
        hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
        bootapp = "clockonly";
    }
    return getPtrByName(bootapp.c_str());
}

extern const uint8_t defaultAppIcon[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x00,
    0x00, 0x03, 0x80, 0x01, 0x00, 0x05, 0x40, 0x01, 0x00, 0x09, 0x20, 0x01,
    0x00, 0x11, 0x10, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x81, 0x03, 0x01, 0x00, 0x41, 0x04, 0x01, 0x00, 0x01, 0x04, 0x01,
    0x00, 0x01, 0x02, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x11, 0x10, 0x01, 0x00, 0x09, 0x20, 0x01,
    0x00, 0x05, 0x40, 0x01, 0x00, 0x03, 0x80, 0x01, 0x00, 0xfe, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t goBackIcon[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00,
    0x00, 0xb0, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00,
    0x00, 0x86, 0x00, 0x00, 0x00, 0x83, 0xff, 0x07, 0x80, 0x81, 0x00, 0x08,
    0xc0, 0x80, 0x00, 0x10, 0x60, 0x80, 0x00, 0x10, 0x30, 0x80, 0x00, 0x10,
    0x18, 0x80, 0x00, 0x10, 0x30, 0x81, 0x00, 0x14, 0x60, 0x82, 0x00, 0x14,
    0xc0, 0x84, 0xf8, 0x13, 0x80, 0x89, 0x00, 0x08, 0x00, 0xb3, 0xff, 0x07,
    0x00, 0x86, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00,
    0x00, 0xb0, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t wifiIcon[] = {
    0x00, 0x00, 0xf0, 0x0f, 0xfc, 0x3f, 0x1e, 0x78, 0x07, 0xe0, 0xe0, 0x07,
    0xf8, 0x1f, 0x38, 0x1c, 0x00, 0x00, 0x80, 0x01, 0xc0, 0x03, 0x80, 0x01,
    0x00, 0x00};

void AppManager::gotoApp(AppBase *appPtr)
{
    if (appPtr == NULL)
        return;
    this->app_to = appPtr;
    method = APPMANAGER_GOTOAPP;
}

void AppManager::gotoApp(const char *appName)
{
    AppBase *appPtr = getPtrByName(appName);
    if (appPtr != NULL)
    {
        gotoApp(appPtr);
    }
    else if (luaLoaded == false)
    {
        loadLuaApps();
        gotoApp(appName);
    }
}

void AppManager::goBack()
{
    if (appStack.empty() == true)
    {
        return;
    }
    method = APPMANAGER_GOBACK;
}
static AppBase *realAppList[MAX_APP_COUNT];
static int realAppCount = 0;
void buildAppList(bool showHidden)
{
    realAppCount = 0;
    for (int16_t i = 0; i < tail; i++)
    {
        if (showHidden == false)
        {
            appList[i]->set();
            if (appList[i]->_showInList == false)
            {
                // log_d("APP %s 已在APP列表中%s", appList[i]->title, appList[i]->_showInList ? "true" : "隐藏");
                continue;
            }
            if (peripherals.checkAvailable(appList[i]->peripherals_requested) != 0)
            {
                continue;
            }
        }
        realAppList[realAppCount] = appList[i];
        realAppCount++;
    }
}
void AppManager::App_Preferences_init()
{
    if (hal.pref.getBool("app_pref_init", false))
    {
        return;
    }
    else
    {
        log_i("初始化APP隐藏控制参数");
        for (int16_t i = 0; i < tail; i++)
        {
            appList[i]->set();
            hal.pref.putBool(hal.get_char_sha_key(appList[i]->title), appList[i]->_showInList);
            log_i("APP %s 状态:%s", appList[i]->title, appList[i]->_showInList ? "显示在列表" : "隐藏");
        }
        hal.pref.putBool("app_pref_init", true);
        log_i("APP隐藏控制参数初始化结束");
    }
}

const int menu_x_offset = 20;
const int menu_y_offset = 32;

// AppList每页11个，算左上角一个返回共12个
// 先build再show
void AppManager::showAppList(int page)
{
    const int page_app_cont = 13;
    int totalPage = realAppCount / page_app_cont;
    if (realAppCount % page_app_cont)
        ++totalPage;
    // 下面是标题部分
    {
        char buf[30];
        display.drawRoundRect(0, 0, MAX_X, MAX_Y, 3, 0);
        // 标题栏
        display.drawFastHLine(0, 16, MAX_X, 0);
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setForegroundColor(0);
        sprintf(buf, "%02d:%02d", hal.timeinfo.tm_hour, hal.timeinfo.tm_min);
        u8g2Fonts.drawUTF8(2, 12, buf);
        if (realAppCount > page_app_cont)
        {
            sprintf(buf, "第%d页/共%d页", page + 1, totalPage);
            u8g2Fonts.drawUTF8(40, 12, buf);
        }
        // 右侧状态图标
        int16_t x = MAX_X - 2;
        // 电池
        if (hal.pref.getBool(hal.get_char_sha_key("精准电量显示"), false) && hal.VCC < 4300 && !hal.isCharging)
        {
            display.drawXBitmap(x - 20, 0, getBatteryIcon(true), 20, 16, 0);
            display.fillRect(x - 17, 6, getBatterysoc(), 4, TFT_BLACK);
        }
        else
            display.drawXBitmap(x - 20, 0, getBatteryIcon(), 20, 16, 0);
        x -= 20 - 2;
        // 电池电量数值
        u8g2Fonts.setCursor(x - 44, 12);
        if (hal.bat_info.voltage != 0.0)
            u8g2Fonts.printf("%.03fV", hal.bat_info.voltage);
        else
            u8g2Fonts.printf("%d.%03dV", hal.VCC / 1000, hal.VCC % 1000);
        x -= 50 - 4;
        // WiFi
        if (WiFi.isConnected())
        {
            display.drawXBitmap(x - 16, 2, wifiIcon, 16, 13, 0);
            x -= 16 - 2;
        }
    }
    // 返回按钮
    display.drawXBitmap(menu_x_offset + 8, menu_y_offset, goBackIcon, 32, 32, 0);
    u8g2Fonts.drawUTF8(menu_x_offset + 12, menu_y_offset + 45, "返回");
    int pagebase = page_app_cont * page; // 页基数（这一页第一个）
    int pageItemsCount;
    if (page == totalPage - 1)
    {
        pageItemsCount = realAppCount % page_app_cont;
        if (pageItemsCount == 0)
            pageItemsCount = page_app_cont;
    }
    else
    {
        pageItemsCount = page_app_cont;
    }
    // 调整图标间隔：水平 50 像素，垂直 70 像素
    for (int16_t i = 0; i < pageItemsCount; i++)
    {
        int16_t x, y;
        x = ((i + 1) / 2) * 50 + menu_x_offset; // 原 54 -> 50
        y = ((i + 1) % 2) * 70 + menu_y_offset; // 原 72 -> 70，App 左上角位置
        if (realAppList[pagebase + i]->image != NULL)
        {
            display.drawXBitmap(x + 8, y, realAppList[pagebase + i]->image, 32, 32, 0);
        }
        else
        {
            display.drawXBitmap(x + 8, y, defaultAppIcon, 32, 32, 0);
        }
        int w = u8g2Fonts.getUTF8Width(realAppList[pagebase + i]->title);
        int x_font_offset = 0;
        if (w <= 48)
        {
            x_font_offset = (48 - w) / 2;
        }
        u8g2Fonts.drawUTF8(x + x_font_offset, y + 45, realAppList[pagebase + i]->title);
    }
}

void AppManager::animateAppSelection(int appIndex, int currentPage, AppBase *selectedApp)
{
    const int page_app_cont = 13;
    // 计算选中图标位置（与 showAppList 中的计算方式一致）
    int i = appIndex;                           // 页内索引（0-based）
    int x = ((i + 1) / 2) * 50 + menu_x_offset; // 图标绘制时的 x 偏移（用于计算矩形和图标起始点）
    int y = ((i + 1) % 2) * 70 + menu_y_offset; // 图标绘制时的 y 偏移
    // 起始矩形（选择框）左上角及宽高
    int start_rect_x = x - 1;
    int start_rect_y = y - 2;
    int start_rect_w = 50;
    int start_rect_h = 50;
    // 结束矩形为全屏
    int end_rect_x = 0;
    int end_rect_y = 0;
    int end_rect_w = MAX_X;
    int end_rect_h = MAX_Y;
    // 图标起始位置（左上角）
    int start_icon_x = x + 8;
    int start_icon_y = y;
    // 图标结束位置（屏幕中心，减去图标半宽）
    int end_icon_x = (MAX_X - 32) / 2;
    int end_icon_y = (MAX_Y - 32) / 2;

    // 步骤数
    const int steps = 10;

    display.swapBuffer(2);
    display.clearScreen();
    showAppList(currentPage); // 将背景保存到缓冲区2
    display.swapBuffer(1);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(20); // 运行周期
    // 动画主循环
    for (int s = 0; s <= steps; s++)
    {
        float t = (float)s / steps; // 插值因子 0..1

        // 插值矩形参数
        int cur_rect_x = start_rect_x + (end_rect_x - start_rect_x) * t;
        int cur_rect_y = start_rect_y + (end_rect_y - start_rect_y) * t;
        int cur_rect_w = start_rect_w + (end_rect_w - start_rect_w) * t;
        int cur_rect_h = start_rect_h + (end_rect_h - start_rect_h) * t;

        // 插值图标位置
        int cur_icon_x = start_icon_x + (end_icon_x - start_icon_x) * t;
        int cur_icon_y = start_icon_y + (end_icon_y - start_icon_y) * t;

        display.copyBuffer(1, 2);

        display.fillRoundRect(cur_rect_x, cur_rect_y, cur_rect_w, cur_rect_h, 5, TFT_WHITE);
        display.drawRoundRect(cur_rect_x, cur_rect_y, cur_rect_w, cur_rect_h, 5, TFT_BLACK);

        // 绘制应用图标（使用黑色，保证在白色背景上可见）
        if (selectedApp->image != NULL)
        {
            display.drawXBitmap(cur_icon_x, cur_icon_y, selectedApp->image, 32, 32, TFT_BLACK);
        }
        else
        {
            display.drawXBitmap(cur_icon_x, cur_icon_y, defaultAppIcon, 32, 32, TFT_BLACK);
        }

        // 将缓冲区 2 显示出来
        display.display();

        // 帧间延时，控制动画速度
        xTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

    // 可选：动画结束后再显示全屏白色背景中央图标，为加载文字做准备
    display.clearScreen();
    if (selectedApp->image != NULL)
    {
        display.drawXBitmap(end_icon_x, end_icon_y, selectedApp->image, 32, 32, TFT_BLACK);
    }
    else
    {
        display.drawXBitmap(end_icon_x, end_icon_y, defaultAppIcon, 32, 32, TFT_BLACK);
    }
    display.display();
}

AppBase *AppManager::appSelector(bool showHidden)
{
    const int page_app_cont = 13;
    // // display.epd2.PLL_set(hal.pref.getUInt("pllset", 0x3C));
    bool finished = false; // 是否完成选择，用于超过一页的情况
    int currentPage = 0;
    loadLuaApps();
    buildAppList(showHidden);
    int totalPage = realAppCount / page_app_cont;
    if (realAppCount % page_app_cont)
        ++totalPage;
    int pageItemsCount;
    int selected = 0;
    int last_selected = 0;
    int idleTime = 0;
    bool waitc = false;
    display.setPowerMode(POWER_MODE_HPM);
    display.swapBuffer(2);
    display.clearScreen();
    // display.display();
    showAppList(currentPage);
    display.drawRoundRect(menu_x_offset - 1, menu_y_offset - 2, 50, 50, 5, 0); // 绘制选择框
    display.swapBuffer(1);
    display.copyBuffer(1, 0);
    display.slideScreenFull(SLIDE_DOWN, 250, 2);
    display.display();

    int code = 0;
    esp_err_t err = esp_console_run(hal.pref.getString("init_cmd", "wsconsole start").c_str(), &code);
    log_printf("command returned %d, code %d\n", err, code);

    // 下面是选择
    hal.hookButton();
    while (finished == false)
    {
        if (currentPage == totalPage - 1)
        {
            pageItemsCount = realAppCount % page_app_cont;
            if (pageItemsCount == 0)
                pageItemsCount = page_app_cont;
        }
        else
        {
            pageItemsCount = page_app_cont;
        }
        // 下面是选择
        idleTime = 0;
        waitc = false;
        while (1)
        {
            if (hal.btnl.isPressing())
            {
                idleTime = 0;
                selected--;
                if (selected < 0)
                {
                    if (totalPage == 1)
                        selected = realAppCount; // 这里没问题，不要改，因为0是返回
                    else
                    {
                        if (currentPage == 0)
                            currentPage = totalPage - 1;
                        else
                            --currentPage;
                        if (currentPage == totalPage - 1)
                        {
                            pageItemsCount = realAppCount % page_app_cont;
                            if (pageItemsCount == 0)
                                pageItemsCount = page_app_cont;
                        }
                        else
                        {
                            pageItemsCount = page_app_cont;
                        }
                        selected = pageItemsCount;
                        break;
                    }
                }
            }
            if (hal.btnr.isPressing())
            {
                idleTime = 0;
                selected++;
                if (selected > pageItemsCount) // 这里也没问题
                {
                    if (totalPage == 1)
                        selected = 0;
                    else
                    {
                        selected = 0;
                        ++currentPage;
                        if (currentPage == totalPage)
                            currentPage = 0;
                        break;
                    }
                }
            }
            if (hal.btnc.isPressing())
            {
                delay(20);
                if (hal.btnc.isPressing())
                {
                    if (GUI::waitLongPress(PIN_BUTTONC) == true)
                    {
                        selected = 0;
                        waitc = true;
                    }
                    else
                    {
                        finished = true;
                        break;
                    }
                }
            }
            if (selected != last_selected)
            {
                // 记录当前页码，以判断是否翻页
                int currentPageBefore = currentPage;
                // 计算旧位置和新位置（使用新的间隔）
                int16_t prev = last_selected;
                int16_t old_x = (prev / 2) * 50 + menu_x_offset;
                int16_t old_y = (prev % 2) * 70 + menu_y_offset;
                int16_t new_x = (selected / 2) * 50 + menu_x_offset;
                int16_t new_y = (selected % 2) * 70 + menu_y_offset;

                // 如果页面已改变，则不执行动画，仅在后续绘制选框
                if (currentPage != currentPageBefore)
                {
                    // 直接更新选中状态，后面的绘制逻辑会在页面切换后绘制框
                    last_selected = selected;
                }
                else
                {
                    // 根据移动距离动态计算动画步数，确保平滑且不太快
                    int dx = new_x - old_x;
                    int dy = new_y - old_y;
                    int distance = std::max(abs(dx), abs(dy));     // 取最大轴向距离
                    const int MAX_STEPS = 25;                      // 最大步数限制
                    int steps = std::min(MAX_STEPS, distance / 4); // 步长5像素，最大步数限制
                    display.swapBuffer(2);
                    display.clearScreen();
                    showAppList(currentPage);
                    display.swapBuffer(1);
                    TickType_t xLastWakeTime = xTaskGetTickCount();
                    TickType_t xFrequency = pdMS_TO_TICKS(18); // 运行周期
                    for (int s = 1; s <= steps; ++s)
                    {
                        // 线性插值计算中间坐标
                        int16_t ix = old_x + ((new_x - old_x) * s) / steps;
                        int16_t iy = old_y + ((new_y - old_y) * s) / steps;
                        display.copyBuffer(1, 2);
                        display.drawRoundRect(ix - 1, iy - 2, 50, 50, 5, 0);
                        display.display();
                        xTaskDelayUntil(&xLastWakeTime, xFrequency);
                    }
                    // 更新选中状态
                    last_selected = selected;
                }
            }
            if (waitc == true)
            {
                waitc = false;
                while (hal.btnc.isPressing())
                    delay(10);
                delay(10);
            }
            delay(10);
            idleTime++;
            if (idleTime > 6000)
            {
                // 60s无操作，自动返回
                selected = 0;
                finished = true;
                break;
            }
        }
        if (finished == false)
        {
            int16_t x, y;
            // 使用新的图标间隔（水平 50，垂直 70）计算选择框位置
            x = (selected / 2) * 50 + menu_x_offset;
            y = (selected % 2) * 70 + menu_y_offset; // App左上角位置
            display.clearScreen();
            showAppList(currentPage);
            display.drawRoundRect(x - 1, y - 2, 50, 50, 5, 0); // 绘制选择框
            last_selected = selected;
            display.display();
        }
    }
    hal.unhookButton();
    if (selected == 0)
    {
        display.slideScreenFull(SLIDE_UP, 250, 0);
        display.swapBuffer(0);
        display.display();
    }
    else
    {
        // display.clearScreen();
        // display.setCursor(60, 72);
        // display.setFont(&FreeSans18pt7b);
        // display.print("Loading...");
        // display.display();
        if (!showHidden)
        {
            int appIndex = selected - 1; // 页内应用索引
            AppBase *selectedApp = realAppList[currentPage * page_app_cont + appIndex];
            animateAppSelection(appIndex, currentPage, selectedApp);
            skipSwitchAnimation = true;
            delay(500);
        }
        display.swapBuffer(0);
        display.clearScreen();
    }
    if (selected == 0)
        return NULL;
    selected -= 1;
    selected += currentPage * page_app_cont;
    return realAppList[selected]; // 这里没问题
}

void AppManager::update()
{
    static bool updateAgain = false;
    if (currentApp == NULL && app_to == NULL)
        return; // App还未加载，直接返回
    // 判断是否需要进入App选择界面
    if (method == APPMANAGER_SHOWAPPSELECTOR)
    {
        method = APPMANAGER_NOOPERATION;
        AppBase *res = appSelector();
        if (res != NULL)
        {
            this->app_to = res;
            method = APPMANAGER_GOTOAPP;
            log_i("正在跳转到APP：%d:%s", app_to->appID, app_to->name);
            return;
        }
        updateAgain = true;
    }
    else if (method == APPMANAGER_GOTOAPP)
    {
        method = APPMANAGER_NOOPERATION;
        // 注意只能有一个App占有屏幕，所以在切换App时先退出上个App
        if (currentApp != NULL)
        {
            if (currentApp->exit != NULL)
            {
                currentApp->exit();
            }
        }
        // 执行切换动画（除非已被选择器动画替代）
        if (!skipSwitchAnimation)
        {
            display.swapBuffer(2);
            display.clearScreen();
            if (app_to != NULL && app_to->image != NULL)
            {
                display.drawXBitmap(176, 68, app_to->image, 32, 32, TFT_BLACK);
            }
            else
            {
                display.drawXBitmap(176, 68, defaultAppIcon, 32, 32, TFT_BLACK);
            }
            display.swapBuffer(1);
            display.clearScreen();
            if (currentApp != NULL && currentApp->image != NULL)
            {
                display.drawXBitmap(176, 68, currentApp->image, 32, 32, TFT_BLACK);
            }
            else
            {
                display.drawXBitmap(176, 68, defaultAppIcon, 32, 32, TFT_BLACK);
            }
            delay(50);
            display.slideScreenFull(SLIDE_DOWN, 250, 2);
            delay(200);
            display.swapBuffer(0);
        }
        skipSwitchAnimation = false; // 重置标志
        attachLocalEvent();
        if (app_to != currentApp && currentApp != NULL)
        {
            appStack.push(currentApp);
        }
        fTimer = NULL;
        timer_interval = 0;
        nextWakeup = 0;
        noDeepSleep = false;
        currentApp = app_to;
        if (currentApp->_reentrant)
            strncpy(latest_appname, app_to->name, 36);
        hal.setWakeupIO(currentApp->wakeupIO[0], currentApp->wakeupIO[1]);
        if (currentApp->noDefaultEvent)
            hal.detachAllButtonEvents();
        if (peripherals.load(currentApp->peripherals_requested) == false)
        {
            GUI::msgbox("错误", "外设加载失败，APP运行将不稳定");
            log_w("外设加载失败!");
        }
        currentApp->setup();
        parameter = "";
        updateAgain = true;
    }
    else if (method == APPMANAGER_GOBACK)
    {
        method = APPMANAGER_NOOPERATION;
        if (appStack.size() == 0)
            return;
        // goback时：currentApp就是当前前台App，appStack未变化
        // 首先执行app退出
        if (currentApp->exit != NULL)
            currentApp->exit();

        AppBase *last_app = currentApp;
        // 然后准备环境
        currentApp = appStack.top();
        // 执行切换动画（除非已被选择器动画替代）
        if (!skipSwitchAnimation)
        {
            display.swapBuffer(2);
            display.clearScreen();
            if (currentApp != NULL && currentApp->image != NULL)
            {
                display.drawXBitmap(176, 68, currentApp->image, 32, 32, TFT_BLACK);
            }
            else
            {
                display.drawXBitmap(176, 68, defaultAppIcon, 32, 32, TFT_BLACK);
            }
            display.swapBuffer(1);
            display.clearScreen();
            if (last_app != NULL && last_app->image != NULL)
            {
                display.drawXBitmap(176, 68, last_app->image, 32, 32, TFT_BLACK);
            }
            else
            {
                display.drawXBitmap(176, 68, defaultAppIcon, 32, 32, TFT_BLACK);
            }
            delay(50);
            display.slideScreenFull(SLIDE_UP, 250, 2);
            delay(200);
            display.swapBuffer(0);
        }
        skipSwitchAnimation = false; // 重置标志
        appStack.pop();
        if (currentApp->_reentrant)
            strncpy(latest_appname, currentApp->name, 36);
        fTimer = NULL;
        timer_interval = 0;
        nextWakeup = 0;
        noDeepSleep = false;
        // 然后执行前一app初始化
        hal.setWakeupIO(currentApp->wakeupIO[0], currentApp->wakeupIO[1]);
        if (currentApp->noDefaultEvent)
            hal.detachAllButtonEvents();
        if (peripherals.load(currentApp->peripherals_requested) == false)
        {
            GUI::msgbox("错误", "外设加载失败，APP运行将不稳定");
            log_w("外设加载失败!");
        }
        currentApp->setup();
        updateAgain = true;
    }
    else
    {
        method = APPMANAGER_NOOPERATION;
    }
    // 判断是否需要执行定时器
    if (fTimer != NULL && hal.now >= timer_triggertime)
    {
        fTimer();
        timer_triggertime = hal.now + timer_interval;
        updateAgain = true;
    }
    if (updateAgain)
    {
        updateAgain = false;
        return;
    }
    if (hal.btnl.isPressing() == false && hal.btnr.isPressing() == false && hal.btnc.isPressing() == false && hal.btnl.isIdle() == true && hal.btnr.isIdle() == true && hal.btnc.isIdle() == true)
    {
        // 准备进入睡眠模式
        // 等待EPD2
#if defined(Queue)
        if (// display.epd2.isBusy())
            return;
        if(uxQueueMessagesWaiting(// display.epd2.getQueue()) != 0)
            return;
#endif
        // 计算下一次唤醒时间
        int realNextWakeup = 0;
        if (nextWakeup != 0)
        {
            if (nextWakeup > timer_triggertime && fTimer != NULL && timer_triggertime != 0 && timer_interval != 0)
            {
                realNextWakeup = timer_triggertime - hal.now;
            }
            else
            {
                realNextWakeup = nextWakeup;
            }
        }
        else if (fTimer != NULL && timer_triggertime != 0 && timer_interval != 0)
        {
            realNextWakeup = timer_interval;
        }
        else
        {
            realNextWakeup = 0;
        }
        if (realNextWakeup == 0)
        {
            int now_min = hal.timeinfo.tm_hour * 60 + hal.timeinfo.tm_min;
            realNextWakeup = (alarms.getNextWakeupMinute() - now_min) * 60;
        }
        else
        {
            int currentTime = alarms.getNextWakeupMinute();
            if (currentTime != 0)
            {
                int now_min = hal.timeinfo.tm_hour * 60 + hal.timeinfo.tm_min;
                realNextWakeup = min(realNextWakeup, (alarms.getNextWakeupMinute() - now_min) * 60);
            }
        }
        hal.noDeepSleep = noDeepSleep;
        if (noDeepSleep == true)
        {
            if (currentApp->lightsleep != NULL)
            {
                currentApp->lightsleep();
            }
        }
        else
        {
            if (currentApp->deepsleep != NULL)
            {
                currentApp->deepsleep();
            }
            else if (currentApp->exit != NULL)
            {
                currentApp->exit();
            }
        }
        // 等待锁
        while (hal.SleepUpdateMutex)
            delay(2);
        hal.SleepUpdateMutex = true;
        if (realNextWakeup != 0)
        {
            hal.goSleep(realNextWakeup);
        }
        else
        {
            hal.powerOff(false);
        }
        hal.SleepUpdateMutex = false;
        method = APPMANAGER_NOOPERATION;
        if (currentApp->wakeup != NULL)
        {
            currentApp->wakeup();
        }
    }
}

void AppManager::setTimer(uint32_t second, void (*fn)())
{
    if (second == 0 || fn == NULL)
        return;
    fTimer = fn;
    timer_interval = second;
    timer_triggertime = hal.now + second;
}

void AppManager::clearTimer()
{
    fTimer = NULL;
    timer_interval = 0;
}

void AppManager::attachLocalEvent()
{
    hal.detachAllButtonEvents();
    log_printf("正在更新按键事件\n");
    hal.btnc.attachLongPressStart([](void *scope)
                                  {if( ((AppManager *)scope)->currentApp->noDefaultEvent == false) ((AppManager *)scope)->method = APPMANAGER_SHOWAPPSELECTOR; },
                                  this);
    hal.btnl.attachLongPressStart([](void *scope)
                                  { if( ((AppManager *)scope)->currentApp->noDefaultEvent == false) {((AppManager *)scope)->method = APPMANAGER_GOBACK; log_printf("Back.\n"); } },
                                  this);
}
void AppManager::loadLuaApps()
{
    if (luaLoaded == false)
    {
        log_printf("延迟加载Lua APP列表\n");
        searchForLuaAPP();
        luaLoaded = true;
    }
}
void AppManager::gotoAppBoot(const char *appName)
{
    appStack.push(getRealClock());
    gotoApp(appName);
}

bool AppManager::recover(AppBase *home)
{
    if (latest_appname[0] != 0)
    {
        log_printf("重新打开上个APP：");
        log_printf("%s\n", latest_appname);
        if (home != NULL)
            appStack.push(home);
        else
            appStack.push(getRealClock());
        if (strcmp(latest_appname, "clock") == 0)
        {
            if (strcmp(home->name, "clockonly") == 0)
            {
                log_printf("已设置离线模式，此App被替换为clockonly\n");
                gotoApp("clockonly");
                return true;
            }
        }
        gotoApp(latest_appname);
        return true;
    }
    return false;
}