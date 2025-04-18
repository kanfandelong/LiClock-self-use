#include "AppManager.h"
#include "TinyGPSPlus.h"

#define RXD_2 25
#define TXD_2 26
#define GPS_POWER TXD_2
// 存储上一个点的经纬度
double previousLat = 0.0;
double previousLng = 0.0;

// 总距离
double totalDistance = 0.0;

// 地球半径（单位：公里）
const double earthRadius = 6371.0;

// GPS信号丢失状态
bool gpsSignalLost = false;
unsigned long signalLostTime = 0;
const unsigned long signalLostThreshold = 5000; // 信号丢失阈值（5秒）

// 任务句柄
TaskHandle_t distanceTaskHandle = NULL;
TaskHandle_t serial_read = NULL;

// 标志位，用于控制任务是否运行
volatile bool isRunning = true;

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
bool task_end = false;
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
    {NULL,"清空里程数据"},
    {NULL,NULL},
};
void buffer_handler(void *){
    while(1)
    {
        while(Serial1.available())
        {
            gps.encode(Serial1.read());
        }
        if (task_end)
            vTaskDelete(NULL);
        delay(1);
    }
}
static void appgps_exit(){
    task_end = true;
    vTaskDelete(serial_read);
    vTaskDelete(distanceTaskHandle);
    Serial1.end();
    digitalWrite(GPS_POWER, LOW);
    detachInterrupt(digitalPinToInterrupt(PIN_BUTTONC));
    gpio_set_drive_capability(GPIO_NUM_26, GPIO_DRIVE_CAP_DEFAULT);
    pinMode(GPS_POWER, OUTPUT);
    pinMode(RXD_2, OUTPUT);
    hal.pref.putDouble("totalDistance", totalDistance);
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
    double calculateDistance(double lat1, double lng1, double lat2, double lng2);
    void GPS_restart();
    void GPS_band();
    void GPS_mode();
    void GPS_update_freq();
    void GPS_menu();
    const char *get_fangxiang(double deg);
    void display_show();
    void setup();
};
static AppGps app;

void distanceCalculationTask(void *parameter) {
  while (1) {
    // 检查GPS信号是否丢失
    if (!gps.location.isValid() || gps.satellites.value() == 0) {
      if (!gpsSignalLost) {
        gpsSignalLost = true;
        signalLostTime = millis(); // 记录信号丢失的时间
      } else if (millis() - signalLostTime > signalLostThreshold) {
        // Serial.println("GPS signal lost for too long, using speed estimation.");
        isRunning = false; // 暂停正常距离计算，启用速度预估
      }

      // 如果GPS信号丢失且启用了速度预估
      if (!isRunning && gps.speed.isValid()) {
        double speed = gps.speed.kmph(); // 获取速度（单位：公里/小时）
        double timeElapsed = 1.0; // 假设1秒更新一次
        double distance = (speed * timeElapsed) / 3600.0; // 计算距离（单位：公里）

        // 估算新位置（简单直线运动模型）
        double estimatedLat = previousLat + (distance / 111.32); // 纬度每度约111.32公里
        double estimatedLng = previousLng + (distance / (111.32 * cos(radians(previousLat)))); // 经度每度距离随纬度变化

        // 使用估算的位置进行计算
        double calculatedDistance = app.calculateDistance(previousLat, previousLng, estimatedLat, estimatedLng);
        totalDistance += calculatedDistance;

        // 更新上一个点的经纬度
        previousLat = estimatedLat;
        previousLng = estimatedLng;

        // Serial.print("Estimated Distance: ");
        // Serial.print(calculatedDistance, 6);
        // Serial.print(" km, Total Distance: ");
        // Serial.print(totalDistance, 6);
        // Serial.println(" km");
      }
    } else {
      if (gpsSignalLost) {
        gpsSignalLost = false;
        isRunning = true; // 恢复正常距离计算
        // Serial.println("GPS signal restored!");
      }

      // 如果任务正在运行，计算距离
      if (isRunning) {
        double currentLat = gps.location.lat();
        double currentLng = gps.location.lng();

        if (previousLat != 0.0 && previousLng != 0.0) {
          double distance = app.calculateDistance(previousLat, previousLng, currentLat, currentLng);
          totalDistance += distance;
        //   Serial.print("Distance: ");
        //   Serial.print(distance, 6);
        //   Serial.print(" km, Total Distance: ");
        //   Serial.print(totalDistance, 6);
        //   Serial.println(" km");
        }

        // 更新上一个点的经纬度
        previousLat = currentLat;
        previousLng = currentLng;
      }
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS); // 任务延迟1秒
  }
}
void AppGps::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

// 计算两点之间的距离（单位：公里）
double AppGps::calculateDistance(double lat1, double lng1, double lat2, double lng2) {
  double dLat = radians(lat2 - lat1);
  double dLng = radians(lng2 - lng1);

  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(radians(lat1)) * cos(radians(lat2)) *
             sin(dLng / 2) * sin(dLng / 2);

  double c = 2 * atan2(sqrt(a), sqrt(1 - a));

  return earthRadius * c;
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
    Serial1.print("$PCAS00*01\r\n");
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
                vTaskSuspend(serial_read);
                Serial1.end();
                Serial1.setPins(RXD_2, GPIO_NUM_NC);
                Serial1.begin(hal.pref.getLong("gps_baud", 9600));
                pinMode(GPS_POWER, OUTPUT);
                gpio_set_drive_capability(GPIO_NUM_26, GPIO_DRIVE_CAP_3);
                digitalWrite(GPS_POWER, HIGH);
                vTaskResume(serial_read);
            }
        break;
        case 7:
            {
                totalDistance = 0.0;
                GUI::msgbox("提示", "已重置总里程为0");
            }
        break;
        default:
            GUI::info_msgbox("警告", "非法的输入值");
        break;
    }

}/* 
0° - 22.5° : 北 (N)
22.5° - 67.5° : 东北 (NE)
67.5° - 112.5° : 东 (E)
112.5° - 157.5° : 东南 (SE)
157.5° - 202.5° : 南 (S)
202.5° - 247.5° : 西南 (SW)
247.5° - 292.5° : 西 (W)
292.5° - 337.5° : 西北 (NW)
337.5° - 360° : 北 (N) */
char deg_str[10];
const char *AppGps::get_fangxiang(double deg){
    deg = fmod(deg, 360.0);
    if (deg < 0.0) {
        deg += 360.0;
    }

    // 根据角度判断方向
    if (deg == 0.0 || deg == 360.0) {
        sprintf(deg_str, "正北(%.2f)°", deg);
        return deg_str;
    } else if (deg > 0.0 && deg < 44.5) {
        sprintf(deg_str, "北偏东%.2f°", deg);
        return deg_str;
    } else if (deg >= 44.5 && deg <= 45.5) {
        sprintf(deg_str, "东北(%.2f)°", 45.0 - deg);
        return deg_str;
    } else if (deg > 45.5 && deg < 90.0) {
        sprintf(deg_str, "东偏北%.2f°", 90.0 - deg);
        return deg_str;
    } else if (deg == 90.0) {
        sprintf(deg_str, "正东(%.2f)°", deg);
        return deg_str;
    } else if (deg > 90.0 && deg < 134.5) {
        sprintf(deg_str, "东偏南%.2f°", deg - 90.0);
        return deg_str;
    } else if (deg >= 134.5 && deg <= 135.5) {
        sprintf(deg_str, "东南(%.2f)°", 135.0 - deg);
        return deg_str;
    } else if (deg > 135.5 && deg < 180) {
        sprintf(deg_str, "南偏东%.2f°", 180.0 - deg);
        return deg_str;
    } else if (deg == 180.0) {
        sprintf(deg_str, "正南(%.2f)°", deg);
        return deg_str;
    } else if (deg > 180 && deg < 224.5) {
        sprintf(deg_str, "南偏西%.2f°", deg - 180.0);
        return deg_str;
    } else if (deg >= 224.5 && deg <= 225.5) {
        sprintf(deg_str, "西南(%.2f)°", 225.0 - deg);
        return deg_str;
    } else if (deg > 225.5 && deg < 270) {
        sprintf(deg_str, "西偏南%.2f°", 270.0 - deg);
        return deg_str;
    } else if (deg == 270.0) {
        sprintf(deg_str, "正西(%.2f)°", deg);
        return deg_str;
    } else if (deg > 270.0 && deg < 314.5) {
        sprintf(deg_str, "西偏北%.2f°", deg - 270.0);
        return deg_str;
    } else if (deg >= 314.5 && deg <= 315.5) {
        sprintf(deg_str, "西北(%.2f)°", 315.0 - deg);
        return deg_str;
    } else if (deg > 315.5 && deg < 360.0) {
        sprintf(deg_str, "北偏西%.2f°", 360.0 - deg);
        return deg_str;
    }
    return "-----"; 
}
void AppGps::display_show(){
    display.clearScreen();
    GUI::drawWindowsWithTitle("定位系统信息");
    u8g2Fonts.setCursor(2, 28);
    u8g2Fonts.printf("正在使用的卫星数:%02d 可见卫星数:%s", gps.satellites.value(), numsv.value());
    u8g2Fonts.setCursor(2, 42);
    u8g2Fonts.printf("经度:%.6f° 纬度:%.6f°", previousLng, previousLat);
    u8g2Fonts.setCursor(2, 56);
    u8g2Fonts.printf("速度:%.2f m/s %.2f km/h 海拔高度:%.2fm", gps.speed.mps(), gps.speed.kmph(), gps.altitude.meters());
    u8g2Fonts.setCursor(2, 70);
    u8g2Fonts.printf("航向:%.2f° %s 水平精度递减:%d", gps.course.deg(), get_fangxiang(gps.course.deg()), gps.hdop.value());
    u8g2Fonts.setCursor(2, 84);
    u8g2Fonts.printf("授时时间:%d.%d.%d %02d:%02d:%02d 总里程：%.3fKm", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second(), totalDistance);
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
    totalDistance = hal.pref.getDouble("totalDistance", 0.0);
    display.clearScreen();
    GUI::drawWindowsWithTitle("定位系统信息");
    display.display();
    if (hal.pref.getBool("gps_self_power", true)){
        pinMode(RXD_2, INPUT);
        pinMode(TXD_2, OUTPUT);
        Serial1.setPins(RXD_2 ,TXD_2);
        Serial1.begin(hal.pref.getLong("gps_baud", 9600)); 
    }else{
        Serial1.setPins(RXD_2, GPIO_NUM_NC);
        pinMode(RXD_2, INPUT);
        Serial1.begin(hal.pref.getLong("gps_baud", 9600));
        pinMode(GPS_POWER, OUTPUT);
        gpio_set_drive_capability(GPIO_NUM_26, GPIO_DRIVE_CAP_3);
        digitalWrite(GPS_POWER, HIGH);   
    }
    //attachInterrupt(digitalPinToInterrupt(RXD_2), RXD_interrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTONC), button_interrupt, FALLING);
    xTaskCreatePinnedToCore(buffer_handler, "buffer_handler", 2048, NULL, 5, &serial_read, 0);
      // 创建距离计算任务
    xTaskCreate(
        distanceCalculationTask, // 任务函数
        "Distance Calculation",  // 任务名称
        2048,                   // 任务堆栈大小
        NULL,                   // 任务参数
        4,                      // 任务优先级
        &distanceTaskHandle     // 任务句柄
    );
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