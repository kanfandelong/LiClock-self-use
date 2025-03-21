#include "hal.h"
#include <LittleFS.h>

void HAL::printBatteryInfo() {
    Serial.println("\n------ Battery Information ------");
    
    // 基础信息
    Serial.print("SOC: "); Serial.print(hal.bat_info.soc); Serial.println("%");
    Serial.print("SOH: "); Serial.print(hal.bat_info.soh); Serial.println("%");
    Serial.printf("Temperature: %.3f ℃\n", hal.bat_info.temp);
    Serial.printf("Voltage: %.3f mV\n", hal.bat_info.voltage);
    Serial.print("Avg Power: "); Serial.print(hal.bat_info.power); Serial.println(" mW");
  
    // 电流信息
    Serial.println("\n-- Current --");
    Serial.print("Average: "); Serial.print(hal.bat_info.current.avg); Serial.println(" mA");
    Serial.print("Max: "); Serial.print(hal.bat_info.current.max); Serial.println(" mA");
    Serial.print("Standby: "); Serial.print(hal.bat_info.current.stby); Serial.println(" mA");
  
    // 容量信息
    Serial.println("\n-- Capacity --");
    Serial.print("Remaining: "); Serial.print(hal.bat_info.capacity.remain); Serial.println(" mAh");
    Serial.print("Full: "); Serial.print(hal.bat_info.capacity.full); Serial.println(" mAh");
    Serial.print("Available: "); Serial.print(hal.bat_info.capacity.avail); Serial.println(" mAh");
    Serial.print("Available Full: "); Serial.print(hal.bat_info.capacity.avail_full); Serial.println(" mAh");
    Serial.print("Remaining Filtered: "); Serial.print(hal.bat_info.capacity.remain_f); Serial.println(" mAh");
    Serial.print("Full Filtered: "); Serial.print(hal.bat_info.capacity.full_f); Serial.println(" mAh");
    Serial.print("Design: "); Serial.print(hal.bat_info.capacity.design); Serial.println(" mAh");
  
    // 状态标志
    Serial.println("\n-- Flags --");
    Serial.print("Discharging: "); Serial.println(hal.bat_info.flag.DSG ? "Yes" : "No");
    Serial.print("Fully Charged: "); Serial.println(hal.bat_info.flag.FC ? "Yes" : "No");
    Serial.print("Charging Allowed: "); Serial.println(hal.bat_info.flag.CHG ? "Yes" : "No");
  
    Serial.println("---------------------------------\n");
}

void HAL::task_bat_info_update(){
    //while(1){    
        xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
        hal.bat_info.voltage = (float)lipo.voltage() / 1000.0;
        hal.bat_info.soc = lipo.soc(FILTERED);
        hal.bat_info.power = lipo.power();
        hal.bat_info.temp = (float)lipo.temperature(BATTERY) / 100.0;
        hal.bat_info.capacity.remain = lipo.capacity(REMAIN);
        hal.bat_info.capacity.full = lipo.capacity(FULL);
        hal.bat_info.capacity.avail = lipo.capacity(AVAIL);
        hal.bat_info.capacity.avail_full = lipo.capacity(AVAIL_FULL);
        hal.bat_info.capacity.remain_f = lipo.capacity(REMAIN_F);
        hal.bat_info.capacity.full_f = lipo.capacity(FULL_F);
        hal.bat_info.capacity.design = lipo.capacity(DESIGN);
        hal.bat_info.current.avg = lipo.current(AVG);
        hal.bat_info.current.max = lipo.current(MAX);
        hal.bat_info.current.stby = lipo.current(STBY);
        hal.bat_info.flag.CHG = lipo.chgFlag();
        hal.bat_info.flag.DSG = lipo.dsgFlag();
        hal.bat_info.flag.FC = lipo.fcFlag();
        xSemaphoreGive(peripherals.i2cMutex);
        delay(1000);
    //}
}

void task_hal_update(void *)
{
    while (1)
    {
        if (hal._hookButton)
        {
            while (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing())
            {
                hal.btnr.tick();
                hal.btnl.tick();
                hal.btnc.tick();
                delay(20);
            }
            hal.btnr.tick();
            hal.btnl.tick();
            hal.btnc.tick();
            while (hal._hookButton)
            {
                while (hal.SleepUpdateMutex)
                    delay(10);
                hal.update();
                delay(20);
            }
            while (hal.btnr.isPressing() || hal.btnl.isPressing() || hal.btnc.isPressing())
            {
                delay(20);
            }
        }
        while (hal.SleepUpdateMutex)
            delay(10);
        hal.SleepUpdateMutex = true;
        hal.btnr.tick();
        hal.btnl.tick();
        hal.btnc.tick();
        hal.SleepUpdateMutex = false;
        delay(20);
        while (hal.SleepUpdateMutex)
            delay(10);
        hal.SleepUpdateMutex = true;
        hal.btnr.tick();
        hal.btnl.tick();
        hal.btnc.tick();
        hal.update();
        hal.SleepUpdateMutex = false;
        delay(20);
    }
}
/**
 * @brief 按键音任务函数
 */
void task_btn_buzzer(void *){
    bool buz_l = false, buz_r = false, buz_c = false;
    int buz_freq = hal.pref.getInt("btn_buz_freq", 150);
    int buz_time = hal.pref.getInt("btn_buz_time", 100);
    while(1){
        if (hal.btnl.isPressing() && !buz_l){
            buz_l = true;
            buzzer.append(buz_freq, buz_time);
        }else if (hal.btnr.isPressing() && !buz_r){
            buz_r = true;
            buzzer.append(buz_freq, buz_time);
        }else if (hal.btnc.isPressing() && !buz_c){
            buz_c = true;
            buzzer.append(buz_freq, buz_time);
        }
        if (!hal.btnl.isPressing() && buz_l)
            buz_l = false;
        else if(!hal.btnr.isPressing() && buz_r)
            buz_r = false;
        if (!hal.btnc.isPressing() && buz_c)
            buz_c = false;
        delay(50);
    }
}

#include "esp_wifi.h"
/**
 * @brief 连接WiFi并检查连接状态
 * @param ssid 要连接的WiFi SSID
 * @param pass 要连接的WiFi密码
 * @return true表示连接成功，false表示连接失败
 */
bool HAL::connected_wifi(const char* ssid, const char* pass){
    WiFi.begin(ssid, pass);
    log_i("Connecting to %s", ssid);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(500);
    }
    if (WiFi.status() == WL_CONNECTED) {
        log_i("Connected to %s", ssid);
        return true;
    } else {
        log_i("Connection failed");
        return false;
    }
}
/**
 * @brief WIFI连接配置管理函数
 * @details 尝试连接默认WiFi，如果失败，则搜索并连接已保存的WiFi，以及在配置新的WiFi时自动保存至配置文件中
 * @note 如果默认WiFi连接失败，会尝试搜索并连接已保存的WiFi，如果失败，则会提示错误并返回false。如果连接成功，但PASS不匹配，则会更新JSON配置文件。
 * @return true表示连接成功，false表示连接失败
 */
bool HAL::wifi_config_manger(){
    bool isConnected = false;
    isConnected = connected_wifi(config[PARAM_SSID].as<const char *>(), config[PARAM_PASS].as<const char *>());

    if (!LittleFS.exists(wifi_config_file)){
        File file = LittleFS.open(wifi_config_file, "w");
        file.print(DEFAULT_WIFI_CONFIG);
        file.close();
    }
    // 读取JSON配置文件
    File configFile = LittleFS.open(wifi_config_file);
    if (!configFile) {
        Serial.println("Failed to open file for reading");
        return false;
    }

    StaticJsonDocument<2048> wifi_config;
    deserializeJson(wifi_config, configFile);
    configFile.close();

    if (!isConnected) {
        GUI::info_msgbox("错误", "默认WIFI连接失败，开始尝试保存过的可用WIFI");
        // 如果默认连接失败，搜索并连接已保存的WIFI
        JsonArray networks = wifi_config["networks"];
        WiFi.disconnect();
        int n = WiFi.scanNetworks(); // 扫描周围的WiFi网络
        if (n == 0) {
            log_w("没有找到可用的WiFi网络");
            GUI::info_msgbox("错误", "没有找到可用的WiFi网络");
            delay(3000);
            return false;
        } else {
            for (JsonObject network : networks) {
                const char* ssid = network["ssid"];
                const char* pass = network["pass"];
                for (int i = 0; i < n; ++i) {
                    if (strcmp(WiFi.SSID(i).c_str(), ssid) == 0) {
                        isConnected = connected_wifi(ssid, pass);
                        if (isConnected) {
                            config[PARAM_SSID] = ssid;
                            config[PARAM_PASS] = pass;
                            saveConfig();
                            char buf[128];
                            sprintf(buf, "成功连接：%s,默认WiFi已切换至此WiFi", WiFi.SSID().c_str());
                            GUI::info_msgbox("成功", buf);
                            break;
                        }
                    }
                }
                if (isConnected) {
                    break;
                }
            }
        }
    }

    // 如果连接成功，但PASS不匹配，更新JSON配置文件
    if (isConnected) {
        String currentSSID = WiFi.SSID();
        String currentPASS = WiFi.psk();
        JsonArray networks = wifi_config["networks"];
        bool found = false;
        for (JsonObject network : networks) {
            if (network["ssid"] == currentSSID) {
                found = true;
                if (network["pass"] != currentPASS) {
                    network["pass"] = currentPASS;
                    savewifiConfig(wifi_config);
                } 
                break;
            }
        }
        if (!found) {
            JsonObject newNetwork = networks.createNestedObject();
            newNetwork["ssid"] = currentSSID;
            newNetwork["pass"] = currentPASS;
            savewifiConfig(wifi_config);
        }
    } else {
        return false;
    }
    return true;
}
/**
 * @brief 保存WiFi配置文件
 * @param wifi_config 要保存的WiFi配置的json对象
 */
void HAL::savewifiConfig(StaticJsonDocument<2048>& wifi_config){
    File configFile = LittleFS.open(wifi_config_file, "w");
    if (!configFile)
    {
        Serial.println("Failed to open wifi config file for writing");
        return;
    }
    serializeJson(wifi_config, configFile);
    configFile.close();
}

void HAL::saveConfig()
{
    File configFile = LittleFS.open("/System/config.json", "w");
    if (!configFile)
    {
        Serial.println("Failed to open config file for writing");
        return;
    }
    serializeJson(config, configFile);
    configFile.close();
}
void HAL::loadConfig()
{
    File configFile = LittleFS.open("/System/config.json", "r");
    if (!configFile)
    {
        Serial.println("Failed to open config file");
        return;
    }
    deserializeJson(config, configFile);
    configFile.close();
}

void HAL::getTime()
{
    int64_t tmp;
    if ((peripherals.peripherals_current & PERIPHERALS_DS3231_BIT) && !dis_DS3231)
    {
        xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
        timeinfo.tm_year = peripherals.rtc.getYear() + 100;
        timeinfo.tm_mon = peripherals.rtc.getMonth() - 1;
        timeinfo.tm_mday = peripherals.rtc.getDate();
        timeinfo.tm_hour = peripherals.rtc.getHour();
        timeinfo.tm_min = peripherals.rtc.getMinute();
        timeinfo.tm_sec = peripherals.rtc.getSecond();
        if (peripherals.rtc.getDoW() == 7)
            timeinfo.tm_wday = 0;
        else
            timeinfo.tm_wday = peripherals.rtc.getDoW();
        now = mktime(&timeinfo);
        xSemaphoreGive(peripherals.i2cMutex);
    }
    else
    {
        time(&now);
        if (delta != 0 && lastsync < now)
        {
            // 下面修正时钟频率偏移
            tmp = now - lastsync;
            tmp *= delta;
            tmp /= every;
            now -= tmp;
        }
        localtime_r(&now, &timeinfo);
    }
}
#include <esp32\rom\sha.h>
    char key[16];  // 存储经过SHA-256运算后结果的前15个字符
/**
 * @brief 计算字符串的SHA-256哈希值，并返回前15个字符组成的字符串
 * @param str 要计算哈希值的字符串
 * @return 返回前15个字符组成的字符串
 */
char* HAL::get_char_sha_key(const char *str){
    SHA_CTX ctx;
    uint8_t temp[32];
    char hex_hash[65];  // 64 字节的十六进制字符串 + 1 字节的 null 终止符
    ets_sha_enable();
    ets_sha_init(&ctx);  // 初始化上下文
    ets_sha_update(&ctx, SHA2_256, (const uint8_t *)str, strlen(str) * 8); // 更新哈希值
    ets_sha_finish(&ctx, SHA2_256, temp); // 完成哈希计算
    // 将哈希值转换为十六进制字符串
    for (int j = 0; j < 32; j++) {
        sprintf(hex_hash + j * 2, "%02x", temp[j]);
    }
    // 截取前 15 个字符作为 key
    strncpy(key, hex_hash, 15);
    key[15] = '\0';  // 确保字符串以 null 结尾
    ets_sha_disable();
    return key;
}
/**
 * @brief 获取当前设备的IP地址（根据WIFI模式自动切换获取）
 * @return 返回IP地址
 */
IPAddress HAL::getip(){
    wifi_mode_t mode;
    mode = WiFi.getMode();
    if (mode == WIFI_MODE_STA){
        return WiFi.localIP();
    }else if (mode == WIFI_MODE_AP){
        return WiFi.softAPIP();
    }else{
        return IPAddress(0, 0, 0, 0);
    }
}

#define is_test 1
#define url_is_test 0
#define url_test "http://192.168.101.12:5500/firmware-info.json"
#define url_firmware "https://kanfandelong.github.io/liclock-web-flash/firmware-info.json"
#define CAcert_file "/System/_.github.io.crt"
/* const char* root_ca= \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
"MrY=\n" \
"-----END CERTIFICATE-----"; */
bool HAL::cheak_firmware_update(){
    log_i("开始检查固件更新...");
    if (!WiFi.isConnected())
        return false;
    else
        GUI::info_msgbox("提示", "检查固件更新...");
    HTTPClient http;
    char* ca_cert;
    if (LittleFS.exists(CAcert_file)){
        File CAcert = LittleFS.open(CAcert_file, "r");
        // 计算动态缓冲区大小（考虑CRLF可能被替换为LF）
        size_t file_size = CAcert.size();
        // 假设每个CRLF可能被替换为LF，最大需要file_size * 2的空间（极端情况）
        ca_cert = new (std::nothrow) char[file_size + 1]; // +1为终止符
          // 读取证书内容并替换CRLF为LF
        size_t index = 0;
        while (CAcert.available()) {
            char c = CAcert.read();
            if (c == '\r' && CAcert.peek() == '\n') {
                // 遇到CRLF，替换为LF
                ca_cert[index++] = '\n';
                CAcert.read(); // 跳过下一个字符（\n）
            } else {
                ca_cert[index++] = c;
            }
            // 防止缓冲区溢出
            if (index >= file_size * 2) {
                Serial.println("缓冲区溢出，证书可能被截断");
                break;
            }
        }
        ca_cert[index] = '\0'; // 添加终止符
    }
    else {
        GUI::msgbox("提示", "CA证书文件不存在!请上传CA证书到littlefs的System文件夹");
        return false;
    }
    log_i("CAcert: \n%s", ca_cert);
    http.setTimeout(20000); 
    if (url_is_test)
        http.begin((String)url_test);
    else
        http.begin((String)url_firmware, ca_cert);
    int httpCode = http.GET();
    run:
    if (httpCode == HTTP_CODE_OK){
        DynamicJsonDocument doc(2048);
        String http_str = http.getString();
        deserializeJson(doc, http_str);
        Serial.println("正在写入固件版本检查文件...");
        File f = LittleFS.open("/System/CFU.json", "w");
        f.print(http_str);
        f.close();
    } else {
        for (int i = 0; i < 5; i++)
        {
            Serial.println("连接失败，正在重试...");
            delay(1000);
            httpCode = http.GET();
            if (httpCode != HTTP_CODE_OK) {
                Serial.printf("请求失败，http code: %d, 重试次数: %d\n", httpCode, i + 1);
                delay(1000); // 等待1秒后重试
            }else
                goto run;
        }
        log_e("无法获取固件更新状态,http code:%d", httpCode);
        http.end();
        return false;
    }
    http.end();
    log_i("结束固件更新状态检查");
    return true;
}
/**
 * @brief 检查CPU频率，若低于80MHz则设置CPU频率为80MHz
 */
void HAL::cheak_freq()
{
    int freq = ESP.getCpuFreqMHz();
    if (freq < 80){
        Serial.end();
        bool cpuset = setCpuFrequencyMhz(80);
        Serial.begin(115200);
        Serial.setDebugOutput(true);
        ESP_LOGI("hal", "CpuFreq: %dMHZ -> 80MHZ", freq);
        F_LOG("CpuFreq: %dMHZ -> 80MHZ", freq);
        if(cpuset)
            {ESP_LOGI("hal", "ok");F_LOG("已调节CPU频率至可启用WIFI的状态");}
        else{ESP_LOGW("hal", "err");F_LOG("CPU频率调节失败");}
    }
}


void HAL::WiFiConfigSmartConfig()
{
    ESP_LOGI("hal", "WiFiConfigManual");
    cheak_freq();
#include "img_esptouch.h"
    display.fillScreen(GxEPD_WHITE);
    display.drawXBitmap(0, 0, esptouch_bits, 296, 128, GxEPD_BLACK);
    display.display();
    WiFi.beginSmartConfig();
    int count = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        ++count;
        if (count >= 240) // 120秒超时
        {
            Serial.println("SmartConfig超时");
            display.fillScreen(GxEPD_WHITE);
            u8g2Fonts.setCursor(70, 80);
            u8g2Fonts.print("SmartConfig超时");
            display.display();
            delay(100);
            hal.powerOff(false);
            ESP.restart();
        }
    }
    /*
    void esp_dpp_start();
    esp_dpp_start();
    */
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
        Serial.println("WiFi connected");
        config[PARAM_SSID] = WiFi.SSID();
        config[PARAM_PASS] = WiFi.psk();
        hal.saveConfig();
    }
}

#include <DNSServer.h>
void HAL::WiFiConfigManual()
{
    ESP_LOGI("hal", "WiFiConfigManual");
    cheak_freq();
    DNSServer dnsServer;
#include "img_manual.h"
    String passwd = String((esp_random() % 1000000000L) + 10000000L); // 生成随机密码
    String str = "WIFI:T:WPA2;S:WeatherClock;P:" + passwd + ";;";
    WiFi.softAP("WeatherClock", passwd.c_str());
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    beginWebServer();
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(7)];
    uint32_t last_millis = millis();
    bool show_qr = false, show_ssid = false, have_station = false;
    uint8_t StationNum = 0;
    while (1)
    {
        dnsServer.processNextRequest();
        updateWebServer();
        delay(5);
        if (WiFi.softAPgetStationNum() > 0)
        {
            last_millis = millis();
            if (!show_qr)
            {
                String str = "http://192.168.4.1";
                display.fillScreen(GxEPD_WHITE);
                //QRCode qrcode;
                //uint8_t qrcodeData[qrcode_getBufferSize(7)];
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
                u8g2Fonts.setCursor(120, ((128 - (14 * 6)) / 2) + 14);
                char buf[256];
                sprintf(buf, "如果使用的是电脑或手机未跳转至配置界面(移动数据可能会干扰跳转),请扫描二维码打开配置界面或浏览器打开http://192.168.4.1");
                GUI::autoIndentDraw(buf, 280, 120, 14);
                display.display();
                show_qr = true;
                have_station = true;
            }
        }
        if (WiFi.softAPgetStationNum() == 0 && have_station) {
            show_qr = false;
            show_ssid = false;
            have_station = false;
        }
        if (!show_ssid){
            display.fillScreen(GxEPD_WHITE);
            display.drawXBitmap(0, 0, wifi_manual_bits, 296, 128, GxEPD_BLACK);
            qrcode_initText(&qrcode, qrcodeData, 6, 0, str.c_str());
            Serial.println(qrcode.size);
            for (uint8_t y = 0; y < qrcode.size; y++)
            {
                // Each horizontal module
                for (uint8_t x = 0; x < qrcode.size; x++)
                {
                    display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode, x, y) ? GxEPD_BLACK : GxEPD_WHITE);
                }
            }
            display.setFont(&FreeSans9pt7b);
            display.setCursor(192, 124);
            display.print(passwd);
            display.display();
            show_ssid = true;
        }
        if (millis() - last_millis > 300000) // 10分钟超时
        {
            Serial.println("手动配置超时");
            display.fillScreen(GxEPD_WHITE);
            u8g2Fonts.setCursor(70, 80);
            u8g2Fonts.print("手动配置超时");
            display.display();
            delay(100);
            hal.powerOff(false);
            ESP.restart();
        }
        if (LuaRunning)
            continue;
        if (hal.btnl.isPressing())
        {
            while (hal.btnl.isPressing())
                delay(20);
            LittleFS.end();
            ESP.restart();
            break;
        }
    }
}
void HAL::ReqWiFiConfig()
{
    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setCursor(0, 20);
    u8g2Fonts.print("无法连接到WiFi");
    u8g2Fonts.setCursor(0, 40);
    u8g2Fonts.print("向左:网页配置");
    u8g2Fonts.setCursor(0, 60);
    u8g2Fonts.print("向右:SmartConfig");
    u8g2Fonts.setCursor(0, 80);
    u8g2Fonts.print("中间:离线模式");
    display.display();
    uint32_t last_millis = millis();
    int a = 0;
    while (1)
    {
        if (hal.btnl.isPressing())
        {
            WiFiConfigManual();
        }
        if (hal.btnr.isPressing())
        {
            WiFiConfigSmartConfig();
        }
        if (hal.btnc.isPressing())
        {
            WiFi.disconnect(true);
            config[PARAM_CLOCKONLY] = "1";
            hal.saveConfig();
            ESP.restart();
            break;
        }
        delay(5);
        if (millis() - last_millis > 60000) // 1分钟超时
        {
            Serial.println("\033[33mWiFi配置方式选择超时\033[32m");
            if (a < 4){
                Serial.println("尝试重连WiFi");
                autoConnectWiFi();
                a++;
                last_millis = millis();
            }
            else{
                break;
            }
        }
    }
    if (WiFi.isConnected() == false)
    {
        config[PARAM_CLOCKONLY] = "1";
        hal.saveConfig();
        ESP.restart();
    }else{
        a = 0;
    }
}
#include "esp_spi_flash.h"
#include "esp_rom_md5.h"
#include "esp_partition.h"
#define PARTITION_TOTAL 4
#define PARTITIONS_OFFSET 0x8000
#define PARTITION_SPIFFS (4 - 1)

void test_littlefs_size(bool format = true)
{
    uint32_t size_request; // 存储目的分区大小
    size_t size_physical = 0;
    esp_flash_get_physical_size(esp_flash_default_chip, &size_physical);
    size_request = size_physical - 0x300000;// - 0x1000
    if (hal.pref.getUInt("size", 0) != size_request)
    {
        Serial.println("检测到分区大小不一致，正在格式化");
        hal.pref.putUInt("size", size_request);
        LittleFS.format();
    }
}
void refresh_partition_table()
{
    md5_context_t ctx;
    static uint8_t table[16 * 20];
    static uint8_t table1[16 * 20];
    esp_rom_md5_init(&ctx);
    union
    {
        uint32_t size;
        uint8_t size_byte[4];
    } partition_size;
    uint32_t size_request; // 存储目的分区大小
    size_t size_physical = 0;
    esp_flash_get_physical_size(esp_flash_default_chip, &size_physical);
    size_request = size_physical - 0x300000;// - 0x1000
    esp_flash_read(esp_flash_default_chip, table, 0x8000, sizeof(table));
    memcpy(partition_size.size_byte, &table[16 * 2 * PARTITION_SPIFFS + 0x8], 4);
    Serial.printf("当前LittleFS分区大小%d\n期望LittleFS分区大小%d\n", partition_size.size, size_request);
    if (partition_size.size != size_request)
    {
        Serial.printf("正在修改分区表\n");
        partition_size.size = size_request;
        memcpy(&table[16 * 2 * PARTITION_SPIFFS + 0x8], partition_size.size_byte, 4);
        Serial.println("正在计算MD5\n");
        esp_rom_md5_update(&ctx, table, 16 * 2 * PARTITION_TOTAL);
        esp_rom_md5_final(&table[16 * (2 * PARTITION_TOTAL + 1)], &ctx);
        esp_flash_set_chip_write_protect(esp_flash_default_chip, false);
        Serial.println("\n正在写入");
        if (esp_flash_erase_region(esp_flash_default_chip, 0x8000, 0x1000) != ESP_OK)
        {
            Serial.println("擦除失败");
            while (1)
                vTaskDelay(1000);
        }
        if (esp_flash_write(esp_flash_default_chip, table, 0x8000, sizeof(table)) != ESP_OK)
        {
            Serial.println("写入失败");
            while (1)
                vTaskDelay(1000);
        }
        Serial.println("完成，正在校验结果");
        esp_flash_read(esp_flash_default_chip, table1, 0x8000, sizeof(table1));
        if (memcmp(table, table1, sizeof(table)) != 0)
        {
            Serial.println("校验失败");
            while (1)
                vTaskDelay(1000);
        }
        else
        {
            for (size_t i = 0; i < 16 * 12; i++)
            {
                Serial.printf("0x%02X ", table[i]);
                if ((i + 1) % 16 == 0)
                {
                    Serial.println();
                }
            }
        }
        ESP.restart();
    }
}
#include "TinyGPSPlus.h"
extern TinyGPSPlus gps;
extern RTC_DATA_ATTR bool has_buffer;
void hal_buffer_handler(void *){
    while(1)
    {
        if (has_buffer)
        {
            while(Serial1.available())
            {
                gps.encode(Serial1.read());
            }
        }else
        {
            delay(20);
        }
    }
}
void HAL::task_buffer_handler(){
    xTaskCreatePinnedToCore(hal_buffer_handler, "hal_buffer_handler", 8192, NULL, 5, NULL, 0);
}
void HAL::wait_input(uint32_t sleeptime){
    if (sleeptime == 0)
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    else 
        esp_sleep_enable_timer_wakeup(sleeptime * 1000000UL);
    if (hal.btn_activelow){
        gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTONL, 0);
        esp_sleep_enable_ext1_wakeup((1LL << PIN_BUTTONR), ESP_EXT1_WAKEUP_ALL_LOW);
    }else{
        esp_sleep_enable_ext1_wakeup((1ULL << PIN_BUTTONC) | (1ULL << PIN_BUTTONL) | (1ULL << PIN_BUTTONR), ESP_EXT1_WAKEUP_ANY_HIGH);
    }
    log_i("进入lightsleep");
    esp_light_sleep_start();
}
static const char esp_rst_str[12][64] = {"UNKNOWN", "POWERON", "EXT", "SW", "PANIC", "INT_WDT", "TASK_WDT", "WDT", "DEEPSLEEP", "BROWNOUT", "SDIO"};
static const char esp_sleep_str[13][64] = {"WAKEUP_UNDEFINED", "WAKEUP_ALL", "WAKEUP_EXT0", "WAKEUP_EXT1", "WAKEUP_TIMER", "WAKEUP_TOUCHPAD", "WAKEUP_ULP", "WAKEUP_GPIO", "WAKEUP_UART", "WAKEUP_WIFI", "WAKEUP_COCPU", "WAKEUP_COCPU_TRAP_TRIG", "WAKEUP_BT"};
bool HAL::init()
{
    int16_t total_gnd = 0;
    bool timeerr = false;
    bool initial = true;
    Serial.begin(115200);
    // 读取时钟偏移
    pref.begin("clock");

    int date = pref.getInt("CpuFreq", 80);
    int freq = ESP.getCpuFreqMHz();
    if (freq != date)
    {
        Serial.end();
        bool cpuset = setCpuFrequencyMhz(date);
        Serial.begin(115200);
        Serial.setDebugOutput(true);
        ESP_LOGI("HAL", "CpuFreq: %dMHZ -> %dMHZ ......", freq, date);
        if(cpuset){ESP_LOGI("HAL","ok");}
        else {ESP_LOGI("hal", "err");}
    }

    log_i("nvs分区可用空闲条目数量:%d", (int)pref.freeEntries());
    pinMode(PIN_BUTTONR, INPUT);
    pinMode(PIN_BUTTONL, INPUT);
    pinMode(PIN_BUTTONC, INPUT);
    total_gnd += digitalRead(PIN_BUTTONR);
    total_gnd += digitalRead(PIN_BUTTONL);
    total_gnd += digitalRead(PIN_BUTTONC);
    if (total_gnd <= 1)
    {
        btnl._buttonPressed = 1;
        btnr._buttonPressed = 1;
        btnc._buttonPressed = 1;
        btn_activelow = false;
    }
    else
    {
        ESP_LOGW("HAL", "此设备为旧版硬件，建议尽快升级以获得最佳体验。");
        btnl._buttonPressed = 0;
        btnr._buttonPressed = 0;
        btnc._buttonPressed = 0;
        btn_activelow = true;
    }
    esp_task_wdt_init(portMAX_DELAY, false);
    pinMode(PIN_CHARGING, INPUT);
    pinMode(PIN_SD_CARDDETECT, INPUT_PULLUP);
    pinMode(PIN_SDVDD_CTRL, OUTPUT);
    pinMode(PIN_SCL, OUTPUT | PULLUP);
    pinMode(PIN_SDA, OUTPUT | PULLUP);
    digitalWrite(PIN_SDVDD_CTRL, 1);
    digitalWrite(PIN_BUZZER, 0);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, 0);

    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    if (pref.getUInt("size", 0) != p->size)
    {
        pref.putUInt("size", p->size);
    }
    refresh_partition_table();
    if (pref.getUInt("lastsync") == 0)
    {
        pref.putUInt("lastsync", 1);  // 上次同步时间的准确时间
        pref.putInt("every", 100);    // 两次校准间隔多久
        pref.putInt("delta", 0);      // 这两次校准之间时钟偏差秒数，时钟时间-准确时间
        pref.putInt("upint", 2 * 60); // NTP同步间隔
    }
    lastsync = pref.getUInt("lastsync", 1); // 上次同步时间的准确时间
    every = pref.getInt("every", 100);      // 两次校准间隔多久
    delta = pref.getInt("delta", 0);        // 这两次校准之间时钟偏差秒数，时钟时间-准确时间
    upint = pref.getInt("upint", 2 * 60);   // NTP同步间隔
    auto_sleep_mv = pref.getInt("auto_sleep_mv", 2800);
    ppc = pref.getInt("ppc", 7230);
    // 系统“自检”
    dis_DS3231 = pref.getBool(get_char_sha_key("停用DS3231"), false);

    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED)
        initial = false;
    // 下面进行初始化

    WiFi.mode(WIFI_OFF);
#if defined(Queue)
    display.epd2.startQueue();
#endif
    display.init(115200, initial);
    display.setRotation(pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3));
    display.setTextColor(GxEPD_BLACK);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.begin(display);
    if (hal.btnl.isPressing() && (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED))
    {
        // 复位时检查左键是否按下，可以用于无限重启时临时关机
        powerOff(true);
        ESP.restart();
    }
    if (LittleFS.begin(false) == false)
    {
        display.fillScreen(GxEPD_WHITE);
        u8g2Fonts.setCursor(70, 80);
        u8g2Fonts.print("格式化LittleFS...");
        display.display();
        LittleFS.format();
        if (LittleFS.begin(false) == false)
        {
            Serial.println("LittleFS格式化失败");
            display.fillScreen(GxEPD_WHITE);
            u8g2Fonts.setCursor(70, 80);
            u8g2Fonts.print("LittleFS格式化失败");
            display.display(true);
            delay(100);
            powerOff(false);
            ESP.restart();
        }
        test_littlefs_size(false);
    }
    test_littlefs_size(true);
    if(LittleFS.exists("/System") == false){LittleFS.mkdir("/System");}
    if(LittleFS.exists("/dat") == false){LittleFS.mkdir("/dat");}
    if (LittleFS.exists("/System/config.json") == false)
    {
        Serial.println("正在写入默认配置");
        File f = LittleFS.open("/System/config.json", "w");
        f.print(DEFAULT_CONFIG);
        f.close();
    }
    if(LittleFS.exists("/System/log.txt"))
    {
        File log_file = LittleFS.open("/System/log.txt", "r");
        if(log_file.size() > 1024 * 50){
            log_file.close();
            LittleFS.remove("/System/log.txt");
        }
        log_file.close();
    }else{      
        File log_file = LittleFS.open("/System/log.txt", "a");
        // 添加 BOM 头
        log_file.write(0xEF);
        log_file.write(0xBB);
        log_file.write(0xBF);
        log_file.close();
    }
    F_LOG("ESP32复位,原因:ESP_RST_%s", esp_rst_str[esp_reset_reason()]);
    if(esp_reset_reason() == ESP_RST_DEEPSLEEP){
        F_LOG("复位为DeepSleep,唤醒源:ESP_SLEEP_%s", esp_sleep_str[esp_sleep_get_wakeup_cause()]);
    }
    loadConfig();
    setenv("TZ", config[Time_Zone].as<const char *>(), 1);
    tzset();
    peripherals.init();
    weather.begin();
    buzzer.init();
    TJpgDec.setCallback(GUI::epd_output);
    if (hal.pref.getBool(get_char_sha_key("按键音"), false))
        xTaskCreate(task_btn_buzzer, "btn_buzzer", 2048, NULL, 9, NULL);
    xTaskCreate(task_hal_update, "hal_update", 2048, NULL, 10, NULL);
    cmd.begin();
    //xTaskCreate(task_bat_info_update, "bat_info_update", 2048, NULL, 10, NULL);
    getTime();
    if ((timeinfo.tm_year < (2016 - 1900)))
    {
        timeerr = true;              // 需要同步时间
        pref.putUInt("lastsync", 1); // 清除上次同步时间，但不清除时钟偏移信息
        lastsync = 1;
    }
    if (initial == false && timeerr == false)
    {
        return false;
    }
    return true;
}

void HAL::rtc_offset(){
    // 计算时间间隔Δt（秒）
    //time_t deltaT = currentSync - previousSync;
    time_t deltaT = pref.getInt("every", 0);
    if (deltaT <= 0) return; // 避免除以零或负数
    
    // DS3231的误差ΔT（秒）
    // 负数代表DS3231慢于实际时间，正数代表DS3231快于实际时间
    time_t error = pref.getInt("rtc_offset", 0);
    
    // 计算误差率（ppm）
    double errorRate_ppm = (error / (double)deltaT) * 1e6;
    
    // 调整振荡器的频率。每个LSB代表大约0.12ppm的频率变化，正值会减慢时间基准，负值会加快时间基准
    // 计算校准值offset（注意符号方向）
    int8_t offset = round(-errorRate_ppm / 0.12); // 负号修正误差方向
    
    // 限制offset在±127范围内
    offset = constrain(offset, -127, 127);
    
    // 写入Aging Offset寄存器
    peripherals.rtc.writeOffset(offset);
}

bool HAL::autoConnectWiFi(bool need_wifi_config)
{
    log_i("hal", "autoConnectWiFi");
    cheak_freq();
    if (WiFi.isConnected())
    {
        return true;
    }
    // 下面连接WiFi
    if (config[PARAM_SSID] == "")
    {
        ReqWiFiConfig();
    }
    else
    {
        WiFi.setHostname("weatherclock");
        WiFi.mode(WIFI_STA);
        // WiFi.begin(config[PARAM_SSID].as<const char *>(), config[PARAM_PASS].as<const char *>());
        if (!wifi_config_manger())
        {
            if (need_wifi_config)
                hal.ReqWiFiConfig();
            else
                return false;
        }
        if (esp_wifi_set_max_tx_power(hal.pref.getUChar("wifitxpower", 78)) != ESP_OK)
        F_LOG("Failed set wifi max tx power to %.2f dBm", (float)hal.pref.getUChar("wifitxpower", 78) * 0.25);
        log_i("set wifi tx power to %.2f dBm", (float)hal.pref.getUChar("wifitxpower", 78) * 0.25);
    }
    // if (!WiFi.isConnected())
    // {
    //     if (WiFi.waitForConnectResult(20000) != WL_CONNECTED)
    //     {
    //         if (need_wifi_config)
    //             hal.ReqWiFiConfig();
    //         else
    //             return false;
    //     }
    // }
    F_LOG("成功连接:%s", WiFi.SSID().c_str());
    F_LOG("IP:%s", WiFi.localIP().toString().c_str());
    F_LOG("MAC:%s", WiFi.macAddress().c_str());
    F_LOG("信号强度:%d", WiFi.RSSI());
    sntp_stop();
    return true;
}

void HAL::searchWiFi()
{
    ESP_LOGI("hal", "searchWiFi");
    cheak_freq();
    WiFi.mode(WIFI_STA);
    hal.numNetworks = WiFi.scanNetworks(false, false, false, 500);
    if(hal.numNetworks == 0)
    {
        hal.numNetworks = WiFi.scanNetworks(false, false, false, 500);
        if(hal.numNetworks == 0)
        {
            Serial.printf("没有搜索到WIFI");
            F_LOG("没有搜索到WIFI");
        }
    }
}

extern RTC_DATA_ATTR bool ebook_run;
void HAL::set_sleep_set_gpio_interrupt()
{
    if (hal.btn_activelow)
    {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 0);
        esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ALL_LOW);
    }
    else
    {
        if (hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) == true && ebook_run == true){
            esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 1);
            esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ANY_HIGH);
        }else{
            esp_sleep_enable_ext1_wakeup((1ULL << PIN_BUTTONC) | (1ULL << PIN_BUTTONL) | (1ULL << PIN_BUTTONR), ESP_EXT1_WAKEUP_ANY_HIGH);
        }
    }
}

#include "driver/ledc.h"
static void pre_sleep()
{
    //cmd.end();
    peripherals.sleep();
    hal.set_sleep_set_gpio_interrupt();
    display.hibernate();
    buzzer.waitForSleep();
    LittleFS.end();
    hal.pref.end();
    hal.nvs_.end();
    delay(10);
    ledcDetachPin(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, 0);
}
static void wait_display()
{
#if defined(Queue)
    while(uxQueueMessagesWaiting(display.epd2.getQueue()) > 0)
    {
        delay(10);
    }
    while(display.epd2.isBusy())
    {
        delay(10);
    }
#endif
}
void HAL::goSleep(uint32_t sec)
{
    hal.getTime();
    long nextSleep = 0;
    if (sec != 0)
        nextSleep = sec;
    else
    {
        nextSleep = 1;
    }
    Serial.printf("下次唤醒:%ld s\n", nextSleep);
    nextSleep = nextSleep * 1000000UL;
    pre_sleep();
    esp_sleep_enable_timer_wakeup(nextSleep);
    wait_display();
    delay(1);
    if (noDeepSleep)
    {
        esp_light_sleep_start();
        display.init(115200, false);
        LittleFS.begin(false);
        peripherals.wakeup();
        ledcAttachPin(PIN_BUZZER, 0);
    }
    else
    {
        esp_deep_sleep_start();
    }
}

void HAL::powerOff(bool displayMessage)
{
    if (displayMessage)
    {
        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);
        u8g2Fonts.setCursor(120, 70);
        u8g2Fonts.print("已关机");
        display.display();
    }
    force_full_update = true;
    pre_sleep();
    WiFi.disconnect(true);
    set_sleep_set_gpio_interrupt();
    wait_display();
    delay(1);
    if (noDeepSleep)
    {
        esp_light_sleep_start();
        display.init(115200, false);
        LittleFS.begin(false);
        peripherals.wakeup();
    }
    else
    {
        esp_deep_sleep_start();
    }
}
void HAL::update(void)
{
    static int count = 0;
    if (count++ % 30 == 0)
    {
        count = 0;
        getTime();
    }

    long adc;
    adc = analogRead(PIN_ADC);
    adc = adc * ppc / 4096; // pref.getInt("ppc",7230)
    VCC = adc;
    // int auto_sleep_mv = hal.pref.getInt("auto_sleep_mv", 2800);
    char buf[128];
    if(hal.VCC < auto_sleep_mv)
    {
        sprintf(buf, "电池电压极低，当前电压为：%d mV，低于自动关机电压%d mV,设备自动关机", hal.VCC, auto_sleep_mv);
        GUI::info_msgbox("警告", buf);
        hal.powerOff(false);
    }
    if (adc > 4400)
    {
        USBPluggedIn = true;
    }
    else
    {
        USBPluggedIn = false;
    }
    if (digitalRead(PIN_CHARGING) == 0)
    {
        isCharging = true;
    }
    else
    {
        isCharging = false;
    }
}
int HAL::getNTPMinute()
{
    int res[] = {
        0,
        2 * 60,
        4 * 60,
        6 * 60,
        12 * 60,
        24 * 60,
        36 * 60,
        48 * 60,
    };
    int val = pref.getUChar(SETTINGS_PARAM_NTP_INTERVAL, 1);
    return res[val];
}
#include "img_goodnightmorning.h"
uint8_t RTC_DATA_ATTR night_sleep_today = -1; // 用于判断今天是否退出过夜间模式
uint8_t RTC_DATA_ATTR night_sleep = 0;
void HAL::checkNightSleep()
{
    if (hal.timeinfo.tm_year < (2016 - 1900))
    {
        Serial.println("[夜间模式] 时间错误，直接返回");
        return;
    }
    if (config[PARAM_SLEEPATNIGHT].as<String>() == "0")
    {
        Serial.println("[夜间模式] 夜间模式已禁用");
        return;
    }
    if (night_sleep_today == hal.timeinfo.tm_mday)
    {
        Serial.println("[夜间模式] 当天暂时退出夜间模式");
        return;
    }
    if (hal.timeinfo.tm_year < (2016 - 1900))
    {
        Serial.println("[夜间模式] 时间错误");
        night_sleep = 0;
        night_sleep_today = -1;
        return;
    }
    String tmp = config[PARAM_SLEEPATNIGHT_START].as<String>();
    // 转换时间数据到分钟
    int sleepStart = tmp.substring(0, 2).toInt() * 60 + tmp.substring(3, 5).toInt();
    tmp = config[PARAM_SLEEPATNIGHT_END].as<String>();
    int sleepEnd = tmp.substring(0, 2).toInt() * 60 + tmp.substring(3, 5).toInt();
    bool end_at_nextday = sleepStart > sleepEnd; // 是否在第二天结束
    int now = hal.timeinfo.tm_hour * 60 + hal.timeinfo.tm_min;
    uint8_t night_sleep_pend = 0; // 当前夜间模式状态
    if (end_at_nextday)
    {
        if (now >= sleepStart)
        {
            // 晚安
            night_sleep_pend = 1;
        }
        else if (now < sleepEnd)
        {
            // 早上好
            night_sleep_pend = 2;
        }
        else
        {
            night_sleep_pend = 0;
        }
    }
    else
    {
        int mid = sleepStart + sleepEnd;
        mid = mid / 2;
        if (now >= sleepStart && now <= sleepEnd)
        {
            if (now < mid)
            {
                night_sleep_pend = 1;
            }
            else
            {
                night_sleep_pend = 2;
            }
        }
        else
        {
            night_sleep_pend = 0;
        }
    }
    // 判断当前屏幕显示
    if (night_sleep != night_sleep_pend)
    {
        Serial.println("[DEBUG] 夜间模式重绘");
        night_sleep = night_sleep_pend;
        display.clearScreen();
        if (night_sleep == 1)
        {
            // 晚安
            display.drawXBitmap(0, 0, goodnight_bits, 296, 128, 0);
        }
        else if (night_sleep == 2)
        {
            // 早上好
            display.drawXBitmap(0, 0, goodmorning_bits, 296, 128, 0);
        }
        display.display(false);
    }
    // 判断是否进入睡眠
    if (night_sleep != 0)
    {
        hal.goSleep(1800); // 休眠半小时再看
    }
}
void HAL::setWakeupIO(int io1, int io2)
{
    _wakeupIO[0] = io1;
    _wakeupIO[1] = io2;
}
bool HAL::copy(File &newFile, File &file)
{
    log_i("开始文件复制");

    // 分配缓冲区内存
    const size_t bufferSize = 512;
    char *buf = (char *)malloc(bufferSize);
    if (!buf) {
        log_e("内存分配失败");
        F_LOG("内存分配失败");
        return false;
    }

    int fileSize = file.size();
    int fileSize_kb = fileSize / 1024;
    char filename[256];
    sprintf(filename, "%s", file.name());
    size_t bytesRead = 0;
    size_t totalBytesRead = 0;
    float progress = 0.0;
    unsigned long time = 0;
    newFile.setBufferSize((size_t)8192); // 设置缓冲区大小为8KB
    while ((bytesRead = file.readBytes(buf, bufferSize)) > 0) {
        // 将缓冲区中的数据写入到目标文件中
        size_t bytesWritten = newFile.write((uint8_t *)buf, bytesRead);
        if (bytesWritten != bytesRead) {
            log_e("文件在写入过程中发生错误");
            for(int i = 0;i < 3;i++){
                newFile.seek(-bytesWritten,SeekCur);
                bytesWritten = newFile.write((uint8_t *)buf, bytesRead);
                log_e("尝试重新写入，bytesWritten = %d", bytesWritten);
                if(bytesWritten == bytesRead){
                    goto tray;
                }
            }
            for(int i = 0;i < 3;i++)
            {
                buzzer.append(3000,200);
                delay(350);
            }
            GUI::msgbox("警告", "写入过程中发生错误");
            free(buf);
            return false;
        }
        tray:
        totalBytesRead += bytesRead;
        // 计算进度百分比
        // 如果进度有变化，则更新显示
        if (millis() - time >= 2000) {
            progress = ((float)totalBytesRead * 100.0) / (float)fileSize;
            display.clearScreen();
            u8g2Fonts.setCursor(1, 20);
            u8g2Fonts.printf("正在复制：%s", filename);
            u8g2Fonts.setCursor(1, 35);
            u8g2Fonts.printf("总计：%dKB 剩余：%dKB", fileSize_kb, (fileSize - totalBytesRead) / 1024);
            u8g2Fonts.setCursor(1, 50);
            u8g2Fonts.printf("进度: %0.2f%%", progress);
            u8g2Fonts.setCursor(1, 65);
            log_i("进度: %0.2f%%", progress);
            u8g2Fonts.printf("提示:长按左键中止复制");
            u8g2Fonts.setCursor(1, 80);
            u8g2Fonts.printf("提示:长按中键暂停，暂停后按任意键恢复复制");
            display.display(true);
            time = millis();
            newFile.flush();
        }
        if (GUI::waitLongPress(PIN_BUTTONL))
            return false;
        if (GUI::waitLongPress(PIN_BUTTONC)){
            display.fillRect(0, 22, 296, 22, GxEPD_WHITE);
            u8g2Fonts.setCursor(1, 20);
            u8g2Fonts.printf("暂停复制：%s", filename);
            display.display(true);
            hal.wait_input();
        }
    }

    // 确保显示最终完成的进度
    if (totalBytesRead == fileSize) { 
        display.clearScreen();
        u8g2Fonts.setCursor(1, 20);
        u8g2Fonts.printf("复制完成：%s", filename);
        u8g2Fonts.setCursor(1, 35);
        u8g2Fonts.printf("总计：%dKB 剩余：%dKB", fileSize_kb, 0);
        u8g2Fonts.setCursor(1, 50);
        u8g2Fonts.printf("进度: 100%%", progress);
        display.display(true);
    } else {
        log_w("文件复制不完整");
        free(buf);
        return false;
    }
    // 释放缓冲区内存
    free(buf);
    log_i("文件复制完成");
    file.close();
    newFile.close();
    return true;
}
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

void HAL::rm_rf(const char *path)
{
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;

    GUI::info_msgbox("提示", "正在删除文件夹...");
    // 打开目录
    if ((dp = opendir(path)) == NULL)
    {
        perror("opendir");
        return;
    }

    // 迭代读取目录中的文件
    while ((entry = readdir(dp)) != NULL)
    {
        // 获取文件的完整路径
        char filePath[256];
        sprintf(filePath, "%s/%s", path, entry->d_name);

        // 获取文件信息
        if (stat(filePath, &statbuf) == -1)
        {
            perror("lstat");
            continue;
        }

        // 判断是否是目录
        if (S_ISDIR(statbuf.st_mode))
        {
            // 忽略.和..目录
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            // 递归删除子目录
            rm_rf(filePath);
        }
        else
        {
            // 删除文件
            if (remove(filePath) != 0)
            {
                perror("remove");
            }
        }
    }

    // 关闭目录
    closedir(dp);

    // 删除空目录
    if (rmdir(path) != 0)
    {
        perror("rmdir");
    }
}

HAL hal;