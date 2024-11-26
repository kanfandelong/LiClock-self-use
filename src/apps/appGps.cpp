#include "AppManager.h"
#include "TinyGPSPlus.h"

#define RXD_2 36
#define TXD_2 32
#define GPS_POWER TXD_2

static const uint8_t gps_bits[] = {
   0x00, 0xf0, 0x0f, 0x00, 0x00, 0xfc, 0x3f, 0x00, 0x00, 0x1e, 0x78, 0x00,
   0x00, 0x0f, 0xe0, 0x00, 0x00, 0x07, 0xc0, 0x00, 0x80, 0xe3, 0xc7, 0x01,
   0x80, 0xf1, 0x8f, 0x01, 0x80, 0x71, 0x8e, 0x01, 0x80, 0x31, 0x8c, 0x01,
   0x80, 0x71, 0x8e, 0x81, 0x98, 0xf1, 0x8f, 0xf1, 0x9c, 0xe3, 0xc7, 0xfd,
   0x8f, 0x03, 0xc0, 0xd9, 0x07, 0x03, 0xe0, 0xc0, 0x03, 0x07, 0xe0, 0xc0,
   0x03, 0x0e, 0x70, 0xc0, 0x03, 0x0c, 0x38, 0xc0, 0x03, 0x1c, 0x1c, 0xc0,
   0x03, 0x38, 0x1c, 0xc0, 0x03, 0x70, 0x0e, 0xc0, 0x03, 0xe0, 0x07, 0xc0,
   0x03, 0xc0, 0x03, 0xc0, 0x03, 0x80, 0x01, 0xc0, 0x03, 0x00, 0x00, 0xc0,
   0x03, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0,
   0x03, 0x0e, 0x00, 0xc0, 0xe3, 0xff, 0x00, 0xfc, 0xff, 0xff, 0xcf, 0x7f,
   0x7f, 0xc0, 0xff, 0x0f, 0x07, 0x00, 0xfe, 0x00 };

TinyGPSPlus gps;
TinyGPSCustom numsv(gps, "GPGSV", 3);

RTC_DATA_ATTR bool has_buffer = false;
bool menu_open = false;
bool while_end = true;
u32_t failedChecksum_last = 0;
int part_update = 0;

static const menu_item menu_GPS[] =
{
    {NULL,"返回"},
    {NULL,"退出"},
    {NULL,"重启"},
    {NULL,"波特率"},
    {NULL,"接收机模式"},
    {NULL,"数据更新频率"},
    {NULL,"接收机供电模式"},
    {NULL,NULL},
};
void buffer_handler(void *){
    while(1)
    {
        while(Serial1.available())
        {
            gps.encode(Serial1.read());
        }
        delay(1);
    }
}
static void appgps_exit(){
    Serial1.end();
    digitalWrite(GPS_POWER, LOW);
    gpio_set_drive_capability(GPIO_NUM_32, GPIO_DRIVE_CAP_0);
    pinMode(GPS_POWER, INPUT);
}
void IRAM_ATTR RXD_interrupt(){
    if (Serial1.available())
    {
        has_buffer = true;
    }
}
void IRAM_ATTR button_interrupt(){
    menu_open = true;
}

class AppGps : public AppBase
{
private:
    /* data */
public:
    AppGps()
    {
        name = "gps";
        title = "定位";
        description = "外接定位模块数据解析";
        image = gps_bits;
    }
    void set();
    void GPS_restart();
    void GPS_band();
    void GPS_mode();
    void GPS_update_freq();
    void GPS_menu();
    void display_show();
    void setup();
};
static AppGps app;
void AppGps::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
static const menu_item menu_GPS_reset[] =
{
    {NULL,"返回"},
    {NULL,"热启动"},
    {NULL,"温重启"},
    {NULL,"冷启动"},
    {NULL,"出厂启动"},
    {NULL,NULL},
};
void AppGps::GPS_restart(){
    int res = GUI::menu("选择重启方式", menu_GPS_reset);
    switch(res){
        case 0:
        break;
        case 1:
            Serial1.print("$PCAS10,0*1C\r\n");
        break;
        case 2:
            Serial1.print("$PCAS10,1*1D\r\n");
        break;
        case 3:
            Serial1.print("$PCAS10,2*1E\r\n");
        break;
        case 4:
            Serial1.print("$PCAS10,3*1F\r\n");
        break;
        default:
            GUI::info_msgbox("警告", "非法的输入值");
        break;
    }
}
static const menu_item menu_GPS_band[] =
{
    {NULL,"返回"},
    {NULL,"4800bps"},
    {NULL,"9600bps"},
    {NULL,"19200bps"},
    {NULL,"38400bps"},
    {NULL,"57600bps"},
    {NULL,"115200bps"},
    {NULL,NULL},
};
void AppGps::GPS_band(){
    int res = GUI::menu("选择波特率", menu_GPS_band);
    if (res == 0){
        return;
    }
    long band;
    switch(res){
        case 1:
            band = 4800;
            Serial1.print("$PCAS01,0*1C\r\n");
        break;
        case 2:
            band = 9600;
            Serial1.print("$PCAS01,1*1D\r\n");
        break;
        case 3:
            band = 19200;
            Serial1.print("$PCAS01,2*1E\r\n");
        break;
        case 4:
            band = 38400;
            Serial1.print("$PCAS01,3*1F\r\n");
        break;
        case 5:
            band = 57600;
            Serial1.print("$PCAS01,4*18\r\n");
        break;
        case 6:
            band = 115200;
            Serial1.print("$PCAS01,5*15\r\n");
        break;
    }
    hal.pref.putLong("gps_baud", band);
    Serial1.end();
    Serial1.setPins(RXD_2, TXD_2);
    Serial1.begin(band);
}
static const menu_item menu_GPS_mode[] =
{
    {NULL,"返回"},
    {NULL,"便携模式"},
    {NULL,"静态模式"},
    {NULL,"步行模式"},
    {NULL,"车载模式"},
    {NULL,"航海模式"},
    {NULL,"航海模式(<1g)"},
    {NULL,"航海模式(<2g)"},
    {NULL,"航海模式(<4g)"},
    {NULL,NULL},
};
void AppGps::GPS_mode(){
    int res = GUI::menu("选择接收机模式", menu_GPS_mode);
    if (res == 0){
        return;
    }
    char str[20];  // 适当增大数组大小以容纳整个字符串
    sprintf(str, "PCAS11,%d", res);

    unsigned char xor_result = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        xor_result ^= str[i];  // 对每个字符进行异或
    }
    Serial1.printf("$%s*%02X\r\n", str, xor_result);
}
static const menu_item menu_GPS_update_freq[] =
{
    {NULL,"返回"},
    {NULL,"1Hz"},
    {NULL,"2Hz"},
    {NULL,"4Hz"},
    {NULL,"5Hz"},
    {NULL,"10Hz"},
    {NULL,"自定义"},
    {NULL,NULL},
};
int GPS_delay = 600;
void AppGps::GPS_update_freq(){
    int res = GUI::menu("选择数据更新频率", menu_GPS_update_freq);
    if (res == 0){
        return;
    }
    GPS_delay = 1000 / res;
    if (res == 6)
    {
        GPS_delay = GUI::msgbox_number("更新延迟(ms)", 4, 1000);
    }
    char str[20];  // 适当增大数组大小以容纳整个字符串
    sprintf(str, "PCAS02,%d", GPS_delay);

    unsigned char xor_result = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        xor_result ^= str[i];  // 对每个字符进行异或
    }
    Serial1.printf("$%s*%02X\r\n", str, xor_result);
}
void AppGps::GPS_menu(){
    int res = GUI::menu("菜单", menu_GPS);
    switch(res){
        case 0:
        break;
        case 1:
            while_end = false;
            appManager.goBack();
        break;
        case 2:
            GPS_restart();
        break;
        case 3:
            GPS_band();
            Serial1.print("$PCAS00*01\r\n");
        break;
        case 4:
            GPS_mode();
            Serial1.print("$PCAS00*01\r\n");
        break;
        case 5:
            GPS_update_freq();
        break;
        case 6:
            if (GUI::msgbox_yn("接收机供电模式", "1.自供电\n2.由ESP32提供电源", "1", "2")){
                hal.pref.putBool("gps_self_power", true);
            }else{
                hal.pref.putBool("gps_self_power", false);
                Serial1.end();
                Serial1.setPins(RXD_2, GPIO_NUM_NC);
                Serial1.begin(hal.pref.getLong("gps_baud", 9600));
                pinMode(GPS_POWER, OUTPUT);
                gpio_set_drive_capability(GPIO_NUM_32, GPIO_DRIVE_CAP_3);
                digitalWrite(GPS_POWER, HIGH);
            }
        break;
        default:
            GUI::info_msgbox("警告", "非法的输入值");
        break;
    }
}
void AppGps::display_show(){
    display.clearScreen();
    GUI::drawWindowsWithTitle("定位系统信息");
    u8g2Fonts.setCursor(2, 28);
    u8g2Fonts.printf("正在使用的卫星数:%02d 可见卫星数:%s", gps.satellites.value(), numsv.value());
    u8g2Fonts.setCursor(2, 42);
    u8g2Fonts.printf("经度:%.6f° 纬度:%.6f°", gps.location.lng(), gps.location.lat());
    u8g2Fonts.setCursor(2, 56);
    u8g2Fonts.printf("速度:%.2f m/s %.2f km/h 海拔高度:%.2fm", gps.speed.mps(), gps.speed.kmph(), gps.altitude.meters());
    u8g2Fonts.setCursor(2, 70);
    u8g2Fonts.printf("航向:%.2f°  水平精度递减:%d", gps.course.deg(), gps.hdop.value());
    u8g2Fonts.setCursor(2, 84);
    u8g2Fonts.printf("授时时间:%d.%d.%d %02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    u8g2Fonts.setCursor(2, 98);
    u8g2Fonts.printf("接收字节数:%d 校验通过语句:%d", gps.charsProcessed(), gps.passedChecksum());
    u8g2Fonts.setCursor(2, 112);
    u8g2Fonts.printf("电源电压:%d", hal.VCC);
    if (part_update <= 15){
        display.display(true);
        part_update++;
    }else{
        display.display();
        part_update = 0;
    }
}    
void AppGps::setup(){
    exit = appgps_exit;
    display.clearScreen();
    GUI::drawWindowsWithTitle("定位系统信息");
    display.display();
    if (hal.pref.getBool("gps_self_power", true)){
        Serial1.setPins(RXD_2 ,TXD_2);
        Serial1.begin(hal.pref.getLong("gps_baud", 9600)); 
    }else{
        Serial1.setPins(RXD_2, GPIO_NUM_NC);
        Serial1.begin(hal.pref.getLong("gps_baud", 9600));
        pinMode(GPS_POWER, OUTPUT);
        gpio_set_drive_capability(GPIO_NUM_32, GPIO_DRIVE_CAP_3);
        digitalWrite(GPS_POWER, HIGH);
    }
    //attachInterrupt(digitalPinToInterrupt(RXD_2), RXD_interrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTONC), button_interrupt, FALLING);
    xTaskCreatePinnedToCore(buffer_handler, "buffer_handler", 8192, NULL, 5, NULL, 0);
    //hal.task_buffer_handler();
    while(while_end)
    {
        delay(GPS_delay);
        if(menu_open){
            GPS_menu();
            menu_open = false;
            GPS_delay = GPS_delay - 400;
            if (GPS_delay < 0) GPS_delay = 0;
        }
        if (failedChecksum_last != gps.failedChecksum()){
            char msg[64];
            sprintf(msg, "总计错误数量：%d", gps.failedChecksum());
            GUI::info_msgbox("NMEA 校验错误", msg);
            failedChecksum_last = gps.failedChecksum();
        }
        if (gps.location.isUpdated() || gps.date.isUpdated() || gps.time.isUpdated() || gps.speed.isUpdated() || gps.altitude.isUpdated() || gps.satellites.isUpdated() || gps.course.isUpdated()){
            display_show();
        }
    }
}