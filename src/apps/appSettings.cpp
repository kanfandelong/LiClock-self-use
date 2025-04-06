#include "AppManager.h"
#include "DS3231.h"
DS3231 Srtc;
static const uint8_t settings_bits[] = {
    0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xfc, 0x03, 0x00, 0x00, 0xff, 0x03,
    0x00, 0x80, 0xc7, 0x03, 0x00, 0xc0, 0xe3, 0x01, 0x00, 0xc0, 0xf1, 0x00,
    0x00, 0xc0, 0x78, 0x70, 0x00, 0xe0, 0x38, 0x78, 0x00, 0xe0, 0x38, 0xfc,
    0x00, 0x60, 0x30, 0xfe, 0x00, 0xe0, 0xf0, 0xef, 0x00, 0xe0, 0xf0, 0xe7,
    0x00, 0xe0, 0x80, 0x63, 0x00, 0x70, 0x00, 0x70, 0x00, 0x38, 0x00, 0x38,
    0x00, 0x1c, 0x00, 0x3c, 0x00, 0x0e, 0xb8, 0x1f, 0x00, 0x07, 0xfc, 0x0f,
    0x80, 0x03, 0xfe, 0x01, 0xc0, 0x01, 0x07, 0x00, 0xe0, 0x80, 0x03, 0x00,
    0x70, 0xc0, 0x01, 0x00, 0x38, 0xe0, 0x00, 0x00, 0x1c, 0x70, 0x00, 0x00,
    0x0e, 0x38, 0x00, 0x00, 0x07, 0x1c, 0x00, 0x00, 0x27, 0x0e, 0x00, 0x00,
    0x07, 0x07, 0x00, 0x00, 0x86, 0x03, 0x00, 0x00, 0xfe, 0x01, 0x00, 0x00,
    0xfc, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00};

static const menu_item settings_menu_main[] =
    {
        {NULL, "退出"},
        {NULL, "时间设置"},
        {NULL, "闹钟设置"},
        {NULL, "网络设置"},
        {NULL, "重新扫描外设"},
        {NULL, "TF卡信息"},
        {NULL, "其它设置"},
        {NULL, "关于"},
        {NULL, NULL},
};

static const menu_select settings_menu_time[] =
    {
        {false, "返回上一级"},
        {false, "手动触发NTP"},
        {false, "时间同步间隔设置"},
        {false, "RTC线性偏移修正"},
        {false, "离线模式"},
        {false, "在复位为“power on”时自动对时"},
        {true,  "停用DS3231"},
        {false, NULL},
};
static const menu_item settings_menu_network[] =
    {
        {NULL, "返回上一级"},
        {NULL, "选择默认WiFi"},
        {NULL, "搜索周围的WIFI"},
        {NULL, "设置WiFi发射功率"},
        {NULL, "ESPTouch配网"},
        {NULL, "启动HTTP服务器"},
        {NULL, "启动文件服务器"},
        {NULL, "ESPNow设备扫描"},
        {NULL, "蓝牙扫描"},
        {NULL, "退出Bilibili账号"},
        {NULL, "分享当前配置的WiFi"},
        {NULL, "配置界面和Blockly"},
        {NULL, NULL},
};

static const menu_item settings_menu_peripheral[] =
    {
        {NULL, "返回上一级"},
        {NULL, "重新扫描外设"},
        {NULL, "AHT20"},
        {NULL, "BMP280"},
        {NULL, "SGP30"},
        {NULL, "DS3231"},
        {NULL, "SD卡"},
        {NULL, NULL},
};

static const menu_select settings_menu_other[] =
    {
        {false, "返回上一级"},
        {false, "屏幕方向"},
        {false, "天气更新间隔"},
        {false, "立即更新天气"},
        {false, "主屏幕应用选择"},
        {false, "应用管理"},
        {false, "TF加载方式"},
        {false, "恢复出厂设置"},
        {false, "格式化Littlefs"},
        {false, "电池电压校准"},
        {false, "sd_clk_freq set"},
        {false, "cpufreq set"},
        {false, "自动休眠电压"},
        {false, "长按时间"},
        {false, "DS3231设置"},
        {false, "电子书设置"},
        {false, "检查网页固件版本文件"},
        {false, "按键音设置"},
        {false, "屏幕全刷间隔"},
        {true,  "精准电量显示"},
        {false, "电量计算起点电压"},
        {false, "BQ27441初始化设置"},
        {false, "NVS备份设置"},
        {false, NULL},
};

class AppSettings : public AppBase
{
private:
    /* data */
    String toApp = "";
    bool hasToApp = false;

public:
    AppSettings()
    {
        name = "settings";
        title = "设置";
        description = "简单的设置";
        image = settings_bits;
        noDefaultEvent = true;
        peripherals_requested = PERIPHERALS_SD_BIT;
    }
    void set();
    void setup();
    void menu_time();
    void menu_alarm();
    void menu_network();
    void menu_peripheral();
    void menu_other();
    int binToDec(int bin);
    int decToBin(int dec);
    void cheak_config(char *a);
    void menu_SWQ();
    void menu_DS3231();
private:
    void show_wifi_power_set()
    {
        char buf[128];
        sprintf(buf, "已将WiFi发射功率设置为:%.2f dbm", (float)hal.pref.getUChar("wifitxpower", 78) * 0.25);
        GUI::msgbox("提示", buf);
    }
};
static AppSettings app;
void AppSettings::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void AppSettings::setup()
{
    display.clearScreen();
    GUI::drawWindowsWithTitle("设置", 0, 0, 296, 128);
    u8g2Fonts.drawUTF8(120, 75, "请稍等...");
    // 下面是设置菜单
    int res = 0;
    bool end = false;
    while (end == false && hasToApp == false)
    {
        display.display(false); // 每次进入设置全局刷新一次
        res = GUI::menu("设置", settings_menu_main);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            // 时间设置
            menu_time();
            break;
        case 2:
            // 闹钟设置
            menu_alarm();
            break;
        case 3:
            // 网络设置
            menu_network();
            break;
        case 4:
            // 重新扫描外设
            peripherals.check();
            break;
        case 5:
            //SD卡信息
            {
            display.clearScreen();
            GUI::drawWindowsWithTitle("TF卡信息",0,0,296,128);
            display.display();
            //u8g2Fonts.setCursor(5,30);
            //u8g2Fonts.printf("类型：%s",SD.cardType());
            if (hal.TF_connected){
                u8g2Fonts.setCursor(5,30);
                float cardSizeMB = (float)SD.cardSize() / 1024.0 / 1024.0;
                u8g2Fonts.printf("大小：%uBytes %.2fMB ",SD.cardSize(),cardSizeMB);
                u8g2Fonts.setCursor(5,45);
                u8g2Fonts.printf("扇区数量：%u",SD.numSectors());
                u8g2Fonts.setCursor(5,60);
                u8g2Fonts.printf("扇区大小：%u Bytes",SD.sectorSize());
                u8g2Fonts.setCursor(5,75);
                float cardSizeuse = (float)SD.usedBytes() / 1024.0 / 1024.0;
                float cardSizetotal = (float)SD.totalBytes() / 1024.0 / 1024.0;
                u8g2Fonts.printf("空间使用:%0.2f%%(%.2f/%.2f)MB", cardSizeuse * 100.0 / cardSizetotal, cardSizeuse, cardSizetotal);
                u8g2Fonts.setCursor(5,90);
                u8g2Fonts.printf("可用空间：%.2fMB", cardSizetotal - cardSizeuse);
                u8g2Fonts.setCursor(5,105);
                char tf_type[20];
                sdcard_type_t tf_type_num = SD.cardType();
                switch (tf_type_num)
                {
                    case CARD_NONE:
                        sprintf(tf_type, "此卡类型字段为空");
                        break;
                    case CARD_MMC:
                        sprintf(tf_type, "MMC卡");
                        break;
                    case CARD_SD:
                        sprintf(tf_type, "SD卡");
                        break;
                    case CARD_SDHC:
                        sprintf(tf_type, "SDHC卡");
                        break;
                    case CARD_UNKNOWN:
                        sprintf(tf_type, "未知的卡类型");
                        break;
                }
                u8g2Fonts.printf("TF卡类型:%s", tf_type);
                display.display(true);
            }else{
                GUI::info_msgbox("提示", "未插入TF卡，无法显示信息");
            }
            hal.wait_input();
            /* while (!hal.btnl.isPressing() && !hal.btnr.isPressing() && !hal.btnc.isPressing()) {
                delay(100);
            } */
            }
            break;
        case 6:
            // 其它设置
            menu_other();
            break;
        case 7:
        {
            display.clearScreen();
            GUI::drawWindowsWithTitle("关于本设备", 0, 0, 296, 128);
            u8g2Fonts.setCursor(5,30);
            u8g2Fonts.printf("设备名称:LiClock 版本:%s DS3231:%d.%d %d:%d:%d", code_version, Srtc.getMonth(), Srtc.getDate(), Srtc.getHour(), Srtc.getMinute(), Srtc.getSecond());
            u8g2Fonts.drawUTF8(5,45,"CPU:Xtensa@32-bit LX6 @0.24GHz X2+ULP");
            u8g2Fonts.setCursor(5,60);
            u8g2Fonts.printf("内存:520KB SRAM+16KB RTC SRAM   存储:%dMB",ESP.getFlashChipSize() / 1024 / 1024);
            u8g2Fonts.setCursor(5,75);
            u8g2Fonts.printf("文件系统(已用/总空间):%d%% %d/%d kB", LittleFS.usedBytes() * 100 / LittleFS.totalBytes(), LittleFS.usedBytes()/1024, LittleFS.totalBytes() / 1024);
            u8g2Fonts.setCursor(5,90);
            u8g2Fonts.printf("屏幕类型:EPD  屏幕分辨率:296X128 CPU_freq:%uMHz", getCpuFrequencyMhz());
            u8g2Fonts.setCursor(5,105);
            u8g2Fonts.printf("电池容量:1000mAh chip model:%s", ESP.getChipModel());
            u8g2Fonts.drawUTF8(5,120,"原开源程序网址:https://github.com/diylxy/LiClock");
            display.display();
            hal.wait_input();
            /* while (!hal.btnl.isPressing() && !hal.btnr.isPressing() && !hal.btnc.isPressing()) {
                delay(100);
            } */
        }
        break;
        default:
            break;
        }
    }
    if (hasToApp == true)
    {
        hasToApp = false;
        if (toApp != "")
        {
            appManager.gotoApp(toApp.c_str());
        }
        return;
    }
    appManager.goBack();
}

// 时间设置
void AppSettings::menu_time()
{
    int res = 0;
    bool end = false;
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("时间设置", settings_menu_time);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            // 手动触发NTP
            if (GUI::msgbox_yn("手动触发NTP", "将连接WiFi并同步时间") == true)
            {
                // 同步时间
                hal.autoConnectWiFi();
                NTPSync();
                GUI::msgbox("手动触发NTP", "同步完成");
            }
            break;
        case 2:
            // 时间同步间隔设置
            {
                const menu_item menu[] = {
                    {NULL, "取消"},
                    {NULL, "禁用时间同步"},
                    {NULL, "2小时"},
                    {NULL, "4小时"},
                    {NULL, "6小时"},
                    {NULL, "12小时"},
                    {NULL, "24小时"},
                    {NULL, "36小时"},
                    {NULL, "48小时"},
                    {NULL, NULL},
                };
                res = GUI::menu("时间同步间隔设置", menu);
                if (res > 0)
                {
                    // 设置时间同步间隔
                    hal.pref.putUChar(SETTINGS_PARAM_NTP_INTERVAL, res - 1);
                    GUI::msgbox("时间同步间隔设置", "设置完成");
                }
            }
            break;
        case 3:
            // RTC线性偏移修正
            toApp = "rtcoffset";
            hasToApp = true;
            end = true;
            break;
        case 4:
            // 离线模式
            if (GUI::msgbox_yn("仅时钟模式", "是否启用仅时钟模式？", "仅时钟", "天气时钟"))
            {
                // 启用
                config[PARAM_CLOCKONLY] = "1";
                hal.saveConfig();
            }
            else
            {
                config[PARAM_CLOCKONLY] = "0";
                hal.saveConfig();
            }
            break;
        case 5:
            // 离线模式
            if (GUI::msgbox_yn("时间设置", "在芯片上电复位时自动联网更新ESP32的时间", "启用", "禁用"))
            {
                // 启用
                config[autontpsync] = "1";
                hal.saveConfig();
            }
            else
            {
                config[autontpsync] = "0";
                hal.saveConfig();
            }
            break;
        default:
            break;
        }
    }
}

// 闹钟设置
void AppSettings::menu_alarm()
{
    int res = 0;
    bool end = false;
    menu_item settings_menu_alarm[] = {
        {NULL, "返回上一级"},
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
        {NULL, "闹钟铃声"},
        {NULL, NULL},
    };
    const menu_item settings_menu_alarm_sub[] = {
        {NULL, "返回"},
        {NULL, "时间"},
        {NULL, "重复周期"},
        {NULL, NULL},
    };
    const menu_item settings_menu_alarm_time[] = {
        {NULL, "返回"},
        {NULL, "关闭"},
        {NULL, "单次"},
        {NULL, "周一到周五"},
        {NULL, "周六日"},
        {NULL, "周一"},
        {NULL, "周二"},
        {NULL, "周三"},
        {NULL, "周四"},
        {NULL, "周五"},
        {NULL, "周六"},
        {NULL, "周日"},
        {NULL, "手动输入"},
        {NULL, NULL},
    };
    char alarm_buf[5][30];
    char alarm_buf_week[25];
    char bit_week[7] = {0};
    while (end == false && hasToApp == false)
    {
        // 读取闹钟设置
        for (int i = 0; i < 5; ++i)
        {
            if (alarms.alarm_table[i].enable == 0)
            {
                sprintf(alarm_buf[i], "%d：%02d:%02d，关闭", i + 1, alarms.alarm_table[i].time / 60, alarms.alarm_table[i].time % 60, alarms.getEnable(alarms.alarm_table + i).c_str());
            }
            else
            {
                sprintf(alarm_buf[i], "%d：%02d:%02d,%s", i + 1, alarms.alarm_table[i].time / 60, alarms.alarm_table[i].time % 60, alarms.getEnable(alarms.alarm_table + i).c_str());
            }
            settings_menu_alarm[i + 1].title = alarm_buf[i];
        }
        res = GUI::menu("闹钟设置", settings_menu_alarm);
        if (res == 0)
            break;
        if (res == 6)
        {
            const char *str = GUI::fileDialog("请选择闹钟铃声文件", false, "buz");
            if (str)
            {
                hal.pref.putString(SETTINGS_PARAM_ALARM_TONE, String(str));
            }
            else
            {
                if (GUI::msgbox_yn("你选择了返回", "是否使用默认铃声，或者保留之前的设置", "使用默认", "取消"))
                {
                    hal.pref.remove(SETTINGS_PARAM_ALARM_TONE);
                }
            }
        }
        int selected = res - 1;
        res = GUI::menu(alarm_buf[selected], settings_menu_alarm_sub);
        switch (res)
        {
        case 0:
            break;
        case 1:
        {
            alarms.alarm_table[selected].time = GUI::msgbox_time("请输入闹钟时间", alarms.alarm_table[selected].time);
            if (alarms.alarm_table[selected].enable == 0)
                alarms.alarm_table[selected].enable = ALARM_ENABLE_ONCE;
            break;
        }
        case 2:
        {
            int res;
            res = GUI::menu("请选择重复周期", settings_menu_alarm_time);
            enum alarm_enable_enum res_table[] = {
                ALARM_DISABLE,
                ALARM_ENABLE_ONCE,
                (enum alarm_enable_enum)0b00111110,
                (enum alarm_enable_enum)0b01000001,
                ALARM_ENABLE_MONDAY,
                ALARM_ENABLE_TUESDAY,
                ALARM_ENABLE_WEDNESDAY,
                ALARM_ENABLE_THURSDAY,
                ALARM_ENABLE_FRIDAY,
                ALARM_ENABLE_SATDAY,
                ALARM_ENABLE_SUNDAY,
            };
            switch (res)
            {
            case 0:
                break;
            case 12:
            {
                int time = binToDec(GUI::msgbox_number("六五四三二一日", 7, decToBin(alarms.alarm_table[selected].enable)));
                alarms.alarm_table[selected].enable = (enum alarm_enable_enum)(time % 256);
            }
            break;
            default:
                alarms.alarm_table[selected].enable = (enum alarm_enable_enum)(res_table[(res - 1) % 11]);
                break;
            }
        }
        break;
        default:
            break;
        }
    }
    alarms.save();
}

// 网络设置
void AppSettings::menu_network()
{
    int res = 0;
    bool end = false;
    DNSServer dnsServer;
    while (end == false && hasToApp == false)
    {
        res = GUI::menu("网络设置", settings_menu_network);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            {
                if (!LittleFS.exists(wifi_config_file)){
                    File file = LittleFS.open(wifi_config_file, "w");
                    file.print(DEFAULT_WIFI_CONFIG);
                    file.close();
                }
                // 读取JSON配置文件
                File configFile = LittleFS.open(wifi_config_file);
                if (!configFile) {
                    Serial.println("Failed to open file for reading");
                }

                StaticJsonDocument<1024> wifi_list;
                deserializeJson(wifi_list, configFile);
                configFile.close();
                JsonArray networks = wifi_list["networks"];
                int i = 0, j = 1;
                for (JsonObject network : networks) {
                    i++;
                }
                menu_item *_wifi_list = new menu_item[i + 2];
                char pass[i][32];
                _wifi_list[0].title = "返回";
                _wifi_list[0].icon = NULL;
                JsonArray wifi = wifi_list["networks"];
                for (JsonObject network : wifi) {
                    _wifi_list[j].title = network["ssid"].as<const char*>();
                    strcpy(pass[j - 1], network["pass"].as<const char*>());
                    _wifi_list[j].icon = NULL;
                    j++;
                }
                _wifi_list[i + 1].title = NULL;
                _wifi_list[i + 1].icon = NULL;
                int res = 0;
                bool end = false;
                while (end == false && hasToApp == false)
                {
                    res = GUI::menu("WiFi列表", _wifi_list);
                    if(res == 0)
                    {    
                        delete[] _wifi_list;    
                        end = true;
                        break;
                    }else{
                        config[PARAM_SSID] = _wifi_list[res].title;
                        config[PARAM_PASS] = pass[res - 1];
                        char buf[256];
                        sprintf(buf, "已将默认WiFi配置为: \nSSID: %s\nPASS: %s\n下次连接时将使用新的配置", _wifi_list[res].title, pass[res - 1]);
                        hal.saveConfig();
                        hal.loadConfig();
                        delete[] _wifi_list;
                        GUI::msgbox("WiFi设置", buf);
                        break;  
                    }     
                
                }
            }  
            break;
        case 2:
            {
            GUI::info_msgbox("WiFi搜索", "正在搜索WiFi...");
            static const uint8_t WIFI_5_bits[] = {
                0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x80, 0x02, 0x80, 0x02, 0xa0, 0x02,
                0xa0, 0x02, 0xa8, 0x02, 0xa8, 0x02, 0xaa, 0x02, 0xaa, 0x02, 0x00, 0x00 };
            static const uint8_t WIFI_4_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x00, 0xa0, 0x00,
                0xa0, 0x00, 0xa8, 0x00, 0xa8, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0x00, 0x00 };  
            static const uint8_t WIFI_3_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00,
                0x20, 0x00, 0x28, 0x00, 0x28, 0x00, 0x2a, 0x00, 0x2a, 0x00, 0x00, 0x00 };
            static const uint8_t WIFI_2_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x00, 0x00 };
            static const uint8_t WIFI_1_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00 };
            WiFi.mode(WIFI_STA);
            hal.searchWiFi();
            Serial.printf("搜索到的个数:%d",hal.numNetworks);
            char winfo[hal.numNetworks][64];
            int rssis[hal.numNetworks];
            char _ssid[hal.numNetworks][64];
            if(hal.numNetworks != 0)
            {
                for(int i = 0;i < hal.numNetworks;i++)
                {
                    String ssid = WiFi.SSID(i);
                    char ssidArray[ssid.length() + 1]; // +1 是为了包含字符串末尾的 null 字符
                    ssid.toCharArray(ssidArray, sizeof(ssidArray));
                    sprintf(winfo[i],"%s %d",ssidArray,WiFi.RSSI(i));
                    rssis[i] = WiFi.RSSI(i);
                    sprintf(_ssid[i],"%s",ssidArray);
                }
            }
            menu_item *WiFi_list = new menu_item[hal.numNetworks + 2];
            WiFi_list[0].title = "返回";
            WiFi_list[0].icon = NULL;
            for(int i = 1;i < hal.numNetworks + 1;i++)
            {
                WiFi_list[i].title = winfo[i - 1];
                if (rssis[i - 1] > -55)
                    WiFi_list[i].icon = WIFI_5_bits;
                else if (rssis[i - 1] >= -66)
                    WiFi_list[i].icon = WIFI_4_bits;
                else if (rssis[i - 1] >= -77)
                    WiFi_list[i].icon = WIFI_3_bits;
                else if (rssis[i - 1] >= -88)
                    WiFi_list[i].icon = WIFI_2_bits;
                else if (rssis[i - 1] >= -99)
                    WiFi_list[i].icon = WIFI_1_bits;
                else
                    WiFi_list[i].icon = NULL;
            }
            WiFi_list[hal.numNetworks + 1].title = NULL;
            WiFi_list[hal.numNetworks + 1].icon = NULL;
            int res = 0;
            bool end = false;
            while (end == false && hasToApp == false)
            {
                res = GUI::menu("扫描到的WIFI", WiFi_list, 12, 12);
                if(res == 0)
                {    
                    delete[] WiFi_list;    
                    end = true;
                    break;
                }else{
                    delete[] WiFi_list;
                    cheak_config(_ssid[res - 1]);
                    break;  
                }     
                
            }
            WiFi.mode(WIFI_OFF);
            }
            break;

        case 3:
            {
                static const menu_item wifi_power[] = {
                    {NULL, "返回"},
                    {NULL, "自定义"},
                    {NULL, "19.5dbm"},
                    {NULL, "19dbm"},
                    {NULL, "18.5dbm"},
                    {NULL, "17dbm"},
                    {NULL, "15dbm"},
                    {NULL, "13dbm"},
                    {NULL, "11dbm"},
                    {NULL, "8.5dbm"},
                    {NULL, "7dbm"},
                    {NULL, "5dbm"},
                    {NULL, "2dbm"},
                    {NULL, NULL}
                };
                int res = 0;
                bool end = false;
                while (end == false && hasToApp == false)
                {
                    res = GUI::menu("WiFi发射功率设置", wifi_power);
                    switch(res){
                        case 0:
                            end = true;
                            break;
                        case 1:
                            {
                                uint wifitxpower = GUI::msgbox_number("[8,84],0.25dbm", 2, hal.pref.getUChar("wifitxpower", 78));
                                hal.pref.putUChar("wifitxpower", wifitxpower);
                                show_wifi_power_set();
                            }
                            break;
                        case 2:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_19_5dBm);
                            show_wifi_power_set();
                            break;
                        case 3:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_19dBm);
                            show_wifi_power_set();
                            break;
                        case 4:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_18_5dBm);
                            show_wifi_power_set();
                            break;
                        case 5:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_17dBm);
                            show_wifi_power_set();
                            break;
                        case 6:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_15dBm);
                            show_wifi_power_set();
                            break;
                        case 7:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_13dBm);
                            show_wifi_power_set();
                            break;
                        case 9:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_11dBm);
                            show_wifi_power_set();
                            break;
                        case 10:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_8_5dBm);
                            show_wifi_power_set();
                            break;
                        case 11:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_7dBm);
                            show_wifi_power_set();
                            break;
                        case 12:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_5dBm);
                            show_wifi_power_set();
                            break;
                        case 13:
                            hal.pref.putUChar("wifitxpower", WIFI_POWER_2dBm);
                            show_wifi_power_set();
                            break;
                        default:
                            break;
                    }
                }
            }
            break;
        case 4:
            // ESPTouch配网
            hal.WiFiConfigSmartConfig();
            break;
        case 5:
            // 启动HTTP服务器
            toApp = "webserver";
            hasToApp = true;
            end = true;
            break;
        case 6:
            {
                bool wifi = hal.autoConnectWiFi(false);
                String passwd = String((esp_random() % 1000000000L) + 10000000L); // 生成随机密码
                String str = "WIFI:T:WPA2;S:WeatherClock;P:" + passwd + ";;", str1;
                display.fillScreen(GxEPD_WHITE);
                if (wifi){
                    beginWebServer();
                    str1 = WiFi.localIP().toString();
                }else{
                    hal.cheak_freq();
                    WiFi.softAP("WeatherClock", passwd.c_str());
                    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
                    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
                    beginWebServer();
                    str1 = "192.168.4.1";
                    QRCode qrcode;
                    uint8_t qrcodeData[qrcode_getBufferSize(7)];
                    qrcode_initText(&qrcode, qrcodeData, 6, 2, str.c_str());
                    Serial.println(qrcode.size);
                    for (uint8_t y = 0; y < qrcode.size; y++)
                    {
                        // Each horizontal module
                        for (uint8_t x = 0; x < qrcode.size; x++)
                        {
                            display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode, x, y) ? GxEPD_BLACK : GxEPD_WHITE);
                        }
                    }
                }
                beginFileServer();
                u8g2Fonts.setCursor(120, (128 - (13 * 2)) / 2);
                GUI::autoIndentDraw(str1.c_str(), 296, 120, 13);
                display.display();
                while(1){
                    if (hal.btnl.isPressing()){
                        if (GUI::waitLongPress(PIN_BUTTONL)){
                            while(hal.btnl.isPressing())delay(20);
                            server.end();
                            break;
                        }
                    }
                }
            }
            break;
        case 7:
            // ESPNow设备扫描
            GUI::msgbox("提示", "ESPNow设备扫描功能未实现");
            break;
        case 8:
            // 蓝牙扫描
            GUI::msgbox("提示", "蓝牙扫描功能未实现");
            break;
        case 9:
            // 退出Bilibili账号
            if (LittleFS.exists("/blCookies.txt"))
            {
                LittleFS.remove("/blCookies.txt");
                GUI::msgbox("完成", "Bilibili账户登录信息已删除");
                break;
            }
            else
            {
                GUI::msgbox("提示", "Bilibili Cookies不存在");
                break;
            }
            break;
        case 10:
            {
                String ssid = config[PARAM_SSID].as<String>();
                String pass = config[PARAM_PASS].as<String>();
                String str = "WIFI:T:WPA2;S:" + ssid + ";P:" + pass + ";;";
                display.fillScreen(GxEPD_WHITE);
                QRCode qrcode;
                uint8_t qrcodeData[qrcode_getBufferSize(7)];
                qrcode_initText(&qrcode, qrcodeData, 6, 2, str.c_str());
                Serial.println(qrcode.size);
                for (uint8_t y = 0; y < qrcode.size; y++)
                {
                    // Each horizontal module
                    for (uint8_t x = 0; x < qrcode.size; x++)
                    {
                        display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode, x, y) ? GxEPD_BLACK : GxEPD_WHITE);
                    }
                }
                u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
                u8g2Fonts.setCursor(120, (128 - (17 * 2)) / 2);
                char buf[50];
                sprintf(buf, "扫描二维码以连接本机分享的WiFi");
                GUI::autoIndentDraw(buf, 296, 120, 13);
                display.display();
                hal.wait_input();
            }
            break;
        case 11:
            {
                String str1, str2;
                bool wifi = hal.autoConnectWiFi(false);
                bool ap = false;
                if (wifi){
                    beginWebServer();
                    str1 = "http://" + WiFi.localIP().toString();
                    str2 = "http://" + WiFi.localIP().toString() + "/blockly";
                }else{
                    hal.cheak_freq();
                    String passwd = String((esp_random() % 1000000000L) + 10000000L); // 生成随机密码
                    String str = "WIFI:T:WPA2;S:WeatherClock;P:" + passwd + ";;";
                    ap = true;
                    wifi = WiFi.softAP("WeatherClock", passwd.c_str());
                    wifi = WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
                    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
                    beginWebServer();
                    str1 = "http://192.168.4.1";
                    str2 = "http://192.168.4.1/blockly";
                }
                display.fillScreen(GxEPD_WHITE);
                QRCode qrcode1, qrcode2;
                uint8_t qrcodeData[2][qrcode_getBufferSize(7)];
                qrcode_initText(&qrcode1, qrcodeData[0], 6, 2, str1.c_str());
                qrcode_initText(&qrcode2, qrcodeData[1], 6, 2, str2.c_str());
                Serial.println(qrcode1.size);
                Serial.println(qrcode2.size);
                for (uint8_t y = 0; y < qrcode1.size; y++)
                {
                    // Each horizontal module
                    for (uint8_t x = 0; x < qrcode1.size; x++)
                    {
                        display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode1, x, y) ? GxEPD_BLACK : GxEPD_WHITE);
                    }
                }
                for (uint8_t y = 0; y < qrcode2.size; y++)
                {
                    // Each horizontal module
                    for (uint8_t x = 0; x < qrcode2.size; x++)
                    {
                        display.fillRect(2 * x + 196, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode2, x, y) ? GxEPD_BLACK : GxEPD_WHITE);
                    }
                }
                u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
                char buf[2][32];
                sprintf(buf[0], "网页配置界面");
                sprintf(buf[1], "Blockly界面");
                u8g2Fonts.setCursor(120, 30);
                GUI::autoIndentDraw(buf[0], 135, 120, 12);
                u8g2Fonts.setCursor(160, 21);
                GUI::autoIndentDraw(buf[1], 167, 160, 12);
                u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
                display.display();
                while (1)
                {
                    updateWebServer();
                    if (LuaRunning)
                        continue;
                    if (hal.btnl.isPressing())
                    {
                        while(hal.btnl.isPressing())delay(20);
                        if(wifi)
                            server.end();
                        else{
                            server.end();
                            if (ap)
                                dnsServer.stop();
                        }
                        WiFi.disconnect(true);
                        break;
                    }
                }
            }
            break;
        default:
            break;
        }
    }
}

#include <nvs_flash.h>
// 其它设置
void AppSettings::menu_other()
{
    int res = 0;
    bool end = false;
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("其它设置", settings_menu_other);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            // 屏幕方向
            if (GUI::msgbox_yn("屏幕方向选择", "正常意为开关在屏幕左上，否则为右下", "正常（右）", "反转（左）"))
            {
                hal.pref.putUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3);
            }
            else
            {
                hal.pref.putUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 1);
            }
            display.display();
            GUI::msgbox("提示", "按键控制方式请修改GPIO宏定义"); // 为了节省内存并加快速度
            break;
        case 2:
            // 天气更新间隔
            {
                int res = GUI::msgbox_number("请输入天气更新间隔", 2, atoi(config[PARAM_FULLUPDATE].as<const char *>()));
                if (res < 5 || res > 40)
                {
                    res = 20;
                }
                char tmp[4];
                sprintf(tmp, "%d", res);
                config[PARAM_FULLUPDATE] = tmp;
                hal.saveConfig();
                break;
            }
        case 3:
            // 立即更新天气
            {
                GUI::info_msgbox("提示", "正在联网更新天气信息...");
                int res = weather.refresh();
                if (res == 0){
                    GUI::msgbox("更新完成", "已将天气信息保存至/littlefs/System/weather.bin");
                }else if (res == -2){
                    GUI::msgbox("发生错误", "错误原因：http错误或异常");
                }else if (res == -3){
                    GUI::msgbox("发生错误", "错误原因：彩云天气API失效");
                }
                break;
            }
        case 4:
            // 主屏幕应用选择
            {
                AppBase *tmp = appManager.appSelector(true);
                if (tmp)
                {
                    Serial.println(tmp->name);
                    if (GUI::msgbox_yn("警告", "选择不兼容的App可能会导致无法开机，是否确认？") == true)
                    {
                        if(strcmp(tmp->name, "clock") == 0)
                        {
                            config[PARAM_CLOCKONLY] = "0";
                            hal.saveConfig();
                            hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
                        }
                        else if(strcmp(tmp->name, "clockonly") == 0)
                        {
                            config[PARAM_CLOCKONLY] = "0";
                            hal.saveConfig();
                            hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
                        }
                        else
                        {
                            hal.pref.putString(SETTINGS_PARAM_HOME_APP, tmp->name);
                        }
                        GUI::msgbox("设置成功", "重启或下次唤醒后生效");
                    }
                }
                break;
            }
        case 5:
            // 已安装应用管理
            toApp = "installer";
            hasToApp = true;
            end = true;
            return;
            break;
        case 6:  
            if(GUI::msgbox_yn("修改TF卡加载模式","模式\n1.当卸载后才断电\n2.休眠后就断电","模式1","模式2"))
            {
                config[TFmode] = "1";
                hal.saveConfig();
                Serial.printf("修改TF卡加载模式,当卸载后才断电");
            }
            else
            {
                config[TFmode] = "0";
                hal.saveConfig();
                Serial.printf("修改TF卡加载模式,休眠后就断电");
            }
            break; 
        case 7:
            // 恢复出厂设置
            {
                if (GUI::msgbox_yn("此操作不可撤销", "是否恢复出厂设置？"))
                {
                    if (GUI::msgbox_yn("这是最后一次提示", "将格式化nvs和LittleFS存储区", "取消（右）", "确认（左）") == false)
                    {
                        display.clearScreen();
                        u8g2Fonts.drawUTF8(30, 40, "正在格式化NVS存储");
                        display.display();
                        nvs_flash_erase();
                        display.clearScreen();
                        u8g2Fonts.drawUTF8(30, 40, "正在格式化LittleFS存储");
                        display.display(true);
                        LittleFS.end();
                        LittleFS.format();
                        display.clearScreen();
                        u8g2Fonts.drawUTF8(30, 40, "完成，正在重启");
                        display.display(true);
                        ESP.restart();
                    }
                }
            }
            break;
        case 8:
            {
                if(GUI::msgbox_yn("警告","格式化将丢失所有文件，包括配置文件","确定","取消")){
                    if(GUI::msgbox_yn("警告","这是最后一次提醒，是否仍要格式化","取消","确定"))
                    {
                        
                    }else{
                        if(LittleFS.format()){
                            GUI::msgbox("提示","LiClock成功进行了格式化");}
                        else{
                            for(int i = 0;i < 3;i++)
                            {
                                buzzer.append(3000,200);
                                delay(350);
                            }
                            GUI::msgbox("提示","LiClock在格式化时发生了错误,格式化未能完成");
                        }
                    }
                }
            }
            break;
        case 9:
            {
                int vcc;
                if(GUI::msgbox_yn("校准方式","1.使用芯片ADC校准数据\n在使用1前，请使用esptool\n确定芯片有正确校准数据\n2.使用外部仪表读数","1","2")){
                    vcc = 2 * analogReadMilliVolts(PIN_ADC);
                }
                else{
                    vcc = GUI::msgbox_number("外部仪表读数",4,hal.VCC);
                }
                int ppc;
                int adc = analogRead(PIN_ADC);
                ppc = vcc * 4096 / adc;
                hal.pref.putInt("ppc",ppc);
                char buf[40];
                sprintf(buf,"新的分压系数: %d\n%d->%d mV",ppc,vcc,hal.VCC);
                GUI::msgbox("提示",buf);
            }
            break;
        case 10:
            {
                int new_clk_freq = GUI::msgbox_number("SD卡时钟", 8, hal.pref.getInt("sd_clk_freq" , 3500000));
                hal.pref.putInt("sd_clk_freq" , new_clk_freq);
            }
            break;
        case 11:
            {
                GUI::msgbox("提示", "频率只能为以下数值\n240,160,80,40,20,10MHz\n");
                int new_freq = GUI::msgbox_number("new freq", 3, getCpuFrequencyMhz());
                hal.pref.putInt("CpuFreq", new_freq);
                int freq = ESP.getCpuFreqMHz();
                if (freq != new_freq)
                {
                    bool cpuset = setCpuFrequencyMhz(new_freq);
                    Serial.begin(115200);
                    Serial.printf("CpuFreq: %dMHZ -> %dMHZ ......", freq, new_freq);
                    if(cpuset){Serial.print("ok\n");GUI::msgbox("提示", "频率修改成功");}
                    else {Serial.print("err\n");GUI::msgbox("错误", "频率未能修改");F_LOG("CPU频率修改失败,设置的值:%d", new_freq);}
                }
            }
            break;
        case 12:
            {
                int auto_sleep_mv = GUI::msgbox_number("自动休眠电压", 4, hal.pref.getInt("auto_sleep_mv", 2800));
                if (auto_sleep_mv < 2800){
                    auto_sleep_mv = 2800;
                    GUI::msgbox("提示", "自动休眠电压不能小于2800mV,已自动设置为最低值2800mV");
                }
                hal.pref.putInt("auto_sleep_mv", auto_sleep_mv);
            }
            break;   
        case 13:
            {
                int long_pres_time = GUI::msgbox_number("长按时间", 4, hal.pref.getInt("lpt", 25) * 10);
                hal.pref.putInt("lpt", long_pres_time / 10);
            }
            break;
        case 14:
            menu_DS3231();
            break;
        case 15:
            {
                static const menu_select ebook_set[] = {
                    {false, "返回"},
                    {true, "根据唤醒源翻页"},
                    {true, "自动翻页"},
                    {false, "自动翻页延时"},
                    {true, "使用lightsleep"},
                    {true, "反色显示"},
                    {true, "使用备选txt解析程序1"},
                    {true, "甘草索引程序"},
                    {false, NULL},
                };
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
                        default:
                            GUI::info_msgbox("错误", "无效的选项");
                            break;
                    }
                }
            }
            break;
        case 16:
            {
                if (GUI::msgbox_yn("提示", "是否联网更新CFU.json文件？")){
                    GUI::info_msgbox("提示", "正在联网更新CFU.json文件");
                    if (hal.cheak_firmware_update())
                        GUI::info_msgbox("提示", "CFU.json文件已更新");
                    else
                        GUI::info_msgbox("提示", "http错误,CFU.json文件未更新");
                    hal.wait_input();
                }
                File cfufile = LittleFS.open("/System/CFU.json", "r");
                bool file_true = true;
                if (!cfufile)
                {
                    Serial.println("Failed to open cfu file");
                    file_true = false;
                }
                deserializeJson(cfu, cfufile);
                cfufile.close();
                char buf[128];
                display.clearScreen();
                GUI::drawWindowsWithTitle("文件内容");
                u8g2Fonts.setCursor(2, 28);
                u8g2Fonts.printf("name:%s newver:%s version:%s isbeta:%s", cfu["name"].as<const char *>(), cfu["newversion"].as<bool>() ? "yes" : "no", cfu["version"].as<const char *>(), cfu["isbeta"].as<bool>() ? "yes" : "no");
                u8g2Fonts.setCursor(2, 42);
                u8g2Fonts.printf("bigupdate:%s", cfu["updateinfo"]["bigupdate"].as<bool>() ? "yes" : "no");
                JsonArray updatelog = cfu["updateinfo"]["log"];
                int i = 0;
                for (JsonVariant item : updatelog){
                    u8g2Fonts.setCursor(2, 42 + ((i + 1) * 14));
                    u8g2Fonts.printf("%d.%s", i + 1, item.as<const char *>());
                    i++;
                }
                u8g2Fonts.setCursor(2, u8g2Fonts.getCursorY() + 14);
                u8g2Fonts.printf("%s", cfu["updateinfo"]["url"].as<const char *>());
                u8g2Fonts.setCursor(2, u8g2Fonts.getCursorY() + 14);
                u8g2Fonts.printf("%s", cfu["updateinfo"]["url1"].as<const char *>());
                display.display();
                hal.wait_input();
            }
            break;
        case 17:
            {
                static const menu_select settings_btn_buz[] =
                {
                    {false, "返回"},
                    {true, "按键音"},
                    {false, "声音频率"},
                    {false, "声音长度"},
                    {false, NULL}
                };
                int res = 0;
                bool end = false;
                while (!end){
                    res = GUI::select_menu("按键音设置", settings_btn_buz);
                    switch (res)
                    {
                        case 0:
                            end = true;
                            break;
                        case 1:
                            break;
                        case 2:
                            hal.pref.putInt("btn_buz_freq", GUI::msgbox_number("输入频率Hz", 5, hal.pref.getInt("btn_buz_freq", 150)));
                            break;
                        case 3:
                            hal.pref.putInt("btn_buz_time", GUI::msgbox_number("输入时长ms", 5, hal.pref.getInt("btn_buz_time", 100)));
                            break;
                        default:
                            break;
                    }
                }
            }
            break;
        case 18:
            {
                hal.pref.putInt("display_count", GUI::msgbox_number("输入全刷间隔", 2, hal.pref.getInt("display_count", 15)));
            }
            break;
        case 20:
            {
                int voltage = GUI::msgbox_number("输入计算起点电压", 4, hal.pref.getInt("soc_voltage", 2900));
                if (voltage < 2900){
                    voltage = 2900;
                    GUI::msgbox("提示", "电压不能小于2900mV，已自动设置为最低值2900mV");}
                if (voltage > 3700){
                    voltage = 3700;
                    GUI::msgbox("提示", "电压不能大于3700mV，已自动设置为最高值3700mV");}
                hal.pref.putInt("soc_voltage", voltage);
                hal.pref.putUChar("soc_10%", (uint8_t)((4220 - voltage) / 13));
            }
            break;
        case 21:
            {    
                if (GUI::msgbox_yn("提示", "选择对BQ27441", "配置参数", "打印状态")){
                    xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
                    // Set BATTERY_CAPACITY to the design capacity of your battery in mAh.
                    uint16_t BATTERY_CAPACITY = GUI::msgbox_number("输入电池容量mAh", 5, 1000);

                    //lowest operational voltage in mV
                    uint16_t TERMINATE_VOLTAGE = GUI::msgbox_number("输入低电压门槛", 5, 3000);

                    //current at which charger stops charging battery in mA
                    //in case of Sparkfun Battery Babysitter board:
                    // 100mA charge current --> 12mA
                    // 500mA charge current --> 60mA
                    uint16_t TAPER_CURRENT = GUI::msgbox_number("输入低流电压门槛", 5, 24);

                    GUI::info_msgbox("提示", "正在写入配置...");
            
                    lipo.enterConfig();                 // To configure the values below, you must be in config mode
                    lipo.setCapacity(BATTERY_CAPACITY); // Set the battery capacity
            
                    /*
                        Design Energy should be set to be Design Capacity × 3.7 if using the bq27441-G1A or Design
                        Capacity × 3.8 if using the bq27441-G1B
                    */
                    lipo.setDesignEnergy(BATTERY_CAPACITY * 3.7f);
            
                    /*
                        Terminate Voltage should be set to the minimum operating voltage of your system. This is the target
                        where the gauge typically reports 0% capacity
                    */
                    lipo.setTerminateVoltage(TERMINATE_VOLTAGE);
            
                    /*
                        Taper Rate = Design Capacity / (0.1 * Taper Current)
                    */
                    lipo.setTaperRate(10 * BATTERY_CAPACITY / TAPER_CURRENT);
            
                    lipo.exitConfig(); // Exit config mode to save changes
                    xSemaphoreGive(peripherals.i2cMutex);
                }
                else{
                    hal.task_bat_info_update();
                    hal.printBatteryInfo();
                }
            }
            break;
        case 22:
            {
                #define NVS_BACKUP_FILE "/System/nvs.bin"
                // 获取NVS分区信息
                const esp_partition_t* nvs_partition = esp_partition_find_first(
                    ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
                if (!nvs_partition) {
                    log_e("找不到NVS分区");
                    break;
                }
                uint8_t* buffer;
                File file;
                hal.pref.end();
                if (GUI::msgbox_yn("NVS备份及恢复程序", "NVS备份文件存储在System文件夹中的nvs.bin文件", "备份", "恢复")){
                    buffer = (uint8_t*)malloc(nvs_partition->size);
                    if (!buffer) {
                        log_e("内存分配失败");
                        break;
                    }
                    // 读取Flash数据
                    if (esp_partition_read(nvs_partition, 0, buffer, nvs_partition->size) != ESP_OK) {
                        log_e("读取NVS失败");
                        free(buffer);
                        break;
                    }
                    // 写入文件
                    file = LittleFS.open(NVS_BACKUP_FILE, "w");
                    if (!file) {
                        GUI::info_msgbox("发生错误", "无法创建文件");
                        free(buffer);
                        break;
                    }
                    size_t written = file.write(buffer, nvs_partition->size);
                    file.close();
                    free(buffer);
                    if (written != nvs_partition->size) {
                        GUI::info_msgbox("发生错误", "文件写入错误");
                        LittleFS.remove(NVS_BACKUP_FILE);
                    } else {
                        Serial.printf("备份成功，大小：%d字节\n", written);
                        GUI::info_msgbox("操作成功", "已创建备份文件nvs.bin");
                    }
                } else {
                    if (!LittleFS.exists(NVS_BACKUP_FILE)){
                        GUI::info_msgbox("发生错误", "nvs.bin不存在");
                    }
                    // 打开备份文件
                    file = LittleFS.open(NVS_BACKUP_FILE, "r");
                    if (!file) {
                        GUI::info_msgbox("发生错误", "无法打开备份文件");
                        return;
                    }
                    size_t fileSize = file.size();
                    // 读取文件内容
                    buffer = (uint8_t*)malloc(fileSize);
                    if (!buffer) {
                        log_e("内存分配失败");
                        file.close();
                        return;
                    }
                    size_t read = file.read(buffer, fileSize);
                    file.close();
                    if (read != fileSize) {
                        log_e("读取不完整");
                        free(buffer);
                        return;
                    }
                    // 擦除并写入分区
                    if (esp_partition_erase_range(nvs_partition, 0, nvs_partition->size) != ESP_OK) {
                        GUI::info_msgbox("发生错误", "NVS擦除失败");
                        free(buffer);
                        return;
                    }
                    if (esp_partition_write(nvs_partition, 0, buffer, fileSize) != ESP_OK)
                        GUI::info_msgbox("发生错误", "NVS写入失败");
                    else
                        GUI::info_msgbox("操作成功", "成功从备份文件恢复NVS");
                    free(buffer);
                }
                hal.pref.begin("clock");
            }
            break;
        default:
            break;
        }
    }
}
int AppSettings::decToBin(int dec) {
    if (dec < 0 || dec > 255) {
        GUI::info_msgbox("警告", "非法的输入值");
        log_e("非法的输入值");
        return 0;
    }
    int bin = 0;
    int base = 1;
    for (int i = 0; i < 8; ++i) {
        if (dec & 1) { // 检查最低位是否为1
            bin += base;
        }
        dec >>= 1; // 右移一位
        base *= 10; // 更新权重
    }
    return bin;
}
// 将二进制数转换为十进制数
int AppSettings::binToDec(int bin) {
    int decimal = 0;
    int base = 1; // 用于计算每一位的权重
    // 从右到左遍历二进制数，计算十进制数
    while (bin > 0) {
        int digit = bin % 10; // 获取二进制数的最低位
        decimal += digit * base;
        bin /= 10; // 去掉二进制数的最低位
        base *= 2; // 更新权重
    }
    return decimal;
}

void AppSettings::cheak_config(char *a)
{
    if(GUI::msgbox_yn("提示写入选中WIFI",a,"确定","取消"))
    {
        config[PARAM_SSID] = a;
        hal.saveConfig();
    }
    if(GUI::msgbox_yn("密码是否仅有数字",a,"是","否"))
    {
        char pass[32];
        sprintf(pass, "%d", GUI::msgbox_number("输入密码",8,0));
        config[PARAM_PASS] = pass;
        config[PARAM_CLOCKONLY] = "0";
        hal.saveConfig();
        GUI::msgbox("提示","已写入配置，即将重启！");
        esp_restart();
    }
    else
    {
        if(GUI::msgbox_yn("提示","是否启动网页服务器配置密码","确定","取消"))
        {
            hal.WiFiConfigManual();
            ESP.restart();
        }
        else{
            if(GUI::msgbox_yn("提示","是否使用简易输入法输入密码","确定","取消")){
                config[PARAM_PASS] = GUI::englishInput("输入WiFi密码");
            }
        }
    }
}
void AppSettings::menu_SWQ()
{
    static const menu_item settings_menu_DS3231_SWQ[] =
    {
        {NULL,"返回"},
        {NULL,"后备电源振荡器使能设置"},
        {NULL,"后备电源1Hz输出"},
        {NULL,"频率设置"},
        {NULL,"1Hz方波输出"},
        {NULL,NULL},
    };
                            int res = 0;
                            bool end = false;
                            while (!end){
                            res = GUI::menu("振荡器设置", settings_menu_DS3231_SWQ);
                            bool tf;
                            bool bat;
                            byte frequency; 
                            tf = hal.pref.getBool("tf", true);
                            bat = hal.pref.getBool("bat", true);
                            frequency = hal.pref.getInt("frequency", 0);
                            switch (res)
                            {
                                case 0:
                                    end = true;
                                    break;
                                case 1:
                                    tf = GUI::msgbox_yn("振荡器使能设置","使用后备电源是否开启振荡器","开启","关闭");
                                    hal.pref.putBool("tf", tf);
                                    Srtc.enableOscillator(tf,bat,frequency);
                                    break;
                                case 2:
                                    bat = GUI::msgbox_yn("后备电源是否工作","在只有备用电源时1Hz是否输出","开启","关闭");
                                    hal.pref.putBool("bat", bat);
                                    Srtc.enableOscillator(tf,bat,frequency);
                                    break;
                                case 3:
                                    GUI::msgbox("提示","0 = 1HZ\n1 = 1024Hz\n2 = 4096Hz\n3 = 8192Hz ");
                                    frequency = GUI::msgbox_number("输入频率代号",1,0);
                                    hal.pref.putInt("frequency", frequency);
                                    Srtc.enableOscillator(tf,bat,frequency);
                                    break;
                                case 4:
                                    {
                                        bool TF = GUI::msgbox_yn("1Hz方波输出","是否开启1Hz方波输出","开启","关闭");
                                        Srtc.enable1Hz(TF);
                                    }
                                default:
                                    break;
                            }
                            }
                            
}
void AppSettings::menu_DS3231()
{
            static const menu_item settings_menu_DS3231[] =
                {
                    {NULL,"返回"},
                    {NULL,"偏移量读取"},
                    {NULL,"设置偏移量"},
                    {NULL,"振荡器设置"},
                    {NULL,"时间格式设置"},
                    {NULL,"读取芯片温度"},
                    {NULL,"读取当前时间"},
                    {NULL,"振荡器停止标志"},
                    {NULL,"完全手动设置时间"},
                    {NULL,"32.786KHZ输出使能"},
                    {NULL,NULL},
            };
            int res = 0;
            bool end = false;
            while (end == false)
            {
                res = GUI::menu("DS3231设置", settings_menu_DS3231);
                switch (res)
                {
                    case 0:
                        end = true;
                        break;
                    case 1:
                        {
                            int8_t offset=Srtc.readOffset();
                            char buf[128];
                            sprintf(buf,"1lsb≈0.12ppm@25°C\n偏移量：%d\n间隔%d秒%s%d秒", offset, hal.pref.getInt("every", 100), hal.pref.getInt("rtc_offset", 0) < 0 ? "慢" : "快", abs(hal.pref.getInt("rtc_offset", 0)));
                            GUI::msgbox("DS3231设置",buf);
                        }
                        break;
                    case 2:
                        {
                            int offset = peripherals.rtc.readOffset();
                            offset = GUI::msgbox_number("输入偏移量", 3, offset);
                            if (!(offset <128 && offset > -128)){
                                offset = 0;
                                GUI::msgbox("提示","偏移量超出范围\n-127~127\n已重置为0");
                            }
                            peripherals.rtc.writeOffset((int8_t)offset);
                        }
                        break;
                    case 3:
                        menu_SWQ();
                        break;
                    case 4:
                        if(GUI::msgbox_yn("时间格式","设置DS3231的时间格式","24h","12h"))
                        {
                            Srtc.setClockMode(false);
                        }
                        else
                        {
                            Srtc.setClockMode(true);
                        }
                        break;
                    case 5:
                        {
                            float c=Srtc.getTemperature();
                            char buf[30];
                            sprintf(buf,"温度：%f℃",c);
                            GUI::msgbox("芯片温度",buf);
                        }
                        break;
                    case 6:
                        {
                            char buf[60];
                            sprintf(buf, "20%d年%d月%d日 星期%d %d:%d:%d",Srtc.getYear(),Srtc.getMonth(),Srtc.getDate(),Srtc.getDoW(),Srtc.getHour(),Srtc.getMinute(),Srtc.getSecond());
                            Serial.println(buf);
                            GUI::msgbox("DS3231时间", buf);
                        }
                        break;
                    case 7:
                        {
                            if(Srtc.oscillatorCheck())
                            {
                                GUI::msgbox("OSF标志","true\n震荡器未停止过\n时间正常");
                            }
                            else
                            {
                                GUI::msgbox("OSF标志","false\n振荡器停止过\n时间可能为错误");
                            }
                        }
                        break;
                    case 8:
                        {
                            Srtc.setSecond(GUI::msgbox_number("输入秒",2,0));
                            Srtc.setMinute(GUI::msgbox_number("输入分",2,0));
                            Srtc.setHour(GUI::msgbox_number("输入时",2,0));
                            Srtc.setDoW(GUI::msgbox_number("输入星期",1,0));
                            Srtc.setDate(GUI::msgbox_number("输入日",2,0));
                            Srtc.setMonth(GUI::msgbox_number("输入月",2,0));
                            Srtc.setYear(GUI::msgbox_number("输入年的后两位",2,0));
                        }
                        break;
                    case 9:
                        Srtc.enable32kHz(GUI::msgbox_yn("使能32.786KHZ","","开启","关闭"));
                        break;
                    default:
                        break;
                }
            }
}
