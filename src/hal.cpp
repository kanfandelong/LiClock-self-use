#include "hal.h"
#include <LittleFS.h>
#include "git_info.h"

// 统一的文件系统接口，支持SD卡和LittleFS，路径以"/sd/"或"/littlefs/"开头来区分
// {
void HAL::cheak_sd()
{
    if ((!peripherals.isSDLoaded()) && digitalRead(PIN_SD_CARDDETECT) == LOW)
        peripherals.load(PERIPHERALS_SD_BIT);
}

File HAL::open(const char *path, const char *mode, const bool create)
{
    if (strncmp(path, "/sd/", 4) == 0)
    {
        cheak_sd();
        return SD_MMC.open(remove_path_prefix(path, "/sd"), mode, create);
    }
    else if (strncmp(path, "/littlefs/", 10) == 0)
    {
        return LittleFS.open(remove_path_prefix(path, "/littlefs"), mode, create);
    }
    else
    {
        return File();
    }
}

File HAL::open(const String &path, const char *mode, const bool create)
{
    return open(path.c_str(), mode, create);
}

bool HAL::exists(const char *path)
{
    if (strncmp(path, "/sd/", 4) == 0)
    {
        cheak_sd();
        return SD_MMC.exists(remove_path_prefix(path, "/sd"));
    }
    else if (strncmp(path, "/littlefs/", 10) == 0)
    {
        return LittleFS.exists(remove_path_prefix(path, "/littlefs"));
    }
    else
    {
        return false;
    }
}

bool HAL::exists(const String &path)
{
    return exists(path.c_str());
}

bool HAL::remove(const char *path)
{
    if (strncmp(path, "/sd/", 4) == 0)
    {
        cheak_sd();
        return SD_MMC.remove(remove_path_prefix(path, "/sd"));
    }
    else if (strncmp(path, "/littlefs/", 10) == 0)
    {
        return LittleFS.remove(remove_path_prefix(path, "/littlefs"));
    }
    else
    {
        return false;
    }
}

bool HAL::remove(const String &path)
{
    return remove(path.c_str());
}

bool HAL::rename(const char *pathFrom, const char *pathTo)
{
    if (strncmp(pathFrom, "/sd/", 4) == 0)
    {
        cheak_sd();
        return SD_MMC.rename(remove_path_prefix(pathFrom, "/sd"), pathTo);
    }
    else if (strncmp(pathFrom, "/littlefs/", 10) == 0)
    {
        return LittleFS.rename(remove_path_prefix(pathFrom, "/littlefs"), pathTo);
    }
    else
    {
        return false;
    }
}

bool HAL::rename(const String &pathFrom, const String &pathTo)
{
    return rename(pathFrom.c_str(), pathTo.c_str());
}

bool HAL::mkdir(const char *path)
{
    if (strncmp(path, "/sd/", 4) == 0)
    {
        cheak_sd();
        return SD_MMC.mkdir(remove_path_prefix(path, "/sd"));
    }
    else if (strncmp(path, "/littlefs/", 10) == 0)
    {
        return LittleFS.mkdir(remove_path_prefix(path, "/littlefs"));
    }
    else
    {
        return false;
    }
}

bool HAL::mkdir(const String &path)
{
    return mkdir(path.c_str());
}

bool HAL::rmdir(const char *path)
{
    if (strncmp(path, "/sd/", 4) == 0)
    {
        cheak_sd();
        return SD_MMC.rmdir(remove_path_prefix(path, "/sd"));
    }
    else if (strncmp(path, "/littlefs/", 10) == 0)
    {
        return LittleFS.rmdir(remove_path_prefix(path, "/littlefs"));
    }
    else
    {
        return false;
    }
}

bool HAL::rmdir(const String &path)
{
    return rmdir(path.c_str());
}

// }

void HAL::printBatteryInfo()
{
    log_printf("\n------ Battery Information ------\n");

    // 基础信息
    log_printf("SOC: %d%%\n", hal.bat_info.soc);
    log_printf("SOH: %d%%\n", hal.bat_info.soh);
    log_printf("Temperature: %.3f ℃\n", hal.bat_info.temp);
    log_printf("S3 Temperature: %.3f ℃\n", hal.bat_info.s3_temp);
    log_printf("Voltage: %.3f V\n", hal.bat_info.voltage);
    log_printf("Avg Power: %d mW\n", hal.bat_info.power);

    // 电流信息
    log_printf("\n-- Current --\n");
    log_printf("Average: %d mA\n", hal.bat_info.current.avg);
    log_printf("Max: %d mA\n", hal.bat_info.current.max);
    log_printf("Standby: %d mA\n", hal.bat_info.current.stby);

    // 容量信息
    log_printf("\n-- Capacity --\n");
    log_printf("Remaining: %d mAh\n", hal.bat_info.capacity.remain);
    log_printf("Full: %d mAh\n", hal.bat_info.capacity.full);
    log_printf("Available: %d mAh\n", hal.bat_info.capacity.avail);
    log_printf("Available Full: %d mAh\n", hal.bat_info.capacity.avail_full);
    log_printf("Remaining Filtered: %d mAh\n", hal.bat_info.capacity.remain_f);
    log_printf("Full Filtered: %d mAh\n", hal.bat_info.capacity.full_f);
    log_printf("Design: %d mAh\n", hal.bat_info.capacity.design);

    // 状态标志
    log_printf("\n-- Flags --\n");
    log_printf("Discharging: %s\n", hal.bat_info.flag.DSG ? "Yes" : "No");
    log_printf("Fully Charged: %s\n", hal.bat_info.flag.FC ? "Yes" : "No");
    log_printf("Charging Allowed: %s\n", hal.bat_info.flag.CHG ? "Yes" : "No");

    log_printf("Update Time: [%06d]\n", hal.bat_info.update_time);

    log_printf("---------------------------------\n");
}

void task_bat_info(void *)
{
    if (xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (!lipo.begin())
        {
            log_e("电量计初始化失败");
            xSemaphoreGive(peripherals.i2cMutex);
            log_w("正在终止电量计更新任务");
            vTaskDelete(NULL);
        }
        xSemaphoreGive(peripherals.i2cMutex);
    }
    TickType_t xDelay = 1200 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        if (xSemaphoreTake(peripherals.i2cMutex, 1000) == pdTRUE)
        {
            if (!lipo.begin())
            {
                log_e("电量计初始化失败");
                xSemaphoreGive(peripherals.i2cMutex);
                log_w("正在终止电量计更新任务");
                vTaskDelete(NULL);
            }
            hal.bat_info.voltage = (float)lipo.voltage() / 1000.0;
            hal.bat_info.soc = lipo.soc(FILTERED);
            hal.bat_info.soh = lipo.soh();
            hal.bat_info.power = lipo.power();
            hal.bat_info.temp = (float)lipo.temperature(INTERNAL_TEMP) / 100.0;
            hal.bat_info.s3_temp = temperatureRead();
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
            hal.bat_info.update_time = esp_log_timestamp();
            xSemaphoreGive(peripherals.i2cMutex);
            xDelay = 1500 / portTICK_PERIOD_MS;
        }
        else
        {
            xDelay = 3000 / portTICK_PERIOD_MS;
            log_w("I2C信号量获取超时");
        }
        // delay(1000);
        xTaskDelayUntil(&xLastWakeTime, xDelay);
    }
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
void task_btn_buzzer(void *)
{
    bool buz_l = false, buz_r = false, buz_c = false;
    int buz_freq = hal.pref.getInt("btn_buz_freq", 150);
    int buz_time = hal.pref.getInt("btn_buz_time", 100);
    while (1)
    {
        if (hal.btnl.isPressing() && !buz_l)
        {
            buz_l = true;
            buzzer.append(buz_freq, buz_time);
        }
        else if (hal.btnr.isPressing() && !buz_r)
        {
            buz_r = true;
            buzzer.append(buz_freq, buz_time);
        }
        else if (hal.btnc.isPressing() && !buz_c)
        {
            buz_c = true;
            buzzer.append(buz_freq, buz_time);
        }
        if (!hal.btnl.isPressing() && buz_l)
            buz_l = false;
        else if (!hal.btnr.isPressing() && buz_r)
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
bool HAL::connected_wifi(const char *ssid, const char *pass)
{
    WiFi.begin(ssid, pass);
    log_i("Connecting to %s", ssid);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < 10000)
    {
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        log_i("Connected to %s", ssid);
        return true;
    }
    else
    {
        log_w("Connection failed");
        log_w("failed reason: %d", WiFi.status());
        WiFi.disconnect();
        return false;
    }
}
/**
 * @brief WIFI连接配置管理函数
 * @details 尝试连接默认WiFi，如果失败，则搜索并连接已保存的WiFi，以及在配置新的WiFi时自动保存至配置文件中
 * @note 如果默认WiFi连接失败，会尝试搜索并连接已保存的WiFi，如果失败，则会提示错误并返回false。如果连接成功，但PASS不匹配，则会更新JSON配置文件。
 * @return true表示连接成功，false表示连接失败
 */
bool HAL::wifi_config_manger()
{
    bool isConnected = false;
    isConnected = connected_wifi(config[PARAM_SSID].as<const char *>(), config[PARAM_PASS].as<const char *>());

    if (!LittleFS.exists(wifi_config_file))
    {
        File file = LittleFS.open(wifi_config_file, "w");
        if (!file)
        {
            log_e("Failed to open file for w");
            return false;
        }
        file.print(DEFAULT_WIFI_CONFIG);
        file.close();
    }
    // 读取JSON配置文件
    File configFile = LittleFS.open(wifi_config_file);
    if (!configFile)
    {
        log_e("Failed to open file for reading");
        return false;
    }

    StaticJsonDocument<2048> wifi_config;
    // 或许可以使用DynamicJsonDocument
    deserializeJson(wifi_config, configFile);
    configFile.close();

    if (!isConnected)
    {
        GUI::info_msgbox("错误", "默认WIFI连接失败，开始尝试保存过的可用WIFI");
        delay(1000);
        // 如果默认连接失败，搜索并连接已保存的WIFI
        JsonArray networks = wifi_config["networks"];
        WiFi.disconnect();
        int n = WiFi.scanNetworks(); // 扫描周围的WiFi网络
        if (n == 0)
        {
            WiFi.scanDelete();
            log_w("没有找到可用的WiFi网络");
            GUI::info_msgbox("错误", "没有找到可用的WiFi网络");
            delay(1500);
            return false;
        }
        else
        {
            for (JsonObject network : networks)
            {
                const char *ssid = network["ssid"];
                const char *pass = network["pass"];
                for (int i = 0; i < n; ++i)
                {
                    if (strcmp(WiFi.SSID(i).c_str(), ssid) == 0)
                    {
                        isConnected = connected_wifi(ssid, pass);
                        if (isConnected)
                        {
                            config[PARAM_SSID] = ssid;
                            config[PARAM_PASS] = pass;
                            saveConfig();
                            char buf[128];
                            sprintf(buf, "成功连接：%s,默认WiFi已切换至此WiFi", WiFi.SSID().c_str());
                            GUI::info_msgbox("成功", buf);
                            delay(1500);
                            break;
                        }
                    }
                }
                if (isConnected)
                {
                    // WiFi.scanDelete();
                    break;
                }
            }
            WiFi.scanDelete();
        }
    }

    // 如果连接成功，但PASS不匹配，更新JSON配置文件
    if (isConnected)
    {
        String currentSSID = WiFi.SSID();
        String currentPASS = WiFi.psk();
        JsonArray networks = wifi_config["networks"];
        bool found = false;
        for (JsonObject network : networks)
        {
            if (network["ssid"] == currentSSID)
            {
                found = true;
                if (network["pass"] != currentPASS)
                {
                    network["pass"] = currentPASS;
                    savewifiConfig(wifi_config);
                }
                break;
            }
        }
        if (!found)
        {
            JsonObject newNetwork = networks.createNestedObject();
            newNetwork["ssid"] = currentSSID;
            newNetwork["pass"] = currentPASS;
            savewifiConfig(wifi_config);
        }
    }
    else
    {
        return false;
    }
    return true;
}
/**
 * @brief 保存WiFi配置文件
 * @param wifi_config 要保存的WiFi配置的json对象
 */
void HAL::savewifiConfig(StaticJsonDocument<2048> &wifi_config)
{
    File configFile = LittleFS.open(wifi_config_file, "w");
    if (!configFile)
    {
        log_e("Failed to open wifi config file for writing");
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
        log_e("Failed to open config file for writing");
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
        log_e("Failed to open config file");
        return;
    }
    deserializeJson(config, configFile);
    configFile.close();
}

#include "DS3231.h"

void HAL::getTime()
{
    if ((peripherals.peripherals_current & PERIPHERALS_DS3231_BIT) && !dis_DS3231)
    {
        xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
        struct tm utc_tm;
        /*utc_tm.tm_year = peripherals.rtc.getYear() + 100;   // 假设 getYear 返回 0-99
        utc_tm.tm_mon  = peripherals.rtc.getMonth() - 1;
        utc_tm.tm_mday = peripherals.rtc.getDate();
        utc_tm.tm_hour = peripherals.rtc.getHour();
        utc_tm.tm_min  = peripherals.rtc.getMinute();
        utc_tm.tm_sec  = peripherals.rtc.getSecond();
        utc_tm.tm_isdst = 0;   // UTC 无夏令时
        */
        DateTime time = RTClib::now();
        xSemaphoreGive(peripherals.i2cMutex);

        // 临时将系统时区改为 UTC，使 mktime 将输入解释为 UTC 时间
        // const char* oldTZ = getenv("TZ");
        // setenv("TZ", "UTC", 1);
        // tzset();
        // now = mktime(&utc_tm);
        now = time.unixtime();
        // 恢复原时区
        // if (oldTZ) {
        //     setenv("TZ", oldTZ, 1);
        // } else {
        //     unsetenv("TZ");
        // }
        // tzset();

        localtime_r(&now, &timeinfo);
    }
    else
    {
        time(&now);
        if (delta != 0 && lastsync < now)
        {
            int64_t tmp = (now - lastsync) * delta / every;
            now -= tmp;
        }
        localtime_r(&now, &timeinfo);
    }
}
#include <mbedtls/sha256.h>
// #include <esp32s3/rom/sha.h>
/**
 * @brief 计算字符串的SHA-256哈希值，并返回前15个字符组成的字符串
 * @param str 要计算哈希值的字符串
 * @return 返回前15个字符组成的字符串
 */
char *HAL::get_char_sha_key(const char *str, bool mode)
{
    static char key[16]; // 返回的静态缓冲区，注意多线程重入问题（当前使用场景是安全的）
    uint8_t hash[32];

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 表示 SHA-256
    mbedtls_sha256_update(&ctx, (const unsigned char *)str, strlen(str));
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    if (mode)
    {
        for (int i = 0; i < 15; i++)
        {
            key[i] = (hash[i] % 94) + 33; // 映射到可打印 ASCII
        }
    }
    else
    {
        char hex_hash[65];
        for (int j = 0; j < 32; j++)
        {
            sprintf(hex_hash + j * 2, "%02x", hash[j]);
        }
        strncpy(key, hex_hash, 15);
    }
    key[15] = '\0';
    return key;
}

String HAL::get_CAcert(const char *filePath)
{
    File CAcert = hal.open(filePath, "r");
    if (!CAcert)
    {
        log_e("Failed to open CAcert file");
        return String();
    }
    size_t file_size = CAcert.size();
    char ca_cert[file_size + 1]; // +1为终止符
    size_t index = 0;
    while (CAcert.available()) // 读取证书内容并替换CRLF为LF
    {
        char c = CAcert.read();
        if (c == '\r' && CAcert.peek() == '\n')
        {
            // 遇到CRLF，替换为LF
            ca_cert[index++] = '\n';
            CAcert.read(); // 跳过下一个字符（\n）
        }
        else
        {
            ca_cert[index++] = c;
        }
        // 防止缓冲区溢出
        if (index >= file_size + 1)
        {
            log_e("缓冲区溢出，证书可能被截断");
            break;
        }
    }
    ca_cert[index] = '\0'; // 添加终止符
    return String(ca_cert);
}

String HAL::get_yiyan(uint8_t maxlen)
{
    if (WiFi.isConnected())
    {
        HTTPClient http;
        String ca_cert = get_CAcert("/littlefs/System/GTS Root R4.crt");
        static const char *url_yiyan = "https://v1.hitokoto.cn/?c=c&c=a&c=d&c=f&c=i&encode=text&charset=utf-8&max_length=";
        String _url = String(url_yiyan) + String(maxlen);
        http.begin((String)_url, ca_cert.c_str());
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            http.end();
            return payload;
        }
        else
        {
            log_e("一言获取失败: %s", http.errorToString(httpCode).c_str());
            http.end();
            return String("一言获取失败");
        }
    }
    else
    {
        return String("网络未连接");
    }
}

/**
 * @brief 获取当前设备的IP地址（根据WIFI模式自动切换获取）
 * @return 返回IP地址
 */
IPAddress HAL::getip()
{
    wifi_mode_t mode;
    mode = WiFi.getMode();
    if (mode == WIFI_MODE_STA)
    {
        return WiFi.localIP();
    }
    else if (mode == WIFI_MODE_AP)
    {
        return WiFi.softAPIP();
    }
    else
    {
        return IPAddress(0, 0, 0, 0);
    }
}

#define is_test 1
#define url_is_test 0
#define url_test "http://192.168.101.12:5500/firmware-info.json"
#define url_firmware "https://kanfandelong.github.io/liclock-web-flash/firmware-info.json"
#define CAcert_file "/System/_.github.io.crt"
/* const char* root_ca= \
"-----BEGIN CERTIFICATE-----\n"  \
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
bool HAL::cheak_firmware_update()
{
    log_i("开始检查固件更新...");
    if (!WiFi.isConnected())
        return false;
    else
        GUI::info_msgbox("提示", "检查固件更新...");
    HTTPClient http;
    char *ca_cert;
    if (LittleFS.exists(CAcert_file))
    {
        File CAcert = LittleFS.open(CAcert_file, "r");
        // 计算动态缓冲区大小（考虑CRLF可能被替换为LF）
        size_t file_size = CAcert.size();
        // 假设每个CRLF可能被替换为LF，最大需要file_size * 2的空间（极端情况）
        ca_cert = new char[file_size + 1]; // +1为终止符
                                           // 读取证书内容并替换CRLF为LF
        size_t index = 0;
        while (CAcert.available())
        {
            char c = CAcert.read();
            if (c == '\r' && CAcert.peek() == '\n')
            {
                // 遇到CRLF，替换为LF
                ca_cert[index++] = '\n';
                CAcert.read(); // 跳过下一个字符（\n）
            }
            else
            {
                ca_cert[index++] = c;
            }
            // 防止缓冲区溢出
            if (index >= file_size * 2)
            {
                log_printf("缓冲区溢出，证书可能被截断");
                break;
            }
        }
        ca_cert[index] = '\0'; // 添加终止符
    }
    else
    {
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
    if (httpCode == HTTP_CODE_OK)
    {
        DynamicJsonDocument doc(2048);
        String http_str = http.getString();
        deserializeJson(doc, http_str);
        log_printf("正在写入固件版本检查文件...");
        File f = LittleFS.open("/System/CFU.json", "w");
        f.print(http_str);
        f.close();
    }
    else
    {
        for (int i = 0; i < 5; i++)
        {
            log_printf("连接失败，正在重试...");
            delay(1000);
            httpCode = http.GET();
            if (httpCode != HTTP_CODE_OK)
            {
                log_printf("请求失败，http code: %d, 重试次数: %d\n", httpCode, i + 1);
                delay(1000); // 等待1秒后重试
            }
            else
                goto run;
        }
        log_e("无法获取固件更新状态,http code:%d", httpCode);
        http.end();
        delete[] ca_cert;
        return false;
    }
    http.end();
    log_i("结束固件更新状态检查");
    delete[] ca_cert;
    return true;
}

extern void reinstall_ws_putc2();

/**
 * @brief 检查当前 CPU 频率，若低于指定频率或需要强制设置，则调整为指定频率
 *
 * 该函数用于确保 ESP32 的 CPU 运行在期望的频率上。用于系统初始化、低功耗控制或性能需求切换时调用。
 *
 * @param _freq 目标 CPU 频率（单位 MHz）
 * @param setfreq 是否强制设置频率（忽略当前频率是否高于目标频率）
 *
 * ### 主要逻辑：
 * - 获取当前 CPU 频率（单位 MHz）
 * - 如果当前频率 **小于**目标 `_freq` 或者设置了 `setfreq=true` 且频率不一致：
   - 停止串口通信以避免波特率错乱
   - 调用 `setCpuFrequencyMhz()` 设置新的 CPU 频率
   - 重新初始化串口并开启调试输出
   - 输出日志记录频率变化结果

 * ### 注意事项：
 * - 若未强制设置 (`setfreq=false`)，仅当当前频率低于 `_freq` 才会调整
 * - 默认检查参数为 80MHz（启用射频条件）
 * - 日志同时通过 `ESP_LOGI/W` 和自定义日志函数 [F_LOG](file://e:\LiClock-dev_multithread\include\A_Config.h#L58-L71) 输出
 */
void HAL::cheak_freq(int _freq, bool setfreq)
{
    int freq = ESP.getCpuFreqMHz();
    if (freq < _freq || (setfreq && (freq != _freq)))
    {
        bool cpuset = setCpuFrequencyMhz(_freq);
        uart->end();
        uart->setRxBufferSize(4096);
        uart->begin(pref.getUInt("uart_baud", 115200));
        uart->setDebugOutput(true);
        reinstall_putc2();
        reinstall_ws_putc2();
        cmd.SetCallback();
        log_i("CpuFreq: %dMHZ -> %dMHZ", freq, _freq);
        if (cpuset)
        {
            log_i("已调节CPU频率至目标频率");
        }
        else
        {
            log_e("CPU频率调节失败");
        }
    }
}

void HAL::WiFiConfigSmartConfig()
{
    ESP_LOGI("hal", "WiFiConfigManual");
    cheak_freq();
#include "img_esptouch.h"
    display.fillScreen(TFT_WHITE);
    display.drawXBitmap(0, 0, esptouch_bits, 296, 128, TFT_BLACK);
    display.display();
    WiFi.beginSmartConfig();
    int count = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        log_printf(".");
        ++count;
        if (count >= 240) // 120秒超时
        {
            log_printf("SmartConfig超时\n");
            display.fillScreen(TFT_WHITE);
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
        log_printf("WiFi connected\n");
        config[PARAM_SSID] = WiFi.SSID();
        config[PARAM_PASS] = WiFi.psk();
        hal.saveConfig();
    }
}

void HAL::WiFiConfigManual()
{
    ESP_LOGI("hal", "WiFiConfigManual");
    cheak_freq();
    DNSServer dnsServer;
#include "img_manual.h"
    String passwd = String((esp_random() % 1000000000L) + 10000000L); // 生成随机密码
    String str = "WIFI:T:WPA2;S:" + hal.pref.getString("hostname", String("LiClock-S3")) + ";P:" + passwd + ";;";
    WiFi.softAP(hal.pref.getString("hostname", String("LiClock-S3")).c_str(), passwd.c_str());
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
                display.fillScreen(TFT_WHITE);
                // QRCode qrcode;
                // uint8_t qrcodeData[qrcode_getBufferSize(7)];
                qrcode_initText(&qrcode, qrcodeData, 6, 2, str.c_str());
                log_printf("QR Code Size: %d\n", qrcode.size);
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
                u8g2Fonts.setCursor(120, ((128 - (14 * 6)) / 2) + 14);
                char buf[256];
                sprintf(buf, "如果使用的是电脑或手机未跳转至配置界面(移动数据可能会干扰跳转),请扫描二维码打开配置界面或浏览器打开http://192.168.4.1");
                GUI::autoIndentDraw(buf, 280, 120, 14);
                display.display();
                show_qr = true;
                have_station = true;
            }
        }
        if (WiFi.softAPgetStationNum() == 0 && have_station)
        {
            show_qr = false;
            show_ssid = false;
            have_station = false;
        }
        if (!show_ssid)
        {
            display.fillScreen(TFT_WHITE);
            display.drawXBitmap(0, 0, wifi_manual_bits, 296, 128, TFT_BLACK);
            qrcode_initText(&qrcode, qrcodeData, 6, 0, str.c_str());
            log_printf("QR Code Size: %d\n", qrcode.size);
            for (uint8_t y = 0; y < qrcode.size; y++)
            {
                // Each horizontal module
                for (uint8_t x = 0; x < qrcode.size; x++)
                {
                    display.fillRect(2 * x + 20, 2 * y + 20, 2, 2, qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE);
                }
            }
            display.setFont(&FreeSans9pt7b);
            display.setCursor(192, 109);
            display.print(hal.pref.getString("hostname", String("LiClock-S3")));
            display.setCursor(192, 124);
            display.print(passwd);
            display.display();
            show_ssid = true;
        }
        if (millis() - last_millis > 300000) // 10分钟超时
        {
            log_printf("手动配置超时\n");
            display.fillScreen(TFT_WHITE);
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
            dnsServer.stop();
            MDNS.end();
            WiFi.disconnect(true);
            hal.can_sleep = true;
            LittleFS.end();
            pref.end();
            ESP.restart();
            break;
        }
    }
}
void HAL::ReqWiFiConfig()
{
    display.fillScreen(TFT_WHITE);
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
            hal.pref.putBool(hal.get_char_sha_key("离线模式"), true);
            hal.saveConfig();
            ESP.restart();
            break;
        }
        delay(5);
        if (millis() - last_millis > 60000) // 1分钟超时
        {
            log_w("WiFi配置方式选择超时");
            if (a < 4)
            {
                log_i("尝试重连WiFi");
                autoConnectWiFi();
                a++;
                last_millis = millis();
            }
            else
            {
                break;
            }
        }
    }
    if (WiFi.isConnected() == false)
    {
        hal.pref.putBool(hal.get_char_sha_key("离线模式"), true);
        hal.saveConfig();
        ESP.restart();
    }
    else
    {
        a = 0;
    }
}
#include "esp_spi_flash.h"
#include "esp_rom_md5.h"
#include "esp_partition.h"
#define PARTITION_TOTAL 4
#define PARTITIONS_OFFSET 0x8000
#define PARTITION_SPIFFS (4 - 1)

/* void test_littlefs_size(bool format = true)
{
    uint32_t size_request; // 存储目的分区大小
    size_t size_physical = 0;
    esp_flash_get_physical_size(esp_flash_default_chip, &size_physical);
    size_request = size_physical - 0x310000;// - 0x1000
    if (hal.pref.getUInt("size", 0) != size_request)
    {
        log_println("检测到分区大小不一致，正在格式化");
        hal.pref.putUInt("size", size_request);
        LittleFS.format();
    }
} */
/* void refresh_partition_table()
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
    size_request = size_physical - 0x310000;// - 0x1000
    esp_flash_read(esp_flash_default_chip, table, 0x8000, sizeof(table));
    memcpy(partition_size.size_byte, &table[16 * 2 * PARTITION_SPIFFS + 0x8], 4);
    log_printf("当前LittleFS分区大小%d\n期望LittleFS分区大小%d\n", partition_size.size, size_request);
    if (partition_size.size != size_request)
    {
        log_printf("正在修改分区表\n");
        partition_size.size = size_request;
        memcpy(&table[16 * 2 * PARTITION_SPIFFS + 0x8], partition_size.size_byte, 4);
        log_println("正在计算MD5\n");
        esp_rom_md5_update(&ctx, table, 16 * 2 * PARTITION_TOTAL);
        esp_rom_md5_final(&table[16 * (2 * PARTITION_TOTAL + 1)], &ctx);
        esp_flash_set_chip_write_protect(esp_flash_default_chip, false);
        log_println("\n正在写入");
        if (esp_flash_erase_region(esp_flash_default_chip, 0x8000, 0x1000) != ESP_OK)
        {
            log_println("擦除失败");
            while (1)
                vTaskDelay(1000);
        }
        if (esp_flash_write(esp_flash_default_chip, table, 0x8000, sizeof(table)) != ESP_OK)
        {
            log_println("写入失败");
            while (1)
                vTaskDelay(1000);
        }
        log_println("完成，正在校验结果");
        esp_flash_read(esp_flash_default_chip, table1, 0x8000, sizeof(table1));
        if (memcmp(table, table1, sizeof(table)) != 0)
        {
            log_println("校验失败");
            while (1)
                vTaskDelay(1000);
        }
        else
        {
            for (size_t i = 0; i < 16 * 12; i++)
            {
                log_printf("0x%02X ", table[i]);
                if ((i + 1) % 16 == 0)
                {
                    log_println();
                }
            }
        }
        ESP.restart();
    }
} */
#include "driver/uart.h"
#include "driver/uart_wakeup.h"
void HAL::wait_input(uint32_t sleeptime)
{
    if (hal.can_light_sleep)
    {
        if (sleeptime == 0)
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        else
            esp_sleep_enable_timer_wakeup(sleeptime * 1000000UL);
        uart_wakeup_cfg_t uart_wakeup_cfg = {};
        uart_wakeup_cfg.wakeup_mode = UART_WK_MODE_ACTIVE_THRESH;
        uart_wakeup_cfg.rx_edge_threshold = 3;
        uart_wakeup_setup(UART_NUM_0, &uart_wakeup_cfg);
        esp_sleep_enable_uart_wakeup(UART_NUM_0);
        if (hal.btn_activelow)
        {
            gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable((gpio_num_t)PIN_BUTTONR, GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable((gpio_num_t)PIN_BUTTONL, GPIO_INTR_LOW_LEVEL);
        }
        else
        {
            gpio_wakeup_enable((gpio_num_t)PIN_BUTTONC, GPIO_INTR_HIGH_LEVEL);
            gpio_wakeup_enable((gpio_num_t)PIN_BUTTONR, GPIO_INTR_HIGH_LEVEL);
            gpio_wakeup_enable((gpio_num_t)PIN_BUTTONL, GPIO_INTR_HIGH_LEVEL);
        }
        esp_sleep_enable_gpio_wakeup();
        log_i("进入lightsleep");
        esp_light_sleep_start();
    }
    else
    {
        while (!(hal.btnc.isPressing() || hal.btnl.isPressing() || hal.btnr.isPressing()))
        {
            delay(100);
        }
    }
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UART)
    {
        log_i("uart唤醒");
    }
}

const char* get_exc_cause_name(uint32_t exc_cause) {
    switch (exc_cause) {
        case 0: return "IllegalInstructionCause";
        case 1: return "SyscallCause";
        case 2: return "InstructionFetchErrorCause";
        case 3: return "LoadStoreErrorCause";
        case 4: return "Level1InterruptCause";
        case 5: return "AllocaCause";
        case 6: return "IntegerDivideByZeroCause";
        case 7: return "Reserved for Tensilica";
        case 8: return "PrivilegedCause";
        case 9: return "LoadStoreAlignmentCause";
        case 10:
        case 11: return "Reserved for Tensilica";
        case 12: return "InstrPIFDataErrorCause";
        case 13: return "LoadStorePIFDataErrorCause";
        case 14: return "InstrPIFAddrErrorCause";
        case 15: return "LoadStorePIFAddrErrorCause";
        case 16: return "InstTLBMissCause";
        case 17: return "InstTLBMultiHitCause";
        case 18: return "InstFetchPrivilegeCause";
        case 19: return "Reserved for Tensilica";
        case 20: return "InstFetchProhibitedCause";
        case 21:
        case 22:
        case 23: return "Reserved for Tensilica";
        case 24: return "LoadStoreTLBMissCause";
        case 25: return "LoadStoreTLBMultiHitCause";
        case 26: return "LoadStorePrivilegeCause";
        case 27: return "Reserved for Tensilica";
        case 28: return "LoadProhibitedCause";
        case 29: return "StoreProhibitedCause";
        case 30:
        case 31: return "Reserved for Tensilica";
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39: return "CoprocessornDisabled";
        default: return "Reserved";
    }
}

#include "protected/my_coredump.h"

esp_err_t app_core_dump_get_summary(esp_core_dump_summary_t *summary) {
    if (!summary) return ESP_ERR_INVALID_ARG;

    // 1. 查找分区
    const esp_partition_t *core_part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (!core_part) {
        log_e("Core dump partition not found");
        return ESP_ERR_NOT_FOUND;
    }

    // 2. 在 PSRAM 中分配缓冲区（如果 PSRAM 不可用，则降级到内部 RAM）
    size_t buf_size = core_part->size;          // 你的分区是 64 KB，注意实际可用大小
    uint8_t *buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // MALLOC_CAP_SPIRAM
    if (!buf) {
        // 如果 PSRAM 分配失败，尝试内部 RAM（但可能不够）
        buf = (uint8_t *)malloc(buf_size);
        if (!buf) {
            log_e("Failed to allocate memory for core dump");
            return ESP_ERR_NO_MEM;
        }
    }

    // 3. 将分区内容全部读入缓冲区
    esp_err_t err = esp_partition_read(core_part, 0, buf, buf_size);
    if (err != ESP_OK) {
        log_e("Failed to read core dump partition");
        free(buf);
        return err;
    }

    uint8_t *ptr = buf + 24;

    elf_note_content_t target_notes[2] = {
        [0] = { .n_type = ELF_ESP_CORE_DUMP_EXTRA_INFO_TYPE, .n_ptr = NULL },
        [1] = { .n_type = ELF_ESP_CORE_DUMP_INFO_TYPE, .n_ptr = NULL }
    };

    app_core_dump_parse_note_section(ptr, target_notes, sizeof(target_notes) / sizeof(target_notes[0]));
    if (target_notes[0].n_ptr) {
        app_core_dump_summary_parse_extra_info(summary, target_notes[0].n_ptr);
    }
    if (target_notes[1].n_ptr) {
        elf_parse_version_info(summary, target_notes[1].n_ptr);
    }

    /* Following code assumes that task stack segment follows the TCB segment for the respective task.
     * In general ELF does not impose any restrictions on segments' order so this can be changed without impacting core dump version.
     * More universal and flexible way would be to retrieve stack start address from crashed task TCB segment and then look for the stack segment with that address.
     */
    elfhdr *eh = (elfhdr *)ptr;
    elf_phdr *phdr = (elf_phdr *)(ptr + eh->e_phoff);
    int flag = 0;
    for (unsigned int i = 0; i < eh->e_phnum; i++) {
        const elf_phdr *ph = &phdr[i];
        if (ph->p_type == PT_LOAD) {
            if (flag) {
                app_core_dump_summary_parse_exc_regs(summary, (void *)(ptr + ph->p_offset));
                app_core_dump_summary_parse_backtrace_info(&summary->exc_bt_info, (void *)ph->p_vaddr,
                                                           (void *)(ptr + ph->p_offset), ph->p_memsz);
                break;
            }
            if (ph->p_vaddr == summary->exc_tcb) {
                app_elf_parse_exc_task_name(summary, (void *)(ptr + ph->p_offset));
                flag = 1;
            }
        }
    }

    free(buf);

    log_i("Core dump summary parsed from PSRAM buffer");
    return ESP_OK;
}

void show_check_info() {
    // 获取核心转储摘要
    esp_core_dump_summary_t summary;
    app_core_dump_get_summary(&summary);

    // 1. 清屏并设置为白色背景
    display.setDrawWindow();
    display.fillScreen(TFT_WHITE);

    // 2. 绘制黑色标题栏，白色文字（使用稍大字体）
    display.fillRect(0, 0, 384, 24, TFT_BLACK);
    u8g2Fonts.setForegroundColor(TFT_WHITE);
    u8g2Fonts.setBackgroundColor(TFT_BLACK);
    u8g2Fonts.setFont(u8g2_font_logisoso22_tf);  // 大标题字体
    u8g2Fonts.setCursor(10, 20);                 // 左对齐，垂直居中
    u8g2Fonts.print("Liclock CRASH!");

    // 3. 切换回默认字体（12x12 等效字体），黑色文字白色背景
    // u8g2Fonts.setFont(u8g2_font_6x12_tf);       // 宽6高12，接近12x12点阵
    u8g2Fonts.setForegroundColor(TFT_BLACK);
    u8g2Fonts.setBackgroundColor(TFT_WHITE);
    u8g2Fonts.setCursor(4, 34);                 // 标题栏下方留白

    // 4. 打印详细信息
    u8g2Fonts.println("INFO:");
    u8g2Fonts.printf("TASK \"%s\" @ 0x%08lX\n", summary.exc_task, summary.exc_pc);
    u8g2Fonts.printf("EXCVADDR: 0x%08lX\n", summary.ex_info.exc_vaddr);
    u8g2Fonts.printf("CAUSE: %s\n", get_exc_cause_name(summary.ex_info.exc_cause));

    // 增加一个空行（通过换行或光标偏移）
    u8g2Fonts.setCursor(u8g2Fonts.getCursorX(), u8g2Fonts.getCursorY() + 6);

    // 5. 打印回溯信息
    if (summary.exc_bt_info.depth > 0) {
        u8g2Fonts.println("Backtrace:");
        int addr_per_line = 8;                 // 每行显示8个地址
        for (int i = 0; i < summary.exc_bt_info.depth; i++) {
            u8g2Fonts.printf("0x%08lX ", summary.exc_bt_info.bt[i]);
            if ((i + 1) % addr_per_line == 0 || i == summary.exc_bt_info.depth - 1) {
                u8g2Fonts.println();           // 换行
            }
        }
    }

    // 6. 底部显示 SHA 和版本信息（避免覆盖，从坐标 Y=148 开始，屏幕高度168，底部留20像素）
    u8g2Fonts.setCursor(4, 148);
    u8g2Fonts.printf("SHA: %s (%s-%s)", summary.app_elf_sha256, GIT_BRANCH, GIT_COMMIT_HASH_SHORT);

    // 刷新显示
    display.display(true);

    delay(1000);

    // 7. 等待按键
    hal.wait_input();

    // 8. 显示重启提示并重启
    display.fillScreen(TFT_WHITE);
    u8g2Fonts.setFont(u8g2_font_logisoso22_tf);
    u8g2Fonts.setForegroundColor(TFT_BLACK);
    u8g2Fonts.setBackgroundColor(TFT_WHITE);
    u8g2Fonts.setCursor((384 - u8g2Fonts.getUTF8Width("REBOOT...")) / 2, 84);
    u8g2Fonts.print("REBOOT...");
    display.display(true);

    esp_restart();
}

void HAL::coredump_file()
{
#define CoreDump_File "/System/coredump.elf"
    // 获取coredump分区信息
    const esp_partition_t *coredump_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
    if (!coredump_partition)
    {
        log_e("找不到coredump分区");
    }
    uint8_t *buffer;
    File file;
    buffer = (uint8_t *)malloc(coredump_partition->size);
    if (!buffer)
    {
        log_e("内存分配失败");
    }
    // 读取Flash数据
    if (esp_partition_read(coredump_partition, 0, buffer, coredump_partition->size) != ESP_OK)
    {
        log_e("读取coredump失败");
        free(buffer);
    }
    // 写入文件
    file = LittleFS.open(CoreDump_File, "w");
    if (!file)
    {
        GUI::info_msgbox("发生错误", "无法创建coredump文件");
        free(buffer);
    }
    size_t written = file.write(buffer, coredump_partition->size);
    file.close();
    free(buffer);
    if (written != coredump_partition->size)
    {
        GUI::info_msgbox("发生错误", "文件写入错误");
        LittleFS.remove(CoreDump_File);
    }
    else
    {
        log_i("已转储coredump分区至/System/coredump.elf，大小：%d字节", written);
        if (esp_reset_reason() == ESP_RST_PANIC)
        {    
            GUI::msgbox("系统异常", "zako~zako~,程序崩溃了呢~", 5);
            // show_check_info();
        }
        else
            GUI::msgbox("调试信息", "coredump分区已转储至/System/coredump.elf", 5);
    }
}

// 定义关机处理函数
static void shutdown_handler(void) {
    log_i("正在终止应用程序...");
    log_system_deinit();
    peripherals.sleep();
    LittleFS.end();
    hal.pref.end();
    ledcDetach(PIN_BUZZER);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, 0);  
}

static const char esp_rst_str[12][32] = {"UNKNOWN", "POWERON", "EXT", "SW", "PANIC", "INT_WDT", "TASK_WDT", "WDT", "DEEPSLEEP", "BROWNOUT", "SDIO"};
static const char esp_sleep_str[13][32] = {"WAKEUP_UNDEFINED", "WAKEUP_ALL", "WAKEUP_EXT0", "WAKEUP_EXT1", "WAKEUP_TIMER", "WAKEUP_TOUCHPAD", "WAKEUP_ULP", "WAKEUP_GPIO", "WAKEUP_UART", "WAKEUP_WIFI", "WAKEUP_COCPU", "WAKEUP_COCPU_TRAP_TRIG", "WAKEUP_BT"};

bool HAL::init()
{
    int16_t total_gnd = 0;
    bool timeerr = false;
    bool initial = true;
    bool fast_boot;
    pref.begin("clock");
    uint32_t uart_band = pref.getUInt("uart_baud", 115200);
    log_i("change band to %lu", uart_band);
#ifdef USE_CDC
    uart = &Serial;
#else
#if ARDUINO_USB_CDC_ON_BOOT
    uart = &Serial0;
#else
    uart = &Serial;
#endif
#endif
    uart->setRxBufferSize(4096);
    uart->begin(uart_band);
    uart->setDebugOutput(true);
    log_i("\n\n"
          "   © 2024 - 2026 看番の龙 | LiClock   \n"
          "          Powered by 看番の龙         \n"
          "       github.com/kanfandelong       \n");
    log_i("系统初始化，固件版本:%s  构建日期:%s %s 构建主机: GNU/Linux 6.6.87.2 Ubuntu24.04 x86_64", code_version, __DATE__, __TIME__);
    log_i("IDF:%s Arduino:%s 构建分支:%s 构建提交:%s", esp_get_idf_version(), ESP_ARDUINO_VERSION_STR, GIT_BRANCH, GIT_COMMIT_HASH_SHORT);
    sprintf(_tz, "%s", hal.pref.getString("TZ", String("CST-8")).c_str());
    log_i("TZ: %s", _tz);
    setenv("TZ", _tz, 1); // 设置时区为东八区
    tzset();
    esp_register_shutdown_handler(shutdown_handler);
    // 读取时钟偏移

    if (pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3) == 1 || pref.getBool("switch_btn"))
    {
        hal.btnl = OneButton(PIN_BUTTONR);
        hal.btnr = OneButton(PIN_BUTTONL);
    }
    else
    {
        hal.btnl = OneButton(PIN_BUTTONL);
        hal.btnr = OneButton(PIN_BUTTONR);
    }
    lpt = pref.getInt("lpt", 25);
    uint32_t longPress = lpt * 10;
    hal.btnl.setLongPressIntervalMs(longPress);
    hal.btnc.setLongPressIntervalMs(longPress);
    hal.btnr.setLongPressIntervalMs(longPress);

    int freq = pref.getInt("CpuFreq", 80);
    cheak_freq(freq);

    log_i("nvs分区可用空闲条目数量:%d", (int)pref.freeEntries());
    pinMode(PIN_BUTTONR, INPUT);
    pinMode(PIN_BUTTONL, INPUT);
    pinMode(PIN_BUTTONC, INPUT);
    total_gnd += digitalRead(PIN_BUTTONR);
    total_gnd += digitalRead(PIN_BUTTONL);
    total_gnd += digitalRead(PIN_BUTTONC);
    // if (total_gnd != 3) // 神秘错误,错误识别了按键电平,
    // {
    btnl._buttonPressed = 1;
    btnr._buttonPressed = 1;
    btnc._buttonPressed = 1;
    btn_activelow = false;
    // }
    // else
    // {
    //     ESP_LOGW("HAL", "此设备为旧版硬件，建议尽快升级以获得最佳体验。");
    //     btnl._buttonPressed = 0;
    //     btnr._buttonPressed = 0;
    //     btnc._buttonPressed = 0;
    //     btn_activelow = true;
    // }
    pinMode(PIN_CHARGING, INPUT_PULLUP);
    pinMode(PIN_SD_CARDDETECT, INPUT_PULLUP);
    pinMode(PIN_SCL, OUTPUT | PULLUP);
    pinMode(PIN_SDA, OUTPUT | PULLUP);

    pinMode(PIN_DAC_FMT, OUTPUT);
    digitalWrite(PIN_DAC_FMT, 0);
    pinMode(PIN_DAC_EN, OUTPUT);
    digitalWrite(PIN_DAC_EN, 0);
    pinMode(PIN_DAC_XSMT, OUTPUT);
    digitalWrite(PIN_DAC_XSMT, 0);
    pinMode(PIN_I2S_MCLK, OUTPUT);
    digitalWrite(PIN_I2S_MCLK, 0);

    pinMode(PIN_SDVDD_CTRL, OUTPUT);
    digitalWrite(PIN_SDVDD_CTRL, 1);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, 0);

    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    if (pref.getUInt("size", 0) != p->size)
    {
        pref.putUInt("size", p->size);
    }
    // refresh_partition_table();
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
    fast_boot = pref.getBool("fast_boot");
    // 系统“自检”
    dis_DS3231 = pref.getBool(get_char_sha_key("停用DS3231"), false);

    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED)
        initial = false;
    // 下面进行初始化

    WiFi.mode(WIFI_OFF);
#if defined(Queue)
    // display.epd2.startQueue(hal.pref.getUInt("display_list", 3), hal.pref.getUInt("disp_priority", 1));
#endif
    // // display.epd2.T5D_mode(!pref.getBool("UC8151C"));
    log_i("初始化屏幕...");
    display.set_te_interrupt_mode(pref.getInt("te_int_mode", RISING));
    display.begin(initial);
    display.display_Inversion(pref.getBool("Inversion", true));
    display.setRotation(pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3));
    display.setTextColor(TFT_BLACK);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(TFT_BLACK);
    u8g2Fonts.setBackgroundColor(TFT_WHITE);

    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312_self, 209899L);

    u8g2Fonts.begin(display);

    // display.epd2.PLL_set(pref.getUInt("pllset", 0x3C)); // 配置屏幕PLL，默认为50HZ
    if (hal.btnl.isPressing() && (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED))
    {
        // 复位时检查左键是否按下，可以用于无限重启时临时关机
        powerOff(true);
        ESP.restart();
    }
    if (LittleFS.begin(false) == false)
    {
        display.fillScreen(TFT_WHITE);
        u8g2Fonts.setCursor(70, 80);
        u8g2Fonts.print("格式化LittleFS...");
        display.display();
        LittleFS.format();
        if (LittleFS.begin(false) == false)
        {
            log_printf("LittleFS格式化失败\n");
            display.fillScreen(TFT_WHITE);
            u8g2Fonts.setCursor(70, 80);
            u8g2Fonts.print("LittleFS格式化失败");
            display.display();
            delay(100);
            powerOff(false);
            ESP.restart();
        }
        // test_littlefs_size(false);
    }
    else
        log_i("LittleFS已成功挂载");
    if (!fast_boot)
        if (LittleFS.exists("/System/log.txt"))
        {
            File log_file = LittleFS.open("/System/log.txt", "r");
            if (log_file.size() > 1024 * pref.getInt("log_size_max", 512))
            {
                log_file.close();
                LittleFS.remove("/System/log.txt");
            }
            log_file.close();
        }
    log_system_init();
    // test_littlefs_size(true);
    esp_reset_reason_t reset_reason = esp_reset_reason();
    esp_sleep_wakeup_cause_t sleep_wakeup_cause = esp_sleep_get_wakeup_cause();
    if (reset_reason != ESP_RST_DEEPSLEEP && reset_reason != ESP_RST_EXT && reset_reason != ESP_RST_PANIC)
    {
        if (hal.exists("/littlefs/System/start.vlbm"))
        {
            display.setPowerMode(POWER_MODE_HPM);
            setCpuFrequencyMhz(240);
            GUI::PlayLBM_V(0, 0, "/littlefs/System/start.vlbm", TFT_BLACK);
            display.setPowerMode(POWER_MODE_LPM);
        }
    }
    if (!fast_boot)
    {
        if (LittleFS.exists("/System") == false)
        {
            LittleFS.mkdir("/System");
        }
        if (LittleFS.exists("/dat") == false)
        {
            LittleFS.mkdir("/dat");
        }
        if (LittleFS.exists("/System/config.json") == false)
        {
            log_printf("正在写入默认配置\n");
            File f = LittleFS.open("/System/config.json", "w");
            f.print(DEFAULT_CONFIG);
            f.close();
        }
        log_i("ESP32复位,原因:ESP_RST_%s", esp_rst_str[reset_reason]);
        if (reset_reason == ESP_RST_DEEPSLEEP)
        {
            log_i("唤醒源:ESP_SLEEP_%s", esp_sleep_str[sleep_wakeup_cause]);
        }
        if (reset_reason == ESP_RST_PANIC)
        {
            coredump_file();
            ESP.restart();
        }
        if (reset_reason == ESP_RST_BROWNOUT)
        {
            GUI::msgbox("电源警告", "欠压检测器被触发，请检查系统电源状态", 60);
        }
    }
    loadConfig();
    if (!(hal.pref.getString("system_font", "default") == "default"))
        u8g2Fonts.setFont(hal.pref.getString("system_font", "default").c_str());

    peripherals.init();
    weather = new Weather();
    weather->begin();
    buzzer.init();
    TJpgDec.setCallback(GUI::epd_output);
    ttf.setFramebuffer(296, 128, 1);
    xTaskCreate(task_hal_update, "hal_update", 3072, NULL, 10, NULL);
    if (sleep_wakeup_cause != ESP_SLEEP_WAKEUP_TIMER)
    {
        if (hal.pref.getBool(get_char_sha_key("按键音"), false))
            xTaskCreate(task_btn_buzzer, "btn_buzzer", 2048, NULL, 9, NULL);
        cmd.begin();
    }
    else
    {
        log_i("由定时器唤醒，不加载串口工具和按键音");
    }
    // if (pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3) == 3)
    // {
    //     hal.btnr = OneButton(PIN_BUTTONR);
    //     hal.btnl = OneButton(PIN_BUTTONL);
    // }
    // else if (pref.getUChar(SETTINGS_PARAM_SCREEN_ORIENTATION, 3) == 1)
    // {
    //     hal.btnr = OneButton(PIN_BUTTONL);
    //     hal.btnl = OneButton(PIN_BUTTONR);
    // }
    if (peripherals.peripherals_current & PERIPHERALS_BQ27441_BIT)
        xTaskCreate(task_bat_info, "bat_info_update", 3072, NULL, 2, NULL);
    else
        log_e("未安装BQ27441电量计，无法运行电池信息更新任务");
    getTime();
    if ((timeinfo.tm_year < (2016 - 1900)))
    {
        timeerr = true;              // 需要同步时间
        pref.putUInt("lastsync", 1); // 清除上次同步时间，但不清除时钟偏移信息。
        lastsync = 1;
    }
    log_i("初始化完成");
    if (initial == false && timeerr == false)
    {
        return false;
    }
    return true;
}

void HAL::rtc_offset()
{
    // 计算时间间隔Δt（秒）
    // time_t deltaT = currentSync - previousSync;
    time_t deltaT = pref.getInt("every", 0);
    if (deltaT <= 0)
        return; // 避免除以零或负数

    // DS3231的误差ΔT（秒）
    // 负数代表DS3231慢于实际时间，正数代表DS3231快于实际时间
    int error = pref.getInt("rtc_offset", 0);

    if (abs(error) < 3)
    {
        log_i("误差较小，不进行计算");
        return;
    }

    // 计算误差率（ppm）
    double errorRate_ppm = ((double)error / (double)deltaT) * 1e6;

    // 调整振荡器的频率。每个LSB代表大约0.12ppm的频率变化，正值会减慢时间基准，负值会加快时间基准
    // 计算校准值offset（注意符号方向）
    int8_t offset = (int8_t)round(-errorRate_ppm / 0.12); // 负号修正误差方向

    // 限制offset在±127范围内
    offset = constrain(offset, -127, 127);

    xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
    // 写入Aging Offset寄存器
    peripherals.rtc.writeOffset(offset);
    xSemaphoreGive(peripherals.i2cMutex);
}

bool HAL::autoConnectWiFi(bool need_wifi_config)
{
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
        WiFi.setHostname(hal.pref.getString("hostname", String("LiClock-S3")).c_str());
        WiFi.mode(WIFI_STA);
        // WiFi.begin(config[PARAM_SSID].as<const char *>(), config[PARAM_PASS].as<const char *>());
        if (!wifi_config_manger())
        {
            if (need_wifi_config)
                hal.ReqWiFiConfig();
            else
                return false;
        }
        if (esp_wifi_set_max_tx_power(hal.pref.getInt("wifitxpower", 84)) != ESP_OK)
            log_e("Failed set wifi max tx power to %.2f dBm", (float)hal.pref.getInt("wifitxpower", 84) * 0.25);
        else
            log_i("set wifi tx power to %.2f dBm", (float)hal.pref.getInt("wifitxpower", 84) * 0.25);
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
    log_i("成功连接:%s", WiFi.SSID().c_str());
    log_i("IP:%s", WiFi.localIP().toString().c_str());
    log_i("MAC:%s", WiFi.macAddress().c_str());
    log_i("信号强度:%d", WiFi.RSSI());
    esp_sntp_stop();
    return true;
}

void HAL::searchWiFi()
{
    ESP_LOGI("hal", "searchWiFi");
    cheak_freq();
    WiFi.mode(WIFI_STA);
    hal.numNetworks = WiFi.scanNetworks(false, false, false, 500);
    if (hal.numNetworks == 0)
    {
        hal.numNetworks = WiFi.scanNetworks(false, false, false, 500);
        if (hal.numNetworks == 0)
        {
            log_w("没有搜索到WIFI");
        }
    }
}

extern RTC_DATA_ATTR bool ebook_run;
void HAL::set_sleep_set_gpio_interrupt()
{
    rtc_gpio_init((gpio_num_t)PIN_BUTTONC);
    rtc_gpio_init((gpio_num_t)PIN_BUTTONL);
    rtc_gpio_init((gpio_num_t)PIN_BUTTONR);
    if (hal.btn_activelow)
    {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 0);
        esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ANY_LOW);
        rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTONC);
        rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTONL);
        rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTONR);
        rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTONC);
        rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTONL);
        rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTONR);
    }
    else
    {
        if (hal.pref.getBool(hal.get_char_sha_key("根据唤醒源翻页")) == true && ebook_run == true)
        {
            esp_sleep_enable_ext0_wakeup((gpio_num_t)hal._wakeupIO[0], 1);
            esp_sleep_enable_ext1_wakeup((1LL << hal._wakeupIO[1]), ESP_EXT1_WAKEUP_ANY_HIGH);
        }
        else
        {
            esp_sleep_enable_ext1_wakeup((1ULL << PIN_BUTTONC) | (1ULL << PIN_BUTTONL) | (1ULL << PIN_BUTTONR), ESP_EXT1_WAKEUP_ANY_HIGH);
        }
        rtc_gpio_pullup_dis((gpio_num_t)PIN_BUTTONC);
        rtc_gpio_pullup_dis((gpio_num_t)PIN_BUTTONL);
        rtc_gpio_pullup_dis((gpio_num_t)PIN_BUTTONR);
        rtc_gpio_pulldown_en((gpio_num_t)PIN_BUTTONC);
        rtc_gpio_pulldown_en((gpio_num_t)PIN_BUTTONL);
        rtc_gpio_pulldown_en((gpio_num_t)PIN_BUTTONR);
    }
}
void printDisplayVertical()
{
    uint8_t BIT_MASK_LUT[8] = {
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    uint8_t Y_BYTE_OFFSET[4] = {0, 2, 4, 6};
    uint8_t *buffer = display.getBuffer();

    // 创建行缓冲区，用于构建每行的字符串（168个字符 + 换行符 + 终止符）
    char lineBuffer[170];

    // 竖向打印：遍历384行（对应原图的x坐标）
    for (int x = 0; x < PHYSICAL_WIDTH; x++)
    {
        // 遍历168列（对应原图的y坐标）
        for (int y = 0; y < PHYSICAL_HEIGHT; y++)
        {
            // 按照drawPixel中的相同逻辑计算字节索引和位掩码
            uint16_t col = x >> 1;   // 每2列共享一个CGRAM字节
            uint8_t y_div4 = y >> 2; // y / 4，确定垂直字节位置
            uint16_t byte_index = col * 42 + y_div4;

            // 计算位掩码
            uint8_t y_mod4 = y & 0x03; // y % 4
            uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (x & 0x01);
            uint8_t bit_mask = BIT_MASK_LUT[bit_offset];

            // 检查像素值
            if (buffer[byte_index] & bit_mask)
            {
                lineBuffer[y] = '*'; // 像素为1，打印*
            }
            else
            {
                lineBuffer[y] = ' '; // 像素为0，打印空格
            }
        }
        lineBuffer[168] = '\n'; // 每行末尾加换行
        lineBuffer[169] = '\0'; // 字符串终止符

        // 打印这一行
        // 注意：实际串口输出可能需要根据你的串口库调整
        uart->write((const uint8_t *)lineBuffer, 169); // 输出168个字符+换行符
        // 或者使用 uart->print(lineBuffer);
    }
}
#include "driver/ledc.h"
static void pre_sleep()
{
    if (!hal.can_sleep)
        log_i("等待睡眠允许标志位");
    while (!hal.can_sleep)
    {
        delay(100);
        log_printf("\r|");
        delay(100);
        log_printf("\r/");
        delay(100);
        log_printf("\r-");
        delay(100);
        log_printf("\r\\");
    }
    cmd.end();
    peripherals.sleep();
    hal.set_sleep_set_gpio_interrupt();
    display.setPowerMode(POWER_MODE_LPM);
    buzzer.waitForSleep();
    log_system_deinit();
    LittleFS.end();
    // hal.pref.end();
    // printDisplayVertical();
    delay(10);
    ledcDetach(PIN_BUZZER);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, 0);
}
static void wait_display()
{
#if defined(Queue)
    while (uxQueueMessagesWaiting(// display.epd2.getQueue()) > 0)
    {
        delay(10);
    }
    while (// display.epd2.isBusy())
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
    // display.hibernate();
    pre_sleep();
    if (WiFi.isConnected())
        WiFi.disconnect(true);
    log_i("下次唤醒:%ld s", nextSleep);
    nextSleep = nextSleep * 1000000UL;
    esp_sleep_enable_timer_wakeup(nextSleep);
    // wait_display();
    delay(100); // delay some time
    if (noDeepSleep)
    {
        esp_light_sleep_start();
        display.begin();
        LittleFS.begin(false);
        peripherals.wakeup();
        ledcAttach(PIN_BUZZER, 20000, 10);
    }
    else
    {
        esp_deep_sleep_start();
    }
}

void HAL::powerOff(bool displayMessage)
{
    if (hal.pref.getBool("en_poff_image"))
    {
        display.fillScreen(TFT_WHITE);
        display.display();
        GUI::drawLBM(0, 0, hal.pref.getString("poweroff_image").c_str(), TFT_BLACK);
    }
    else if (displayMessage)
    {
        display.fillScreen(TFT_WHITE);
        u8g2Fonts.setCursor(120, 70);
        u8g2Fonts.print("已关机");
        display.display();
    }
    force_full_update = true;
    // display.hibernate();
    pre_sleep();
    if (WiFi.isConnected())
        WiFi.disconnect(true);
    set_sleep_set_gpio_interrupt();
    wait_display();
    display.display_sleep();
    delay(1);
    if (noDeepSleep)
    {
        esp_light_sleep_start();
        // display.begin();
        display.display_sleep(false);
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
    uint16_t bat_voltage = (hal.bat_info.voltage == 0.0) ? hal.VCC : (uint16_t)(hal.bat_info.voltage * 1000);
    if (bat_voltage < auto_sleep_mv)
    {
        // sprintf(buf, "电池电压极低，当前电压为：%d mV，低于自动关机电压%d mV,设备自动关机", hal.VCC, auto_sleep_mv);
        // GUI::info_msgbox("警告", buf);
        // hal.powerOff();
        low_battery = true;
    }
    if (adc > 4300)
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
        log_printf("[夜间模式] 时间错误，直接返回\n");
        return;
    }
    if (config[PARAM_SLEEPATNIGHT].as<String>() == "0")
    {
        log_printf("[夜间模式] 夜间模式已禁用\n");
        return;
    }
    if (night_sleep_today == hal.timeinfo.tm_mday)
    {
        log_printf("[夜间模式] 当天暂时退出夜间模式\n");
        return;
    }
    if (hal.timeinfo.tm_year < (2016 - 1900))
    {
        log_printf("[夜间模式] 时间错误\n");
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
        log_printf("[DEBUG] 夜间模式重绘\n");
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
        display.display();
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
/**
 * @brief 复制文件内容到新文件。
 *
 * 该函数从源文件 `file` 读取数据块并写入目标文件 `newFile`，
 * 在复制过程中会实时显示进度并支持长按按钮暂停或中止。
 *
 * @param newFile 目标文件对象，写入复制的数据。
 * @param file    源文件对象，读取数据的来源。
 * @return true   复制成功完成。
 * @return false  复制过程中出现错误或被用户中止。
 */
bool HAL::copy(File &newFile, File &file)
{
    log_i("开始文件复制");

    // 分配缓冲区内存
    const size_t bufferSize = 512;
    char *buf = (char *)malloc(bufferSize);
    if (!buf)
    {
        log_e("内存分配失败");
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
    while ((bytesRead = file.readBytes(buf, bufferSize)) > 0)
    {
        // 将缓冲区中的数据写入到目标文件中
        size_t bytesWritten = newFile.write((uint8_t *)buf, bytesRead);
        if (bytesWritten != bytesRead)
        {
            log_e("文件在写入过程中发生错误");
            for (int i = 0; i < 3; i++)
            {
                newFile.seek(-bytesWritten, SeekCur);
                bytesWritten = newFile.write((uint8_t *)buf, bytesRead);
                log_e("尝试重新写入，bytesWritten = %d", bytesWritten);
                if (bytesWritten == bytesRead)
                {
                    goto tray;
                }
            }
            for (int i = 0; i < 3; i++)
            {
                buzzer.append(3000, 200);
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
        if (millis() - time >= 2000)
        {
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
            display.display();
            time = millis();
            newFile.flush();
        }
        if (GUI::waitLongPress(hal.btnl.pin()))
            return false;
        if (GUI::waitLongPress(PIN_BUTTONC))
        {
            display.fillRect(0, 22, 296, 22, TFT_WHITE);
            u8g2Fonts.setCursor(1, 20);
            u8g2Fonts.printf("暂停复制：%s", filename);
            display.display();
            hal.wait_input();
        }
    }

    // 确保显示最终完成的进度
    if (totalBytesRead == fileSize)
    {
        display.clearScreen();
        u8g2Fonts.setCursor(1, 20);
        u8g2Fonts.printf("复制完成：%s", filename);
        u8g2Fonts.setCursor(1, 35);
        u8g2Fonts.printf("总计：%dKB 剩余：%dKB", fileSize_kb, 0);
        u8g2Fonts.setCursor(1, 50);
        u8g2Fonts.printf("进度: 100%%", progress);
        display.display();
    }
    else
    {
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
