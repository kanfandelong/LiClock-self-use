#include "AppManager.h"
#include <nvs_flash.h>
#include "DS3231.h"
#include "RDA5807.h"

#define SDA_1 26
#define SCL_1 25

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
    void menu_alarm();
    void menu_time();
    void menu_network();
    void menu_display();
    void menu_power();
    void menu_peripherals();
    void menu_system();
    void about();
    void menu_SWQ();
    void menu_DS3231();
    int binToDec(int bin);
    int decToBin(int dec);
    void cheak_config(char *a);
    void tfcard_info();
    void bat_info();

private:
    void show_wifi_power_set()
    {
        char buf[128];
        sprintf(buf, "已将WiFi发射功率设置为:%.2f dbm", (float)hal.pref.getUChar("wifitxpower", 78) * 0.25);
        GUI::msgbox("提示", buf);
    }
};
static AppSettings app;
void AppSettings::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void AppSettings::setup()
{
    display.clearScreen();
    display.display();
    display.setPowerMode(POWER_MODE_HPM);
    GUI::drawWindowsWithTitle("设置");
    u8g2Fonts.drawUTF8(120, 75, "请稍等...");
    // 下面是设置菜单
    int res = 0;
    bool end = false;
    static const menu_item settings_menu_main[] =
        {
            {NULL, "退出"},
            {NULL, "时间与闹钟"},
            {NULL, "网络设置"},
            {NULL, "显示与声音"},
            {NULL, "电源管理"},
            {NULL, "存储与外设"},
            {NULL, "系统设置"},
            {NULL, "关于"},
            {NULL, NULL},
        };
    display.display();
    while (end == false && hasToApp == false)
    {
        res = GUI::menu("设置", settings_menu_main, 8, 8, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            // 时间与闹钟
            menu_time();
            break;
        case 2:
            // 网络设置
            menu_network();
            break;
        case 3:
            // 显示与声音
            menu_display();
            break;
        case 4:
            // 电源管理
            menu_power();
            break;
        case 5:
            // 存储与外设
            menu_peripherals();
            break;
        case 6:
            // 系统设置
            menu_system();
            break;
        case 7:
            // 关于
            about();
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
    static const menu_select settings_menu_time[] =
        {
            {false, "< 返回", nullptr},
            {false, "手动触发NTP", nullptr},
            {false, "时间同步间隔设置", nullptr},
            {false, "RTC线性偏移修正", nullptr},
            {true, "在上电复位时使用联网对时", set_rtc_in_rst},
            {false, "设置时钟字体", nullptr},
            {false, "闹钟设置", nullptr},
            {false, NULL, nullptr},
        };
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("时间设置", settings_menu_time, res);
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
                    {NULL, "取消"},         // 0
                    {NULL, "禁用时间同步"}, // 1
                    {NULL, "2小时"},        // 2
                    {NULL, "4小时"},        // 3
                    {NULL, "6小时"},
                    {NULL, "12小时"},
                    {NULL, "24小时"},
                    {NULL, "36小时"},
                    {NULL, "48小时"},
                    {NULL, "自定义"}, // 9
                    {NULL, NULL},
                };
                res = GUI::menu("时间同步间隔设置", menu);
                if (res > 0 && res < 9)
                {
                    // 设置时间同步间隔
                    hal.pref.putUChar(SETTINGS_PARAM_NTP_INTERVAL, res - 1);
                    GUI::msgbox("时间同步间隔设置", "设置完成");
                }
                else if (res == 9)
                {
                    // 自定义
                    int hour = GUI::msgbox_number("2-999h", 3, hal.pref.putUChar(SETTINGS_PARAM_NTP_INTERVAL, 1));
                    if (hour > 1)
                    {
                        hal.pref.putUChar(SETTINGS_PARAM_NTP_INTERVAL, hour);
                        GUI::msgbox("时间同步间隔设置", "设置完成");
                    }
                }
            }
            break;
        case 3:
            // RTC线性偏移修正
            toApp = "rtcoffset";
            hasToApp = true;
            end = true;
            break;
        case 5:
        {
            const char *str = GUI::fileDialog("请选择时钟字体文件", false, "ttf\nTTF");
            if (str == NULL)
            {
                hal.pref.remove("clock_font");
                GUI::msgbox("提示", "已恢复默认字体");
            }
            else
            {
                hal.pref.putString("clock_font", String(str));
                GUI::msgbox("提示", "时钟字体设置完成，重启生效");
            }
        }
        break;
        case 6:
            menu_alarm();
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
    menu_item *settings_menu_alarm = new menu_item[alarms.alarm_num + 3];
    const menu_item settings_menu_alarm_sub[] = {
        {NULL, "< 返回"},
        {NULL, "时间"},
        {NULL, "重复周期"},
        {NULL, NULL},
    };
    const menu_item settings_menu_alarm_time[] = {
        {NULL, "< 返回"},
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
    char alarm_buf[alarms.alarm_num][30];
    char alarm_buf_week[25];
    char bit_week[7] = {0};
    while (end == false && hasToApp == false)
    {
        settings_menu_alarm[0].title = "< 返回";
        settings_menu_alarm[0].icon = NULL;
        // 读取闹钟设置
        for (int i = 0; i < alarms.alarm_num; ++i)
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
            settings_menu_alarm[i + 1].icon = NULL;
        }
        settings_menu_alarm[alarms.alarm_num + 1].title = "设置闹钟铃声";
        settings_menu_alarm[alarms.alarm_num + 1].icon = NULL;
        settings_menu_alarm[alarms.alarm_num + 2].title = NULL;
        settings_menu_alarm[alarms.alarm_num + 2].icon = NULL;
        res = GUI::menu("闹钟设置", settings_menu_alarm);
        if (res == 0)
            break;
        if (res == alarms.alarm_num + 1)
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
    static const menu_select settings_menu_network[] =
        {
            {false, "< 返回", nullptr},
            {false, "选择默认WiFi", nullptr},
            {false, "搜索周围的WIFI", nullptr},
            {false, "设置WiFi发射功率", nullptr},
            {false, "ESPTouch配网", nullptr},
            {false, "启动HTTP服务器", nullptr},
            {false, "启动文件服务器", nullptr},
            {true, "启用mDNS", "en_mdns"},
            {false, "ESPNow设备扫描", nullptr},
            {false, "蓝牙扫描", nullptr},
            {false, "退出Bilibili账号", nullptr},
            {false, "分享当前配置的WiFi", nullptr},
            {false, "配置界面和Blockly", nullptr},
            {false, NULL, nullptr},
        };
    DNSServer dnsServer;
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("网络设置", settings_menu_network, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
        {
            if (!LittleFS.exists(wifi_config_file))
            {
                File file = LittleFS.open(wifi_config_file, "w");
                file.print(DEFAULT_WIFI_CONFIG);
                file.close();
            }
            // 读取JSON配置文件
            File configFile = LittleFS.open(wifi_config_file);
            if (!configFile)
            {
                log_printf("Failed to open file for reading\n");
            }

            StaticJsonDocument<1024> wifi_list;
            deserializeJson(wifi_list, configFile);
            configFile.close();
            JsonArray networks = wifi_list["networks"];
            int i = 0, j = 1;
            for (JsonObject network : networks)
            {
                i++;
            }
            menu_item *_wifi_list = new menu_item[i + 2];
            char pass[i][32];
            _wifi_list[0].title = "返回";
            _wifi_list[0].icon = NULL;
            JsonArray wifi = wifi_list["networks"];
            for (JsonObject network : wifi)
            {
                _wifi_list[j].title = network["ssid"].as<const char *>();
                strcpy(pass[j - 1], network["pass"].as<const char *>());
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
                if (res == 0)
                {
                    delete[] _wifi_list;
                    end = true;
                    break;
                }
                else
                {
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
                0xa0, 0x02, 0xa8, 0x02, 0xa8, 0x02, 0xaa, 0x02, 0xaa, 0x02, 0x00, 0x00};
            static const uint8_t WIFI_4_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x00, 0xa0, 0x00,
                0xa0, 0x00, 0xa8, 0x00, 0xa8, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0x00, 0x00};
            static const uint8_t WIFI_3_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00,
                0x20, 0x00, 0x28, 0x00, 0x28, 0x00, 0x2a, 0x00, 0x2a, 0x00, 0x00, 0x00};
            static const uint8_t WIFI_2_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x00, 0x00};
            static const uint8_t WIFI_1_bits[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00};
            WiFi.mode(WIFI_STA);
            hal.searchWiFi();
            log_printf("搜索到的个数:%d", hal.numNetworks);
            char winfo[hal.numNetworks][96];
            int rssis[hal.numNetworks];
            char _ssid[hal.numNetworks][96];
            if (hal.numNetworks != 0)
            {
                const int MAX_PX = 233; // 最大像素宽度

                for (int i = 0; i < hal.numNetworks; i++)
                {
                    String ssid = WiFi.SSID(i);
                    int rssi = rssis[i] = WiFi.RSSI(i);
                    int channel = WiFi.channel(i);
                    String bssid = WiFi.BSSIDstr(i);

                    char rightPart[96];
                    sprintf(rightPart, "%s|ch%2d|%3d", bssid.c_str(), channel, rssi); // 固定宽度格式

                    // 测量宽度
                    int leftW = u8g2Fonts.getUTF8Width(ssid.c_str());
                    int rightW = u8g2Fonts.getUTF8Width(rightPart);
                    int spaceW = u8g2Fonts.getUTF8Width(" "); // 单个空格宽度

                    int available = MAX_PX - leftW - rightW;
                    if (available < 0)
                    {
                        // SSID 太长，需要截断（可省略，直接截取字符）
                        // 这里简单截断到可用宽度
                        while (ssid.length() > 0 && u8g2Fonts.getUTF8Width(ssid.c_str()) > MAX_PX - rightW - spaceW)
                        {
                            ssid.remove(ssid.length() - 1);
                        }
                        leftW = u8g2Fonts.getUTF8Width(ssid.c_str());
                        available = MAX_PX - leftW - rightW;
                    }

                    int spaceCount = available / spaceW; // 需要填充的空格数
                    if (spaceCount < 0)
                        spaceCount = 0;

                    // 构造字符串
                    char *p = winfo[i];
                    p += sprintf(p, "%s", ssid.c_str());
                    for (int j = 0; j < spaceCount; j++)
                        p += sprintf(p, " ");
                    sprintf(p, "%s", rightPart);

                    sprintf(_ssid[i], "%s", ssid.c_str());
                }
            }
            menu_item *WiFi_list = new menu_item[hal.numNetworks + 2];
            WiFi_list[0].title = "返回";
            WiFi_list[0].icon = NULL;
            for (int i = 1; i < hal.numNetworks + 1; i++)
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
                if (res == 0)
                {
                    delete[] WiFi_list;
                    end = true;
                    break;
                }
                else
                {
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
                {NULL, "< 返回"},
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
                {NULL, NULL}};
            int res = 0;
            bool end = false;
            while (end == false && hasToApp == false)
            {
                res = GUI::menu("WiFi发射功率设置", wifi_power);
                switch (res)
                {
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
            display.fillScreen(TFT_WHITE);
            if (wifi)
            {
                // beginWebServer();
                str1 = WiFi.localIP().toString();
            }
            else
            {
                WiFi.softAP("WeatherClock", passwd.c_str());
                WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
                dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
                beginWebServer();
                str1 = "192.168.4.1";
                QRCode qrcode;
                uint8_t qrcodeData[qrcode_getBufferSize(7)];
                qrcode_initText(&qrcode, qrcodeData, 6, 2, str.c_str());
                log_printf("%u\n", qrcode.size);
                for (uint8_t y = 0; y < qrcode.size; y++)
                {
                    // Each horizontal module
                    for (uint8_t x = 0; x < qrcode.size; x++)
                    {
                        display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE);
                    }
                }
            }
            beginFileServer(GUI::msgbox_yn("提示", "请选择文件服务器目标文件系统", "TF卡", "内置存储"));
            u8g2Fonts.setCursor(120, (128 - (13 * 2)) / 2);
            GUI::autoIndentDraw(str1.c_str(), 296, 120, 13);
            display.display();
            while (1)
            {
                if (hal.btnl.isPressing())
                {
                    if (GUI::waitLongPress(hal.btnl.pin()))
                    {
                        while (hal.btnl.isPressing())
                            delay(20);
                        // free(server);
                        MDNS.end();
                        if (!wifi)
                            dnsServer.stop();
                        WiFi.disconnect(true);
                        esp_restart();
                        hal.can_sleep = true;
                        break;
                    }
                }
                else
                    delay(20);
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
            display.fillScreen(TFT_WHITE);
            QRCode qrcode;
            uint8_t qrcodeData[qrcode_getBufferSize(7)];
            qrcode_initText(&qrcode, qrcodeData, 6, 2, str.c_str());
            log_printf("%u\n", qrcode.size);
            for (uint8_t y = 0; y < qrcode.size; y++)
            {
                // Each horizontal module
                for (uint8_t x = 0; x < qrcode.size; x++)
                {
                    display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE);
                }
            }
            if (hal.pref.getString("system_font", "default") == "default")
                u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312_self, 209899L);
            else
                u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());
            u8g2Fonts.setCursor(120, (128 - (17 * 2)) / 2);
            char buf[50];
            sprintf(buf, "扫描二维码以连接本机分享的WiFi");
            GUI::autoIndentDraw(buf, 296, 120, 13);
            display.display(true);
            hal.wait_input();
        }
        break;
        case 11:
        {
            String str1, str2;
            bool wifi = hal.autoConnectWiFi(false);
            bool ap = false;
            if (wifi)
            {
                beginWebServer();
                str1 = "http://" + WiFi.localIP().toString();
                str2 = "http://" + WiFi.localIP().toString() + "/blockly";
            }
            else
            {
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
            display.fillScreen(TFT_WHITE);
            QRCode qrcode1, qrcode2;
            uint8_t qrcodeData[2][qrcode_getBufferSize(7)];
            qrcode_initText(&qrcode1, qrcodeData[0], 6, 2, str1.c_str());
            qrcode_initText(&qrcode2, qrcodeData[1], 6, 2, str2.c_str());
            log_printf("%u\n", qrcode1.size);
            log_printf("%u\n", qrcode2.size);
            for (uint8_t y = 0; y < qrcode1.size; y++)
            {
                // Each horizontal module
                for (uint8_t x = 0; x < qrcode1.size; x++)
                {
                    display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode1, x, y) ? TFT_BLACK : TFT_WHITE);
                }
            }
            for (uint8_t y = 0; y < qrcode2.size; y++)
            {
                // Each horizontal module
                for (uint8_t x = 0; x < qrcode2.size; x++)
                {
                    display.fillRect(2 * x + 196, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode2, x, y) ? TFT_BLACK : TFT_WHITE);
                }
            }
            u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312_self, 209899L);
            char buf[2][32];
            sprintf(buf[0], "网页配置界面");
            sprintf(buf[1], "Blockly界面");
            u8g2Fonts.setCursor(120, 30);
            GUI::autoIndentDraw(buf[0], 137, 120, 12);
            u8g2Fonts.setCursor(160, 21);
            GUI::autoIndentDraw(buf[1], 177, 160, 12);
            display.display();
            while (1)
            {
                updateWebServer();
                if (LuaRunning)
                    continue;
                if (hal.btnl.isPressing())
                {
                    while (hal.btnl.isPressing())
                        delay(20);
                    WiFi.disconnect(true);
                    esp_restart();
                    hal.can_sleep = true;
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

void AppSettings::menu_display()
{
    int res = 0;
    bool end = false;
    static const menu_select settings_menu_display[] =
        {
            {false, "< 返回", nullptr},
            {false, "屏幕方向", nullptr},
            {false, "屏幕全刷间隔", nullptr},
            {false, "屏幕PLL设定", nullptr},
            {false, "按键音设置", nullptr},
            // {false, "屏幕队列深度", nullptr},
            // {false, "屏幕线程优先级", nullptr},
            {true, "高刷模式", "high_fps"},
            {true, "屏幕deepsleep", "en_disp_sleep"},
            {true, "Inversion", "Inversion"},
            {false, NULL},
        };
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("显示与声音", settings_menu_display, res);
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
            // display.display();
            GUI::msgbox("提示", "按键控制方式请修改GPIO宏定义"); // 为了节省内存并加快速度
            break;
        case 2:
        {
            hal.pref.putInt("display_count", GUI::msgbox_number("输入全刷间隔", 2, hal.pref.getInt("display_count", 15)));
        }
        break;
        case 3:
        {
            uint val = GUI::msgbox_hex("set_PLL", 2, hal.pref.getUInt("pllset", 0x3C));
            hal.pref.putUInt("pllset", val);
            // display.epd2.PLL_set(val);
        }
        break;
        case 4:
        {
            static const menu_select settings_btn_buz[] =
                {
                    {false, "< 返回", nullptr},
                    {true, "按键音", nullptr},
                    {false, "声音频率", nullptr},
                    {false, "声音长度", nullptr},
                    {false, NULL, nullptr}};
            int res = 0;
            bool end = false;
            while (!end)
            {
                res = GUI::select_menu("按键音设置", settings_btn_buz, res);
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
        // case 5:
        // {
        //     uint32_t _time = GUI::msgbox_number("设置屏幕队列深度", 2, hal.pref.getUInt("display_list", 3));
        //     hal.pref.putUInt("display_list", _time);
        // }
        // break;
        // case 6:
        // {
        //     uint32_t priority = GUI::msgbox_number("设置屏幕线程优先级", 2, hal.pref.getUInt("disp_priority", 1));
        //     hal.pref.putUInt("disp_priority", priority);
        // }
        // break;
        default:
            break;
        }
    }
}

void AppSettings::menu_power()
{
    int res = 0;
    bool end = false;
    static const menu_select settings_menu_display[] =
        {
            {false, "< 返回", nullptr},
            {false, "电池状态", nullptr},
            {false, "电池电压校准", nullptr},
            {false, "自动休眠电压", nullptr},
            {true, "精准电量显示", nullptr},
            {false, "电量计算起点电压", nullptr},
            {false, "电量计初始化", nullptr},
            {true, "启用关机图片", "en_poff_image"},
            {false, "设置关机图片", nullptr},
            {false, NULL, nullptr},
        };
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("电源管理", settings_menu_display, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            bat_info();
            break;
        case 2:
        {
            int vcc;
            if (GUI::msgbox_yn("校准方式", "1.使用芯片ADC校准数据\n在使用1前，请使用esptool\n确定芯片有正确校准数据\n2.使用外部仪表读数", "1", "2"))
            {
                vcc = 2 * analogReadMilliVolts(PIN_ADC);
            }
            else
            {
                vcc = GUI::msgbox_number("外部仪表读数", 4, hal.VCC);
            }
            int ppc;
            int adc = analogRead(PIN_ADC);
            ppc = vcc * 4096 / adc;
            hal.pref.putInt("ppc", ppc);
            char buf[40];
            sprintf(buf, "新的分压系数: %d\n%d->%d mV", ppc, hal.VCC, vcc);
            GUI::msgbox("提示", buf);
        }
        break;
        case 3:
        {
            int auto_sleep_mv = GUI::msgbox_number("自动休眠电压", 4, hal.pref.getInt("auto_sleep_mv", 2800));
            if (auto_sleep_mv < 2800)
            {
                auto_sleep_mv = 2800;
                GUI::msgbox("提示", "自动休眠电压不能小于2800mV,已自动设置为最低值2800mV");
            }
            hal.pref.putInt("auto_sleep_mv", auto_sleep_mv);
        }
        break;
        case 5:
        {
            int voltage = GUI::msgbox_number("输入计算起点电压", 4, hal.pref.getInt("soc_voltage", 2900));
            if (voltage < 2900)
            {
                voltage = 2900;
                GUI::msgbox("提示", "电压不能小于2900mV，已自动设置为最低值2900mV");
            }
            if (voltage > 3700)
            {
                voltage = 3700;
                GUI::msgbox("提示", "电压不能大于3700mV，已自动设置为最高值3700mV");
            }
            hal.pref.putInt("soc_voltage", voltage);
            hal.pref.putUChar("soc_10%", (uint8_t)((4220 - voltage) / 13));
        }
        break;
        case 6:
        {
            if (GUI::msgbox_yn("提示", "选择对BQ27441", "配置参数", "打印状态"))
            {
                xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
                // Set BATTERY_CAPACITY to the design capacity of your battery in mAh.
                uint16_t BATTERY_CAPACITY = GUI::msgbox_number("输入电池容量mAh", 5, 1000);

                // lowest operational voltage in mV
                uint16_t TERMINATE_VOLTAGE = GUI::msgbox_number("输入低电压门槛", 5, 3000);

                // current at which charger stops charging battery in mA
                // in case of Sparkfun Battery Babysitter board:
                //  100mA charge current --> 12mA
                //  500mA charge current --> 60mA
                uint16_t TAPER_CURRENT = GUI::msgbox_number("输入低流电压门槛", 5, 24);

                GUI::info_msgbox("提示", "正在写入配置...");

                lipo.softReset();
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
            else
            {
                // hal.task_bat_info_update();
                hal.printBatteryInfo();
            }
        }
        break;
        case 8:
        {
            const char *poweroff_image;
            poweroff_image = GUI::fileDialog("选择关机图片", false, "lbm", NULL, "/", "LittleFS");
            hal.pref.putString("poweroff_image", poweroff_image);
        }
        break;
        default:
            break;
        }
    }
}

void AppSettings::menu_peripherals()
{
    int res = 0;
    bool end = false;
    static const menu_select settings_menu_peripherals[] =
        {
            {false, "< 返回", nullptr},
            {false, "TF卡信息", nullptr},
            {false, "LittleFS使用情况", nullptr},
            {false, "TF卡电源控制", nullptr},
            {true, "温湿度日志", "temp_log"},
            {false, "扫描I2C外设", nullptr},
            {false, "格式化LittleFS", nullptr},
            {false, "清除nvs存储", nullptr},
            {false, "DS3231设置", nullptr},
            {false, NULL, nullptr},
        };
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("存储与外设", settings_menu_peripherals, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            tfcard_info();
            break;
        case 2:
        {
            char char_buf[64];
            int used = 0, total = 0, free = 0, a;
            GUI::info_msgbox("提示", "正在获取LittleFS文件系统信息...");
            total = LittleFS.totalBytes() / 1024;
            used = LittleFS.usedBytes() / 1024;
            a = (used * 100) / total;
            free = total - used;
            sprintf(char_buf, "已用：%dKB(%d%%)\n剩余：%dKB\n总计：%dKB\n", used, a, free, total);
            GUI::msgbox("LittleFS文件系统信息", char_buf);
        }
        break;
        case 3:
            if (GUI::msgbox_yn("修改TF卡电源控制", "模式\n1.当卸载后才断电\n2.休眠后就断电", "模式1", "模式2"))
            {
                config[TFmode] = "1";
                hal.saveConfig();
                log_printf("修改TF卡电源控制,当卸载后才断电");
            }
            else
            {
                config[TFmode] = "0";
                hal.saveConfig();
                log_printf("修改TF卡电源控制,休眠后就断电");
            }
            break;
        case 5:
            // 重新扫描外设
            {
                if (GUI::msgbox_yn("外设扫描", "选择I2C端口\nI2C1端口扫描上限为64个，I2C0端口取决于系统固件注册的可用外设", "I2C0", "I2C1"))
                    peripherals.check();
                else
                {
                    GUI::info_msgbox("提示", "正在扫描I2C1端口上响应的设备");
                    Wire1.setPins(SDA_1, SCL_1);
                    RDA5807 rda;
                    uint8_t device_list[64];
                    uint8_t devices = rda.checkI2C(device_list);
                    String buf;
                    char str[3];
                    buf += "响应的从机地址：";
                    for (uint8_t i = 0; i < devices; i++)
                    {
                        sprintf(str, "%02x", device_list[i]);
                        buf += "0x";
                        buf += str;
                        buf += "  ";
                    }
                    GUI::info_msgbox("I2C1扫描结果", buf.c_str());
                    hal.wait_input();
                }
            }
            break;
        case 6:
        {
            if (GUI::msgbox_yn("警告", "格式化将丢失所有文件，包括配置文件和网页资源文件", "确定", "取消"))
            {
                if (GUI::msgbox_yn("警告", "这是最后一次提醒，是否仍要格式化", "取消", "确定"))
                {
                }
                else
                {
                    if (LittleFS.format())
                    {
                        GUI::msgbox("提示", "LiClock成功进行了格式化");
                    }
                    else
                    {
                        for (int i = 0; i < 3; i++)
                        {
                            buzzer.append(3000, 200);
                            delay(350);
                        }
                        GUI::msgbox("提示", "LiClock在格式化时发生了错误,格式化未能完成");
                    }
                }
            }
        }
        break;
        case 7:
        {
            if (GUI::msgbox_yn("警告", "这将丢失设置数据", "确定", "取消"))
            {
                if (GUI::msgbox_yn("警告", "这是最后一次提醒，是否仍要清除", "取消", "确定"))
                {
                }
                else
                {
                    if (hal.pref.clear())
                    {
                        GUI::msgbox("提示", "LiClock成功清除了nvs数据");
                    }
                    else
                    {
                        for (int i = 0; i < 3; i++)
                        {
                            buzzer.append(3000, 200);
                            delay(350);
                        }
                        GUI::msgbox("提示", "LiClock在清除nvs时发生了错误,nvs未能清除完成");
                    }
                }
            }
        }
        break;
        case 8:
            menu_DS3231();
            break;
        default:
            break;
        }
    }
}

void AppSettings::menu_system()
{
    int res = 0;
    bool end = false;
    static const menu_select settings_menu_system[] =
        {
            {false, "< 返回", nullptr},
            {false, "主屏幕应用", nullptr},
            {false, "应用管理", nullptr},
            {false, "天气更新间隔", nullptr},
            {false, "立即更新天气", nullptr},
            {false, "电子书设置", nullptr},
            {true, "快速启动", "fast_boot"}, // 6
            {false, "CPU频率设置", nullptr},
            {false, "TF卡时钟频率设置", nullptr},
            {false, "I2C时钟频率设置", nullptr},
            {false, "长按识别时间", nullptr},
            {true, "交换左右按键", "switch_btn"},
            {false, "检查更新", nullptr},
            {true, "离线模式", nullptr},   // 13
            {true, "停用DS3231", nullptr}, // 14
            {true, "系统日志", "sys_log"}, // 15
            {false, "NVS备份和恢复", nullptr},
            {false, "core_dump", nullptr},
            {false, "设置菜单快速滚动阈值", nullptr},
            {false, "串口波特率设置", nullptr},
            {false, "启动OTA", nullptr},
            {false, "恢复出厂设置", nullptr},
            {false, "设置系统全局字体", nullptr},
            {false, NULL, nullptr},
        };
    while (end == false && hasToApp == false)
    {
        res = GUI::select_menu("系统设置", settings_menu_system, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
            // 主屏幕应用选择
            {
                AppBase *tmp = appManager.appSelector(true);
                if (tmp)
                {
                    log_printf("%s\n", tmp->name);
                    if (GUI::msgbox_yn("警告", "选择不兼容的App可能会导致无法进入菜单，是否确认？") == true)
                    {
                        if (strcmp(tmp->name, "clock") == 0)
                        {
                            hal.pref.putBool(hal.get_char_sha_key("离线模式"), false);
                            hal.saveConfig();
                            hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
                        }
                        else if (strcmp(tmp->name, "clockonly") == 0)
                        {
                            hal.pref.putBool(hal.get_char_sha_key("离线模式"), false);
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
            break;
        case 2:
            // 已安装应用管理
            toApp = "installer";
            hasToApp = true;
            end = true;
            return;
            break;
        case 3:
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
            break;
        case 4:
            // 立即更新天气
            {
                GUI::info_msgbox("提示", "正在联网更新天气信息...");
                int res = weather.refresh();
                if (res == 0)
                {
                    GUI::msgbox("更新完成", "已将天气信息保存至/littlefs/System/weather.bin");
                }
                else if (res == -2)
                {
                    GUI::msgbox("发生错误", "错误原因：http错误或异常");
                }
                else if (res == -3)
                {
                    GUI::msgbox("发生错误", "错误原因：彩云天气API失效");
                }
                break;
            }
            break;
        case 5:
        {
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
                {true, "降频运行Ebook", nullptr},
                {false, NULL, nullptr},
            };
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
                default:
                    GUI::info_msgbox("错误", "无效的选项");
                    break;
                }
            }
        }
        break;
        case 7:
        {
            GUI::msgbox("提示", "频率只能为以下数值\n240,160,80,40,20,10MHz\n");
            int new_freq = GUI::msgbox_number("new freq", 3, getCpuFrequencyMhz());
            hal.pref.putInt("CpuFreq", new_freq);
            int freq = ESP.getCpuFreqMHz();
            if (freq != new_freq)
            {
                bool cpuset = setCpuFrequencyMhz(new_freq);
                uart->begin(115200);
                log_printf("CpuFreq: %dMHZ -> %dMHZ ......", freq, new_freq);
                if (cpuset)
                {
                    log_printf("ok\n");
                    GUI::msgbox("提示", "频率修改成功");
                }
                else
                {
                    log_printf("err\n");
                    GUI::msgbox("错误", "频率未能修改");
                    log_e("CPU频率修改失败,设置的值:%d", new_freq);
                }
            }
        }
        break;
        case 8:
        {
            int new_clk_freq = GUI::msgbox_number("TF卡时钟", 8, hal.pref.getInt("sd_clk_freq", 3500000));
            hal.pref.putInt("sd_clk_freq", new_clk_freq);
        }
        break;
        case 9:
        {
            int new_clk_freq = GUI::msgbox_number("I2C时钟", 7, hal.pref.getInt("I2C_freq", 100000));
            hal.pref.putInt("I2C_freq", new_clk_freq);
        }
        break;
        case 10:
        {
            int long_pres_time = GUI::msgbox_number("长按时间", 4, hal.pref.getInt("lpt", 25) * 10);
            hal.pref.putInt("lpt", long_pres_time / 10);
        }
        break;
        case 12:
        {
            if (GUI::msgbox_yn("提示", "是否联网更新CFU.json文件？"))
            {
                GUI::info_msgbox("提示", "正在联网更新CFU.json文件");
                hal.autoConnectWiFi();
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
                log_printf("Failed to open cfu file\n");
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
            for (JsonVariant item : updatelog)
            {
                u8g2Fonts.setCursor(2, 42 + ((i + 1) * 14));
                u8g2Fonts.printf("%d.%s", i + 1, item.as<const char *>());
                i++;
            }
            u8g2Fonts.setCursor(2, u8g2Fonts.getCursorY() + 14);
            u8g2Fonts.printf("%s", cfu["updateinfo"]["url"].as<const char *>());
            u8g2Fonts.setCursor(2, u8g2Fonts.getCursorY() + 14);
            u8g2Fonts.printf("%s", cfu["updateinfo"]["url1"].as<const char *>());
            display.display(true);
            hal.wait_input();
        }
        break;
        case 16:
        {
#define NVS_BACKUP_FILE "/System/nvs.bin"
            // 获取NVS分区信息
            const esp_partition_t *nvs_partition = esp_partition_find_first(
                ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
            if (!nvs_partition)
            {
                log_e("找不到NVS分区");
                break;
            }
            uint8_t *buffer;
            File file;
            hal.pref.end();
            if (GUI::msgbox_yn("NVS备份及恢复程序", "NVS备份文件存储在System文件夹中的nvs.bin文件", "备份", "恢复"))
            {
                buffer = (uint8_t *)malloc(nvs_partition->size);
                if (!buffer)
                {
                    log_e("内存分配失败");
                    break;
                }
                // 读取Flash数据
                if (esp_partition_read(nvs_partition, 0, buffer, nvs_partition->size) != ESP_OK)
                {
                    log_e("读取NVS失败");
                    free(buffer);
                    break;
                }
                // 写入文件
                file = LittleFS.open(NVS_BACKUP_FILE, "w");
                if (!file)
                {
                    GUI::info_msgbox("发生错误", "无法创建文件");
                    free(buffer);
                    break;
                }
                size_t written = file.write(buffer, nvs_partition->size);
                file.close();
                free(buffer);
                if (written != nvs_partition->size)
                {
                    GUI::info_msgbox("发生错误", "文件写入错误");
                    LittleFS.remove(NVS_BACKUP_FILE);
                }
                else
                {
                    log_printf("备份成功，大小：%d字节\n", written);
                    GUI::info_msgbox("操作成功", "已创建备份文件nvs.bin");
                }
            }
            else
            {
                if (!LittleFS.exists(NVS_BACKUP_FILE))
                {
                    GUI::info_msgbox("发生错误", "/System/nvs.bin不存在");
                }
                // 打开备份文件
                file = LittleFS.open(NVS_BACKUP_FILE, "r");
                if (!file)
                {
                    GUI::info_msgbox("发生错误", "无法打开备份文件");
                    return;
                }
                size_t fileSize = file.size();
                // 读取文件内容
                buffer = (uint8_t *)malloc(fileSize);
                if (!buffer)
                {
                    log_e("内存分配失败");
                    file.close();
                    return;
                }
                size_t read = file.read(buffer, fileSize);
                file.close();
                if (read != fileSize)
                {
                    log_e("读取不完整");
                    free(buffer);
                    return;
                }
                // 擦除并写入分区
                if (esp_partition_erase_range(nvs_partition, 0, nvs_partition->size) != ESP_OK)
                {
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
        case 17:
            hal.coredump_file();
            break;
        case 18:
        {
            int _time = GUI::msgbox_number("设置时间(ms)", 4, hal.pref.getUInt("menu_fast_t", 450));
            hal.pref.putUInt("menu_fast_t", _time);
        }
        break;
        case 19:
        {
            uint32_t baud = GUI::msgbox_number("设置波特率", 7, hal.pref.getUInt("uart_baud", 115200));
            hal.pref.putUInt("uart_baud", baud);
        }
        break;
        case 20: // OTA模式
        {
            if (GUI::msgbox_yn("是否连接WiFi", "如果使用WiFi，选择“确定”如果使用SoftAP，选择“取消”", "WiFi(右)", "SoftAP(左)"))
                hal.autoConnectWiFi();
            else
                hal.WiFiConfigManual();
            hal.can_sleep = false;
            ArduinoOTA.setPort(3232);
            ArduinoOTA
                .onStart([]()
                         {
                    String type;
                    if (ArduinoOTA.getCommand() == U_FLASH)
                        type = "sketch";
                    else // U_SPIFFS
                        type = "filesystem";

                    String msg = "开始更新 " + type;
                    GUI::info_msgbox("OTA开始", msg.c_str()); })
                .onEnd([]()
                       { GUI::info_msgbox("OTA结束", "更新完成"); })
                .onProgress([](unsigned int progress, unsigned int total)
                            {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "总计:   %07u字节\n已完成: %07u字节\n进度: %u%%", total, progress, (progress / (total / 100)));
                    GUI::info_msgbox("OTA进度", buf); })
                .onError([](ota_error_t error)
                         {
                    char* msg;
                    if (error == OTA_AUTH_ERROR) msg = "认证失败";
                    else if (error == OTA_BEGIN_ERROR) msg = "开始失败";
                    else if (error == OTA_CONNECT_ERROR) msg = "连接失败";
                    else if (error == OTA_RECEIVE_ERROR) msg = "接收失败";
                    else if (error == OTA_END_ERROR) msg = "结束失败";
                    else msg = "未知错误";

                    GUI::info_msgbox("OTA错误", msg); });
            ArduinoOTA.begin();
            char buf[128];
            sprintf(buf, "请在同局域网下使用此IP进行OTA\nip: %s", hal.getip().toString().c_str());
            GUI::info_msgbox("已就绪", buf);
            while (1)
            {
                ArduinoOTA.handle();
                if (hal.btnl.isPressing())
                {
                    while (hal.btnl.isPressing())
                        delay(20);
                    // free(server);
                    MDNS.end();
                    WiFi.disconnect(true);
                    hal.can_sleep = true;
                    end = false;
                    break;
                }
                delay(1);
            }
        }
        break;
        case 21:
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
                        display.display();
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
        case 22:
        {
            const char *str = GUI::fileDialog("请选择系统全局字体文件", false, NULL, NULL);
            if (str == NULL)
            {
                hal.pref.putString("system_font", String("default"));
                GUI::msgbox("提示", "已恢复系统全局字体为默认字体");
            }
            else
            {
                hal.pref.putString("system_font", String(str));
                u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());
                GUI::msgbox("提示", "系统全局字体设置完成");
            }
        }
        break;
        default:
            break;
        }
    }
}

void AppSettings::about()
{
    display.clearScreen();
    GUI::drawWindowsWithTitle("关于本设备");

    u8g2Fonts.setCursor(5, 30);
    xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
    u8g2Fonts.printf("设备名称: LiClock-S3  版本: %s %s %s", code_version, __DATE__, __TIME__);
    xSemaphoreGive(peripherals.i2cMutex);

    u8g2Fonts.setCursor(5, 45);
    u8g2Fonts.drawUTF8(5, 45, "CPU: Xtensa LX7 32-bit @ 240MHz x2 + ULP");

    u8g2Fonts.setCursor(5, 60);
    u8g2Fonts.printf("内存: 520KB SRAM + 16KB RTC_SRAM  存储: %d MB",
                     ESP.getFlashChipSize() / 1024 / 1024);

    size_t used = 0, total = 0, free = 0;
    total = LittleFS.totalBytes() / 1024;
    used = LittleFS.usedBytes() / 1024;
    free = total - used;

    u8g2Fonts.setCursor(5, 75);
    u8g2Fonts.printf("文件系统: %d%%  %dKB可用 总计 %dKB",
                     used * 100 / total,
                     free, total);

    u8g2Fonts.setCursor(5, 90);
    u8g2Fonts.printf("屏幕: RLCD 168x384 当前运行频率: %u MHz", getCpuFrequencyMhz());

    u8g2Fonts.setCursor(5, 105);
    u8g2Fonts.printf("电池容量: %d mAh  芯片: %s", hal.bat_info.capacity.design, ESP.getChipModel());

    u8g2Fonts.drawUTF8(5, 120, "GitHub: github.com/kanfandelong/LiClock-self-use");

    uint64_t unique_id[2];
    esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, unique_id, 128);
    uint8_t *chip_uid[2];
    chip_uid[0] = (uint8_t *)&unique_id[0];
    chip_uid[1] = (uint8_t *)&unique_id[1];
    u8g2Fonts.setCursor(5, 135);
    u8g2Fonts.printf("  Unique ID:   %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                     chip_uid[0][0], chip_uid[0][1], chip_uid[0][2], chip_uid[0][3],
                     chip_uid[0][4], chip_uid[0][5], chip_uid[0][6], chip_uid[0][7],
                     chip_uid[1][0], chip_uid[1][1], chip_uid[1][2], chip_uid[1][3],
                     chip_uid[1][4], chip_uid[1][5], chip_uid[1][6], chip_uid[1][7]);

    display.display(true);
    hal.wait_input();
}

void AppSettings::menu_SWQ()
{
    static const menu_select settings_menu_DS3231_SWQ[] =
        {
            {false, "< 返回", nullptr},
            {true, "bat振荡器禁用", "tf"},
            {true, "后备电源1Hz输出", "bat"},
            {false, "频率设置", nullptr},
            {true, "1Hz方波输出", "1hz"},
            {false, NULL, nullptr},
        };
    int res = 0;
    bool end = false;

    bool tf, bat, hz;
    byte frequency;
    frequency = hal.pref.getInt("frequency", 0);

    while (!end)
    {
        res = GUI::select_menu("振荡器设置", settings_menu_DS3231_SWQ, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 3:
            GUI::msgbox("提示", "0 = 1HZ\n1 = 1024Hz\n2 = 4096Hz\n3 = 8192Hz ");
            frequency = GUI::msgbox_number("输入频率代号", 1, 0);
            hal.pref.putInt("frequency", frequency);
            break;
        default:
            break;
        }
    }
    tf = hal.pref.getBool("tf", true);
    bat = hal.pref.getBool("bat", true);
    hz = hal.pref.getBool("1hz", true);
    xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
    Srtc.enable1Hz(hz);
    Srtc.enableOscillator(tf, bat, frequency);
    xSemaphoreGive(peripherals.i2cMutex);
}

void AppSettings::menu_DS3231()
{
    static const menu_item settings_menu_DS3231[] =
        {
            {NULL, "< 返回"},
            {NULL, "偏移量读取"},
            {NULL, "设置偏移量"},
            {NULL, "振荡器设置"},
            {NULL, "时间格式设置"},
            {NULL, "读取芯片温度"},
            {NULL, "读取当前时间"},
            {NULL, "振荡器停止标志"},
            {NULL, "完全手动设置时间"},
            {NULL, "32.786KHZ输出使能"},
            {NULL, NULL},
        };
    int res = 0;
    bool end = false;
    while (end == false)
    {
        res = GUI::menu("DS3231设置", settings_menu_DS3231, 8, 8, res);
        switch (res)
        {
        case 0:
            end = true;
            break;
        case 1:
        {
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            int8_t offset = Srtc.readOffset();
            xSemaphoreGive(peripherals.i2cMutex);
            char buf[128];
            sprintf(buf, "1lsb≈0.12ppm@25°C\n偏移量：%d\n间隔%d秒%s%d秒", offset, hal.pref.getInt("every", 100), hal.pref.getInt("rtc_offset", 0) < 0 ? "慢" : "快", abs(hal.pref.getInt("rtc_offset", 0)));
            GUI::msgbox("DS3231设置", buf);
        }
        break;
        case 2:
        {
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            int offset = peripherals.rtc.readOffset();
            offset = GUI::msgbox_number("输入偏移量", 3, offset);
            if (!(offset < 128 && offset > -128))
            {
                offset = 0;
                GUI::msgbox("提示", "偏移量超出范围\n-127~127\n已重置为0");
            }
            peripherals.rtc.writeOffset((int8_t)offset);
            xSemaphoreGive(peripherals.i2cMutex);
        }
        break;
        case 3:
            menu_SWQ();
            break;
        case 4:
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            if (GUI::msgbox_yn("时间格式", "设置DS3231的时间格式", "24h", "12h"))
            {
                Srtc.setClockMode(false);
            }
            else
            {
                Srtc.setClockMode(true);
            }
            xSemaphoreGive(peripherals.i2cMutex);
            break;
        case 5:
        {
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            float c = Srtc.getTemperature();
            xSemaphoreGive(peripherals.i2cMutex);
            char buf[30];
            sprintf(buf, "温度：%f℃", c);
            GUI::msgbox("芯片温度", buf);
        }
        break;
        case 6:
        {
            char buf[60];
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            sprintf(buf, "20%d年%d月%d日 星期%d %d:%d:%d", Srtc.getYear(), Srtc.getMonth(), Srtc.getDate(), Srtc.getDoW(), Srtc.getHour(), Srtc.getMinute(), Srtc.getSecond());
            xSemaphoreGive(peripherals.i2cMutex);
            log_printf("%s\n", buf);
            GUI::msgbox("DS3231时间", buf);
        }
        break;
        case 7:
        {
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            if (Srtc.oscillatorCheck())
            {
                GUI::msgbox("OSF标志", "true\n震荡器未停止过\n时间正常");
            }
            else
            {
                GUI::msgbox("OSF标志", "false\n振荡器停止过\n时间可能为错误");
            }
            xSemaphoreGive(peripherals.i2cMutex);
        }
        break;
        case 8:
        {
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            Srtc.setSecond(GUI::msgbox_number("输入秒", 2, 0));
            Srtc.setMinute(GUI::msgbox_number("输入分", 2, Srtc.getMinute()));
            Srtc.setHour(GUI::msgbox_number("输入时", 2, Srtc.getHour()));
            Srtc.setDoW(GUI::msgbox_number("输入星期", 1, Srtc.getDoW()));
            Srtc.setDate(GUI::msgbox_number("输入日", 2, Srtc.getDate()));
            Srtc.setMonth(GUI::msgbox_number("输入月", 2, Srtc.getMonth()));
            Srtc.setYear(GUI::msgbox_number("输入年的后两位", 2, Srtc.getYear()));
            xSemaphoreGive(peripherals.i2cMutex);
        }
        break;
        case 9:
            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            Srtc.enable32kHz(GUI::msgbox_yn("使能32.786KHZ", "", "开启", "关闭"));
            xSemaphoreGive(peripherals.i2cMutex);
            break;
        default:
            break;
        }
    }
}

int AppSettings::decToBin(int dec)
{
    if (dec < 0 || dec > 255)
    {
        GUI::info_msgbox("警告", "非法的输入值");
        log_e("非法的输入值");
        return 0;
    }
    int bin = 0;
    int base = 1;
    for (int i = 0; i < 8; ++i)
    {
        if (dec & 1)
        { // 检查最低位是否为1
            bin += base;
        }
        dec >>= 1;  // 右移一位
        base *= 10; // 更新权重
    }
    return bin;
}
// 将二进制数转换为十进制数
int AppSettings::binToDec(int bin)
{
    int decimal = 0;
    int base = 1; // 用于计算每一位的权重
    // 从右到左遍历二进制数，计算十进制数
    while (bin > 0)
    {
        int digit = bin % 10; // 获取二进制数的最低位
        decimal += digit * base;
        bin /= 10; // 去掉二进制数的最低位
        base *= 2; // 更新权重
    }
    return decimal;
}

void AppSettings::cheak_config(char *a)
{
    if (GUI::msgbox_yn("提示写入选中WIFI", a, "确定", "取消"))
    {
        config[PARAM_SSID] = a;
        hal.saveConfig();
    }
    else
        return;
    if (GUI::msgbox_yn("提示", "是否启动网页服务器配置密码,否则使用內部简易软键盘输入", "确定", "取消"))
    {
        hal.WiFiConfigManual();
        ESP.restart();
    }
    else
    {
        char *pwd = GUI::englishInput("输入WiFi密码");
        config[PARAM_PASS] = pwd;
        free(pwd);
        hal.saveConfig();
    }
}

void AppSettings::tfcard_info()
{
    display.clearScreen();
    GUI::drawWindowsWithTitle("TF卡信息");

    if (!peripherals.isSDLoaded())
    {
        GUI::info_msgbox("提示", "未插入TF卡或文件系统挂载失败，无法显示信息");
        hal.wait_input();
        return;
    }

    sdmmc_card_t *card = SD_MMC.get();
    if (!card)
    {
        GUI::info_msgbox("错误", "无法获取卡信息");
        hal.wait_input();
        return;
    }

    // ---------- 辅助函数：SD卡制造商名称 ----------
    auto get_manufacturer_name = [](int mfg_id) -> const char *
    {
        switch (mfg_id)
        {
        case 0x01:
            return "Panasonic";
        case 0x02:
            return "Toshiba";
        case 0x03:
            return "SanDisk";
        case 0x1B:
            return "Samsung";
        case 0x1D:
            return "AData";
        case 0x27:
            return "Phision";
        case 0x28:
            return "Lexar";
        case 0x31:
            return "Silicon Power";
        case 0x41:
            return "Kingston";
        case 0x74:
            return "Transcend";
        case 0x82:
            return "Sony";
        default:
            return "Unknown";
        }
    };

    // ---------- 辅助函数：eMMC制造商名称 ----------
    auto get_emmc_manufacturer_name = [](uint8_t mid) -> const char *
    {
        switch (mid)
        {
        case 0x02:
            return "Sandisk";
        case 0x11:
            return "Toshiba";
        case 0x13:
            return "Micron";
        case 0x15:
            return "Samsung";
        case 0x1A:
            return "Hynix";
        case 0x1C:
            return "Intel";
        case 0x37:
            return "Kingston";
        default:
            return "Unknown";
        }
    };

    // ---------- 辅助函数：OEM ID（两个ASCII字符）----------
    auto oem_id_to_str = [](int oem_id, char *out)
    {
        out[0] = (oem_id >> 8) & 0xFF;
        out[1] = oem_id & 0xFF;
        out[2] = '\0';
    };

    // ---------- 辅助函数：版本号（主.次）----------
    auto revision_to_str = [](int rev, char *out)
    {
        int major = (rev >> 4) & 0xF;
        int minor = rev & 0xF;
        sprintf(out, "%d.%d", major, minor);
    };

    int y = 30;                 // 起始Y坐标
    const int line_height = 14; // 行高

    // ---- 第1行：总容量 + 总扇区数 ----
    float totalMB = (float)SD_MMC.cardSize() / 1024.0 / 1024.0;
    u8g2Fonts.setCursor(5, y);
    u8g2Fonts.printf("大小: %.2fMB (扇区: %u)", totalMB, card->csd.capacity);
    y += line_height;

    // ---- 第2行：已用/总空间 + 可用空间 ----
    float usedMB = (float)SD_MMC.usedBytes() / 1024.0 / 1024.0;
    float totalFS_MB = (float)SD_MMC.totalBytes() / 1024.0 / 1024.0;
    float freeMB = totalFS_MB - usedMB;
    float percent = (totalFS_MB > 0) ? (usedMB * 100.0 / totalFS_MB) : 0;
    u8g2Fonts.setCursor(5, y);
    u8g2Fonts.printf("使用:%.2f/%.2fMB (%.1f%%) 可用:%.2fMB",
                     usedMB, totalFS_MB, percent, freeMB);
    y += line_height;

    // ---- 第3行：卡类型、总线宽度、DDR支持 ----
    const char *card_type_str = "未知";
    if (card->is_mmc)
    {
        card_type_str = "eMMC";
    }
    else if (card->is_sdio)
    {
        card_type_str = "SDIO";
    }
    else if (card->is_mem)
    {
        const uint32_t SD_OCR_SDHC_CAP = 1UL << 30;
        if (card->ocr & SD_OCR_SDHC_CAP)
        {
            uint64_t total_bytes = (uint64_t)card->csd.capacity * card->csd.sector_size;
            card_type_str = (total_bytes > 32ULL * 1024 * 1024 * 1024) ? "SDXC" : "SDHC";
        }
        else
        {
            card_type_str = "SDSC";
        }
    }
    int bus_width = 1 << card->log_bus_width;
    const char *ddr = card->is_ddr ? "是" : "否";
    u8g2Fonts.setCursor(5, y);
    u8g2Fonts.printf("类型:%s 总线:%d-bit DDR:%s", card_type_str, bus_width, ddr);
    y += line_height;

    // ==================== 根据卡类型显示CID信息 ====================
    if (card->is_mmc)
    {
        // ---------- eMMC 专用解析（基于字节数组，小端序CPU）----------
        uint8_t *cid = (uint8_t *)card->raw_cid; // cid[0] = 响应第一个字节

        uint8_t mid = cid[0]; // 制造商ID
        // 以下字段位置基于常见eMMC规范（JEDEC）及您的数据验证调整
        uint8_t cbx = (cid[1] >> 6) & 0x03;    // 设备类型（高2位）
        uint16_t oid = (cid[1] << 8) | cid[2]; // OEM/应用ID (16位)
        char pnm[7] = {0};                     // 产品名 (6字节)
        pnm[0] = cid[7];                       // 根据您的数据，产品名从字节7开始
        pnm[1] = cid[8];
        pnm[2] = cid[9];
        pnm[3] = cid[10];
        pnm[4] = cid[11];
        pnm[5] = cid[12];
        pnm[6] = '\0';                                                               // 确保字符串结束
        uint8_t prv = cid[13];                                                       // 产品版本 (8位)
        uint32_t psn = (cid[14] << 24) | (cid[15] << 16) | (cid[16] << 8) | cid[17]; // 序列号 (4字节，注意越界？实际cid只有16字节)
        // 修正：cid只有0-15，序列号可能占用4字节，例如cid[10]-cid[13]
        psn = (cid[10] << 24) | (cid[11] << 16) | (cid[12] << 8) | cid[13];
        uint8_t mdt = cid[14];      // 生产日期 (8位)
        uint8_t crc = cid[15] >> 1; // CRC7 (高7位)

        int year_code = (mdt >> 4) & 0x0F; // 高4位年份码
        int month = mdt & 0x0F;            // 低4位月份
        int year = 2013 + year_code;       // 现代eMMC基准2013

        int major = (prv >> 4) & 0x0F;
        int minor = prv & 0x0F;

        // ---- 第4行：制造商/OEM/产品 ----
        u8g2Fonts.setCursor(5, y);
        u8g2Fonts.printf("制造商:0x%02X (%s) CBX:%d OEM:0x%04X 产品:%-6s",
                         mid, get_emmc_manufacturer_name(mid), cbx, oid, pnm);
        y += line_height;

        // ---- 第5行：版本/序列号/生产日期 ----
        u8g2Fonts.setCursor(5, y);
        u8g2Fonts.printf("版本:%d.%d 序列号:%u 生产日期:%02d/%04d",
                         major, minor, psn, month, year);
        y += line_height;
    }
    else
    {
        // ---------- SD卡通用解析 ----------
        char oem_str[3];
        oem_id_to_str(card->cid.oem_id, oem_str);
        u8g2Fonts.setCursor(5, y);
        u8g2Fonts.printf("制造商:%s (0x%02X) OEM:%s",
                         get_manufacturer_name(card->cid.mfg_id),
                         card->cid.mfg_id, oem_str);
        y += line_height;

        // ---- 第5行：产品名 + 产品版本 ----
        char prod_name[9] = {0};
        strncpy(prod_name, card->cid.name, 8);
        char rev_str[8];
        revision_to_str(card->cid.revision, rev_str);
        u8g2Fonts.setCursor(5, y);
        u8g2Fonts.printf("产品:%-8s 版本:%s", prod_name, rev_str);
        y += line_height;

        // ---- 第6行：序列号 + 生产日期 ----
        int year_raw = card->cid.date >> 4;
        int month = card->cid.date & 0x0F;
        int year = 2000 + year_raw;
        u8g2Fonts.setCursor(5, y);
        u8g2Fonts.printf("序列号:%u 生产日期:%04d/%02d",
                         card->cid.serial, year, month);
        y += line_height;
    }

    // ==================== 通用信息（所有卡类型）====================
    // ---- 第6行（eMMC）/第7行（SD）：CSD版本 + MMC版本 ----
    u8g2Fonts.setCursor(5, y);
    u8g2Fonts.printf("CSD版本:%d MMC版本:%d", card->csd.csd_ver, card->csd.mmc_ver);
    y += line_height;

    // ---- 第7行（eMMC）/第8行（SD）：最大传输速度 + 读块长度 ----
    float speedMBs = (float)card->csd.tr_speed / 1024.0 / 1024.0;
    u8g2Fonts.setCursor(5, y);
    u8g2Fonts.printf("最大速度:%.2fMB/s 读块长:%u", speedMBs, card->csd.read_block_len);
    y += line_height;

    // ---- 第8行（eMMC）/第9行（SD）：命令类 + 卡特定信息 ----
    u8g2Fonts.setCursor(5, y);
    u8g2Fonts.printf("命令类:0x%03X", card->csd.card_command_class);
    if (card->is_mem && !card->is_mmc)
    {
        u8g2Fonts.printf(" SD版本:%d", card->scr.sd_spec);
    }
    if (card->is_mmc)
    {
        u8g2Fonts.printf(" 功率类:%d", card->ext_csd.power_class);
    }
    y += line_height;

    // ---- 第9行（eMMC）/第10行（SD）：最大频率 ----
    u8g2Fonts.setCursor(5, y);
    u8g2Fonts.printf("最大频率:%u kHz", card->max_freq_khz);
    // 最后一行不增加y，留空

    display.display(true);
    hal.wait_input();
}

void AppSettings::bat_info()
{
    display.clearScreen();
    GUI::drawWindowsWithTitle("电池状态");

    // 图表参数
    const int MAX_POINTS = 105;   // 最多存储100个历史点
    const int LEFT_WIDTH = 150;   // 左侧文字区域宽度
    const int CHART_LEFT = 160;   // 图表左边界
    const int CHART_RIGHT = 375;  // 图表右边界（留边距）
    const int CHART_TOP = 28;     // 图表上边界
    const int CHART_BOTTOM = 158; // 图表下边界
    const int POINT_SPACING = 2;  // 点水平间距（像素）

    // 静态历史数据（保留跨调用）
    static int16_t hist_volt[MAX_POINTS];
    static int16_t hist_curr[MAX_POINTS];
    static int16_t hist_soc[MAX_POINTS];
    static int count = 0;            // 当前有效点数
    static uint32_t last_update = 0; // 上次更新时间戳
    static int mode = 0;             // 0:电压 1:电流 2:SOC
    static bool lastRight = false;   // 右键上次状态

    // 等待所有按键释放（消除残留按下）
    while (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing())
    {
        delay(20);
    }

    // 主循环：左键退出
    while (true)
    {
        // 左键按下退出
        if (hal.btnl.isPressing())
        {
            break;
        }

        // 右键单击切换模式（检测上升沿）
        bool rightPressed = hal.btnr.isPressing();
        if (rightPressed && !lastRight)
        {
            mode = (mode + 1) % 3;
        }
        lastRight = rightPressed;

        auto &binfo = hal.bat_info; // 电池信息引用

        // 检查数据更新（update_time变化则添加新点）
        uint32_t now = binfo.update_time;
        if (now != last_update)
        {
            int16_t volt_mv = (int16_t)(binfo.voltage * 1000 + 0.5);
            int16_t curr_ma = binfo.current.avg;
            int16_t soc_val = binfo.soc;

            if (count < MAX_POINTS)
            {
                // 未满：直接追加
                hist_volt[count] = volt_mv;
                hist_curr[count] = curr_ma;
                hist_soc[count] = soc_val;
                count++;
            }
            else
            {
                // 已满：整体左移，新点放最后
                memmove(&hist_volt[0], &hist_volt[1], (MAX_POINTS - 1) * sizeof(int16_t));
                memmove(&hist_curr[0], &hist_curr[1], (MAX_POINTS - 1) * sizeof(int16_t));
                memmove(&hist_soc[0], &hist_soc[1], (MAX_POINTS - 1) * sizeof(int16_t));
                hist_volt[MAX_POINTS - 1] = volt_mv;
                hist_curr[MAX_POINTS - 1] = curr_ma;
                hist_soc[MAX_POINTS - 1] = soc_val;
            }
            last_update = now;
        }

        // --- 清屏并重绘标题 ---
        display.clearScreen();
        GUI::drawWindowsWithTitle("电池状态");

        // ========== 左侧文字信息 ==========
        int leftX = 10;
        int lineY = 30;
        const int lineH = 14;

        // 第1行：SOC + 状态
        u8g2Fonts.setCursor(leftX, lineY);
        if (binfo.flag.FC)
        {
            u8g2Fonts.printf("SOC:%d%% 已充满", binfo.soc);
        }
        else if (binfo.current.avg > 0)
        {
            u8g2Fonts.printf("SOC:%d%% 充电中", binfo.soc);
        }
        else
        { // avg <= 0
            u8g2Fonts.printf("SOC:%d%% 放电中", binfo.soc);
        }
        lineY += lineH;

        // 第2行：电压、温度
        u8g2Fonts.setCursor(leftX, lineY);
        u8g2Fonts.printf("%.2fV %.1f℃", binfo.voltage, binfo.temp);
        lineY += lineH;

        // 第3行：电流、功率
        u8g2Fonts.setCursor(leftX, lineY);
        u8g2Fonts.printf("%dmA %dmW", binfo.current.avg, binfo.power);
        lineY += lineH;

        // 第4行：当前容量
        u8g2Fonts.setCursor(leftX, lineY);
        u8g2Fonts.printf("容量:%dmAh", binfo.capacity.remain_f);
        lineY += lineH;

        // 第5行：时间估算
        u8g2Fonts.setCursor(leftX, lineY);
        if (binfo.flag.FC)
        {
            u8g2Fonts.print("已充满");
        }
        else if (binfo.current.avg > 0)
        {
            // 充电：剩余需充电量
            int32_t need = binfo.capacity.full_f - binfo.capacity.remain_f;
            if (need > 0 && abs(binfo.current.avg) > 0)
            {
                float hours = (float)need / abs(binfo.current.avg);
                if (hours >= 1.0)
                    u8g2Fonts.printf("充满:%dh%dm", (int)hours, (int)((hours - (int)hours) * 60));
                else
                    u8g2Fonts.printf("充满:%dm", (int)(hours * 60));
            }
            else
            {
                u8g2Fonts.print("充满:--");
            }
        }
        else if (binfo.current.avg < 0)
        {
            // 放电：剩余容量 / 放电电流
            if (abs(binfo.current.avg) > 0)
            {
                float hours = (float)binfo.capacity.remain_f / abs(binfo.current.avg);
                if (hours >= 1.0)
                    u8g2Fonts.printf("剩余:%dh%dm", (int)hours, (int)((hours - (int)hours) * 60));
                else
                    u8g2Fonts.printf("剩余:%dm", (int)(hours * 60));
            }
            else
            {
                u8g2Fonts.print("剩余:--");
            }
        }
        else
        {
            u8g2Fonts.print("无电流");
        }

        // ========== 右侧图表 ==========
        // 绘制图表边框
        display.drawRect(CHART_LEFT, CHART_TOP, CHART_RIGHT - CHART_LEFT, CHART_BOTTOM - CHART_TOP, TFT_BLACK);

        // 图表标题（模式名称）
        const char *modeNames[] = {"电压趋势", "电流趋势", "SOC趋势"};
        u8g2Fonts.setCursor(CHART_LEFT, CHART_TOP - 2);
        u8g2Fonts.print(modeNames[mode]);

        // 根据模式设定Y轴范围
        int16_t minVal, maxVal;
        if (mode == 0)
        { // 电压：2.5V ~ 4.2V（mV）
            minVal = 2500;
            maxVal = 4200;
        }
        else if (mode == 1)
        { // 电流：-200mA ~ +200mA
            minVal = -200;
            maxVal = 200;
        }
        else
        { // SOC：0% ~ 100%
            minVal = 0;
            maxVal = 100;
        }

        // 绘制Y轴刻度（左侧）
        const int numTicks = 5;
        for (int i = 0; i < numTicks; i++)
        {
            int tickY = CHART_TOP + i * (CHART_BOTTOM - CHART_TOP) / (numTicks - 1);
            // 短横线
            display.drawLine(CHART_LEFT - 3, tickY, CHART_LEFT, tickY, TFT_BLACK);

            // 计算刻度值
            int16_t val = maxVal - i * (maxVal - minVal) / (numTicks - 1);
            u8g2Fonts.setCursor(CHART_LEFT - 45, tickY);
            if (mode == 0)
            {
                // 电压显示两位小数，避免浮点printf
                int intPart = val / 1000;
                int fracPart = (val % 1000) / 10; // 百分位
                u8g2Fonts.printf("%d.%02dV", intPart, fracPart);
            }
            else if (mode == 1)
            {
                u8g2Fonts.printf("%dmA", val);
            }
            else
            {
                u8g2Fonts.printf("%d%%", val);
            }
        }

        // 绘制趋势线
        if (count > 1)
        {
            int startX = CHART_RIGHT - (count - 1) * POINT_SPACING;
            if (startX < CHART_LEFT)
                startX = CHART_LEFT; // 防溢出（实际MAX_POINTS已控制）

            int prevX = -1, prevY = -1;
            for (int i = 0; i < count; i++)
            {
                int x = CHART_RIGHT - (count - 1 - i) * POINT_SPACING; // i=0最旧，i=count-1最新
                int16_t val;
                if (mode == 0)
                    val = hist_volt[i];
                else if (mode == 1)
                    val = hist_curr[i];
                else
                    val = hist_soc[i];

                // Y坐标映射（注意屏幕Y向下）
                int y = CHART_BOTTOM - (int32_t)(val - minVal) * (CHART_BOTTOM - CHART_TOP) / (maxVal - minVal);
                // 边界裁剪
                if (y < CHART_TOP)
                    y = CHART_TOP;
                if (y > CHART_BOTTOM)
                    y = CHART_BOTTOM;

                if (i > 0 && prevX >= CHART_LEFT)
                {
                    display.drawLine(prevX, prevY, x, y, TFT_BLACK);
                }
                prevX = x;
                prevY = y;
            }
        }
        else if (count == 1)
        {
            // 单个点：绘制一个像素点
            int x = CHART_RIGHT;
            int16_t val = (mode == 0) ? hist_volt[0] : (mode == 1) ? hist_curr[0]
                                                                   : hist_soc[0];
            int y = CHART_BOTTOM - (int32_t)(val - minVal) * (CHART_BOTTOM - CHART_TOP) / (maxVal - minVal);
            if (y < CHART_TOP)
                y = CHART_TOP;
            if (y > CHART_BOTTOM)
                y = CHART_BOTTOM;
            display.drawPixel(x, y, TFT_BLACK);
        }

        // 刷新屏幕
        display.display();

        // 短暂延时，避免占用CPU过高
        delay(20);
    }

    // 退出后等待所有按键释放
    while (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing())
    {
        delay(20);
    }
}