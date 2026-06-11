#include <A_Config.h>

#define GUI_Frequency 20

namespace GUI
{
    int last_buffer_idx = 0;
    /**
     * @brief  确认按键的按下情况
     * @param btn GPIO引脚号
     * @return bool  true:长按，false:非长按
     */
    bool waitLongPress(int btn) // 检查长按，如果是长按则返回true
    {
        for (int16_t i = 0; i < hal.lpt; ++i)
        {
            if (digitalRead(btn) == hal.btn_activelow)
                return false;
            delay(10);
        }
        return true;
    }
    // 自动换行
    /**
     * @brief  自动换行文本显示函数
     * @param str 需要显示的文本
     * @param max_x 最大的X坐标
     * @param start_x 起始X坐标
     * @param fontsize 字体高度（每次换行增加的y坐标值）
     */
    void autoIndentDraw(const char *str, int max_x, int start_x, int fontsize)
    {
        while (*str)
        {
            if ((max_x - u8g2Fonts.getCursorX()) < (fontsize - 1) || *str == '\n')
            {
                u8g2Fonts.setCursor(start_x, u8g2Fonts.getCursorY() + fontsize);
            }
            if (*str != '\n')
            {
                u8g2Fonts.print(*str);
            }
            str++;
        }
    }
    inline void push_buffer()
    {
        last_buffer_idx = display.current_buffer_idx;
        display.swapBuffer(2);
        display.copyBuffer(2, last_buffer_idx);
    }
    inline void pop_buffer()
    {
        display.swapBuffer(last_buffer_idx);
    }
    /**
     * @brief  绘制带标题的窗口
     * @param title 标题文本
     * @param x 窗口x坐标（左上角）
     * @param y 窗口y坐标（左上角）
     * @param w 窗口宽度
     * @param h 窗口高度
     */
    void drawWindowsWithTitle(const char *title, int16_t x, int16_t y, int16_t w, int16_t h)
    {
        int16_t wchar;
        display.setDrawWindow(x, y, w - 1, h);
        display.fillRoundRect(x, y, w, h, 3, 1); // 清空区域
        display.drawRoundRect(x, y, w, h, 3, 0);
        // 标题栏
        display.drawFastHLine(x, y + 14, w, 0);
        if (title)
        {
            u8g2Fonts.setBackgroundColor(1);
            u8g2Fonts.setForegroundColor(0);
            if (hal.pref.getString("system_font", "default") == "default")
                u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312_self, 209899L);
            else
                u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());
            wchar = u8g2Fonts.getUTF8Width(title);
            u8g2Fonts.setCursor(x + (w - wchar) / 2, y + 12);
            u8g2Fonts.print(title);
        }
    }
    uint16_t read16(File &f)
    {
        // BMP数据存储在little endian中，与Arduino相同。
        uint16_t result;
        ((uint8_t *)&result)[0] = f.read(); // LSB 最低有效位 最右侧
        ((uint8_t *)&result)[1] = f.read(); // MSB 最高有效位 最左侧
        return result;
    }

    uint32_t read32(File &f)
    {
        // BMP数据存储在little endian中，与Arduino相同。
        uint32_t result;
        ((uint8_t *)&result)[0] = f.read(); // LSB
        ((uint8_t *)&result)[1] = f.read();
        ((uint8_t *)&result)[2] = f.read();
        ((uint8_t *)&result)[3] = f.read(); // MSB
        return result;
    }

    uint8_t colorThresholdLimit(uint8_t val1, int8_t val2) // 颜色阈值限制
    {
        int16_t val1_int = val1;
        int16_t val2_int = val2;
        int16_t tmp = val1_int + val2_int;
        int16_t out = 0;
        // log_print("val1_int:" + String(val1_int)); log_print(" val2_int:" + String(val2_int)); log_println(" tmp:" + String(tmp));
        if (tmp > 255)
            return 255;
        else if (tmp < 0)
            return 0;
        else
            return tmp;
        return 0;
    }
    // val1附近的像素，val2误差
    uint8_t colorThresholdLimit_jpg(uint8_t val1, int8_t val2) // 颜色阈值限制
    {
        int16_t val1_int = val1;
        int16_t val2_int = val2;
        int16_t tmp = val1_int + val2_int;
        if (tmp > 255)
            return 255;
        else if (tmp < 0)
            return 0;
        else
            return tmp;
        return 0;
    }
    ////////////////////////////////////标准对话框
    /**
     * @brief  消息显示（带确认）GUI
     * @param title 窗口标题
     * @param msg  消息内容
     */
    void msgbox(const char *title, const char *msg, uint16_t timeout)
    {
        // ---------- 窗口尺寸与圆角 ----------
        constexpr int kMsgBoxWidth = 190;
        constexpr int kMsgBoxHeight = 110;
        constexpr int kMsgBoxCornerRadius = 5; // 窗口圆角（若 drawWindowsWithTitle 内部使用）

        // ---------- 内容区域边距 ----------
        constexpr int kContentLeftMargin = 2; // 文字左侧留白
        constexpr int kContentTopOffset = 28; // 从窗口顶部到文字基线的距离（标题栏下方）

        // ---------- “确定”按钮 ----------
        constexpr int kButtonWidth = 70;
        constexpr int kButtonHeight = 15;
        constexpr int kButtonCornerRadius = 3;
        constexpr int kButtonRightMargin = 5;         // 按钮右侧距离窗口右边缘
        constexpr int kButtonBottomMargin = 5;        // 按钮底部距离窗口下边缘
        constexpr int kButtonTextBaselineOffset = 12; // 文字基线距按钮顶部的偏移，用于垂直居中

        // 按钮文字（也可定义为常量，便于多语言）
        constexpr const char *kButtonLabel = "确定";

        // ---------- 计算窗口起始坐标 ----------
        const int start_x = (MAX_X - kMsgBoxWidth) / 2;
        const int start_y = (MAX_Y - kMsgBoxHeight) / 2;

        int16_t w;
        bool result = false; // 保留以备将来扩展

        hal.hookButton();
        push_buffer();

        // 绘制带标题的窗口背景
        drawWindowsWithTitle(title, start_x, start_y, kMsgBoxWidth, kMsgBoxHeight);

        // 绘制消息内容（自动缩进显示）
        if (msg)
        {
            u8g2Fonts.setCursor(start_x + kContentLeftMargin,
                                start_y + kContentTopOffset);
            autoIndentDraw(msg,
                           start_x + kMsgBoxWidth - kContentLeftMargin, // 右边界
                           start_x + kContentLeftMargin);               // 左边界
        }

        // 计算按钮位置（靠右下角对齐）
        const int buttonX = start_x + kMsgBoxWidth - kButtonRightMargin - kButtonWidth;
        const int buttonY = start_y + kMsgBoxHeight - kButtonBottomMargin - kButtonHeight;

        // 绘制按钮矩形
        display.drawRoundRect(buttonX, buttonY,
                              kButtonWidth, kButtonHeight,
                              kButtonCornerRadius, 0);

        // 绘制按钮文字（水平居中，垂直偏移保证视觉居中）
        w = u8g2Fonts.getUTF8Width(kButtonLabel);
        u8g2Fonts.setCursor(buttonX + (kButtonWidth - w) / 2,
                            buttonY + kButtonTextBaselineOffset);
        u8g2Fonts.print(kButtonLabel);

        // 刷新显示
        display.display(true);

        // 等待用户操作（超时或按键）
        hal.wait_input(timeout);

        // 恢复绘制环境
        display.setDrawWindow();
        pop_buffer();
        hal.unhookButton();
    }
    /**
     * @brief  消息显示GUI，不等待输入，显示后立即恢复绘图环境
     * @param title 窗口标题
     * @param msg  消息内容
     * @param start_x 起始X坐标(左上角)
     * @param start_y 起始Y坐标(左上角)
     */
    void info_msgbox(const char *title, const char *msg, int start_x, int start_y)
    {
        // ---------- 窗口固定大小 ----------
        constexpr int kInfoBoxWidth = 190;
        constexpr int kInfoBoxHeight = 110;

        // ---------- 内容区域边距 ----------
        constexpr int kContentLeftMargin = 2; // 文字左侧留白
        constexpr int kContentTopOffset = 28; // 标题栏下方文字基线位置

        if (start_x < 0)
            start_x = (MAX_X - kInfoBoxWidth) / 2;
        if (start_y < 0)
            start_y = (MAX_Y - kInfoBoxHeight) / 2;

        int16_t x = u8g2Fonts.getCursorX(), y = u8g2Fonts.getCursorY();

        push_buffer();
        drawWindowsWithTitle(title, start_x, start_y, kInfoBoxWidth, kInfoBoxHeight);

        // 绘制消息内容（自动缩进显示）
        if (msg)
        {
            u8g2Fonts.setCursor(start_x + kContentLeftMargin,
                                start_y + kContentTopOffset);
            autoIndentDraw(msg,
                           start_x + kInfoBoxWidth - kContentLeftMargin, // 右边界
                           start_x + kContentLeftMargin);                // 左边界
        }

        display.display();
        display.setDrawWindow(); // 恢复绘制窗口
        pop_buffer();
        u8g2Fonts.setCursor(x, y);
    }
    /**
     * @brief  二选一确认对话框，左/右按键选择，30秒超时
     * @param title 窗口标题
     * @param msg  消息内容
     * @param yes  右按钮文本（默认 "确定 (右)"）
     * @param no   左按钮文本（默认 "取消 (左)"）
     * @return bool  true:选择了右按钮，false:选择了左按钮
     */
    bool msgbox_yn(const char *title, const char *msg, const char *yes, const char *no)
    {
        // ---------- 窗口尺寸与圆角 ----------
        constexpr int kMsgBoxWidth = 190;
        constexpr int kMsgBoxHeight = 110;

        // ---------- 内容区域边距 ----------
        constexpr int kContentLeftMargin = 2;
        constexpr int kContentTopOffset = 28;

        // ---------- 按钮通用参数 ----------
        constexpr int kButtonWidth = 70;
        constexpr int kButtonHeight = 15;
        constexpr int kButtonCornerRadius = 3;
        constexpr int kButtonSideMargin = 5;          // 按钮到窗口左右边缘的距离
        constexpr int kButtonBottomMargin = 5;        // 按钮到窗口下边缘的距离
        constexpr int kButtonTextBaselineOffset = 12; // 文字在按钮内的垂直偏移

        // ---------- 超时与默认文本 ----------
        constexpr unsigned long kInputTimeout = 30000; // 30秒超时间隔
        if (yes == nullptr)
            yes = "确定 (右)";
        if (no == nullptr)
            no = "取消 (左)";

        // ---------- 居中定位 ----------
        constexpr int start_x = (MAX_X - kMsgBoxWidth) / 2;
        constexpr int start_y = (MAX_Y - kMsgBoxHeight) / 2;

        int16_t w;
        bool result = false;

        hal.hookButton();
        push_buffer();

        // 绘制带标题的窗口背景
        drawWindowsWithTitle(title, start_x, start_y, kMsgBoxWidth, kMsgBoxHeight);

        // 绘制消息内容
        u8g2Fonts.setCursor(start_x + kContentLeftMargin,
                            start_y + kContentTopOffset);
        autoIndentDraw(msg,
                       start_x + kMsgBoxWidth - kContentLeftMargin,
                       start_x + kContentLeftMargin);

        // 计算两个按钮的坐标
        const int leftButtonX = start_x + kButtonSideMargin;
        const int rightButtonX = start_x + kMsgBoxWidth - kButtonSideMargin - kButtonWidth;
        const int buttonY = start_y + kMsgBoxHeight - kButtonBottomMargin - kButtonHeight;

        // 绘制左按钮（对应 no）
        display.drawRoundRect(leftButtonX, buttonY,
                              kButtonWidth, kButtonHeight,
                              kButtonCornerRadius, 0);
        w = u8g2Fonts.getUTF8Width(no);
        u8g2Fonts.setCursor(leftButtonX + (kButtonWidth - w) / 2,
                            buttonY + kButtonTextBaselineOffset);
        u8g2Fonts.print(no);

        // 绘制右按钮（对应 yes）
        display.drawRoundRect(rightButtonX, buttonY,
                              kButtonWidth, kButtonHeight,
                              kButtonCornerRadius, 0);
        w = u8g2Fonts.getUTF8Width(yes);
        u8g2Fonts.setCursor(rightButtonX + (kButtonWidth - w) / 2,
                            buttonY + kButtonTextBaselineOffset);
        u8g2Fonts.print(yes);

        display.display(true);

        // 等待用户按键，每30秒调用一次 hal.wait_input() 防止看门狗或系统挂起
        unsigned long start = millis();
        while (true)
        {
            delay(10);
            if (millis() - start > kInputTimeout)
            {
                hal.wait_input();
                start = millis();
            }
            if (hal.btnr.isPressing())
            {
                result = true;
                break;
            }
            else if (hal.btnl.isPressing())
            {
                result = false;
                break;
            }
        }

        display.setDrawWindow(); // 恢复绘制窗口
        pop_buffer();
        hal.unhookButton();

        return result;
    }
    /**
     * @brief  菜单GUI
     * @param title 窗口标题
     * @param options 菜单选择列表及对应图标数组
     * @param ico_w 图标宽度
     * @param ico_h 图标高度
     * @param default_selected 默认选中的项目索引（默认为0）
     * @return int类型的选中的菜单项
     */
    int menu(const char *title, const menu_item options[], int16_t ico_w, int16_t ico_h, int default_selected)
    {
        constexpr int w = 260;
        constexpr int h = 128;
        constexpr int max_h = h + 15;
        constexpr int item_height = 16;
        constexpr int start_x = (MAX_X - w) / 2;
        constexpr int start_y = (MAX_Y - max_h) / 2;
        constexpr int item_width = w - 5 - 5 - 5;        // 右侧滚动条
        constexpr int number_of_items = h / item_height; // 每页显示的项目数
        int total = 0;
        bool hasIcon = false;
        bool init = false;

        // 计算总项目数和检查图标
        while (options[total].title != NULL)
        {
            if (options[total].icon != NULL)
                hasIcon = true;
            ++total;
        }

        // 确保默认选中在有效范围内
        int selected = constrain(default_selected, 0, total - 1);
        int prev_selected = selected; // 用于动画起始位置
        int pageStart = 0;

        // 计算初始页面起始位置，确保默认选中项可见
        if (total > number_of_items)
        {
            pageStart = constrain(selected - number_of_items / 2, 0, total - number_of_items);
        }

        int barHeight = number_of_items * 96 / total;
        int barPos = 0;
        bool updated = true;
        bool waitc = false;
        unsigned long wait_time = 0;

        // ---------- 双缓冲相关变量 ----------
        const int STATIC_BUF = 1;  // 静态缓冲区索引（假设有缓冲区0和1）
        bool static_valid = false; // 静态内容是否已绘制且有效
        int last_pageStart = -1;   // 上次绘制静态时的pageStart
        int last_total = -1;       // 上次绘制静态时的总项目数（防止total变化）
        int last_hasIcon = -1;     // 上次是否有图标（防止布局变化）
        // ---------------------------------

        hal.hookButton(true);
        push_buffer();
        wait_time = millis();
        goto init_draw;
        while (1)
        {
            if (hal.btnl.isPressing())
            {
                delay(10);
                if (hal.btnl.isPressing())
                {
                    if (selected == 0)
                    {
                        selected = total;
                    }
                    --selected;
                    if (selected < 0)
                        selected = 0;
                    updated = true;
                }
                wait_time = millis();
            }

            if (hal.btnr.isPressing())
            {
                delay(10);
                if (hal.btnr.isPressing())
                {
                    ++selected;
                    if (selected > total)
                        selected = total;
                    if (selected == total)
                    {
                        selected = 0;
                    }
                    updated = true;
                }
                wait_time = millis();
            }

            if (hal.btnc.isPressing())
            {
                delay(50);
                if (hal.btnc.isPressing())
                {
                    if (waitLongPress(PIN_BUTTONC) == true)
                    {
                        selected = 0;
                        waitc = true;
                        updated = true;
                    }
                    else
                    {
                        break;
                    }
                }
                wait_time = millis();
            }

            if (updated)
            {
            init_draw:
                // 判断是否出界并更新页面起始位置
                if (selected < pageStart)
                {
                    pageStart = selected;
                }
                else if (selected >= pageStart + number_of_items)
                {
                    pageStart = selected - number_of_items + 1;
                }

                // ---------- 如果页面内容发生变化（翻页或总项数/图标改变），重新绘制静态内容 ----------
                if (!static_valid || pageStart != last_pageStart || total != last_total || hasIcon != last_hasIcon)
                {
                    // 1. 完整绘制静态内容（不含选框和滚动条滑块）
                    drawWindowsWithTitle(title, start_x, start_y, w, max_h);
                    display.setDrawWindow(start_x, start_y + 14, w - 2, max_h - 16);
                    int max_items = min(number_of_items, total);
                    for (int i = 0; i < max_items; ++i)
                    {
                        int item_y = start_y + 15 + item_height * i;
                        if (options[i + pageStart].icon != NULL && ico_h <= 14)
                        {
                            display.drawXBitmap(start_x + 5, item_y + (14 - ico_h) / 2,
                                                options[i + pageStart].icon, ico_w, ico_h, 0);
                        }
                        u8g2Fonts.drawUTF8(start_x + 5 + (hasIcon ? ico_w + 2 : 0),
                                           item_y + 13, options[i + pageStart].title);
                    }
                    // 滚动条轨道（只画背景，不画滑块；这里简单画一个浅色矩形作为轨道，与原风格保持一致）
                    if (total > number_of_items)
                    {
                        display.fillRoundRect(start_x + w - 5 + 1, start_y + 15, 3, h, 2, 1); // 轨道背景（灰色）
                    }
                    display.display(); // 此时显示静态内容（用户会看到一次全刷，仅翻页时发生）

                    // 2. 将当前显示内容（静态）复制到静态缓冲区 STATIC_BUF
                    display.copyBuffer(STATIC_BUF, display.current_buffer_idx);

                    // 记录当前静态内容的状态
                    last_pageStart = pageStart;
                    last_total = total;
                    last_hasIcon = hasIcon;
                    static_valid = true;
                }

                // ---------- 动画：平滑移动选框，每帧只绘制动态部分 ----------
                TickType_t xLastWakeTime = xTaskGetTickCount();
                TickType_t xFrequency = pdMS_TO_TICKS(GUI_Frequency);
                const int steps = 6;
                int start_y_rect = start_y + 15 + item_height * (prev_selected - pageStart);
                int target_y_rect = start_y + 15 + item_height * (selected - pageStart);
                for (int step = 0; step <= steps; ++step)
                {
                    // 计算当前选框的 Y 坐标
                    int cur_y_rect = start_y_rect + ((target_y_rect - start_y_rect) * step) / steps;

                    // 从静态缓冲区恢复显示缓冲区（清除上一帧的动态元素）
                    display.copyBuffer(display.current_buffer_idx, STATIC_BUF);

                    // 设置绘制窗口（与静态内容一致）
                    display.setDrawWindow(start_x, start_y + 14, w - 2, max_h - 16);

                    // 绘制选框矩形
                    display.drawRoundRect(start_x + 3, cur_y_rect, w - 5 - 6, 15, 3, 0);

                    // 绘制滚动条滑块（动态部分）
                    if (total > number_of_items)
                    {
                        barPos = selected * (h - barHeight) / total;
                        display.fillRoundRect(start_x + w - 5 + 1, start_y + 15 + barPos, 3, barHeight, 2, 0);
                    }

                    // 刷新显示
                    display.display();
                    xTaskDelayUntil(&xLastWakeTime, xFrequency);
                }

                // 动画结束后更新 prev_selected
                prev_selected = selected;
                updated = false;

                // 等待所有按钮释放（初始进入时）
                while (!init && (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing()))
                {
                    delay(10);
                }
                init = true;
            }

            if (waitc == true)
            {
                waitc = false;
                while (hal.btnc.isPressing())
                    delay(10);
                delay(10);
            }
            delay(10);
            if (millis() - wait_time > 30000)
            {
                hal.wait_input();
                wait_time = millis();
            }
        }

        display.setDrawWindow(); // 恢复绘制窗口
        pop_buffer();
        hal.unhookButton();
        return selected;
    }

    int menu(const char *title, const menu_item_mix options[], int16_t ico_w, int16_t ico_h, int default_selected)
    {
        // 动态窗口尺寸：留边距 20px
        const int MNU_WIN_W = min(384, MAX_X - 20);
        const int MNU_WIN_H = min(168, MAX_Y - 20);
        const int start_x = (MAX_X - MNU_WIN_W) / 2;
        const int start_y = (MAX_Y - MNU_WIN_H) / 2;

        constexpr int number_of_items = 6;                          // 一屏最多显示 6 项
        const int item_height = (MNU_WIN_H - 15) / number_of_items; // 标题占 15px
        const int item_width = MNU_WIN_W - 10 - 5 - 5;              // 留出右侧滚动条宽度

        // 统计菜单项数量，检查图标
        int total = 0;
        bool hasIcon = false;
        while (options[total].title != nullptr)
        {
            if (options[total].icon != nullptr)
                hasIcon = true;
            ++total;
        }

        int selected = constrain(default_selected, 0, total - 1);
        int prev_selected = selected;
        int pageStart = 0;
        if (total > number_of_items)
            pageStart = constrain(selected - number_of_items / 2, 0, total - number_of_items);

        int barHeight = number_of_items * (MNU_WIN_H - 15) / total;
        bool updated = true;
        bool waitc = false;
        unsigned long wait_time = 0;

        hal.hookButton(true);
        push_buffer();
        wait_time = millis();

        // 辅助函数：根据类型绘制行尾状态
        auto drawItemStatus = [&](int itemIdx, int x, int y)
        {
            const menu_item_mix &item = options[itemIdx];
            switch (item.type)
            {
            case MENU_ITEM_CHECKBOX:
            {
                // 方框 + 对勾
                int boxX = x + item_width - 15;
                int boxY = y + 2;
                bool val = hal.pref.getBool(item.key, false);
                display.drawRect(boxX, boxY, 11, 11, 0);
                if (val)
                {
                    display.drawLine(boxX + 2, boxY + 5, boxX + 5, boxY + 8, 0);
                    display.drawLine(boxX + 5, boxY + 8, boxX + 9, boxY + 2, 0);
                }
                break;
            }
            case MENU_ITEM_RADIO:
            {
                // 圆圈 + 圆点
                int cx = x + item_width - 9;
                int cy = y + 7;
                int selectedIdx = hal.pref.getInt(item.key, -1); // 存储本组选中索引
                display.drawCircle(cx, cy, 5, 0);
                if (selectedIdx == itemIdx)
                {
                    display.fillCircle(cx, cy, 3, 0);
                }
                break;
            }
            case MENU_ITEM_VALUE:
            {
                // 右对齐数值
                int val = hal.pref.getInt(item.key, item.min);
                char buf[12];
                snprintf(buf, sizeof(buf), "%d", val);
                int16_t txt_w = u8g2Fonts.getUTF8Width(buf);
                u8g2Fonts.drawUTF8(x + item_width - txt_w - 2, y + 13, buf);
                break;
            }
            default:
                break;
            }
        };

        goto init_draw; // 第一次直接绘图
        while (1)
        {
            // 旋钮 / 按键处理（btnl、btnr 上下移动选项）
            if (hal.btnl.isPressing())
            {
                delay(10);
                if (hal.btnl.isPressing())
                {
                    if (selected > 0)
                        --selected;
                    else if (total > 0)
                        selected = total - 1;
                    updated = true;
                }
                wait_time = millis();
            }
            if (hal.btnr.isPressing())
            {
                delay(10);
                if (hal.btnr.isPressing())
                {
                    if (selected < total - 1)
                        ++selected;
                    else
                        selected = 0;
                    updated = true;
                }
                wait_time = millis();
            }
            // 中心键处理
            if (hal.btnc.isPressing())
            {
                delay(50);
                if (hal.btnc.isPressing())
                {
                    if (waitLongPress(PIN_BUTTONC))
                    {
                        // 长按：退出菜单，返回 -1
                        selected = -1;
                        break;
                    }
                    else
                    {
                        // 短按：根据类型执行动作
                        const menu_item_mix &it = options[selected];
                        switch (it.type)
                        {
                        case MENU_ITEM_ACTION:
                            break; // 跳出循环，返回 selected
                        case MENU_ITEM_CHECKBOX:
                        {
                            bool cur = hal.pref.getBool(it.key, false);
                            hal.pref.putBool(it.key, !cur);
                            updated = true;
                            break;
                        }
                        case MENU_ITEM_RADIO:
                        {
                            hal.pref.putInt(it.key, selected);
                            updated = true;
                            break;
                        }
                        case MENU_ITEM_VALUE:
                        {
                            // 调用数字输入框
                            int cur = hal.pref.getInt(it.key, it.min);
                            int digits = snprintf(nullptr, 0, "%d", it.max);
                            int newVal = msgbox_number(it.title, digits, cur);
                            newVal = constrain(newVal, it.min, it.max);
                            hal.pref.putInt(it.key, newVal);
                            updated = true;
                            break;
                        }
                        }
                        if (it.type == MENU_ITEM_ACTION)
                            break; // 退出 while
                    }
                }
                wait_time = millis();
            }

            if (updated)
            {
            init_draw:
                // 翻页
                if (selected < pageStart)
                    pageStart = selected;
                else if (selected >= pageStart + number_of_items)
                    pageStart = selected - number_of_items + 1;

                TickType_t xLastWakeTime = xTaskGetTickCount();
                TickType_t xFrequency = pdMS_TO_TICKS(GUI_Frequency);

                // 动画移动选择框
                const int steps = 6;
                int start_y_rect = start_y + 15 + item_height * (prev_selected - pageStart);
                int target_y_rect = start_y + 15 + item_height * (selected - pageStart);
                for (int step = 0; step <= steps; ++step)
                {
                    int cur_y_rect = start_y_rect + ((target_y_rect - start_y_rect) * step) / steps;

                    drawWindowsWithTitle(title, start_x, start_y, MNU_WIN_W, MNU_WIN_H);
                    display.setDrawWindow(start_x, start_y + 14, MNU_WIN_W - 2, MNU_WIN_H - 16);

                    int max_items = min(number_of_items, total);
                    for (int i = 0; i < max_items; ++i)
                    {
                        int item_y = start_y + 15 + item_height * i;
                        const menu_item_mix &it = options[i + pageStart];

                        // 图标
                        if (it.icon != NULL && ico_h <= 14)
                        {
                            display.drawXBitmap(start_x + 5, item_y + (14 - ico_h) / 2,
                                                it.icon, ico_w, ico_h, 0);
                        }
                        // 文字
                        int textX = start_x + 5 + (hasIcon ? ico_w + 2 : 0);
                        u8g2Fonts.drawUTF8(textX, item_y + 13, it.title);
                        // 行尾状态
                        drawItemStatus(i + pageStart, start_x, item_y);

                        // 选择框
                        if (i == selected - pageStart)
                        {
                            display.drawRoundRect(start_x + 3, cur_y_rect, MNU_WIN_W - 10, 15, 3, 0);
                        }
                    }

                    // 滚动条
                    if (total > number_of_items)
                    {
                        int barPos = selected * (MNU_WIN_H - 15 - barHeight) / total;
                        display.fillRoundRect(start_x + MNU_WIN_W - 8, start_y + 15 + barPos,
                                              3, barHeight, 2, 0);
                    }

                    display.display();
                    xTaskDelayUntil(&xLastWakeTime, xFrequency);
                }

                prev_selected = selected;
                updated = false;
                while (!waitc && (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing()))
                    delay(10);
            }

            if (waitc)
            {
                waitc = false;
                while (hal.btnc.isPressing())
                    delay(10);
                delay(10);
            }
            delay(10);
            if (millis() - wait_time > 30000)
            {
                hal.wait_input();
                wait_time = millis();
            }
        }

        display.setDrawWindow();
        pop_buffer();
        hal.unhookButton();
        return selected;
    }

#include <mbedtls/sha256.h>
    static const uint8_t select_bits[] = {
        0xfe, 0x07, 0x03, 0x0c, 0x01, 0x08, 0xf1, 0x08, 0xf9, 0x09, 0xf9, 0x09,
        0xf9, 0x09, 0xf9, 0x09, 0xf1, 0x08, 0x01, 0x08, 0x03, 0x0c, 0xfe, 0x07};
    static const uint8_t no_select_bits[] = {
        0xfe, 0x07, 0x03, 0x0c, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08,
        0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x03, 0x0c, 0xfe, 0x07};

    void sha256(const char *input, uint8_t output[32], mbedtls_sha256_context *ctx)
    {
        mbedtls_sha256_init(ctx);
        mbedtls_sha256_starts(ctx, 0); // 0 表示 SHA-256
        mbedtls_sha256_update(ctx, (const unsigned char *)input, strlen(input));
        mbedtls_sha256_finish(ctx, output); // 完成哈希计算
        mbedtls_sha256_free(ctx);
    }
    /**
     * @brief 多项设置GUI
     * @param title 窗口标题
     * @param options 菜单选择列表与是否显示复选框(bool表示是否显示复选框)
     * @return int类型的选中的菜单项
     */
    int select_menu(const char *title, const menu_select options[], int default_selected)
    {
        constexpr uint8_t ico_h = 12, ico_w = 12;
        constexpr int w = 260;
        constexpr int h = 128;
        constexpr int max_h = h + 15;
        constexpr int item_height = 16;
        constexpr int start_x = (MAX_X - w) / 2;
        constexpr int start_y = (MAX_Y - max_h) / 2;
        constexpr int item_width = w - 5 - 5 - 5;        // 右侧滚动条
        constexpr int number_of_items = h / item_height; // 每页显示的项目数
        int pageStart = 0;
        int selected = 0;
        int total = 0;
        int barHeight;
        int barPos = 0;
        bool updated = true;
        bool hasIcon = true;
        bool waitc = false;
        bool init = false;
        unsigned long wait_time = 0;

        while (options[total].title != NULL)
        {
            ++total;
        }

        char sha_option_key[total][16];
        uint8_t temp[32];
        char hex_hash[65];
        barHeight = number_of_items * h / total;

        hal.hookButton(true);
        push_buffer();

        int i = 0;
        mbedtls_sha256_context ctx;
        while (options[i].title != NULL)
        {
            sha256(options[i].title, temp, &ctx);
            for (int j = 0; j < 32; j++)
                sprintf(hex_hash + j * 2, "%02x", temp[j]);
            strncpy(sha_option_key[i], hex_hash, 15);
            sha_option_key[i][15] = '\0';
            ++i;
        }

        selected = constrain(default_selected, 0, total - 1);
        int prev_selected = selected;

        if (total > number_of_items)
            pageStart = constrain(selected - number_of_items / 2, 0, total - number_of_items);

        // ---------- 双缓冲相关变量 ----------
        const int STATIC_BUF = 1;           // 静态缓冲区索引
        bool static_valid = false;          // 静态内容是否有效
        int last_pageStart = -1;            // 上次绘制静态时的页起始
        int last_total = -1;                // 上次绘制静态时的总项数
        bool option_values_changed = false; // 选项值是否变化（需要重绘静态）
        // ---------------------------------

        wait_time = millis();
        goto init_draw;

        while (1)
        {
            if (hal.btnl.isPressing())
            {
                delay(10);
                if (hal.btnl.isPressing())
                {
                    if (selected == 0)
                        selected = total;
                    --selected;
                    if (selected < 0)
                        selected = 0;
                    updated = true;
                }
                wait_time = millis();
            }

            if (hal.btnr.isPressing())
            {
                delay(10);
                if (hal.btnr.isPressing())
                {
                    ++selected;
                    if (selected > total)
                        selected = total;
                    if (selected == total)
                        selected = 0;
                    updated = true;
                }
                wait_time = millis();
            }

            if (hal.btnc.isPressing())
            {
                delay(50);
                if (hal.btnc.isPressing())
                {
                    if (waitLongPress(PIN_BUTTONC) == true)
                    {
                        selected = 0;
                        waitc = true;
                        updated = true;
                    }
                    else
                    {
                        if (selected == 0)
                            break;
                        else
                        {
                            if (options[selected].select == false)
                                break;
                            else
                            {
                                // 切换选项值并保存到 NVS
                                bool new_val;
                                if (options[selected].key == nullptr)
                                    new_val = !hal.pref.getBool(sha_option_key[selected]);
                                else
                                    new_val = !hal.pref.getBool(options[selected].key);
                                if (options[selected].key == nullptr)
                                    hal.pref.putBool(sha_option_key[selected], new_val);
                                else
                                    hal.pref.putBool(options[selected].key, new_val);
                                updated = true;
                                option_values_changed = true; // 标记值改变，需要重绘静态
                            }
                        }
                    }
                }
                wait_time = millis();
            }

            if (updated)
            {
            init_draw:
                // 页面边界调整
                if (selected < pageStart)
                    pageStart = selected;
                else if (selected >= pageStart + number_of_items)
                    pageStart = selected - number_of_items + 1;

                // 判断是否需要重新绘制静态内容（翻页、总数变化或选项值变化）
                if (!static_valid || pageStart != last_pageStart || total != last_total || option_values_changed)
                {
                    // ----- 绘制静态内容（窗口、标题、所有项目文字、复选框图标、滚动条轨道）-----
                    drawWindowsWithTitle(title, start_x, start_y, w, max_h);
                    display.setDrawWindow(start_x, start_y + 14, w - 2, max_h - 16);

                    int max_items = min(number_of_items, total);
                    for (int i = 0; i < max_items; ++i)
                    {
                        int item_y = start_y + 15 + item_height * i;
                        int idx = i + pageStart;
                        // 复选框图标
                        if (options[idx].select == true)
                        {
                            bool option_val;
                            if (options[idx].key == nullptr)
                                option_val = hal.pref.getBool(sha_option_key[idx]);
                            else
                                option_val = hal.pref.getBool(options[idx].key);
                            if (option_val)
                                display.drawXBitmap(start_x + 5, item_y + 2, select_bits, ico_w, ico_h, 0);
                            else
                                display.drawXBitmap(start_x + 5, item_y + 2, no_select_bits, ico_w, ico_h, 0);
                        }
                        // 文字
                        int text_x = start_x + 5 + (options[idx].select ? ico_w + 2 : 0);
                        u8g2Fonts.drawUTF8(text_x, item_y + 13, options[idx].title);
                    }

                    // 滚动条轨道（背景）
                    if (total > number_of_items)
                    {
                        display.fillRoundRect(start_x + w - 5 + 1, start_y + 15, 3, h, 2, 1); // 浅色轨道
                    }

                    display.display(); // 此时显示完整静态内容（仅翻页或值变化时闪一次）

                    // 将当前显示内容复制到静态缓冲区
                    display.copyBuffer(STATIC_BUF, display.current_buffer_idx);

                    // 更新状态
                    last_pageStart = pageStart;
                    last_total = total;
                    static_valid = true;
                    option_values_changed = false;
                }

                // ----- 动画：平滑移动选框，只绘制动态部分 -----
                TickType_t xLastWakeTime = xTaskGetTickCount();
                TickType_t xFrequency = pdMS_TO_TICKS(GUI_Frequency);
                const int steps = 6;
                int start_y_rect = start_y + 15 + item_height * (prev_selected - pageStart);
                int target_y_rect = start_y + 15 + item_height * (selected - pageStart);
                for (int step = 0; step <= steps; ++step)
                {
                    int cur_y_rect = start_y_rect + ((target_y_rect - start_y_rect) * step) / steps;

                    // 从静态缓冲区恢复显示缓冲区（清除上一帧的选框和滑块）
                    display.copyBuffer(display.current_buffer_idx, STATIC_BUF);
                    display.setDrawWindow(start_x, start_y + 14, w - 2, max_h - 16);

                    // 绘制选框矩形
                    display.drawRoundRect(start_x + 3, cur_y_rect, w - 5 - 6, 15, 3, 0);
                    // 绘制滚动条滑块
                    if (total > number_of_items)
                    {
                        barPos = selected * (h - barHeight) / total;
                        display.fillRoundRect(start_x + w - 5 + 1, start_y + 15 + barPos, 3, barHeight, 2, 0);
                    }

                    display.display();
                    xTaskDelayUntil(&xLastWakeTime, xFrequency);
                }

                prev_selected = selected;
                updated = false;
                while (!init && (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing()))
                    delay(10);
                init = true;
            }

            if (waitc == true)
            {
                waitc = false;
                while (hal.btnc.isPressing())
                    delay(10);
                delay(10);
            }
            delay(10);
            if (millis() - wait_time > 30000)
            {
                hal.wait_input();
                wait_time = millis();
            }
        }

        display.setDrawWindow();
        pop_buffer();
        hal.unhookButton();
        return selected;
    }
    /**
     * @brief  全屏英文输入
     * @param name 输入框标题
     * @return char* 输入的字符串（需要调用者负责释放内存）
     */
    char *englishInput(const char *name)
    {
        // ================= 布局常量 (384×168) =================
        constexpr int WIN_W = 364;
        constexpr int WIN_H = 158;
        constexpr int start_x = (MAX_X - WIN_W) / 2;
        constexpr int start_y = (MAX_Y - WIN_H) / 2;

        constexpr int TITLE_H = 15;
        constexpr int INPUT_H = 32;
        constexpr int KEYBOARD_Y = start_y + TITLE_H + INPUT_H;
        constexpr int KEYBOARD_H = WIN_H - TITLE_H - INPUT_H;

        constexpr int KEY_COLS = 11, KEY_ROWS = 5;
        constexpr int KEY_W = WIN_W / KEY_COLS;
        constexpr int KEY_H = KEYBOARD_H / KEY_ROWS;
        constexpr int KEYBOARD_X = start_x + (WIN_W - KEY_W * KEY_COLS) / 2;

        // 字符映射表
        const char keymap_lower[KEY_ROWS][KEY_COLS] = {
            {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '<'},
            {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '='},
            {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '.', '^'},
            {'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '~', '_', '+'},
            {'!', '(', ')', ':', '\'', '"', '?', '/', '-', ' ', ' '}};
        const char keymap_upper[KEY_ROWS][KEY_COLS] = {
            {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '<'},
            {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '='},
            {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '.', '^'},
            {'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '~', '_', '+'},
            {'!', '(', ')', ':', '\'', '"', '?', '/', '-', ' ', ' '}};

        // ================= 按键事件队列 =================
        enum class KeyEvent : uint8_t
        {
            LEFT = 1,
            RIGHT,
            OK,
            LEFT_REPEAT,
            RIGHT_REPEAT // 新增长按连续移动事件
        };

        static QueueHandle_t keyQueue = nullptr;
        keyQueue = xQueueCreate(16, sizeof(KeyEvent));
        if (!keyQueue)
            return nullptr;

        // 统一按键回调（带 void* 参数）
        auto onInputEvent = [](void *param)
        {
            KeyEvent ev = static_cast<KeyEvent>(reinterpret_cast<uintptr_t>(param));
            xQueueSend(keyQueue, &ev, 0);
        };

        // ---------- 注册短按与长按重复事件 ----------
        // 左键：短按 LEFT，长按连续 LEFT_REPEAT
        hal.btnl.attachClick(onInputEvent, reinterpret_cast<void *>(KeyEvent::LEFT));
        hal.btnl.attachDuringLongPress(onInputEvent, reinterpret_cast<void *>(KeyEvent::LEFT_REPEAT));
        hal.btnl.setLongPressIntervalMs(100); // 长按重复间隔 120ms

        // 右键：短按 RIGHT，长按连续 RIGHT_REPEAT
        hal.btnr.attachClick(onInputEvent, reinterpret_cast<void *>(KeyEvent::RIGHT));
        hal.btnr.attachDuringLongPress(onInputEvent, reinterpret_cast<void *>(KeyEvent::RIGHT_REPEAT));
        hal.btnr.setLongPressIntervalMs(100);

        // 中键：仅短按 OK
        hal.btnc.attachClick(onInputEvent, reinterpret_cast<void *>(KeyEvent::OK));

        // ================= 动态状态 =================
        char *inputBuffer = (char *)heap_caps_malloc(512, MALLOC_CAP_8BIT);
        memset(inputBuffer, 0, 128);
        int cursor = 0;
        int selRow = 0, selCol = 0;
        bool uppercase = false;
        bool exitFlag = false;
        bool needRedrawKeyboard = true;

        // ================= 多缓冲区 =================
        enum
        {
            BUF_BACKGROUND = 0,
            BUF_WORK = 1,
            BUF_KEYBOARD = 2
        };

        display.copyBuffer(BUF_BACKGROUND, display.current_buffer_idx);
        display.swapBuffer(BUF_WORK);

        // ---------- 静态键盘底图绘制函数 ----------
        auto drawKeyboardBase = [&]()
        {
            display.swapBuffer(BUF_KEYBOARD);
            drawWindowsWithTitle(name, start_x, start_y, WIN_W, WIN_H);
            display.setDrawWindow(start_x, start_y + TITLE_H, WIN_W, WIN_H - TITLE_H);

            for (int r = 0; r < KEY_ROWS; ++r)
            {
                for (int c = 0; c < KEY_COLS; ++c)
                {
                    int kx = KEYBOARD_X + c * KEY_W;
                    int ky = KEYBOARD_Y + r * KEY_H;
                    display.drawRoundRect(kx, ky, KEY_W, KEY_H, 3, 0);
                    display.fillRoundRect(kx + 1, ky + 1, KEY_W - 2, KEY_H - 2, 2, 1);

                    u8g2Fonts.setForegroundColor(0);
                    u8g2Fonts.setBackgroundColor(1);

                    int centerX = kx + KEY_W / 2;
                    int centerY = ky + KEY_H / 2 + 5;

                    if (r == 1 && c == 10)
                    { // OK
                        const char *label = "OK";
                        u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width(label) / 2, centerY);
                        u8g2Fonts.print(label);
                    }
                    else if (r == 0 && c == 10)
                    { // Del
                        const char *label = "Del";
                        u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width(label) / 2, centerY);
                        u8g2Fonts.print(label);
                    }
                    else if (r == 2 && c == 10)
                    { // Aa/aa
                        const char *label = uppercase ? "Aa" : "aa";
                        u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width(label) / 2, centerY);
                        u8g2Fonts.print(label);
                    }
                    else
                    { // 普通字符
                        char ch = uppercase ? keymap_upper[r][c] : keymap_lower[r][c];
                        u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width(String(ch).c_str()) / 2, centerY);
                        u8g2Fonts.print(ch);
                    }
                }
            }
            display.swapBuffer(BUF_WORK);
            needRedrawKeyboard = false;
        };

        drawKeyboardBase();

        // ================= 主循环 =================
        TickType_t xLastWakeTime = xTaskGetTickCount();
        const TickType_t xFrequency = pdMS_TO_TICKS(20);

        while (!exitFlag)
        {
            KeyEvent ev;
            while (xQueueReceive(keyQueue, &ev, 0) == pdTRUE)
            {
                switch (ev)
                {
                // ---- 短按 / 长按重复：统一移动逻辑 ----
                case KeyEvent::LEFT:
                case KeyEvent::LEFT_REPEAT:
                    if (selCol > 0)
                        --selCol;
                    else if (selRow > 0)
                    {
                        --selRow;
                        selCol = KEY_COLS - 1;
                    }
                    else
                    {
                        selRow = KEY_ROWS - 1;
                        selCol = KEY_COLS - 1;
                    }
                    break;

                case KeyEvent::RIGHT:
                case KeyEvent::RIGHT_REPEAT:
                    if (selCol < KEY_COLS - 1)
                        ++selCol;
                    else if (selRow < KEY_ROWS - 1)
                    {
                        ++selRow;
                        selCol = 0;
                    }
                    else
                    {
                        selRow = 0;
                        selCol = 0;
                    }
                    break;

                case KeyEvent::OK:
                {
                    int r = selRow, c = selCol;
                    if (r == 1 && c == 10)
                    {
                        exitFlag = true;
                    }
                    else if (r == 0 && c == 10)
                    {
                        if (cursor > 0)
                            inputBuffer[--cursor] = '\0';
                    }
                    else if (r == 2 && c == 10)
                    {
                        uppercase = !uppercase;
                        needRedrawKeyboard = true;
                    }
                    else
                    {
                        char ch = uppercase ? keymap_upper[r][c] : keymap_lower[r][c];
                        if (cursor < 127)
                        {
                            inputBuffer[cursor++] = ch;
                            inputBuffer[cursor] = '\0';
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }

            // 更新底图（大小写变化）
            if (needRedrawKeyboard)
            {
                drawKeyboardBase();
            }

            // 合成当前帧
            display.copyBuffer(BUF_WORK, BUF_KEYBOARD);
            display.swapBuffer(BUF_WORK);

            // 输入框与文本
            int input_y = start_y + TITLE_H + 2;
            display.fillRoundRect(start_x + 5, input_y, WIN_W - 10, INPUT_H - 4, 2, 1);
            display.drawRoundRect(start_x + 5, input_y, WIN_W - 10, INPUT_H - 4, 2, 0);
            u8g2Fonts.setForegroundColor(0);
            u8g2Fonts.setBackgroundColor(1);
            if (hal.pref.getString("system_font", "default") == "default")
                u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312_self, 209899L);
            else
                u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());
            u8g2Fonts.setCursor(start_x + 10, input_y + INPUT_H - 10);
            for (int i = 0; i < cursor; ++i)
                u8g2Fonts.print(inputBuffer[i]);
            if (millis() % 1000 < 500)
                u8g2Fonts.print('_');

            // 高亮选中键
            int hlx = KEYBOARD_X + selCol * KEY_W;
            int hly = KEYBOARD_Y + selRow * KEY_H;
            display.fillRoundRect(hlx, hly, KEY_W, KEY_H, 3, 0);
            u8g2Fonts.setForegroundColor(1);
            u8g2Fonts.setBackgroundColor(0);

            int centerX = hlx + KEY_W / 2;
            int centerY = hly + KEY_H / 2 + 5;
            if (selRow == 1 && selCol == 10)
            {
                u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width("OK") / 2, centerY);
                u8g2Fonts.print("OK");
            }
            else if (selRow == 0 && selCol == 10)
            {
                u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width("Del") / 2, centerY);
                u8g2Fonts.print("Del");
            }
            else if (selRow == 2 && selCol == 10)
            {
                const char *label = uppercase ? "Aa" : "aa";
                u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width(label) / 2, centerY);
                u8g2Fonts.print(label);
            }
            else
            {
                char ch = uppercase ? keymap_upper[selRow][selCol] : keymap_lower[selRow][selCol];
                u8g2Fonts.setCursor(centerX - u8g2Fonts.getUTF8Width(String(ch).c_str()) / 2, centerY);
                u8g2Fonts.print(ch);
            }

            display.display();
            xTaskDelayUntil(&xLastWakeTime, xFrequency);
        }

        // 清理
        hal.btnl.attachClick(nullptr);
        hal.btnl.attachDuringLongPress(nullptr);
        hal.btnr.attachClick(nullptr);
        hal.btnr.attachDuringLongPress(nullptr);
        hal.btnc.attachClick(nullptr);
        vQueueDelete(keyQueue);
        keyQueue = nullptr;

        display.swapBuffer(BUF_BACKGROUND);
        display.display();
        return inputBuffer;
    }
    /**
     * @brief 数字输入GUI
     * @param title 标题
     * @param digits 输入位数
     * @param pre_value 预设值
     * @return int 返回输入的数字
     */
    int msgbox_number(const char *title, uint16_t digits, int pre_value) // 注意digits，1表示一位，2表示两位，程序中减一
    {
        constexpr int window_w = 120;
        constexpr int window_h = 48;
        constexpr int start_x = (MAX_X - window_w) / 2;
        constexpr int start_y = (MAX_Y - window_h) / 2;
        constexpr int input_x = start_x + 5;
        constexpr int input_y = start_y + 18;
        constexpr int input_w = window_w - 10;
        constexpr int input_h = window_h - 18 - 3;
        unsigned long wait_time = 0;
        if (digits <= 0)
            return 0;
        --digits;
        if (digits > 8)
            digits = 8;
        hal.hookButton();
        push_buffer();
        int currentNumber = pre_value;
        int current_digit = digits; // 0：个位
        int current_digit_10pow = 1;
        // 计算当前位置
        if (current_digit != 0)
        {
            for (int i = 0; i < current_digit; ++i)
            {
                current_digit_10pow *= 10;
            }
        }
        bool changed = true;
        wait_time = millis();
        while (1)
        {
            if (hal.btnl.isPressing())
            {
                // 减
                if (waitLongPress(hal.btnl.pin()))
                {
                    if (current_digit == digits)
                    {
                        current_digit = 0;
                    }
                    else
                    {
                        current_digit++;
                    }
                }
                else
                {
                    currentNumber -= current_digit_10pow;
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnr.isPressing())
            {
                // 加
                if (waitLongPress(hal.btnr.pin()))
                {
                    if (current_digit == 0)
                    {
                        current_digit = digits;
                    }
                    else
                    {
                        --current_digit;
                    }
                }
                else
                {
                    currentNumber += current_digit_10pow;
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnc.isPressing())
            {
                if (waitLongPress(PIN_BUTTONC))
                {
                    currentNumber = pre_value;
                    changed = true;
                }
                else
                {
                    break;
                }
                wait_time = millis();
            }
            if (changed)
            {
                // 计算当前位置
                current_digit_10pow = 1;
                if (current_digit != 0)
                {
                    for (int i = 0; i < current_digit; ++i)
                    {
                        current_digit_10pow *= 10;
                    }
                }
                changed = false;
                display.fillRoundRect(start_x, start_y, window_w, window_h, 3, 1);
                GUI::drawWindowsWithTitle(title, start_x, start_y, window_w, window_h);
                display.drawRoundRect(input_x, input_y, input_w, input_h, 3, 0);
                display.setFont(&FreeSans9pt7b);
                display.setTextColor(0);
                display.setCursor(input_x + 4, input_y + (input_h - 12) / 2 + 12);
                int currentNumber1 = currentNumber;
                if (currentNumber1 < 0)
                {
                    display.print('-');
                    currentNumber1 = -currentNumber1;
                }
                uint8_t tmp[9];
                for (int i = 0; i <= digits; ++i)
                {
                    tmp[i] = currentNumber1 % 10;
                    currentNumber1 /= 10;
                }
                for (int i = digits; i >= 0; --i)
                {
                    if (i == current_digit)
                    {
                        display.drawFastHLine(display.getCursorX(), display.getCursorY() + 2, 10, 0);
                    }
                    display.print(tmp[i], DEC);
                }
                display.display();
            }
            delay(10);
            if (millis() - wait_time > 30000)
            {
                hal.wait_input();
                wait_time = millis();
            }
        }
        pop_buffer();
        hal.unhookButton();
        display.display();       // 全局刷新一次
        display.setDrawWindow(); // 恢复绘制窗口
        return currentNumber;
    }
    /**
     * @brief 16进制输入GUI
     * @param title 标题
     * @param digits 输入16进制位数（1-8位）
     * @param pre_value 预设值
     * @return uint32_t 返回无符号32位数值
     */
    uint32_t msgbox_hex(const char *title, uint16_t digits, uint32_t pre_value)
    {
        constexpr int window_w = 120;
        constexpr int window_h = 48;
        constexpr int start_x = (MAX_X - window_w) / 2;
        constexpr int start_y = (MAX_Y - window_h) / 2;
        constexpr int input_x = start_x + 5;
        constexpr int input_y = start_y + 18;
        constexpr int input_w = window_w - 10;
        constexpr int input_h = window_h - 18 - 3;
        unsigned long wait_time = 0;

        /* 关键修复1：位数校验逻辑 */
        if (digits == 0)
            return 0;
        digits = (digits > 8) ? 8 : digits; // 保持原始位数值

        /* 关键修复2：最大值计算 */
        const uint32_t bit_mask = (digits == 8) ? 0xFFFFFFFF : (0x1UL << (4 * digits)) - 1;

        hal.hookButton();
        push_buffer();

        /* 关键修复3：数值初始化 */
        uint32_t currentNumber = pre_value & bit_mask;
        int current_digit = digits - 1; // 改为0起始索引
        uint32_t current_digit_16pow = 1;
        for (int i = 0; i < current_digit; ++i)
            current_digit_16pow *= 16;

        bool changed = true;
        wait_time = millis();

        while (1)
        {
            /* 按钮处理优化 */
            if (hal.btnl.isPressing())
            { // 左键
                const bool long_press = waitLongPress(hal.btnl.pin());
                if (long_press)
                { // 长按切换位
                    current_digit = (current_digit == digits - 1) ? 0 : current_digit + 1;
                    // 即时更新位权值
                    current_digit_16pow = 1;
                    for (int i = 0; i < current_digit; ++i)
                        current_digit_16pow *= 16;
                }
                else
                { // 短按减
                    currentNumber = (currentNumber + bit_mask + 1 - current_digit_16pow) & bit_mask;
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnr.isPressing())
            { // 右键
                const bool long_press = waitLongPress(hal.btnr.pin());
                if (long_press)
                { // 长按切换位
                    current_digit = (current_digit == 0) ? digits - 1 : current_digit - 1;
                    // 即时更新位权值
                    current_digit_16pow = 1;
                    for (int i = 0; i < current_digit; ++i)
                        current_digit_16pow *= 16;
                }
                else
                { // 短按加
                    currentNumber = (currentNumber + current_digit_16pow) & bit_mask;
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnc.isPressing())
            { // 确认键
                if (waitLongPress(PIN_BUTTONC))
                { // 长按重置
                    currentNumber = pre_value & bit_mask;
                    changed = true;
                }
                else
                { // 短按确认
                    break;
                }
                wait_time = millis();
            }

            /* 显示逻辑优化 */
            if (changed)
            {
                // 分解数字（含前导零）
                uint8_t hex_digits[8] = {0};
                uint32_t temp = currentNumber;
                for (int i = 0; i < digits; ++i)
                {
                    hex_digits[i] = temp % 16;
                    temp /= 16;
                }

                // 绘制界面
                display.fillRoundRect(start_x, start_y, window_w, window_h, 3, 1);
                GUI::drawWindowsWithTitle(title, start_x, start_y, window_w, window_h);
                display.drawRoundRect(input_x, input_y, input_w, input_h, 3, 0);

                // 设置等宽字体
                display.setFont(&FreeMono9pt7b);
                display.setTextColor(0);
                display.setCursor(input_x + 4, input_y + (input_h - 12) / 2 + 12);

                // 显示带下划线的数字
                const char hex_table[] = "0123456789ABCDEF";
                for (int i = digits - 1; i >= 0; --i)
                {
                    if (i == current_digit)
                    {
                        display.drawFastHLine(display.getCursorX(),
                                              display.getCursorY() + 2,
                                              10, 0);
                    }
                    display.print(hex_table[hex_digits[i]]);
                }

                display.display();
                changed = false;
            }

            delay(10);
            if (millis() - wait_time > 30000)
            {
                hal.wait_input();
                wait_time = millis();
            }
        }

        pop_buffer();
        hal.unhookButton();
        display.display();       // 刷新
        display.setDrawWindow(); // 恢复绘制窗口
        return currentNumber;
    }
    /**
     * @brief int64数字输入GUI（宽窗口）
     * @param title 标题
     * @param digits 输入位数
     * @param pre_value 预设值
     * @return int64_t 返回输入的64位数字
     */
    int64_t msgbox_number64(const char *title, uint16_t digits, int64_t pre_value)
    {
        constexpr int window_w = 180; // 加宽窗口
        constexpr int window_h = 48;
        constexpr int start_x = (MAX_X - window_w) / 2;
        constexpr int start_y = (MAX_Y - window_h) / 2;
        constexpr int input_x = start_x + 5;
        constexpr int input_y = start_y + 18;
        constexpr int input_w = window_w - 10;
        constexpr int input_h = window_h - 18 - 3;
        unsigned long wait_time = 0;

        if (digits <= 0)
            return 0;
        --digits;
        if (digits > 18) // int64最大约18位
            digits = 18;

        hal.hookButton();
        push_buffer();

        int64_t currentNumber = pre_value;
        int current_digit = digits; // 0：个位
        int64_t current_digit_10pow = 1;

        // 计算当前位置
        if (current_digit != 0)
        {
            for (int i = 0; i < current_digit; ++i)
            {
                current_digit_10pow *= 10;
            }
        }

        bool changed = true;
        wait_time = millis();

        while (1)
        {
            if (hal.btnl.isPressing())
            {
                // 减
                if (waitLongPress(hal.btnl.pin()))
                {
                    if (current_digit == digits)
                    {
                        current_digit = 0;
                    }
                    else
                    {
                        current_digit++;
                    }
                }
                else
                {
                    currentNumber -= current_digit_10pow;
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnr.isPressing())
            {
                // 加
                if (waitLongPress(hal.btnr.pin()))
                {
                    if (current_digit == 0)
                    {
                        current_digit = digits;
                    }
                    else
                    {
                        --current_digit;
                    }
                }
                else
                {
                    currentNumber += current_digit_10pow;
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnc.isPressing())
            {
                if (waitLongPress(PIN_BUTTONC))
                {
                    currentNumber = pre_value;
                    changed = true;
                }
                else
                {
                    break;
                }
                wait_time = millis();
            }

            if (changed)
            {
                // 重新计算当前位置的权值
                current_digit_10pow = 1;
                if (current_digit != 0)
                {
                    for (int i = 0; i < current_digit; ++i)
                    {
                        current_digit_10pow *= 10;
                    }
                }
                changed = false;

                // 绘制界面
                display.fillRoundRect(start_x, start_y, window_w, window_h, 3, 1);
                GUI::drawWindowsWithTitle(title, start_x, start_y, window_w, window_h);
                display.drawRoundRect(input_x, input_y, input_w, input_h, 3, 0);

                // 显示数字
                display.setFont(&FreeMono9pt7b); // 使用等宽字体
                display.setTextColor(0);
                display.setCursor(input_x + 4, input_y + (input_h - 12) / 2 + 12);

                int64_t currentNumber1 = currentNumber;
                if (currentNumber1 < 0)
                {
                    display.print('-');
                    currentNumber1 = -currentNumber1;
                }

                uint8_t tmp[20]; // int64最大18位，多留空间
                for (int i = 0; i <= digits; ++i)
                {
                    tmp[i] = currentNumber1 % 10;
                    currentNumber1 /= 10;
                }

                for (int i = digits; i >= 0; --i)
                {
                    if (i == current_digit)
                    {
                        display.drawFastHLine(display.getCursorX(), display.getCursorY() + 2, 10, 0);
                    }
                    display.print(tmp[i], DEC);
                }

                display.display(true);
            }

            delay(10);
            if (millis() - wait_time > 30000)
            {
                hal.wait_input();
                wait_time = millis();
            }
        }
        display.setDrawWindow();
        pop_buffer();
        hal.unhookButton();
        display.display(true);
        return currentNumber;
    }
    int msgbox_time(const char *title, int pre_value)
    {
        constexpr int window_w = 120;
        constexpr int window_h = 48;
        constexpr int start_x = (MAX_X - window_w) / 2;
        constexpr int start_y = (MAX_Y - window_h) / 2;
        constexpr int input_x = start_x + 5;
        constexpr int input_y = start_y + 18;
        constexpr int input_w = window_w - 10;
        constexpr int input_h = window_h - 18 - 3;
        unsigned long wait_time = 0;
        char timeBuffer[4];
        int16_t digit_add[4] = {1, 10, 60, 600};
        hal.hookButton();
        push_buffer();
        uint8_t current_digit = 3;
        int current_value = pre_value;
        bool changed = true;
        wait_time = millis();
        while (1)
        {
            if (hal.btnl.isPressing())
            {
                // 减
                if (waitLongPress(hal.btnl.pin()))
                {
                    if (current_digit == 3)
                    {
                        current_digit = 0;
                    }
                    else
                    {
                        current_digit++;
                    }
                }
                else
                {
                    current_value -= digit_add[current_digit];
                    if (current_value <= 0)
                    {
                        current_value = 0;
                    }
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnr.isPressing())
            {
                // 加
                if (waitLongPress(hal.btnr.pin()))
                {
                    if (current_digit == 0)
                    {
                        current_digit = 3;
                    }
                    else
                    {
                        --current_digit;
                    }
                }
                else
                {
                    current_value += digit_add[current_digit];
                    if (current_value >= 24 * 60)
                    {
                        current_value = 24 * 60 - 1;
                    }
                }
                changed = true;
                wait_time = millis();
            }
            else if (hal.btnc.isPressing())
            {
                if (waitLongPress(PIN_BUTTONC))
                {
                    current_value = pre_value;
                    changed = true;
                }
                else
                {
                    break;
                }
                wait_time = millis();
            }
            if (changed)
            {
                timeBuffer[3] = (current_value / 60) / 10;
                timeBuffer[2] = (current_value / 60) % 10;
                timeBuffer[1] = (current_value % 60) / 10;
                timeBuffer[0] = (current_value % 60) % 10;
                // 计算当前位置
                changed = false;
                display.fillRoundRect(start_x, start_y, window_w, window_h, 3, 1);
                GUI::drawWindowsWithTitle(title, start_x, start_y, window_w, window_h);
                display.drawRoundRect(input_x, input_y, input_w, input_h, 3, 0);
                display.setFont(&FreeSans9pt7b);
                display.setTextColor(0);
                display.setCursor(input_x + 4, input_y + (input_h - 12) / 2 + 12);
                for (int i = 3; i >= 0; --i)
                {
                    if (i == current_digit)
                    {
                        display.drawFastHLine(display.getCursorX(), display.getCursorY() + 2, 10, 0);
                    }
                    display.print(timeBuffer[i], DEC);
                }
                display.display();
            }
            delay(10);
            if (millis() - wait_time > 30000)
            {
                hal.wait_input();
                wait_time = millis();
            }
        }
        pop_buffer();
        hal.unhookButton();
        display.display();       // 全局刷新一次
        display.setDrawWindow(); // 恢复绘制窗口
        return current_value;
    }
    // RLE 解压函数（与 Python 端 rle_compress 对应）
    // 参数：
    //   src    : 压缩数据
    //   src_len: 压缩数据长度
    //   dst    : 输出缓冲区（必须足够容纳解压后数据）
    //   dst_max: 输出缓冲区最大容量，防止溢出
    // 返回值：成功解压的字节数，失败返回 -1
    IRAM_ATTR int rle_decompress(const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_max)
    {
        uint32_t i = 0;
        uint32_t out_idx = 0;
        while (i < src_len)
        {
            uint8_t cmd = src[i++];
            if (cmd < 0x80)
            { // 字面量
                uint32_t len = cmd + 1;
                if (out_idx + len > dst_max)
                    return -1;
                memcpy(dst + out_idx, src + i, len);
                i += len;
                out_idx += len;
            }
            else
            { // 重复字节
                uint32_t len = (cmd & 0x7F) + 1;
                if (out_idx + len > dst_max)
                    return -1;
                uint8_t val = src[i++];
                memset(dst + out_idx, val, len);
                out_idx += len;
            }
        }
        return out_idx;
    }
    /**
     * @brief 播放vlbm视频
     * @param x 起始绘制x坐标
     * @param y 起始绘制y坐标
     * @param filename 完整文件路径
     * @param color 绘制颜色
     */
    void PlayLBM_V(int16_t x, int16_t y, const char *filename, uint16_t color)
    {
        FILE *fp = fopen(filename, "rb");
        if (!fp)
        {
            log_e("File %s not found!", filename);
            return;
        }

        // 读取全局文件头
        LBM_V_HEAD header;
        if (fread(&header, sizeof(LBM_V_HEAD), 1, fp) != 1)
        {
            log_e("Failed to read LBM_V_HEAD");
            fclose(fp);
            return;
        }

        uint16_t w = header.w;
        uint16_t h = header.h;
        uint8_t gray_level = header.gray;
        TickType_t xFrequency = pdMS_TO_TICKS(header.frametime);
        log_i("w=%u h=%u gray=%u frametime=%u ms", w, h, gray_level, header.frametime);

        // 当前仅支持 2 阶灰度（1 bpp）
        if (gray_level != 2)
        {
            log_e("Only gray_level=2 is supported, got %u", gray_level);
            fclose(fp);
            return;
        }

        uint8_t bits_per_pixel = 1;
        uint8_t pixels_per_byte = 8 / bits_per_pixel; // = 8
        uint16_t bytes_per_row = (w + pixels_per_byte - 1) / pixels_per_byte;
        size_t imgsize = bytes_per_row * h; // 原始 LBM 位图大小（字节）

        // 分配原始位图缓冲区（解压后的目标）
        uint8_t *img = (uint8_t *)malloc(imgsize);
        uint8_t *buf = (uint8_t *)malloc(imgsize); // 读取帧数据的临时缓冲区
        if (!img || !buf)
        {
            log_e("malloc img failed!");
            fclose(fp);
            return;
        }

        TickType_t xLastWakeTime = xTaskGetTickCount();
        uint32_t frameCount = 0;
        TickType_t startTick = xTaskGetTickCount();
        bool eof = false;

        while (!eof)
        {
            if (hal.btnl.isPressing())
            {
                delay(100);
                if (hal.btnl.isPressing())
                {
                    log_i("Playback interrupted by user after %lu frames", frameCount);
                    break;
                }
            }
            frame_HEAD fh;
            size_t bytes_read = fread(&fh, sizeof(frame_HEAD), 1, fp);
            if (bytes_read != 1)
            {
                eof = true;
                break; // 文件结束或读取错误
            }

            if (fh.signature == FRAME_MAGIC)
            {
                // 读取帧数据块
                if (fread(buf, 1, fh.data_len, fp) != fh.data_len)
                {
                    log_e("Incomplete frame data");
                    free(buf);
                    break;
                }

                bool success = false;
                if (fh.data_type == FRAME_TYPE_LBM)
                {
                    if (fh.data_len == imgsize)
                    {
                        memcpy(img, buf, imgsize);
                        success = true;
                    }
                    else
                    {
                        log_e("LBM size mismatch: %u vs %u", fh.data_len, imgsize);
                    }
                }
                else if (fh.data_type == FRAME_TYPE_LBM_RLE)
                {
                    int dec_len = rle_decompress(buf, fh.data_len, img, imgsize);
                    if (dec_len == (int)imgsize)
                        success = true;
                    else
                        log_e("RLE decompress error");
                }
                else
                {
                    log_e("Unsupported data_type %d", fh.data_type);
                }
                if (!success)
                    break;
            }
            else
            {
                // ----- 旧格式：回退指针，然后按原始帧大小读取 -----
                fseek(fp, -sizeof(frame_HEAD), SEEK_CUR); // 回到帧起始位置
                size_t read_bytes = fread(img, 1, imgsize, fp);
                if (read_bytes != imgsize)
                {
                    if (feof(fp))
                        break;
                    log_e("Incomplete old-format frame at %lu", frameCount);
                    break;
                }
            }

            // 绘制当前帧
            display.clearScreen();
            display.drawbitmap(x, y, img, w, h, color);
            display.display();

            // 帧同步延时
            xTaskDelayUntil(&xLastWakeTime, xFrequency);
            frameCount++;

            // if (frameCount % 100 == 0)
            // {
            //     long pos = ftell(fp);
            //     log_i("Played frame %lu, file pos %ld", frameCount, pos);
            // }
        }

        TickType_t endTick = xTaskGetTickCount();
        uint32_t elapsedMs = (endTick - startTick) * portTICK_PERIOD_MS;
        float elapsedSec = elapsedMs / 1000.0f;
        float avgFps = (elapsedSec > 0) ? (frameCount / elapsedSec) : 0;
        // 播放完毕或被中断
        log_i("Playback finished. Total frames: %lu, elapsed time: %.2f sec, average FPS: %.2f",
              frameCount, elapsedSec, avgFps);
        fclose(fp);
        free(img);
        free(buf);
    }
    /**
     * @brief 绘制LBM格式的灰度图像
     * @param x 图像左上角X坐标
     * @param y 图像左上角Y坐标
     * @param filename LBM文件路径
     * @param color 仅在1位图像时使用，表示前景色
     * @note 支持1位、2位、4位灰度图像
     * @note 调用此函数绘制图像后不需要再调用display.display()，函数内部会自动刷新显示
     */
    void drawLBM(int16_t x, int16_t y, const char *filename, uint16_t color)
    {
        FILE *fp = fopen(getRealPath(filename), "rb");
        if (!fp)
        {
            log_e("File %s not found!", filename);
            return;
        }
        HEADGRAY header;
        uint16_t w, h;
        uint8_t pixel_bit;
        /* fread(&w, 2, 1, fp);
        fread(&h, 2, 1, fp);
        fread(&grayLevels, 2, 1, fp); */
        fread(&header, sizeof(HEADGRAY), 1, fp);
        w = header.w;
        h = header.h;
        pixel_bit = header.gray;
        log_i("w: %d, h: %d, grayLevels(每像素位数): %d", w, h, pixel_bit);
        size_t imgsize;
        uint16_t tmp = w / (8 / pixel_bit);
        if (w % (8 / pixel_bit) != 0)
            tmp++;
        imgsize = tmp * h;
        uint8_t *img = (uint8_t *)malloc(imgsize);
        if (!img)
        {
            log_printf("malloc failed!\n");
            fclose(fp);
            return;
        }
        fread(img, 1, imgsize, fp);
        fclose(fp);
        if (pixel_bit == 1)
        {
            drawbitmap(x, y, img, w, h, color);
            display.display();
        }
        else if (pixel_bit == 2)
            drawGrayScaleImage(true, x, y, w, h, img);
        else if (pixel_bit == 4)
            drawGrayScaleImage(false, x, y, w, h, img);
        free(img);
    }
    void drawGrayScaleImage(bool is4gray, int x, int y, int w, int h, const uint8_t *bitmap)
    {
        int bytes_per_row = is4gray ? (w + 3) / 4 : (w + 1) / 2;

        if (is4gray)
        {
            // 4阶灰度处理（映射到0,5,10,15）
            const int levels[] = {15, 10, 5, 0};
            for (int l = 0; l < 4; l++)
            {
                int gray_level = levels[l];
                // display.setgray(gray_level);

                for (int j = 0; j < h; j++)
                {
                    for (int i = 0; i < w; i++)
                    {
                        // 计算字节位置和像素值
                        int byte_idx = j * bytes_per_row + (i / 4);
                        uint8_t byte = bitmap[byte_idx];
                        int shift = 6 - (i % 4) * 2;
                        uint8_t p4 = (byte >> shift) & 0x03;

                        if (p4 * 5 == gray_level)
                        {
                            display.drawPixel(x + i, y + j, gray_level == 0 ? TFT_WHITE : TFT_BLACK);
                        }
                    }
                }
                display.display(); // 刷新当前灰阶层
            }
        }
        else
        {
            // 16阶灰度处理
            int display_gray = hal.pref.getInt("dlsplay", 16);
            for (int gray_level = 15; gray_level >= 0; gray_level--)
            {
                if (display_gray == 16)
                {
                    // display.setgray(gray_level);
                    if (gray_level >= 10)
                    {
                        // display.epd2.PLL_set(0x21);
                    }
                    else if (gray_level >= 5)
                    {
                        // display.epd2.PLL_set(0x3a);
                    }
                    else
                    {
                        // display.epd2.PLL_set(0x3c);
                    }
                }
                else
                {
                    // display.setgray(display_gray);
                    // display.display();
                }
                for (int j = 0; j < h; j++)
                {
                    for (int i = 0; i < w; i++)
                    {
                        // 计算字节位置和像素值
                        int byte_idx = j * bytes_per_row + (i / 2);
                        uint8_t byte = bitmap[byte_idx];
                        uint8_t p16 = (i % 2 == 0) ? (byte >> 4) : (byte & 0x0F);

                        if (p16 == gray_level)
                        {
                            display.drawPixel(x + i, y + j, gray_level == 0 ? TFT_WHITE : TFT_BLACK);
                        }
                    }
                }
                display.display(); // 刷新当前灰阶层
            }
        }
        // display.epd2.PLL_set(hal.pref.getUInt("pllset", 0x3C));
    }
    /**
     * 由于lmage2Lcd的像素排列顺序（高位到低位）与XBM（低位到高位）的不同，所以重写了单色位图绘制函数，与Adafruit_GFX库函数的绘制函数在函数输入上（除了位图的像素排列）完全相同
     */
    void drawbitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color)
    {
        int16_t byteWidth = (w + 7) / 8;
        uint8_t b = 0;
        uint8_t bitMask = 0;

        for (int16_t j = 0; j < h; j++, y++)
        {
            for (int16_t i = 0; i < w; i++)
            {
                // 每8像素重新加载字节和初始化掩码
                if ((i & 7) == 0)
                {
                    b = pgm_read_byte(&bitmap[j * byteWidth + (i / 8)]);
                    bitMask = 0x80; // 从最高位开始
                }
                // 检查当前位并绘制像素
                if (!(b & bitMask))
                {
                    display.drawPixel(x + i, y, color);
                }
                bitMask >>= 1; // 右移处理下一位
            }
        }
    }
// 请注意，BMP位图是在屏幕物理方向的物理位置绘制的
#define input_buffer_pixels 10 // 可能会影响性能，数值越大越费动态内存
#define max_row_width 500      // 限制最大尺寸 只能为8的整数
#define max_palette_pixels 500 // 限制最大尺寸 只能为8的整数

    /**
     * @brief  BMP图片抖动显示GUI
     * @param  fs 文件系统
     * @param  filename 文件路径
     * @param  partial_update 是否局刷
     * @param  overwrite 是否为覆盖刷新
     * @param  x 显示坐标
     * @param  y 显示坐标
     * @param  with_color 颜色
     */
    void drawBMP(const char *filename, bool partial_update, bool overwrite, int16_t x, int16_t y, bool with_color)
    {
        uint8_t input_buffer[3 * input_buffer_pixels];        // 深度不超过24
        uint8_t output_row_mono_buffer[max_row_width / 8];    // 用于至少一行黑白比特的缓冲区
        uint8_t output_row_color_buffer[max_row_width / 8];   // 至少一行颜色位的缓冲区
        uint8_t mono_palette_buffer[max_palette_pixels / 8];  // 调色板缓冲区深度<=8黑白
        uint8_t color_palette_buffer[max_palette_pixels / 8]; // 调色板缓冲区深度<=8 c/w
        uint16_t rgb_palette_buffer[max_palette_pixels];      // 对于缓冲图形，调色板缓冲区的深度<=8，需要7色显示

        File file;          // 创建文件对象file
        bool valid = false; // 要处理的有效格式
        bool flip = true;   // 位图自下而上存储
        // uint32_t startTime = millis();
        // if ((x >= display.width()) || (y >= display.height())) return;
        file = hal.open(filename, "r");
        if (!file)
        {
            msgbox("文件不存在", filename);
            log_e("文件 %s 不存在", filename);
            return;
        }
        // 解析BMP标头
        if (read16(file) == 0x4D42) // BMP签名
        {
            uint32_t fileSize = read32(file);     // 文件大小
            uint32_t creatorBytes = read32(file); // 创建者字节
            uint32_t imageOffset = read32(file);  // 图像数据的开始
            uint32_t headerSize = read32(file);   // 标题大小
            int32_t width = read32(file);         // 图像宽度
            int32_t height = read32(file);        // 图像高度
            uint16_t planes = read16(file);       // 平面
            uint16_t depth = read16(file);        // 每像素位数
            uint32_t format = read32(file);       // 格式

            log_printf("width0: %d\n", width);
            log_printf("height0: %d\n", height);

            // 检测图片大小 设置方向
            /*if (width <= display.width() && height > display.height())
                display.setRotation(0);
            else display.setRotation(3);*/

            if (width > max_row_width)
            {
                msgbox("错误", "图片width过大，应小于等于500");
                log_printf("错误：图片width过大，应小于等于500\n");
                return;
            }
            else if (height > max_row_width)
            {
                msgbox("错误", "图片height过大，应小于等于500");
                log_printf("错误：图片height过大，应小于等于500\n");
                return;
            }

            // 数组指针的内存分配
            uint8_t (*bmp8)[6];
            bmp8 = new uint8_t[width][6];

            boolean ddxhFirst = 1; // 抖动循环的首次状态
            uint16_t yrow1 = 0;    // Y轴移位
            uint16_t yrow_old = 0; // 绘制像素点时 初始Y轴存储
            // log_print("depth:"); log_println(depth);
            if (depth >= 32)
            {
                msgbox("错误", "不支持32位深度的图片");
                log_printf("不支持32位深度的图片\n");
                return;
            }
            if ((planes == 1) && ((format == 0) || (format == 3))) // 处理未压缩，565同样
            {
                // BMP行填充为4字节边界（如果需要）
                uint32_t rowSize = (width * depth / 8 + 3) & ~3;
                if (depth < 8)
                    rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
                if (height < 0)
                {
                    height = -height;
                    flip = false;
                }
                uint16_t w = width;
                uint16_t h = height;
                if ((x + w - 1) >= display.width())
                    w = display.width() - x;
                if ((y + h - 1) >= display.height())
                    h = display.height() - y;
                if (w <= max_row_width) // 直接绘图处理
                {
                    valid = true;
                    uint8_t bitmask = 0xFF;
                    uint8_t bitshift = 8 - depth;
                    uint16_t red, green, blue;
                    bool whitish, colored;
                    if (depth == 1)
                        with_color = false;
                    if (depth <= 8) // 8位颜色及以下使用调色板,如不使用有些图会翻转颜色
                    {
                        log_printf("depth: %d\n", depth);
                        if (depth < 8)
                            bitmask >>= depth;
                        // file.seek(54); //调色板始终 @ 54
                        file.seek(imageOffset - (4 << depth)); // 54表示常规，diff表示颜色重要
                        for (uint16_t pn = 0; pn < (1 << depth); pn++)
                        {
                            blue = file.read();
                            green = file.read();
                            red = file.read();
                            file.read();
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));                                                  // 红色还是黄色？
                            if (0 == pn % 8)
                                mono_palette_buffer[pn / 8] = 0;
                            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                            if (0 == pn % 8)
                                color_palette_buffer[pn / 8] = 0;
                            color_palette_buffer[pn / 8] |= colored << pn % 8;
                            rgb_palette_buffer[pn] = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                        }
                    }
                    // display.init(0, 0, 10, 1);
                    if (partial_update)
                        // display.setPartialWindow(x, y, w, h);
                        // else
                        // display.setFullWindow();

                        // display.firstPage();
                        do
                        {
                            if (overwrite)
                                display.fillScreen(TFT_WHITE);
                            uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                            for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // 对于每条线
                            {
                                uint32_t in_remain = rowSize;
                                uint32_t in_idx = 0;
                                uint32_t in_bytes = 0;
                                uint8_t in_byte = 0; // for depth <= 8
                                uint8_t in_bits = 0; // for depth <= 8
                                int16_t color = TFT_WHITE;
                                file.seek(rowPosition);
                                for (uint16_t col = 0; col < w; col++) // 对于每个像素 //width 修补 w
                                {
                                    // 是时候读取更多像素数据了？
                                    if (in_idx >= in_bytes) // 好的，24位也完全匹配（大小是3的倍数）
                                    {
                                        in_bytes = file.read(input_buffer, in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain);
                                        in_remain -= in_bytes;
                                        in_idx = 0;
                                    }
                                    switch (depth) // 深度 //gray = (0.114*Blue+0.587*Green+0.299*Red)
                                    {
                                    case 24:
                                        blue = input_buffer[in_idx++];  // 蓝
                                        green = input_buffer[in_idx++]; // 绿
                                        red = input_buffer[in_idx++];   // 红
                                        // whitish = 发白的
                                        // whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80);
                                        // colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));                // 红色还是黄色？ colored = 有色的
                                        // color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3); // color = 颜色
                                        // color = 0.114 * float(blue) + 0.587 * float(green) + 0.299 * float(red); //灰度转换
                                        color = (114 * blue + 587 * green + 299 * red + 500) / 1000; // 灰度转换
                                        break;
                                    case 16:
                                    {
                                        uint8_t lsb = input_buffer[in_idx++];
                                        uint8_t msb = input_buffer[in_idx++];
                                        if (format == 0) // 555
                                        {
                                            blue = (lsb & 0x1F) << 3;
                                            green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                            red = (msb & 0x7C) << 1;
                                            // color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                                            color = (114 * blue + 587 * green + 299 * red + 500) / 1000; // 灰度转换
                                        }
                                        else // 565
                                        {
                                            blue = (lsb & 0x1F) << 3;
                                            green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                            red = (msb & 0xF8);
                                            // color = (msb << 8) | lsb;
                                            color = (114 * blue + 587 * green + 299 * red + 500) / 1000; // 灰度转换
                                        }
                                        // whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                                        // colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // 微红或微黄?
                                    }
                                    break;
                                    case 1:
                                    case 4:
                                    {
                                        if (0 == in_bits)
                                        {
                                            in_byte = input_buffer[in_idx++];
                                            in_bits = 8;
                                        }
                                        uint16_t pn = (in_byte >> bitshift) & bitmask;
                                        whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                                        colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                                        in_byte <<= depth;
                                        in_bits -= depth;
                                        color = rgb_palette_buffer[pn];
                                    }
                                    break;
                                    case 8:
                                        color = input_buffer[in_idx++];
                                        break;
                                    }

                                    uint16_t yrow = y + (flip ? h - row - 1 : row);
                                    // log_print("x + col:" + String(x + col)); log_println(" yrow:" + String(yrow));
                                    if (depth == 1) // 位深为1位，直接绘制
                                    {
                                        if (whitish)
                                            color = TFT_WHITE;
                                        // else if (colored && with_color) color = GxEPD_COLORED;
                                        else
                                            color = TFT_BLACK;
                                        display.drawPixel(x + col, yrow, color); // 原始
                                    }
                                    else // 位深为24，16，8位 使用像素抖动绘制
                                    {
                                        // 亮度对比度调节
                                        /*float B = 0.1;
                                          float C = -0.1;
                                          float K = tan((45 + 44 * C) / 180 * PI);
                                          color = (color - 127.5 * (1 - B)) * K + 127.5 * (1 + B);
                                          if (color > 255) color = 255;
                                          else if (color < 0) color = 0;*/
                                        // 分段抖动，每3行抖动一次
                                        bmp8[x + col][yrow1] = color;
                                        if (x + col == (w - 1)) // X轴填满，换行 //width 修补 w
                                        {
                                            yrow1++; // Y轴进位
                                            // 首次需要存入6行数据再抖动 ，中间每次在012后面存入3行
                                            if (yrow1 == 6 || (flip == 1 && yrow == 0) || (flip == 0 && yrow == (height - 1)))
                                            {
                                                int err;
                                                uint8_t y_max0 = 4; // 首次抖动0-4行 其余抖动1-4行
                                                // 到最后时将剩余行都一起抖动
                                                if (flip == 1 && yrow == 0)
                                                    y_max0 = yrow1;
                                                else if (flip == 0 && yrow == (height - 1))
                                                    y_max0 = yrow1;

                                                // log_print("y_max0："); log_println(y_max0);
                                                yrow1 = 2; // Y轴进位回到第3行，012

                                                for (uint16_t y = 0; y <= y_max0; y++) // height width
                                                {
                                                    for (uint16_t x = 0; x < w; x++) // width 修补 w
                                                    {
                                                        if (ddxhFirst == 1 || y != 0) // 第一次对01234行抖动处理 后面至抖动1234行
                                                        {
                                                            if (bmp8[x][y] > 127)
                                                            {
                                                                err = bmp8[x][y] - 255;
                                                                bmp8[x][y] = 255;
                                                            }
                                                            else
                                                            {
                                                                err = bmp8[x][y] - 0;
                                                                bmp8[x][y] = 0;
                                                            }
                                                            if (x != w - 1)
                                                                bmp8[x + 1][y + 0] = colorThresholdLimit(bmp8[x + 1][y + 0], (err * 7) / 16);
                                                            if (x != 0)
                                                                bmp8[x - 1][y + 1] = colorThresholdLimit(bmp8[x - 1][y + 1], (err * 3) / 16);
                                                            if (1)
                                                                bmp8[x + 0][y + 1] = colorThresholdLimit(bmp8[x + 0][y + 1], (err * 5) / 16);
                                                            if (x != w - 1)
                                                                bmp8[x + 1][y + 1] = colorThresholdLimit(bmp8[x + 1][y + 1], (err * 1) / 16);
                                                        }
                                                    }
                                                    ddxhFirst = 0; // 首行结束
                                                } // 像素抖动结束

                                                // 绘制像素点 bmp[x][y] x轴绘制需全部完 y轴只绘制前5行
                                                // bmp图片Y轴绘制初始位置
                                                if (flip == 1 && yrow != 0)
                                                    yrow_old = yrow + 5;
                                                else if (flip == 0 && yrow != (height - 1))
                                                    yrow_old = yrow - 5; // bmp图片Y轴绘制初始位置
                                                uint8_t y_max1 = 5;      // 平时绘制5行
                                                // 到最后时全部绘制完
                                                if (flip == 1 && yrow == 0)
                                                    y_max1 = yrow_old + 1;
                                                else if (flip == 0 && yrow == (height - 1))
                                                    y_max1 = height - yrow_old;
                                                // log_print("yrow:"); log_println(yrow);
                                                for (uint16_t y = 0; y < y_max1; y++)
                                                {
                                                    for (uint16_t x = 0; x < w; x++) // width 修补 w
                                                    {
                                                        /*log_print("x:" + String(x));
                                                          log_print(" y:" + String(y));
                                                          log_println(" bmp8:" + String(bmp8[x][y]));*/
                                                        /*if (yrow_old > 110) {
                                                          log_print("yrow_old:"); log_println(yrow_old);
                                                          }*/
                                                        display.drawPixel(x, yrow_old, bmp8[x][y]);
                                                    }
                                                    // Y轴进位
                                                    if (flip == 1 && yrow_old != 0)
                                                        yrow_old--;
                                                    else if (flip == 0 && yrow_old != (height - 1))
                                                        yrow_old++;
                                                }
                                                // bmp8 4、5行移到开头
                                                for (uint16_t x = 0; x < w; x++) // width 修补 w
                                                {
                                                    bmp8[x][0] = bmp8[x][4];
                                                    bmp8[x][1] = bmp8[x][5];
                                                }
                                            } // 像素抖动6行数据处理结束
                                        } // 像素抖动换行结束
                                    }
                                } // end pixel
                            } // end line
                            delete[] bmp8; // 释放内存
                        } while (false);
                    display.display();
                    // display.powerOff(); // 为仅关闭电源
                    log_printf("图像显示完毕");
                }
            }
        }
        file.close();
        if (!valid)
        {
            msgbox("警告", "发生未知错误");
            log_printf("发生未知错误\n");
            return;
        }
    }

    uint16_t jpgWidth, jpgHeight;      // 记录当前JPG的宽高
    uint8_t (*bmp8)[16 + 1];           // 创建像素抖动缓存二维数组（先不定长度），JPG最大的输出区块16+1行缓存（缓存上一次的最后一行）
    uint16_t blockCount_x = 0;         // x轴区块计数
    boolean FirstLineJitterStatus = 1; // 首行抖动状态 1-可以抖动 0-已抖动过
    boolean getXYstate = 1;            // 获取绘制像素点XY像素初始坐标

    void drawJPG(String name, FS fs)
    {
        // 数值初始化
        FirstLineJitterStatus = 1; // 第一行抖动状态
        getXYstate = 1;            // 获取绘制像素点XY像素初始坐标
        blockCount_x = 0;          // X轴区块计数
        jpgWidth = 0;
        jpgHeight = 0;

        // 获取jpeg的宽度和高度（以像素为单位）
        TJpgDec.getFsJpgSize(&jpgWidth, &jpgHeight, name, fs);
        log_printf("jpgWidth = %d\n", jpgWidth);
        log_printf("jpgHeight = %d\n", jpgHeight);

        // 设置屏幕方向
        // display.setRotation(ScreenOrientation); // 用户方向
        // if (jpgWidth != jpgHeight) display.setRotation(jpgWidth > jpgHeight ? 3 : 0);

        // 设置缩放 1-2-4-8
        uint16_t scale = 1;
        for (scale = 1; scale <= 8; scale <<= 1)
        {
            if (jpgWidth <= display.width() * scale && jpgHeight <= display.height() * scale)
            {
                if (scale > 1)
                {
                    scale = scale >> 1; // 屏幕太小，缩得比屏幕小就看不清了，回到上一个缩放
                }
                break;
            }
        }
        if (scale > 8)
            scale = 8; // 至多8倍缩放
        TJpgDec.setJpgScale(scale);

        log_printf("scale: %d\n", scale);

        // 重新计算缩放后的长宽
        jpgWidth = jpgWidth / scale;
        jpgHeight = jpgHeight / scale;

        // 创建指定长度的二维数值，必须在drawFsJpg之前,getFsJpgSize和重新计算缩放后的长宽之后
        bmp8 = new uint8_t[jpgWidth][16 + 1];

        // 自动居中
        int32_t x_center = (display.width() / 2) - (jpgWidth / 2);
        int32_t y_center = (display.height() / 2) - (jpgHeight / 2);
        log_printf("x_center: %d\n", x_center);
        log_printf("y_center: %d\n", y_center);

        // display.init(0, 0, 10, 1);
        // // display.setFullWindow();
        // display.firstPage();
        do
        {
            uint8_t error;
            error = TJpgDec.drawFsJpg(x_center, y_center, name, fs); // 发送文件和坐标
            String str = "";
            if (error == 1)
                str = "被输出功能中断"; // Interrupted by output function
            else if (error == 2)
                str = "设备错误或输入流的错误终止"; // Device error or wrong termination of input stream
            else if (error == 3)
                str = "映像的内存池不足"; // Insufficient memory pool for the image
            else if (error == 4)
                str = "流输入缓冲区不足"; // Insufficient stream input buffer
            else if (error == 5)
                str = "参数错误"; // Parameter error
            else if (error == 6)
                str = "数据格式错误（可能是损坏的数据）"; // Data format error (may be broken data)
            else if (error == 7)
                str = "格式正确但不受支持"; // Right format but not supported
            else if (error == 8)
                str = "不支持JPEG标准"; // Not supported JPEG standard
            if (error != 0)
            {
                // display_partialLine(3, str);
                // display_partialLine(5, name);
                char buf[256];
                sprintf(buf, "文件%s\n错误原因:%s", name.c_str(), str.c_str());
                msgbox("JPG解码库错误", buf);
            }
            log_printf("error: %d %s\n", error, str.c_str());
        } while (false);

        display.display(); // 关闭屏幕电源

        delete[] bmp8; // 释放内存
    }
    int16_t x_p = 0; // 绘制像素点的x轴坐标
    int16_t y_p = 0; // 绘制像素点的y轴坐标
    int16_t x_start; // 绘制像素点的x轴坐标初始值记录
    int16_t y_start; // 绘制像素点的y轴坐标初始值记录
    bool epd_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
    {
        // log_print("x:"); log_println(x);
        // log_print("y:"); log_println(y);
        // log_print("w:"); log_println(w);
        // log_print("h:"); log_println(h);
        //  log_println(" ");

        yield();
        // 绘制像素点的 x y从哪里开始
        if (getXYstate)
        {
            getXYstate = 0;
            x_start = x;
            y_start = y;
            x_p = x;
            y_p = y;
        }

        for (int16_t j = 0; j < h; j++, y++) // y轴
        {
            int16_t x1 = abs(x_start - x); // 计算bmp8的x坐标
            int16_t y1 = j;                // 计算bmp8的y坐标
            if (FirstLineJitterStatus == 0)
                y1 += 1; // 第一次之后从1行开始

            for (int16_t i = 0; i < w; i++, x1++) // x轴
            {
                uint32_t xh = j * w + i;
                uint8_t num = bitmap[xh];
                bmp8[x1][y1] = num;
            }
        }

        // y轴区块计数
        blockCount_x += w;
        //**** 区块已达到x轴边界，开始抖动和绘制图像
        if (blockCount_x >= jpgWidth)
        {
            blockCount_x = 0;

            //**** 抖动
            int err;
            uint8_t y_max; // 抖动多少行，第一次0123456 之后12345678
            if (FirstLineJitterStatus)
                y_max = h - 2; // 首次0123456
            else
                y_max = h - 1; // 非首次01234567

            // 到了最后一行吧剩余的行也一起抖动
            if (y == jpgHeight + y_start)
                y_max = h - 1;

            for (uint16_t y = 0; y <= y_max; y++) // height width
            {
                for (uint16_t x = 0; x < jpgWidth; x++)
                {
                    if (bmp8[x][y] > 127)
                    {
                        err = bmp8[x][y] - 255;
                        bmp8[x][y] = 255;
                    }
                    else
                    {
                        err = bmp8[x][y] - 0;
                        bmp8[x][y] = 0;
                    }
                    if (x != jpgWidth - 1)
                        bmp8[x + 1][y + 0] = colorThresholdLimit_jpg(bmp8[x + 1][y + 0], (err * 7) / 16);
                    if (x != 0)
                        bmp8[x - 1][y + 1] = colorThresholdLimit_jpg(bmp8[x - 1][y + 1], (err * 3) / 16);
                    if (1)
                        bmp8[x + 0][y + 1] = colorThresholdLimit_jpg(bmp8[x + 0][y + 1], (err * 5) / 16);
                    if (x != jpgWidth - 1)
                        bmp8[x + 1][y + 1] = colorThresholdLimit_jpg(bmp8[x + 1][y + 1], (err * 1) / 16);
                }
            } // 像素抖动结束

            uint16_t y_p_max; // 绘制多少行
            if (FirstLineJitterStatus)
                y_p_max = h - 2; // 首次只到6行 0123456
            else
                y_p_max = h - 1; // 8-1=7  01234567

            // 到了最后一行吧剩余的行也一起绘制
            if (y == jpgHeight + y_start)
            {
                y_p_max += (jpgHeight + y_start - 1) - (y_p + y_p_max); // 127-（119+7）
            }

            for (uint16_t y1 = 0; y1 <= y_p_max; y1++, y_p++)
            {
                x_p = x_start; // 回到初始位置
                for (uint16_t x1 = 0; x1 < jpgWidth; x1++, x_p++)
                {
                    display.drawPixel(x_p, y_p, bmp8[x1][y1]);
                }
            }

            // 倒数第1行移动到第1行
            if (FirstLineJitterStatus) // 第一次
            {
                for (uint16_t x = 0; x < jpgWidth; x++)
                    bmp8[x][0] = bmp8[x][h - 1];
            }
            else // 第一次之后
            {
                for (uint16_t x = 0; x < jpgWidth; x++)
                    bmp8[x][0] = bmp8[x][h];
            }
            FirstLineJitterStatus = 0; // 第一次抖动结束
        }
        // 返回1以解码下一个块
        return 1;
    }

} // namespace GUI