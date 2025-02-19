#include <A_Config.h>

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
        for (int16_t i = 0; i < hal.pref.getInt("lpt", 25); ++i)
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
        display.fillRoundRect(x, y, w, h, 3, 1); // 清空区域
        display.drawRoundRect(x, y, w, h, 3, 0);
        // 标题栏
        display.drawFastHLine(x, y + 14, w, 0);
        if (title)
        {
            u8g2Fonts.setBackgroundColor(1);
            u8g2Fonts.setForegroundColor(0);
            u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
            wchar = u8g2Fonts.getUTF8Width(title);
            u8g2Fonts.setCursor(x + (w - wchar) / 2, y + 12);
            u8g2Fonts.print(title);
        }
    }
    uint16_t read16(File& f)
    {
      // BMP数据存储在little endian中，与Arduino相同。
        uint16_t result;
        ((uint8_t *)&result)[0] = f.read(); // LSB 最低有效位 最右侧
        ((uint8_t *)&result)[1] = f.read(); // MSB 最高有效位 最左侧
        return result;
    }

    uint32_t read32(File& f)
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
        //Serial.print("val1_int:" + String(val1_int)); Serial.print(" val2_int:" + String(val2_int)); Serial.println(" tmp:" + String(tmp));
        if (tmp > 255) return 255;
        else if (tmp < 0) return 0;
        else return tmp;
        return 0;
    }
    //val1附近的像素，val2误差
    uint8_t colorThresholdLimit_jpg(uint8_t val1, int8_t val2) // 颜色阈值限制
    {
        int16_t val1_int = val1;
        int16_t val2_int = val2;
        int16_t tmp = val1_int + val2_int;
        if (tmp > 255) return 255;
        else if (tmp < 0) return 0;
        else return tmp;
        return 0;
    }
    ////////////////////////////////////标准对话框
    /**
     * @brief  消息显示（带确认）GUI  
     * @param title 窗口标题
     * @param msg  消息内容
     */
    void msgbox(const char *title, const char *msg)
    {
        // 160*100窗口，圆角5
        constexpr int start_x = (296 - 160) / 2;
        constexpr int start_y = (128 - 96) / 2;
        int16_t w;
        bool result = false;
        hal.hookButton();
        push_buffer();
        drawWindowsWithTitle(title, start_x, start_y, 160, 96);
        // 内容
        if (msg)
        {
            u8g2Fonts.setCursor(start_x + 2, start_y + 28);
            autoIndentDraw(msg, start_x + 160 - 2, start_x + 2);
        }
        // 按钮
        display.drawRoundRect(start_x + 85, start_y + 96 - 20, 70, 15, 3, 0);
        w = u8g2Fonts.getUTF8Width("确定");
        u8g2Fonts.setCursor(start_x + 85 + (70 - w) / 2, start_y + 96 - 20 + 12);
        u8g2Fonts.print("确定");
        display.displayWindow(start_x, start_y, 160, 96);
        unsigned long start = millis();
        while (1)
        {
            if (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing())
                break;
            if (millis() - start > 30000)
                hal.wait_input();
            delay(10);
        }
        pop_buffer();
        hal.unhookButton();
    }
    /**
     * @brief  消息显示GUI  
     * @param title 窗口标题
     * @param msg  消息内容
     * @param start_x 起始X坐标(左上角)
     * @param start_y 起始Y坐标(左上角)
     */
    void info_msgbox(const char *title, const char *msg, int start_x, int start_y)
    {
        // 160*100窗口，圆角5
        //constexpr int start_x = (296 - 160) / 2;
        //constexpr int start_y = (128 - 96) / 2;
        int16_t x = u8g2Fonts.getCursorX(), y = u8g2Fonts.getCursorY();
        push_buffer();
        drawWindowsWithTitle(title, start_x, start_y, 160, 96);
        // 内容
        if (msg)
        {
            u8g2Fonts.setCursor(start_x + 2, start_y + 28);
            autoIndentDraw(msg, start_x + 160 - 2, start_x + 2);
        }
        display.displayWindow(start_x, start_y, 160, 96);
        pop_buffer();
        u8g2Fonts.setCursor(x, y);
    }
    /**
    * @brief  选择GUI  
    * @param title 窗口标题
    * @param msg  消息内容
    * @param yes 右按钮文本
    * @param no 左按钮文本
    * @return bool  true:左键，false:右键
    */
    bool msgbox_yn(const char *title, const char *msg, const char *yes, const char *no)
    {
        // 160*100窗口，圆角5
        constexpr int start_x = (296 - 160) / 2;
        constexpr int start_y = (128 - 96) / 2;
        if (yes == NULL)
            yes = "确定 (右)";
        if (no == NULL)
            no = "取消 (左)";
        int16_t w;
        bool result = false;
        hal.hookButton();
        push_buffer();
        drawWindowsWithTitle(title, start_x, start_y, 160, 96);
        // 内容
        u8g2Fonts.setCursor(start_x + 2, start_y + 28);
        autoIndentDraw(msg, start_x + 160 - 2, start_x + 2);
        // 按钮
        display.drawRoundRect(start_x + 5, start_y + 96 - 20, 70, 15, 3, 0);
        display.drawRoundRect(start_x + 85, start_y + 96 - 20, 70, 15, 3, 0);
        w = u8g2Fonts.getUTF8Width(no);
        u8g2Fonts.setCursor(start_x + 5 + (70 - w) / 2, start_y + 96 - 20 + 12);
        u8g2Fonts.print(no);
        w = u8g2Fonts.getUTF8Width(yes);
        u8g2Fonts.setCursor(start_x + 85 + (70 - w) / 2, start_y + 96 - 20 + 12);
        u8g2Fonts.print(yes);
        display.displayWindow(start_x, start_y, 160, 96);
        unsigned long start = millis();
        while (1)
        {
            delay(10);
            if (millis() - start > 30000)
                hal.wait_input();
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
    * @return int类型的选中的菜单项
    */
    int menu(const char *title, const menu_item options[], int16_t ico_w, int16_t ico_h)
    {
        constexpr int start_x = (296 - 200) / 2;
        constexpr int start_y = (128 - 111) / 2; // 200*96
        constexpr int number_of_items = 6;
        constexpr int item_height = (96) / number_of_items; // 16
        constexpr int item_width = 200 - 5 - 5 - 5;         // 右侧滚动条
        int pageStart = 0;                                  // 屏幕顶部第一个
        int selected = 0;
        int total = 0;
        int barHeight;
        int barPos = 0;
        bool updated = true;
        bool hasIcon = false;
        bool waitc = false;
        unsigned long wait_time = 0;
        while (options[total].title != NULL)
        {
            // 统计一共多少，顺便检查是否有图标
            if (options[total].icon != NULL)
                hasIcon = true;
            ++total;
        }
        barHeight = number_of_items * 96 / total;
        hal.hookButton();
        push_buffer();
        wait_time = millis();
        while (1)
        {
            if (hal.btnl.isPressing())
            {
                delay(20);
                if (hal.btnl.isPressing())
                {
                    if (selected == 0)
                    {
                        selected = total;
                    }
                    --selected;
                    updated = true;
                }
                wait_time = millis();
            }

            if (hal.btnr.isPressing())
            {
                delay(20);
                if (hal.btnr.isPressing())
                {
                    ++selected;
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
                delay(20);
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

            if (updated == true)
            {
                updated = false;
                // 判断是否出界
                if (selected < pageStart)
                {
                    pageStart = selected;
                }
                else if (selected >= pageStart + number_of_items)
                {
                    pageStart = selected - number_of_items + 1;
                }
                // 下面渲染菜单
                drawWindowsWithTitle(title, start_x, start_y, 200, 111);
                // 项目
                int max_items = min(number_of_items, total);
                for (int i = 0; i < max_items; ++i)
                {
                    int item_y = start_y + 15 + item_height * i;
                    if (options[i + pageStart].icon != NULL && ico_h <= 14)
                    {
                        display.drawXBitmap(start_x + 5, item_y + (14 - ico_h) / 2, options[i + pageStart].icon, ico_w, ico_h, 0);
                    }
                    u8g2Fonts.drawUTF8(start_x + 5 + (hasIcon ? ico_w + 2 : 0), item_y + 13, options[i + pageStart].title);
                    if (selected == i + pageStart)
                    {
                        display.drawRoundRect(start_x + 3, item_y, 195 - 6, 15, 3, 0);
                    }
                }
                // 滚动条
                // 以Selected为基准
                if (total > number_of_items)
                {
                    barPos = selected * (96 - barHeight) / total;
                    display.fillRoundRect(start_x + 195 + 1, start_y + 15 + barPos, 3, barHeight, 2, 0);
                }
                display.displayWindow(start_x, start_y, 200, 111);
                yield();
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
                hal.wait_input();
        }
        pop_buffer();
        hal.unhookButton();
        return selected;
    }
    #include <esp32/rom/sha.h>
    static const uint8_t select_bits[] = {
        0xfe, 0x07, 0x03, 0x0c, 0x01, 0x08, 0xf1, 0x08, 0xf9, 0x09, 0xf9, 0x09,
        0xf9, 0x09, 0xf9, 0x09, 0xf1, 0x08, 0x01, 0x08, 0x03, 0x0c, 0xfe, 0x07 };
    static const uint8_t no_select_bits[] = {
        0xfe, 0x07, 0x03, 0x0c, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08,
        0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x03, 0x0c, 0xfe, 0x07 };
    
    void sha256(const char *input, uint8_t output[32], SHA_CTX *ctx) {
        ets_sha_init(ctx);  // 初始化上下文
        ets_sha_update(ctx, SHA2_256, (const uint8_t *)input, strlen(input) * 8); // 更新哈希值
        ets_sha_finish(ctx, SHA2_256, output); // 完成哈希计算
    }
    /**
    * @brief 多项设置GUI  
    * @param title 窗口标题
    * @param options 菜单选择列表与是否显示复选框(bool表示是否显示复选框)
    * @return int类型的选中的菜单项
    */
    int select_menu(const char *title, const menu_select options[])
    {
        uint8_t ico_h = 12, ico_w = 12;
        constexpr int start_x = (296 - 200) / 2;
        constexpr int start_y = (128 - 111) / 2; // 200*96
        constexpr int number_of_items = 6;
        constexpr int item_height = (96) / number_of_items; // 16
        constexpr int item_width = 200 - 5 - 5 - 5;         // 右侧滚动条
        int pageStart = 0;                                  // 屏幕顶部第一个
        int selected = 0;
        int total = 0;
        int barHeight;
        int barPos = 0;
        bool updated = true;
        bool hasIcon = true;
        bool waitc = false;
        unsigned long wait_time = 0;
        while (options[total].title != NULL)
        {
            // 统计一共多少，顺便检查是否有图标
            /* if (options[total].icon != NULL)
                hasIcon = true; */
            ++total;
        }
        char sha_option_key[total][16];
        uint8_t temp[32];
        char hex_hash[65];  // 64 字节的十六进制字符串 + 1 字节的 null 终止符
        barHeight = number_of_items * 96 / total;
        hal.hookButton();
        push_buffer();
        //通过sha256为每一个选项生成一个key，以便存储
        int i = 0;
        SHA_CTX ctx;
        ets_sha_enable();
        while (options[i].title != NULL)
        {
            sha256(options[i].title, temp, &ctx);
            // 将哈希值转换为十六进制字符串
            for (int j = 0; j < 32; j++) {
                sprintf(hex_hash + j * 2, "%02x", temp[j]);
            }
            // 截取前 15 个字符作为 key
            strncpy(sha_option_key[i], hex_hash, 15);
            sha_option_key[i][15] = '\0';  // 确保字符串以 null 结尾
            ++i;
        }
        ets_sha_disable();
        wait_time = millis();
        while (1)
        {
            if (hal.btnl.isPressing())
            {
                delay(20);
                if (hal.btnl.isPressing())
                {
                    if (selected == 0)
                    {
                        selected = total;
                    }
                    --selected;
                    updated = true;
                }
                wait_time = millis();
            }

            if (hal.btnr.isPressing())
            {
                delay(20);
                if (hal.btnr.isPressing())
                {
                    ++selected;
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
                delay(20);
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
                        if (selected == 0){
                            break;
                        }else{
                            if (options[selected].select == false)
                                break;
                            else{
                                hal.pref.putBool(sha_option_key[selected], !hal.pref.getBool(sha_option_key[selected], false));
                                updated = true;
                            }
                        }
                        
                    }
                }
                wait_time = millis();
            }

            if (updated == true)
            {
                updated = false;
                // 判断是否出界
                if (selected < pageStart)
                {
                    pageStart = selected;
                }
                else if (selected >= pageStart + number_of_items)
                {
                    pageStart = selected - number_of_items + 1;
                }
                // 下面渲染菜单
                drawWindowsWithTitle(title, start_x, start_y, 200, 111);
                // 项目
                int max_items = min(number_of_items, total);
                for (int i = 0; i < max_items; ++i)
                {
                    int item_y = start_y + 15 + item_height * i;
                    if (options[i + pageStart].select == true)
                    {
                        if (hal.pref.getBool(sha_option_key[i + pageStart])){
                            display.drawXBitmap(start_x + 5, item_y + (14 - ico_h) / 2, select_bits, ico_w, ico_h, 0);
                        }else{
                            display.drawXBitmap(start_x + 5, item_y + (14 - ico_h) / 2, no_select_bits, ico_w, ico_h, 0);
                        }
                    }
                    u8g2Fonts.drawUTF8(start_x + 5 + (options[i + pageStart].select ? ico_w + 2 : 0), item_y + 13, options[i + pageStart].title);
                    if (selected == i + pageStart)
                    {
                        display.drawRoundRect(start_x + 3, item_y, 195 - 6, 15, 3, 0);
                    }
                }
                // 滚动条
                // 以Selected为基准
                if (total > number_of_items)
                {
                    barPos = selected * (96 - barHeight) / total;
                    display.fillRoundRect(start_x + 195 + 1, start_y + 15 + barPos, 3, barHeight, 2, 0);
                }
                display.displayWindow(start_x, start_y, 200, 111);
                yield();
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
                hal.wait_input();
        }
        pop_buffer();
        hal.unhookButton();
        return selected;
    }
    const int KEY_WIDTH  = 26;
    const int KEY_HEIGHT = 17;

    const char da[5][11] = {
        {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '<'}, // '<' 表示删除键
        {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '='},
        {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '.', '^'},
        {'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '~', '_', '+'}, 
        {'!', '^', '*', '(', ')', ':', '\'','"', '?', '/', '-'} 
    };
    const char xiao[5][11] = {
        {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '<'}, // '<' 表示删除键
        {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '='},
        {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '.', 'v'},
        {'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '~', '_', '+'}, 
        {'!', '^', '*', '(', ')', ':', '\'','"', '?', '/', '-'} 
    };
     
    const int numRows = 5;  //按键行数
    const int numCols = 11; //按键列数
    bool uppercase = false; // 是否大写状态
    /**
    * @brief  键盘绘制函数  
    * @param selectedRow 选中的行
    * @param selectedCol 选中的列
    */
    void drawKeyboard(int selectedRow, int selectedCol) {
        //display.clearScreen();

        u8g2Fonts.setFont(u8g2_font_9x15_me);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        // 绘制键盘
        display.drawRect(0, 0, 296, 43, GxEPD_BLACK);

    // 绘制键盘
        for (int row = 0; row < numRows; row++) {
            for (int col = 0; col < numCols; col++) {
                int x = col * KEY_WIDTH;
                int y = 43 + (row * KEY_HEIGHT); // 键盘从文本框下方开始
            
                display.drawRect(x, y, KEY_WIDTH, KEY_HEIGHT, GxEPD_BLACK);
            
                if (row == selectedRow && col == selectedCol) {
                    display.fillRect(x + 1, y + 1, KEY_WIDTH - 2, KEY_HEIGHT - 2, GxEPD_BLACK);
                    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
                    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
                } else {
                    display.fillRect(x + 1, y + 1, KEY_WIDTH - 2, KEY_HEIGHT - 2, GxEPD_WHITE);
                    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
                    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
                }
            
                u8g2Fonts.setCursor(x + 3, y + 15); // 控制文本位置
                char key;
                if(uppercase == false)
                {key = xiao[row][col];}
                else{key = da[row][col];}
                if (key == '=') {
                    u8g2Fonts.print("OK");
                } else if (key == '|') {
                    if(uppercase == false){
                        u8g2Fonts.print("v");
                    }else{
                        u8g2Fonts.print("^");
                    }
                } else {
                    if (uppercase) {
                    key = toupper(key);}
                    u8g2Fonts.print(key);
                }
            }
        }
        display.display(true);
    }  
    /**
    * @brief  英文输入GUI  
    * @param name 显示在文本框下侧的字符串，可以是中文
    * @return 用户输入的字符
    */
    const char * englishInput(const char *name){
        char* inputBuffer = new char[256];
        unsigned long wait_time = 0;
        //sprintf(inputBuffer,"%s",name);
        display.fillRect(1, 1, 296 - 2, 43 - 2, GxEPD_WHITE);
        u8g2Fonts.drawUTF8(5,75,name);
        memset(inputBuffer, 0, sizeof(inputBuffer));

        int cursorPosition = 0 ;
        int selectedRow = 0;
        int selectedCol = 0;

        drawKeyboard(selectedRow, selectedCol);

        while (true) {
            if (hal.btnl.isPressing()) {
                if (selectedCol > 0) {
                    selectedCol--;
                } else if (selectedRow > 0) {
                    selectedRow--;
                    selectedCol = numCols - 1;
                } else {
                    selectedRow = numRows - 1;
                    selectedCol = numCols - 1;
                }
                drawKeyboard(selectedRow, selectedCol);
                wait_time = millis();
            } else if (hal.btnr.isPressing()) {
                if (selectedCol < numCols - 1) {
                    selectedCol++;
                } else if (selectedRow < numRows - 1) {
                    selectedRow++;
                    selectedCol = 0;
                } else {
                    selectedRow = 0;
                    selectedCol = 0;
                }
                drawKeyboard(selectedRow, selectedCol);
                wait_time = millis();
            } else if (hal.btnc.isPressing()) {
                // 按下中央按钮时的操作
                if (selectedRow == 0 && selectedCol == 10) { // 删除键
                    if (cursorPosition > 0) {
                        inputBuffer[--cursorPosition] = '\0';
                    }
                } else if (selectedRow == 1 && selectedCol == 10) { // 确定键
                    break;
                } else if (selectedRow == 2 && selectedCol == 10) { // 切换大小写键
                    uppercase = !uppercase;
                    drawKeyboard(selectedRow, selectedCol);
                } else {
                    char key;
                    if(uppercase == false)
                    { key = xiao[selectedRow][selectedCol];}
                    else{ key = da[selectedRow][selectedCol];}

                    inputBuffer[cursorPosition++] = key;
                    inputBuffer[cursorPosition + 1] = '\0';
                    /*if (cursorPosition >= sizeof(inputBuffer) - 1) {
                        break; // 输入缓冲区已满，退出
                    }*/
                }
                display.fillRect(1, 1, 296 - 2, 43 - 2, GxEPD_WHITE); // 清空文本框
                u8g2Fonts.setCursor(5, 15); // 在文本框中绘制文本
                u8g2Fonts.setForegroundColor(GxEPD_BLACK);
                u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
                u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
                autoIndentDraw(inputBuffer,190,5);
                u8g2Fonts.drawUTF8(5,40,name);
                display.display(true); // 更新文本框内容
                wait_time = millis();
            }
            //delay(200); // 适当延迟，避免重复输入
            delay(10);
            if (millis() - wait_time > 30000)
                hal.wait_input();
        }
        pop_buffer();
        hal.unhookButton();
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
        constexpr int start_x = (296 - window_w) / 2;
        constexpr int start_y = (128 - window_h) / 2;
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
                if (waitLongPress(PIN_BUTTONL))
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
                if (waitLongPress(PIN_BUTTONR))
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
                display.displayWindow(start_x, start_y, window_w, window_h);
            }
            delay(10);
            if (millis() - wait_time > 30000)
                hal.wait_input();
        }
        pop_buffer();
        hal.unhookButton();
        display.display(); // 全局刷新一次
        return currentNumber;
    }
    int msgbox_time(const char *title, int pre_value)
    {
        constexpr int window_w = 120;
        constexpr int window_h = 48;
        constexpr int start_x = (296 - window_w) / 2;
        constexpr int start_y = (128 - window_h) / 2;
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
                if (waitLongPress(PIN_BUTTONL))
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
                    if(current_value <= 0)
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
                if (waitLongPress(PIN_BUTTONR))
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
                    if(current_value >= 24*60)
                    {
                        current_value = 24*60 - 1;
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
                display.displayWindow(start_x, start_y, window_w, window_h);
            }
            delay(10);
            if (millis() - wait_time > 30000)
                hal.wait_input();
        }
        pop_buffer();
        hal.unhookButton();
        display.display(); // 全局刷新一次
        return current_value;
    }
    void drawLBM(int16_t x, int16_t y, const char *filename, uint16_t color)
    {
        FILE *fp = fopen(getRealPath(filename), "rb");
        if (!fp)
        {
            Serial.printf("File %s not found!\n", filename);
            return;
        }
        uint16_t w, h;
        uint8_t grayLevels;
        fread(&w, 2, 1, fp);
        fread(&h, 2, 1, fp);
        fread(&grayLevels, 1, 1, fp);
        log_i("w: %d, h: %d, grayLevels: %d\n", w, h, grayLevels);
        uint8_t pixel_bit = 0;
        switch (grayLevels)
        {
            case 1:
                pixel_bit = 1;
                break;
            case 4:
                pixel_bit = 2;
                break;
            case 16:
                pixel_bit = 4;
                break;
            default:
                Serial.printf("Unsupported gray levels: %d\n", grayLevels);
                fclose(fp);
                return;
        }
        size_t imgsize;
        uint16_t tmp = w / (8 / pixel_bit);
        if (w % (8 / pixel_bit) != 0)
            tmp++;
        imgsize = tmp * h;
        uint8_t *img = (uint8_t *)malloc(imgsize);
        if (!img)
        {
            Serial.printf("malloc failed!\n");
            fclose(fp);
            return;
        }
        fread(img, 1, imgsize, fp);
        fclose(fp);
        if (pixel_bit == 1){
            display.drawXBitmap(x, y, img, w, h, color);
            display.display();}
        else if (pixel_bit == 2) 
            drawGrayScaleImage(true, x, y, w, h, img);
        else if (pixel_bit == 4) 
            drawGrayScaleImage(false, x, y, w, h, img);
        free(img);
    }
    typedef struct
    {
        uint16_t x;
        uint8_t y;
        uint8_t grayLevel = 20;
    } pixel;
    void drawGrayScaleImage(bool is4Bit, int x, int y, int w, int h, const uint8_t *bitmap) {
        int grayLevels = is4Bit ? 4 : 16;
        int bytesPerRow = w;
        long pixelCount = 0;
        int pixel_x = 0;
        int bitmapaddress = 0;
        //pixel *grayPixels = new pixel[w * h];

        // Collect pixel positions for each gray level is4Bit ? byte >> (2 * (k + 1)) & 0x03 : (4 * (k + 1)) & 0x0F
        log_i("开始进行像素灰度分类");
        for (int a = grayLevels - 1; a > 0; a--){
            display.setgray(a);
            for (int j = 0; j < h; j++) {
                log_i("%d", j + 1);
                for (int i = 0; i < bytesPerRow / (is4Bit ? 4 : 2); i++) {
                    uint8_t byte = bitmap[bitmapaddress];
                    bitmapaddress++;
                    for (int k = 0; k < (is4Bit ? 4 : 2); k++) {
                        int grayValue = is4Bit ? ((byte >> (6 - 2 * k)) & 0x03) * (15 / 3) : ((byte >> (4 - 4 * k)) & 0x0F);
                        // grayPixels[pixelCount].x = x + pixel_x;
                        // grayPixels[pixelCount].y = y + j;
                        // grayPixels[pixelCount].grayLevel = grayValue;
                        pixel_x++;
                        pixelCount++;
                        if (grayValue == a)
                            display.drawPixel(x + pixel_x, y + j, a == 0 ? GxEPD_WHITE : GxEPD_BLACK);
                    }
                }
                pixel_x = 0;
            }
            display.display(true);
        }

        // Draw pixels from dark to light
        // uint8_t _grayLevels = grayLevels - 1;
        // log_i("开始进行灰度像素绘制");
        // for (int a = grayLevels - 1; a > 0; a--){
        //     display.setgray(a);
        //     for (int i = 0; i < pixelCount; i++){
        //         if (grayPixels[i].grayLevel == a)
        //             display.drawPixel(grayPixels[i].x, grayPixels[i].y, a == 0 ? GxEPD_WHITE : GxEPD_BLACK);
        //     }
        //     display.display(true);
        // }
        // delete[] grayPixels;
    }
    void drawgraybitmap(int16_t x, int16_t y, const char *filename, uint16_t color)
    {
        FILE *fp = fopen(getRealPath(filename), "rb");
        if (!fp)
        {
            Serial.printf("File %s not found!\n", filename);
            return;
        }
        uint16_t w, h, grayLevels;
        fread(&w, 2, 1, fp);
        fread(&h, 2, 1, fp);
        fread(&grayLevels, 1, 1, fp);
        size_t imgsize;
        uint16_t tmp = w / grayLevels == 4 ? 4 : 2;
        if (w % grayLevels == 4 ? 4 : 2 != 0)
            tmp++;
        imgsize = tmp * h;
        uint8_t *img = (uint8_t *)malloc(imgsize);
        if (!img)
        {
            Serial.printf("malloc failed!\n");
            fclose(fp);
            return;
        }
        fread(img, 1, imgsize, fp);
        fclose(fp);
        if (grayLevels == 4)
            drawGrayScaleImage(true, x, y, w, h, img);
        else if (grayLevels == 16)
            drawGrayScaleImage(false, x, y, w, h, img);
        free(img);
    }
    //请注意，BMP位图是在屏幕物理方向的物理位置绘制的
    #define input_buffer_pixels 10 // 可能会影响性能，数值越大越费动态内存
    #define max_row_width 500       // 限制最大尺寸 只能为8的整数
    #define max_palette_pixels 500  // 限制最大尺寸 只能为8的整数

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
    void drawBMP(FS *fs, const char *filename, bool partial_update, bool overwrite, int16_t x, int16_t y, bool with_color)
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
        //uint32_t startTime = millis();
        //if ((x >= display.width()) || (y >= display.height())) return;
        file = fs->open(filename, "r");
        if (!file)
        {
            msgbox("文件不存在",filename);
            Serial.print("文件不存在\n");
            Serial.println(filename);
            return;
        }
        // 解析BMP标头
        if (read16(file) == 0x4D42) // BMP签名
        {
            uint32_t fileSize = read32(file);     // 文件大小
            uint32_t creatorBytes = read32(file); // 创建者字节
            uint32_t imageOffset = read32(file);  // 图像数据的开始
            uint32_t headerSize = read32(file);   // 标题大小
            int32_t width  = read32(file);       // 图像宽度
            int32_t height = read32(file);       // 图像高度
            uint16_t planes = read16(file);  // 平面
            uint16_t depth = read16(file);   // 每像素位数
            uint32_t format = read32(file);  // 格式

            Serial.print("width0:"); Serial.println(width);
            Serial.print("height0:"); Serial.println(height);

            // 检测图片大小 设置方向
            /*if (width <= display.width() && height > display.height())
                display.setRotation(0);
            else display.setRotation(3);*/

            if (width > max_row_width)
            {
                msgbox("错误","图片width过大，应小于等于500");
                Serial.print("错误：图片width过大，应小于等于500\n");
                return;
            }
            else if (height > max_row_width)
            {
                msgbox("错误","图片height过大，应小于等于500");
                Serial.print("错误：图片height过大，应小于等于500\n");
                return;
            }

            // 数组指针的内存分配
            uint8_t (*bmp8)[6];
            bmp8 = new uint8_t[width][6];

            boolean ddxhFirst = 1;                   // 抖动循环的首次状态
            uint16_t yrow1 = 0;                      // Y轴移位
            uint16_t yrow_old = 0;                   // 绘制像素点时 初始Y轴存储
            //Serial.print("depth:"); Serial.println(depth);
            if (depth >= 32)
            {
                msgbox("错误","不支持32位深度的图片");
                Serial.print("不支持32位深度的图片\n");
                return;
            }
            if ((planes == 1) && ((format == 0) || (format == 3))) // 处理未压缩，565同样
            {
              // BMP行填充为4字节边界（如果需要）
                uint32_t rowSize = (width * depth / 8 + 3) & ~3;
                if (depth < 8) rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
                if (height < 0)
                {
                    height = -height;
                    flip = false;
                }
                uint16_t w = width;
                uint16_t h = height;
                if ((x + w - 1) >= display.width())  w = display.width()  - x;
                if ((y + h - 1) >= display.height()) h = display.height() - y;
                if (w <= max_row_width) //直接绘图处理
                {
                    valid = true;
                    uint8_t bitmask = 0xFF;
                    uint8_t bitshift = 8 - depth;
                    uint16_t red, green, blue;
                    bool whitish, colored;
                    if (depth == 1) with_color = false;
                    if (depth <= 8) //8位颜色及以下使用调色板,如不使用有些图会翻转颜色
                    {
                        Serial.print("depth:"); Serial.print(depth);
                        if (depth < 8) bitmask >>= depth;
                            //file.seek(54); //调色板始终 @ 54
                        file.seek(imageOffset - (4 << depth)); // 54表示常规，diff表示颜色重要
                        for (uint16_t pn = 0; pn < (1 << depth); pn++)
                        {
                            blue  = file.read();
                            green = file.read();
                            red   = file.read();
                            file.read();
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // 红色还是黄色？
                            if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
                            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                            if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
                            color_palette_buffer[pn / 8] |= colored << pn % 8;
                            rgb_palette_buffer[pn] = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                        }
                    }
                    //display.init(0, 0, 10, 1);
                    if (partial_update) display.setPartialWindow(x, y, w, h);
                    else display.setFullWindow();

                    display.firstPage();
                    do
                    {
                        if (overwrite) display.fillScreen(GxEPD_WHITE);
                        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // 对于每条线
                        {
                            uint32_t in_remain = rowSize;
                            uint32_t in_idx = 0;
                            uint32_t in_bytes = 0;
                            uint8_t in_byte = 0; // for depth <= 8
                            uint8_t in_bits = 0; // for depth <= 8
                            int16_t color = GxEPD_WHITE;
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
                                switch (depth) //深度 //gray = (0.114*Blue+0.587*Green+0.299*Red)
                                {
                                    case 24:
                                        blue = input_buffer[in_idx++];   // 蓝
                                        green = input_buffer[in_idx++];  // 绿
                                        red = input_buffer[in_idx++];    // 红
                                        // whitish = 发白的
                                        // whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80);
                                        // colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));                // 红色还是黄色？ colored = 有色的
                                        // color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3); // color = 颜色
                                        // color = 0.114 * float(blue) + 0.587 * float(green) + 0.299 * float(red); //灰度转换
                                        color = (114 * blue + 587 * green + 299 * red + 500) / 1000; //灰度转换
                                    break;
                                    case 16:
                                    {
                                        uint8_t lsb = input_buffer[in_idx++];
                                        uint8_t msb = input_buffer[in_idx++];
                                        if (format == 0) // 555
                                        {
                                            blue  = (lsb & 0x1F) << 3;
                                            green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                            red   = (msb & 0x7C) << 1;
                                            //color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                                            color = (114 * blue + 587 * green + 299 * red + 500) / 1000; //灰度转换
                                        }
                                        else // 565
                                        {
                                            blue  = (lsb & 0x1F) << 3;
                                            green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                            red   = (msb & 0xF8);
                                            //color = (msb << 8) | lsb;
                                            color = (114 * blue + 587 * green + 299 * red + 500) / 1000; //灰度转换
                                        }
                                        //whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                                        //colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // 微红或微黄?
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
              //Serial.print("x + col:" + String(x + col)); Serial.println(" yrow:" + String(yrow));
              if (depth == 1) // 位深为1位，直接绘制
              {
                if (whitish) color = GxEPD_WHITE;
                //else if (colored && with_color) color = GxEPD_COLORED;
                else color = GxEPD_BLACK;
                display.drawPixel(x + col, yrow, color); // 原始
              }
              else // 位深为24，16，8位 使用像素抖动绘制
              {
                //亮度对比度调节
                /*float B = 0.1;
                  float C = -0.1;
                  float K = tan((45 + 44 * C) / 180 * PI);
                  color = (color - 127.5 * (1 - B)) * K + 127.5 * (1 + B);
                  if (color > 255) color = 255;
                  else if (color < 0) color = 0;*/
                //分段抖动，每3行抖动一次
                bmp8[x + col][yrow1] = color;
                if (x + col == (w - 1)) //X轴填满，换行 //width 修补 w
                {
                  yrow1++;       // Y轴进位
                  // 首次需要存入6行数据再抖动 ，中间每次在012后面存入3行
                  if (yrow1 == 6 || (flip == 1 && yrow == 0) || (flip == 0 && yrow == (height - 1)))
                  {
                    int err;
                    uint8_t y_max0 = 4; //首次抖动0-4行 其余抖动1-4行
                    //到最后时将剩余行都一起抖动
                    if (flip == 1 && yrow == 0)                 y_max0 = yrow1;
                    else if (flip == 0 && yrow == (height - 1)) y_max0 = yrow1;

                    //Serial.print("y_max0："); Serial.println(y_max0);
                    yrow1 = 2;   // Y轴进位回到第3行，012

                    for (uint16_t y = 0; y <= y_max0; y++) // height width
                    {
                      for (uint16_t x = 0; x < w; x++) //width 修补 w
                      {
                        if (ddxhFirst == 1 || y != 0) //第一次对01234行抖动处理 后面至抖动1234行
                        {
                          if (bmp8[x][y] > 127) {
                            err = bmp8[x][y] - 255;
                            bmp8[x][y] = 255;
                          } else {
                            err = bmp8[x][y] - 0;
                            bmp8[x][y] = 0;
                          }
                          if (x != w - 1)  bmp8[x + 1][y + 0] = colorThresholdLimit(bmp8[x + 1][y + 0] , (err * 7) / 16);
                          if (x != 0)          bmp8[x - 1][y + 1] = colorThresholdLimit(bmp8[x - 1][y + 1] , (err * 3) / 16);
                          if (1)               bmp8[x + 0][y + 1] = colorThresholdLimit(bmp8[x + 0][y + 1] , (err * 5) / 16);
                          if (x != w - 1 ) bmp8[x + 1][y + 1] = colorThresholdLimit(bmp8[x + 1][y + 1] , (err * 1) / 16);
                        }
                      }
                      ddxhFirst = 0; //首行结束
                    }//像素抖动结束

                    // 绘制像素点 bmp[x][y] x轴绘制需全部完 y轴只绘制前5行
                    // bmp图片Y轴绘制初始位置
                    if (flip == 1 && yrow != 0)                 yrow_old = yrow + 5;
                    else if (flip == 0 && yrow != (height - 1)) yrow_old = yrow - 5;//bmp图片Y轴绘制初始位置
                    uint8_t y_max1 = 5; // 平时绘制5行
                    // 到最后时全部绘制完
                    if (flip == 1 && yrow == 0)                 y_max1 = yrow_old + 1;
                    else if (flip == 0 && yrow == (height - 1)) y_max1 = height - yrow_old;
                    //Serial.print("yrow:"); Serial.println(yrow);
                    for (uint16_t y = 0; y < y_max1; y++)
                    {
                      for (uint16_t x = 0; x < w; x++) //width 修补 w
                      {
                        /*Serial.print("x:" + String(x));
                          Serial.print(" y:" + String(y));
                          Serial.println(" bmp8:" + String(bmp8[x][y]));*/
                        /*if (yrow_old > 110) {
                          Serial.print("yrow_old:"); Serial.println(yrow_old);
                          }*/
                        display.drawPixel(x, yrow_old, bmp8[x][y]);
                      }
                      //Y轴进位
                      if (flip == 1 && yrow_old != 0)            yrow_old--;
                      else if (flip == 0 && yrow_old != (height - 1)) yrow_old++;
                    }
                    //bmp8 4、5行移到开头
                    for (uint16_t x = 0; x < w; x++) //width 修补 w
                    {
                      bmp8[x][0] = bmp8[x][4];
                      bmp8[x][1] = bmp8[x][5];
                    }
                  } //像素抖动6行数据处理结束
                }//像素抖动换行结束
              }
            }// end pixel
          }// end line
          delete[] bmp8; //释放内存
        } while (display.nextPage());
        display.powerOff(); //为仅关闭电源
        Serial.println("图像显示完毕");
      }
    }
  }
  file.close();
  if (!valid)
  {
    msgbox("警告","发生未知错误");
    Serial.print("发生未知错误\n");
    return;
  }
    }
    
    uint16_t jpgWidth, jpgHeight; // 记录当前JPG的宽高
    uint8_t (*bmp8)[16 + 1];      // 创建像素抖动缓存二维数组（先不定长度），JPG最大的输出区块16+1行缓存（缓存上一次的最后一行）
    uint16_t blockCount_x = 0;    // x轴区块计数
    boolean FirstLineJitterStatus = 1;  // 首行抖动状态 1-可以抖动 0-已抖动过
    boolean getXYstate = 1;             // 获取绘制像素点XY像素初始坐标

    void drawJPG(String name, FS fs)
    {
  //数值初始化
  FirstLineJitterStatus = 1;  // 第一行抖动状态
  getXYstate = 1;             // 获取绘制像素点XY像素初始坐标
  blockCount_x = 0;           // X轴区块计数
  jpgWidth = 0;
  jpgHeight = 0;

  //获取jpeg的宽度和高度（以像素为单位）
  TJpgDec.getFsJpgSize(&jpgWidth, &jpgHeight, name, fs);
  Serial.print("jpgWidth = "); Serial.print(jpgWidth); Serial.print(", jpgHeight = "); Serial.println(jpgHeight);

  //设置屏幕方向
  //display.setRotation(ScreenOrientation); // 用户方向
  //if (jpgWidth != jpgHeight) display.setRotation(jpgWidth > jpgHeight ? 3 : 0);

  //设置缩放 1-2-4-8
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
  if (scale > 8) scale = 8; //至多8倍缩放
  TJpgDec.setJpgScale(scale);

  Serial.print("scale:"); Serial.println(scale);

  // 重新计算缩放后的长宽
  jpgWidth = jpgWidth / scale;
  jpgHeight = jpgHeight / scale;

  //创建指定长度的二维数值，必须在drawFsJpg之前,getFsJpgSize和重新计算缩放后的长宽之后
  bmp8 = new uint8_t[jpgWidth][16 + 1];

  //自动居中
  int32_t x_center = (display.width() / 2) - (jpgWidth / 2);
  int32_t y_center = (display.height() / 2) - (jpgHeight / 2);
  Serial.print("x_center:"); Serial.println(x_center);
  Serial.print("y_center:"); Serial.println(y_center);

  //display.init(0, 0, 10, 1);
  //display.setFullWindow();
  display.firstPage();
  do
  {
    uint8_t error;
    error = TJpgDec.drawFsJpg(x_center, y_center, name, fs); // 发送文件和坐标
    String str = "";
    if (error == 1)      str = "被输出功能中断";                                     //Interrupted by output function 
    else if (error == 2) str = "设备错误或输入流的错误终止";                          //Device error or wrong termination of input stream 
    else if (error == 3) str = "映像的内存池不足";                                   //Insufficient memory pool for the image 
    else if (error == 4) str = "流输入缓冲区不足";                                   //Insufficient stream input buffer 
    else if (error == 5) str = "参数错误";                                          //Parameter error 
    else if (error == 6) str = "数据格式错误（可能是损坏的数据）";                    //Data format error (may be broken data) 
    else if (error == 7) str = "格式正确但不受支持";                                 //Right format but not supported 
    else if (error == 8) str = "不支持JPEG标准";                                    //Not supported JPEG standard 
    if (error != 0)
    {
      //display_partialLine(3, str);
      //display_partialLine(5, name);
      char buf[256];
      sprintf(buf, "文件%s\n错误原因:%s", name.c_str(), str.c_str());
      msgbox("JPG解码库错误", buf);
    }
    Serial.println("error:" + String(error) + " " + str);
    Serial.println("");
  } while (display.nextPage());

  display.powerOff(); //关闭屏幕电源

  delete[] bmp8;//释放内存
}
int16_t x_p = 0;                    // 绘制像素点的x轴坐标
int16_t y_p = 0;                    // 绘制像素点的y轴坐标
int16_t x_start;                    // 绘制像素点的x轴坐标初始值记录
int16_t y_start;                    // 绘制像素点的y轴坐标初始值记录
bool epd_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t* bitmap)
{
  //Serial.print("x:"); Serial.println(x);
  //Serial.print("y:"); Serial.println(y);
  //Serial.print("w:"); Serial.println(w);
  //Serial.print("h:"); Serial.println(h);
  // Serial.println(" ");

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

  for (int16_t j = 0; j < h; j++, y++) //y轴
  {
    int16_t x1 = abs(x_start - x); //计算bmp8的x坐标
    int16_t y1 = j; //计算bmp8的y坐标
    if (FirstLineJitterStatus == 0) y1 += 1; //第一次之后从1行开始

    for (int16_t i = 0; i < w; i++, x1++) //x轴
    {
      uint32_t xh = j * w + i;
      uint8_t num = bitmap[xh];
      bmp8[x1][y1] = num;
    }
  }

  //y轴区块计数
  blockCount_x += w;
  //**** 区块已达到x轴边界，开始抖动和绘制图像
  if (blockCount_x >= jpgWidth)
  {
    blockCount_x = 0;

    //**** 抖动
    int err;
    uint8_t y_max; //抖动多少行，第一次0123456 之后12345678
    if (FirstLineJitterStatus) y_max = h - 2;  //首次0123456
    else                       y_max = h - 1;  //非首次01234567

    //到了最后一行吧剩余的行也一起抖动
    if (y == jpgHeight + y_start) y_max = h - 1;

    for (uint16_t y = 0; y <= y_max; y++) // height width
    {
      for (uint16_t x = 0; x < jpgWidth; x++)
      {
        if (bmp8[x][y] > 127) {
          err = bmp8[x][y] - 255;
          bmp8[x][y] = 255;
        } else {
          err = bmp8[x][y] - 0;
          bmp8[x][y] = 0;
        }
        if (x != jpgWidth - 1)  bmp8[x + 1][y + 0] = colorThresholdLimit_jpg(bmp8[x + 1][y + 0] , (err * 7) / 16);
        if (x != 0)             bmp8[x - 1][y + 1] = colorThresholdLimit_jpg(bmp8[x - 1][y + 1] , (err * 3) / 16);
        if (1)                  bmp8[x + 0][y + 1] = colorThresholdLimit_jpg(bmp8[x + 0][y + 1] , (err * 5) / 16);
        if (x != jpgWidth - 1)  bmp8[x + 1][y + 1] = colorThresholdLimit_jpg(bmp8[x + 1][y + 1] , (err * 1) / 16);
      }
    }//像素抖动结束

    uint16_t y_p_max; // 绘制多少行
    if (FirstLineJitterStatus) y_p_max = h - 2; // 首次只到6行 0123456
    else                       y_p_max = h - 1;  // 8-1=7  01234567

    //到了最后一行吧剩余的行也一起绘制
    if (y == jpgHeight + y_start)
    {
      y_p_max += (jpgHeight + y_start - 1) - (y_p + y_p_max); //127-（119+7）
    }

    for (uint16_t y1 = 0; y1 <= y_p_max; y1++, y_p++)
    {
      x_p = x_start; //回到初始位置
      for (uint16_t x1 = 0; x1 < jpgWidth; x1++, x_p++)
      {
        display.drawPixel(x_p, y_p, bmp8[x1][y1]);
      }
    }

    //倒数第1行移动到第1行
    if (FirstLineJitterStatus) //第一次
    {
      for (uint16_t x = 0; x < jpgWidth; x++)
        bmp8[x][0] = bmp8[x][h - 1];
    }
    else //第一次之后
    {
      for (uint16_t x = 0; x < jpgWidth; x++)
        bmp8[x][0] = bmp8[x][h];
    }
    FirstLineJitterStatus = 0; //第一次抖动结束
  }
  // 返回1以解码下一个块
  return 1;
}

} // namespace GUI