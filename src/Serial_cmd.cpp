#include "Serial_cmd.h"
#include <nvs_flash.h>
#include "chip-debug-report.h"
#include <cstring>

extern SPIClass SDSPI;
CMD cmd;
bool stop_fileserver = false;
static TaskHandle_t console_task_handle = NULL;
extern bool serverRunning;
extern QueueHandle_t multi_thread_queue;

// Forward declaration for custom hint lookup
static const char *find_cmd_hint(const char *cmdName);

// 任务函数
static void console_task(void *pvParameters)
{
    size_t bufIndex = 0;
    memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
    // 使用宏定义统一日志输出颜色
    PRINT_INFO("LiClock Serial Tool");
    PRINT_INFO("Type 'help' for available commands");
    while (1)
    {
        while (uart->available() > 0)
        {
            char c = uart->read();

            if (c == '\n' || c == '\r' || c == '*')
            {
                if (bufIndex == 0)
                    continue; // 忽略空行
                cmd.cmdBuffer[bufIndex] = '\0';
                // 执行命令
                int ret;
                esp_err_t err = esp_console_run(cmd.cmdBuffer, &ret);
                if (err == ESP_ERR_NOT_FOUND)
                {
                    PRINT_ERROR("Command not found");
                }
                else if (err == ESP_OK)
                {
                    if (ret == 1)
                    {
                        PRINT_ERROR("参数错误");
                        // Use custom hint lookup instead of esp_console_get_hint
                        const char *hint_str = find_cmd_hint(cmd.cmdBuffer);
                        if (hint_str)
                        {
                            // Show the hint as normal info (no error prefix)
                            ERROR_COLOR;
                            uart->printf("%s\n", hint_str);
                            RESET_COLOR;
                        }
                    }
                    if (ret == 2)
                    {
                        PRINT_ERROR("命令 '%s' 执行失败", cmd.cmdBuffer);
                    }
                }
                else if (err != ESP_OK)
                {
                    PRINT_ERROR("%s", esp_err_to_name(err));
                }
                bufIndex = 0;
                memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
            }
            else
            {
                if (bufIndex < COMMAND_BUFFER_SIZE - 1)
                {
                    cmd.cmdBuffer[bufIndex++] = c;
                }
                else
                {
                    // 缓冲区溢出处理
                    bufIndex = 0;
                    memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
                    RED;
                    PRINT_ERROR("Error: Buffer overflow");
                }
            }
        }
        // 没有数据时挂起任务，等待中断唤醒
        vTaskSuspend(NULL);
    }
}

void fileserver_task(void *)
{
    DNSServer dnsServer;
    bool wifi = hal.autoConnectWiFi(false);
    String passwd = String((esp_random() % 1000000000L) + 10000000L); // 生成随机密码
    String str = "WIFI:T:WPA2;S:WeatherClock;P:" + passwd + ";;", str1;
    if (wifi)
    {
        str1 = WiFi.localIP().toString();
    }
    else
    {
        WiFi.softAP("WeatherClock", passwd.c_str());
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
        str1 = "192.168.4.1";
        log_i("WiFi pass: %s", passwd.c_str());
    }
    log_i("WiFi IP: %s", str1.c_str());
    beginFileServer();
    while (1)
    {
        if (stop_fileserver)
        {
            if (!wifi)
                dnsServer.stop();
            server.end();
            serverRunning = false;
            hal.can_sleep = true;
            hal.can_light_sleep = true;
            vTaskDelete(NULL);
        }
        else
            vTaskDelay(100);
    }
}

void IRAM_ATTR serialRxCallback()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 唤醒 cmd_task
    cmd.run();

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void CMD::begin()
{
    cmdBuffer = (char *)ps_malloc(COMMAND_BUFFER_SIZE);
    // 初始化 esp_console
    esp_console_config_t console_config = {
        .max_cmdline_length = 8192,
        .max_cmdline_args = 32,
        .hint_color = 39,
        .hint_bold = 0};
    esp_console_init(&console_config);

    // 注册内置 help 命令
    esp_console_register_help_command();

    // 注册自定义命令
    register_commands();

    // 创建控制台任务
    xTaskCreate(console_task, "console_task", 8192, NULL, 4, &console_task_handle);
#ifndef USE_CDC
    uart->onReceive(serialRxCallback, true);
#endif
    is_run = true;
}

void CMD::SetCallback()
{
#ifndef USE_CDC
    uart->onReceive(serialRxCallback, true);
#endif
}

void CMD::stop()
{
    if (console_task_handle != NULL)
        vTaskSuspend(console_task_handle);
}

void CMD::run()
{
    if (console_task_handle != NULL)
        vTaskResume(console_task_handle);
}

void CMD::end()
{
    if (console_task_handle != NULL)
    {
        vTaskSuspend(console_task_handle);
        vTaskDelete(console_task_handle);
        console_task_handle = NULL;
    }
    if (is_run)
    {
        esp_console_deinit();
    }
}

// 此处定义一下命令处理函数的返回值的对应原因
// 0: 命令成功
// 1: 命令参数错误
// 2: 命令执行失败

static int cmd_cpufreq(int argc, char **argv)
{
    if (argc == 1)
    {
        SUCCESS_COLOR;
        PRINT_INFO("Current CPU frequency: %u MHz", ESP.getCpuFreqMHz());
        RESET_COLOR;
        return 0;
    }
    else if (argc == 2)
    {
        int freq = atoi(argv[1]);
        // 检查频率有效性
        bool valid = (freq == 240 || freq == 160 || freq == 80);
        if (!valid)
        {
            return 1;
        }
        if (setCpuFrequencyMhz(freq))
        {
            SUCCESS_COLOR;
            PRINT_INFO("CPU frequency set to %d MHz", freq);
            RESET_COLOR;
        }
        else
        {
            PRINT_ERROR("Failed to set CPU frequency");
            return 2;
        }
        return 0;
    }
    else
    {
        // Usage hint provided via command registration
        return 1;
    }
}

static int cmd_longpress(int argc, char **argv)
{
    if (argc == 1)
    {
        PRINT_INFO("Current long press threshold: %d ms", hal.pref.getInt("lpt", 20) * 10);
        return 0;
    }
    else if (argc == 2)
    {
        int value = atoi(argv[1]);
        hal.pref.putInt("lpt", value);
        PRINT_INFO("Long press threshold updated to %d ms", value * 10);
        return 0;
    }
    else
    {
        // Usage hint provided via command registration
        return 1;
    }
}

static int cmd_cfgcpufreq(int argc, char **argv)
{
    if (argc == 2)
    {
        int freq = atoi(argv[1]);
        hal.pref.putInt("CpuFreq", freq);
        PRINT_INFO("CPU frequency configuration saved to %d MHz (will take effect after reboot)", freq);
        return 0;
    }
    else
    {
        // Usage hint provided via command registration
        return 1;
    }
}

static int cmd_erasenvs(int argc, char **argv)
{
    PRINT_WARNING("WARNING: This will erase all NVS data!");
    if (nvs_flash_erase() == ESP_OK)
    {
        PRINT_SUCCESS("NVS erased successfully");
        PRINT_INFO("Device will restart in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    else
    {
        // Execution failure
        PRINT_ERROR("Failed to erase NVS");
        return 2;
    }
    return 0;
}

static int cmd_lfsformat(int argc, char **argv)
{
    PRINT_WARNING("WARNING: This will format LittleFS and erase all data!");
    LittleFS.end();
    if (LittleFS.format())
    {
        if (LittleFS.begin())
        {
            PRINT_SUCCESS("LittleFS formatted successfully");
        }
        else
        {
            // Execution failure: could not remount after format
            PRINT_ERROR("Failed to remount LittleFS after format");
            return 2;
        }
    }
    else
    {
        // Execution failure: format failed
        PRINT_ERROR("Failed to format LittleFS");
        return 2;
    }
    return 0;
}

static int cmd_lfsinfo(int argc, char **argv)
{
    size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    PRINT_INFO("LITTLEFS FILESYSTEM INFORMATION:");
    PRINT_INFO("  Total space: %5d KB", total / 1024);
    PRINT_INFO("  Used space:  %5d KB (%.02f%%)", used / 1024, (float)used / (float)total * 100.0);
    PRINT_INFO("  Free space:  %5d KB", (total - used) / 1024);
    return 0;
}

static int cmd_heap(int argc, char **argv)
{
    printMemCapsInfo(INTERNAL);
    if (psramFound())
    {
        printMemCapsInfo(SPIRAM);
    }
    return 0;
}

static int cmd_chipinfo(int argc, char **argv)
{
    PRINT_INFO("CHIP INFORMATION:");
    PRINT_INFO("  Model:       %s", ESP.getChipModel());
    PRINT_INFO("  Revision:    %u", ESP.getChipRevision());
    PRINT_INFO("  Cores:       %u", ESP.getChipCores());
    uint64_t chipmacid = ESP.getEfuseMac();
    uint8_t *mac = (uint8_t *)&chipmacid;
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    PRINT_INFO("  MAC:         %s", macStr);
    PRINT_INFO("  Flash size:  %d MB", ESP.getFlashChipSize() / 1048576);
    uint32_t flash_id;
    uint64_t flash_unique_id;
    esp_flash_read_id(esp_flash_default_chip, &flash_id);
    esp_flash_read_unique_chip_id(esp_flash_default_chip, &flash_unique_id);
    PRINT_INFO("  Flash ID:    %04x", flash_id);
    PRINT_INFO("  Unique ID:   %016llx", flash_unique_id);
    return 0;
}

static int cmd_rst(int argc, char **argv)
{
    PRINT_INFO("Restarting device...");
    // hal.pref.end(); // 避免panic时回退
    LittleFS.end();
    delay(1000);
    ESP.restart();
    return 0;
}

static int cmd_runtime(int argc, char **argv)
{
    long timeMillis = millis();
    long hours = timeMillis / 3600000;
    long remaining = timeMillis % 3600000;
    long minutes = remaining / 60000;
    remaining %= 60000;
    long seconds = remaining / 1000;
    long tenths = (remaining % 1000) / 100;
    PRINT_INFO("Device runtime: %4d:%02d:%02d.%d", hours, minutes, seconds, tenths);
    return 0;
}

static int cmd_batinfo(int argc, char **argv)
{
    hal.printBatteryInfo();
    return 0;
}

static int cmd_cpuuse(int argc, char **argv)
{
    PRINT_INFO("CPU usage monitoring only available in ESP-IDF environment");
    return 0;
}

static int cmd_bootapp_clock(int argc, char **argv)
{
    hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
    PRINT_INFO("Default boot application set to 'clock'");
    return 0;
}

static int cmd_lightsleep(int argc, char **argv)
{
    int timeout = 0;
    if (argc == 2)
    {
        timeout = atoi(argv[1]);
        PRINT_INFO("Entering light sleep with timeout: %d ms", timeout);
    }
    else
    {
        PRINT_INFO("Entering light sleep (press button to wake)");
    }
    hal.wait_input(timeout);
    return 0;
}

static int cmd_fserverbegin(int argc, char **argv)
{
    stop_fileserver = false;
    hal.can_sleep = false;
    hal.can_light_sleep = false;
    xTaskCreatePinnedToCore(fileserver_task, "fileserver", 8192, NULL, 1, NULL, 0);
    PRINT_INFO("File server started");
    return 0;
}

static int cmd_fserverend(int argc, char **argv)
{
    stop_fileserver = true;
    delay(200);
    PRINT_INFO("File server stopped");
    return 0;
}

static int cmd_partitioninfo(int argc, char **argv)
{
    PRINT_INFO("PARTITION INFORMATION:");
    printPartitionsInfo();
    return 0;
}

// 辅助函数：将字符串解析为布尔值
static bool parseBool(const char *str, bool &value)
{
    if (strcasecmp(str, "true") == 0 || strcasecmp(str, "1") == 0 ||
        strcasecmp(str, "yes") == 0 || strcasecmp(str, "on") == 0)
    {
        value = true;
        return true;
    }
    if (strcasecmp(str, "false") == 0 || strcasecmp(str, "0") == 0 ||
        strcasecmp(str, "no") == 0 || strcasecmp(str, "off") == 0)
    {
        value = false;
        return true;
    }
    return false;
}

// 辅助函数：将字符串解析为整数，并检查是否完全消耗
template <typename T>
static bool parseInteger(const char *str, T &value, int base = 0)
{
    char *end;
    long long val = strtoll(str, &end, base);
    if (*end != '\0' || end == str)
        return false; // 存在非法字符或没有数字
    value = static_cast<T>(val);
    // 可选：检查范围是否溢出（可根据需要添加）
    return true;
}

// 辅助函数：将字符串解析为无符号整数
template <typename T>
static bool parseUnsigned(const char *str, T &value, int base = 0)
{
    char *end;
    unsigned long long val = strtoull(str, &end, base);
    if (*end != '\0' || end == str)
        return false;
    value = static_cast<T>(val);
    return true;
}

// 辅助函数：将字符串解析为浮点数
static bool parseFloat(const char *str, float &value)
{
    char *end;
    double val = strtod(str, &end);
    if (*end != '\0' || end == str)
        return false;
    value = static_cast<float>(val);
    return true;
}

static bool parseDouble(const char *str, double &value)
{
    char *end;
    value = strtod(str, &end);
    return (*end == '\0' && end != str);
}

// ==================== putnvs 命令 ====================
static int cmd_putnvs(int argc, char **argv)
{
    // Expected usage:
    //   putnvs <key> <value> [type]   (type optional)
    if (argc < 3 || argc > 4)
    {
        return 1; // 参数个数错误
    }

    const char *key = argv[1];
    const char *valueStr = argv[2];
    const char *type = (argc == 4) ? argv[3] : nullptr;
    bool ok = false;

    // ----- 情况1：未指定类型，自动检测 -----
    if (type == nullptr)
    {
        PreferenceType pt = hal.pref.getType(key);
        if (pt == PT_INVALID)
        {
            PRINT_ERROR("Key '%s' not found, cannot auto-detect type", key);
            return 1;
        }
        // 如果已有类型是 BLOB，无法自动写入（可能是 float/double 或原始二进制）
        if (pt == PT_BLOB)
        {
            PRINT_ERROR("Key '%s' is a BLOB (float/double/bytes). Please specify type explicitly.", key);
            return 1;
        }

        // 根据已有类型进行写入
        switch (pt)
        {
        case PT_I8:
        {
            int8_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putChar(key, val);
            break;
        }
        case PT_U8:
        {
            uint8_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUChar(key, val);
            break;
        }
        case PT_I16:
        {
            int16_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putShort(key, val);
            break;
        }
        case PT_U16:
        {
            uint16_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUShort(key, val);
            break;
        }
        case PT_I32:
        {
            int32_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putInt(key, val);
            break;
        }
        case PT_U32:
        {
            uint32_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUInt(key, val);
            break;
        }
        case PT_I64:
        {
            int64_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putLong64(key, val);
            break;
        }
        case PT_U64:
        {
            uint64_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putULong64(key, val);
            break;
        }
        case PT_STR:
        {
            // 字符串直接使用原值，无需转换
            ok = hal.pref.putString(key, valueStr);
            break;
        }
        case PT_BLOB: // 已在前面过滤
        default:
            // 不会到达这里
            return 1;
        }
    }
    // ----- 情况2：指定类型 -----
    else
    {
        // 处理布尔类型（支持字符串和数字）
        if (strcmp(type, "bool") == 0)
        {
            bool val;
            if (!parseBool(valueStr, val))
            {
                PRINT_ERROR("Invalid bool value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putBool(key, val);
        }
        else if (strcmp(type, "int") == 0 || strcmp(type, "i32") == 0)
        {
            int32_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putInt(key, val);
        }
        else if (strcmp(type, "uint") == 0 || strcmp(type, "u32") == 0)
        {
            uint32_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUInt(key, val);
        }
        else if (strcmp(type, "i8") == 0)
        {
            int8_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putChar(key, val);
        }
        else if (strcmp(type, "u8") == 0)
        {
            uint8_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUChar(key, val);
        }
        else if (strcmp(type, "i16") == 0)
        {
            int16_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putShort(key, val);
        }
        else if (strcmp(type, "u16") == 0)
        {
            uint16_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUShort(key, val);
        }
        else if (strcmp(type, "i64") == 0)
        {
            int64_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putLong64(key, val);
        }
        else if (strcmp(type, "u64") == 0)
        {
            uint64_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putULong64(key, val);
        }
        else if (strcmp(type, "float") == 0)
        {
            float val;
            if (!parseFloat(valueStr, val))
            {
                PRINT_ERROR("Invalid float value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putFloat(key, val);
        }
        else if (strcmp(type, "double") == 0)
        {
            double val;
            if (!parseDouble(valueStr, val))
            {
                PRINT_ERROR("Invalid double value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putDouble(key, val);
        }
        else if (strcmp(type, "string") == 0)
        {
            // 字符串直接使用原值
            ok = hal.pref.putString(key, valueStr);
        }
        else
        {
            PRINT_ERROR("Unsupported type: %s", type);
            return 1;
        }
    }

    if (ok)
    {
        PRINT_SUCCESS("successful write NVS key \"%s\"", key);
        return 0;
    }
    else
    {
        PRINT_ERROR("fail write NVS key \"%s\"", key);
        return 2;
    }
}

// ==================== getnvs 命令 ====================
static int cmd_getnvs(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        return 1; // 参数错误
    }

    const char *key = argv[1];
    const char *type = (argc == 3) ? argv[2] : nullptr;

    // 先检查键是否存在
    if (hal.pref.getType(key) == PT_INVALID)
    {
        PRINT_ERROR("Key '%s' not found", key);
        return 1;
    }

    // ----- 指定类型读取（目前仅支持 float/double） -----
    if (type != nullptr)
    {
        if (strcmp(type, "float") == 0)
        {
            // 验证长度是否为4字节
            size_t len = hal.pref.getBytesLength(key);
            if (len != sizeof(float))
            {
                PRINT_ERROR("Key '%s' is not a float (actual size %u bytes)", key, len);
                return 1;
            }
            float val = hal.pref.getFloat(key);
            PRINT_INFO("%s (float): %f", key, val);
        }
        else if (strcmp(type, "double") == 0)
        {
            size_t len = hal.pref.getBytesLength(key);
            if (len != sizeof(double))
            {
                PRINT_ERROR("Key '%s' is not a double (actual size %u bytes)", key, len);
                return 1;
            }
            double val = hal.pref.getDouble(key);
            PRINT_INFO("%s (double): %lf", key, val);
        }
        else
        {
            PRINT_ERROR("Specified type '%s' not supported for reading", type);
            return 1;
        }
        return 0;
    }

    // ----- 未指定类型，自动检测并输出 -----
    PreferenceType pt = hal.pref.getType(key);
    switch (pt)
    {
    case PT_I8:
    {
        int8_t val = hal.pref.getChar(key);
        PRINT_INFO("%s (int8): %d", key, val);
        break;
    }
    case PT_U8:
    {
        uint8_t val = hal.pref.getUChar(key);
        PRINT_INFO("%s (uint8): %u", key, val);
        break;
    }
    case PT_I16:
    {
        int16_t val = hal.pref.getShort(key);
        PRINT_INFO("%s (int16): %d", key, val);
        break;
    }
    case PT_U16:
    {
        uint16_t val = hal.pref.getUShort(key);
        PRINT_INFO("%s (uint16): %u", key, val);
        break;
    }
    case PT_I32:
    {
        int32_t val = hal.pref.getInt(key);
        PRINT_INFO("%s (int32): %d", key, val);
        break;
    }
    case PT_U32:
    {
        uint32_t val = hal.pref.getUInt(key);
        PRINT_INFO("%s (uint32): %u", key, val);
        break;
    }
    case PT_I64:
    {
        int64_t val = hal.pref.getLong64(key);
        PRINT_INFO("%s (int64): %lld", key, (long long)val);
        break;
    }
    case PT_U64:
    {
        uint64_t val = hal.pref.getULong64(key);
        PRINT_INFO("%s (uint64): %llu", key, (unsigned long long)val);
        break;
    }
    case PT_STR:
    {
        String val = hal.pref.getString(key);
        PRINT_INFO("%s (string): \"%s\"", key, val.c_str());
        break;
    }
    case PT_BLOB:
    {
        size_t len = hal.pref.getBytesLength(key);
        char buffer[len + 1];
        hal.pref.getBytes(key, buffer, len + 1);
        PRINT_INFO("%s (blob): length %u bytes", key, len);
        INFO_COLOR;
        for (size_t i = 0; i < len; ++i)
        {
            uart->printf("0x%02X ", (uint8_t)buffer[i]);
        }
        RESET_COLOR;
        break;
    }
    default:
        PRINT_ERROR("Key '%s' has unknown type", key);
        return 1;
    }
    return 0;
}

// 删除 NVS 键值对命令
static int cmd_rmnvs(int argc, char **argv)
{
    // Usage: rmnvs <key>
    if (argc != 2)
    {
        // 参数个数错误，返回1会自动显示帮助
        return 1;
    }

    const char *key = argv[1];

    // 检查键是否存在
    if (hal.pref.getType(key) == PT_INVALID)
    {
        PRINT_ERROR("Key '%s' not found", key);
        return 2;   // 执行失败
    }

    // 执行删除
    bool ok = hal.pref.remove(key);
    if (ok)
    {
        PRINT_SUCCESS("NVS key '%s' removed", key);
        return 0;
    }
    else
    {
        PRINT_ERROR("Failed to remove key '%s'", key);
        return 2;
    }
}

static int cmd_display_debug(int argc, char **argv)
{
    if (argc == 2)
    {
        bool en = atoi(argv[1]);
        bool valid = (en == 1 || en == 0);
        if (!valid)
        {
            return 1;
        }
        display.debug_log((bool)en);
        return 0;
    }
    else
        return 1;
    return 0;
}

static int cmd_file_task_info(int argc, char **argv)
{
    if (multi_thread_queue != NULL)
    {
        UBaseType_t count = uxQueueMessagesWaiting(multi_thread_queue);
        PRINT_INFO("当前文件写入任务队列深度:%lu", count);
    }
    else
        PRINT_WARNING("当前文件写入任务队列为空");
    return 0;
}

#include "soc/soc.h"
#include "soc/system_reg.h"

static int cmd_cpufreq_reg(int argc, char **argv)
{
    if (argc == 2)
    {
        uint8_t val = atoi(argv[1]);
        bool valid = (val < 4);
        if (!valid)
        {
            return 1;
        }
        REG_SET_FIELD(SYSTEM_CPU_PER_CONF_REG, SYSTEM_CPUPERIOD_SEL, val);
        int64_t start_us = esp_timer_get_time();
        for (int i = 0; i < 3000; ++i)
        {
            _NOP();
        }
        int64_t end_us = esp_timer_get_time();
        int64_t elapsed_us = end_us - start_us;
        PRINT_INFO("NOP latency: %lld us\n", elapsed_us);
        return 0;
    }
    else
        return 1;
    return 0;
}

static void on_wifi_task(void *pvParameters)
{
    hal.autoConnectWiFi(false);
    vTaskDelete(NULL);
}

static String urlEncode(const String &str)
{
    String encoded = "";
    char hex[17] = "0123456789ABCDEF"; // hex字符范围

    for (size_t i = 0; i < str.length(); i++)
    {
        unsigned char c = str[i]; // 取出每个字节

        // 保留无需编码的字符：字母、数字、-_.~:/
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ':' || c == '/')
        {
            encoded += (char)c;
        }
        else
        {
            // 其他字符转换为 %XX 格式
            encoded += '%';
            encoded += hex[(c >> 4) & 0xF];
            encoded += hex[c & 0xF];
        }
    }

    return encoded;
}

static int cmd_download(int argc, char **argv)
{
    // 1. 参数检查
    if (argc != 4)
        return 1; // 命令参数错误

    char *ca_id = argv[1];
    String url = argv[2];
    String save_file = argv[3];
    String _url = urlEncode(url); // 假设 urlEncode 已实现

    // 2. 获取 CA 证书（可能为空字符串）
    String CAcert = hal.get_CAcert(ca_id);

    // 3. 确保 WiFi 已连接
    if (!WiFi.isConnected())
    {
        xTaskCreatePinnedToCore(on_wifi_task, "on_wifi_task", 8192, NULL, 1, NULL, 0);

        unsigned long start = millis();
        const unsigned long timeout = 30000; // 30 秒超时
        while (!WiFi.isConnected() && (millis() - start) < timeout)
        {
            delay(100);
            uart->printf("\r|");
            delay(100);
            uart->printf("\r/");
            delay(100);
            uart->printf("\r-");
            delay(100);
            uart->printf("\r\\");
        }

        if (!WiFi.isConnected())
        {
            PRINT_ERROR("WiFi connection timeout!");
            return 2;
        }
        uart->printf("\r\n"); // 清除旋转动画
        PRINT_INFO("WiFi connected.");
    }

    // 4. 初始化 HTTP 客户端
    HTTPClient http;
    bool httpStarted = false;

    if (CAcert.length() == 0)
        httpStarted = http.begin(_url);
    else
        httpStarted = http.begin(_url, CAcert.c_str());

    if (!httpStarted)
    {
        PRINT_ERROR("HTTP begin failed!");
        return 2;
    }

    // 5. 发送 GET 请求
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        PRINT_ERROR("HTTP GET failed, code: %d", httpCode);
        http.end();
        return 2;
    }

    // 6. 获取文件总大小（可能为 -1 表示未知）
    int totalSize = http.getSize(); // HTTPClient 的 getSize() 返回 Content-Length
    if (totalSize <= 0)
    {
        PRINT_WARNING("Cannot determine file size, progress will not be shown.");
    }

    // 7. 打开目标文件
    File file = hal.open(save_file, FILE_WRITE);
    if (!file)
    {
        PRINT_ERROR("Failed to open file: %s", save_file.c_str());
        http.end();
        return 2;
    }

    // 8. 读取数据流并写入文件，同时显示进度
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t totalWritten = 0;
    int lastPercent = -1;
    const int progressBarWidth = 50; // 进度条宽度（字符数）

    while (http.connected() && (stream->available() > 0 || totalWritten < (size_t)totalSize))
    {
        int len = stream->readBytes(buffer, sizeof(buffer));
        if (len > 0)
        {
            size_t written = file.write(buffer, len);
            if (written != (size_t)len)
            {
                PRINT_ERROR("File write error!");
                file.close();
                http.end();
                return 2;
            }
            totalWritten += written;

            // 计算并显示进度（仅当总大小已知时）
            if (totalSize > 0)
            {
                int percent = (int)((totalWritten * 100) / totalSize);
                if (percent != lastPercent)
                {
                    lastPercent = percent;
                    int fill = (percent * progressBarWidth) / 100;
                    uart->printf("\r["); // 开始打印进度条
                    for (int i = 0; i < fill; i++)
                        uart->printf("█"); // 打印已下载部分
                    for (int i = fill; i < progressBarWidth; i++)
                        uart->printf("░");            // 打印未下载部分
                    uart->printf("] %3d%%", percent); // 完成百分比
                }
            }
        }
        else
        {
            // 无数据但连接仍在，稍等再试（避免死循环）
            delay(10);
        }
    }

    // 确保所有数据都已读完（对于 chunked 或未知大小的情况）
    while (stream->available())
    {
        int len = stream->readBytes(buffer, sizeof(buffer));
        if (len > 0)
        {
            size_t written = file.write(buffer, len);
            if (written != (size_t)len)
            {
                PRINT_ERROR("File write error!");
                file.close();
                http.end();
                return 2;
            }
            totalWritten += written;
        }
    }

    // 9. 关闭文件和 HTTP 连接
    file.close();
    http.end();

    // 换行，使下一个输出不覆盖进度条
    uart->printf("\n");

    // 10. 输出结果
    if (totalSize > 0 && totalWritten == (size_t)totalSize)
    {
        PRINT_SUCCESS("Downloaded %u bytes to %s", totalWritten, save_file.c_str());
        return 0;
    }
    else if (totalSize <= 0)
    {
        PRINT_SUCCESS("Downloaded %u bytes to %s (size unknown)", totalWritten, save_file.c_str());
        return 0;
    }
    else
    {
        PRINT_ERROR("Download incomplete: expected %d bytes, got %u", totalSize, totalWritten);
        return 2;
    }
}

// ==================== 辅助函数 ====================

// 打印指定目录的内容（不递归）
static void list_dir(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
    {
        PRINT_ERROR("Cannot open directory: %s", path);
        return;
    }

    uart->printf("Directory: %s\n", path);

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // 跳过当前目录和父目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // 构建完整路径以便获取详细信息
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0)
        {
            PRINT_WARNING("Cannot stat: %s", full_path);
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            uart->printf("\033[36m[DIR] %s\033[0m\n", entry->d_name);
        }
        else if (S_ISREG(st.st_mode))
        {
            uart->printf("[FILE] %s (%u bytes)\n", entry->d_name, (unsigned int)st.st_size);
        }
        else
        {
            uart->printf("[UNKN] %s\n", entry->d_name);
        }
    }

    closedir(dir);
}

// 检查路径是否有效（以 /littlefs 或 /sd 开头）
static bool is_valid_root_path(const char *path)
{
    return (strncmp(path, "/littlefs/", 10) == 0 || strncmp(path, "/sd/", 4) == 0 ||
            strcmp(path, "/littlefs") == 0 || strcmp(path, "/sd") == 0);
}

// ==================== ls 命令 ====================
// 用法：
//   ls               - 列出根挂载点 /littlefs 和 /sd
//   ls <path>        - 列出指定目录内容（路径必须完整）
static int cmd_ls(int argc, char **argv)
{
    if (argc == 1)
    {
        // 无参数：列出两个挂载点
        list_dir("/littlefs");
        list_dir("/sd");
        return 0;
    }
    else if (argc == 2)
    {
        const char *path = argv[1];
        if (!is_valid_root_path(path))
        {
            PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
            return 1;
        }
        list_dir(path);
        return 0;
    }
    else
    {
        return 1;
    }
}

// ==================== echo 命令 ====================
// 用法：
//   echo <text>                     - 直接打印文本
//   echo <text> > <file>            - 覆盖写入文件
//   echo <text> >> <file>           - 追加写入文件
static int cmd_echo(int argc, char **argv) {
    if (argc < 2) {
        return 1;
    }

    // 情况1：无重定向
    if (argc == 2) {
        PRINT_INFO("%s", argv[1]);
        return 0;
    }

    // 情况2：有重定向，必须满足 argc == 4 且 argv[2] 为 ">" 或 ">>"
    if (argc == 4 && (strcmp(argv[2], ">") == 0 || strcmp(argv[2], ">>") == 0)) {
        const char* mode = (strcmp(argv[2], ">") == 0) ? "w" : "a";
        const char* file_path = argv[3];

        if (!is_valid_root_path(file_path)) {
            PRINT_ERROR("Invalid file path: must start with /littlefs/ or /sd/");
            return 1;
        }

        File f = hal.open(file_path, mode, true);
        if (!f) {
            PRINT_ERROR("Failed to open file: %s", file_path);
            return 2;
        }

        size_t written = f.print(argv[1]);
        f.close();

        if (written != strlen(argv[1])) {
            PRINT_ERROR("Write error to file: %s", file_path);
            return 2;
        }

        PRINT_SUCCESS("Written %u bytes to %s", written, file_path);
        return 0;
    }

    return 1;
}

// ==================== cat 命令 ====================
// 用法：cat <file>
static int cmd_cat(int argc, char **argv)
{
    if (argc != 2)
    {
        return 1;
    }
    const char *path = argv[1];
    if (!is_valid_root_path(path))
    {
        PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
        return 1;
    }

    File f = hal.open(path, "r");
    if (!f)
    {
        PRINT_ERROR("Cannot open file: %s", path);
        return 2;
    }

    // 分块读取并输出
    const size_t buf_size = 512;
    uint8_t buffer[buf_size];
    size_t total = 0;
    while (f.available())
    {
        size_t len = f.read(buffer, buf_size);
        if (len > 0)
        {
            uart->write(buffer, len);
            total += len;
        }
    }
    f.close();

    if (total == 0)
    {
        PRINT_WARNING("File is empty: %s", path);
    }
    return 0;
}

// ==================== mkdir 命令 ====================
// 用法：mkdir <dir>
static int cmd_mkdir(int argc, char **argv)
{
    if (argc != 2)
    {
        return 1;
    }
    const char *path = argv[1];
    if (!is_valid_root_path(path))
    {
        PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
        return 1;
    }

    if (hal.exists(path))
    {
        PRINT_ERROR("Path already exists: %s", path);
        return 2;
    }

    if (hal.mkdir(path))
    {
        PRINT_SUCCESS("Directory created: %s", path);
        return 0;
    }
    else
    {
        PRINT_ERROR("Failed to create directory: %s", path);
        return 2;
    }
}

// ==================== rm 命令 ====================
// 用法：
//   rm <file>          - 删除文件
//   rm -r <dir>        - 递归删除目录及其内容
//   rm <empty_dir>     - 删除空目录（需要加 -r 或显式使用 rmdir，这里按 Linux 习惯，rm 默认不删除目录）
static int cmd_rm(int argc, char **argv)
{
    bool recursive = false;
    const char *path = nullptr;

    // 解析参数
    if (argc == 2)
    {
        path = argv[1];
    }
    else if (argc == 3 && strcmp(argv[1], "-r") == 0)
    {
        recursive = true;
        path = argv[2];
    }
    else
    {
        return 1;
    }

    if (!is_valid_root_path(path))
    {
        PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
        return 1;
    }

    // 检查路径是否存在
    if (!hal.exists(path))
    {
        PRINT_ERROR("Path does not exist: %s", path);
        return 2;
    }

    // 打开路径判断类型
    File f = hal.open(path, "r");
    if (!f)
    {
        PRINT_ERROR("Cannot access: %s", path);
        return 2;
    }
    bool is_dir = f.isDirectory();
    f.close();

    if (is_dir)
    {
        if (recursive)
        {
            // 递归删除目录
            hal.rm_rf(path);
            PRINT_SUCCESS("Removed directory recursively: %s", path);
            return 0;
        }
        else
        {
            PRINT_ERROR("'%s' is a directory. Use -r to remove it.", path);
            return 2;
        }
    }
    else
    {
        // 文件：直接删除
        if (hal.remove(path))
        {
            PRINT_SUCCESS("Removed file: %s", path);
            return 0;
        }
        else
        {
            PRINT_ERROR("Failed to remove file: %s", path);
            return 2;
        }
    }
}

// ==================== mv 命令 ====================
// 用法：mv <source> <dest>
// 注意：仅支持同一文件系统内的移动/重命名，跨文件系统会报错。
static int cmd_mv(int argc, char **argv)
{
    if (argc != 3)
    {
        return 1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    if (!is_valid_root_path(src) || !is_valid_root_path(dst))
    {
        PRINT_ERROR("Paths must start with /littlefs/ or /sd/");
        return 1;
    }

    // 检查源是否存在
    if (!hal.exists(src))
    {
        PRINT_ERROR("Source does not exist: %s", src);
        return 2;
    }

    // 检查目标是否已存在（避免覆盖，可按需要修改）
    if (hal.exists(dst))
    {
        PRINT_ERROR("Destination already exists: %s", dst);
        return 2;
    }

    // 尝试重命名/移动
    if (hal.rename(src, dst))
    {
        PRINT_SUCCESS("Moved/Renamed: %s -> %s", src, dst);
        return 0;
    }
    else
    {
        // 可能跨文件系统或权限问题
        PRINT_ERROR("Move failed (possibly cross filesystem): %s -> %s", src, dst);
        return 2;
    }
}

// 命令注册表
// Register all console commands defined in this file
static const char *no_info = "";
static const esp_console_cmd_t cmds[] = {
    {.command = "cpufreq", .help = "获取当前CPU频率或设置频率", .hint = "Usage: cpufreq [freq]\n  freq: 240,160,80", .func = &cmd_cpufreq, .argtable = NULL},
    {.command = "longpress", .help = "设置长按检测时间（X10ms）", .hint = "Usage: longpress [value]", .func = &cmd_longpress, .argtable = NULL},
    {.command = "heap", .help = "显示堆内存使用情况", .hint = no_info, .func = &cmd_heap, .argtable = NULL},
    {.command = "cfgcpufreq", .help = "配置CPU频率（重启后生效）", .hint = "Usage: cfgcpufreq <freq>\n  freq: 240,160,80", .func = &cmd_cfgcpufreq, .argtable = NULL},
    {.command = "erasenvs", .help = "擦除所有NVS数据并重启", .hint = "谨慎操作", .func = &cmd_erasenvs, .argtable = NULL},
    {.command = "lfsformat", .help = "格式化LittleFS文件系统", .hint = "谨慎操作", .func = &cmd_lfsformat, .argtable = NULL},
    {.command = "lfsinfo", .help = "显示LittleFS文件系统信息", .hint = no_info, .func = &cmd_lfsinfo, .argtable = NULL},
    {.command = "chipinfo", .help = "显示芯片信息", .hint = no_info, .func = &cmd_chipinfo, .argtable = NULL},
    {.command = "rst", .help = "重启设备", .hint = no_info, .func = &cmd_rst, .argtable = NULL},
    {.command = "runtime", .help = "显示设备运行时间", .hint = no_info, .func = &cmd_runtime, .argtable = NULL},
    {.command = "batinfo", .help = "显示电池信息", .hint = no_info, .func = &cmd_batinfo, .argtable = NULL},
    {.command = "cpuuse", .help = "CPU使用率（仅ESP-IDF环境）", .hint = no_info, .func = &cmd_cpuuse, .argtable = NULL},
    {.command = "bootapp_clock", .help = "设置默认启动应用为clock", .hint = no_info, .func = &cmd_bootapp_clock, .argtable = NULL},
    {.command = "lightsleep", .help = "进入轻睡眠模式，可选超时", .hint = "Usage: lightsleep [timeout]", .func = &cmd_lightsleep, .argtable = NULL},
    {.command = "fserverbegin", .help = "启动文件服务器", .hint = no_info, .func = &cmd_fserverbegin, .argtable = NULL},
    {.command = "fserverend", .help = "停止文件服务器", .hint = no_info, .func = &cmd_fserverend, .argtable = NULL},
    {.command = "partitioninfo", .help = "显示分区信息", .hint = no_info, .func = &cmd_partitioninfo, .argtable = NULL},
    {.command = "putnvs", .help = "写入NVS键值", .hint = "Usage: putnvs <key> <value> [type]\n  type: bool, int, uint, i8, u8, i16, u16, i32, u32, i64, u64, float, double, string (omit for auto-detect)", .func = &cmd_putnvs, .argtable = NULL},
    {.command = "getnvs", .help = "读取NVS键值", .hint = "Usage: getnvs <key> [type]\n  type: bool, int, uint, i8, u8, i16, u16, i32, u32, i64, u64, float, double, string (omit for auto-detect)", .func = &cmd_getnvs, .argtable = NULL},
    {.command = "rmnvs", .help = "删除NVS键值对", .hint = "Usage: rmnvs <key>", .func = &cmd_rmnvs, .argtable = NULL},
    {.command = "display_debug", .help = "控制屏幕驱动debug输出", .hint = no_info, .func = &cmd_display_debug, .argtable = NULL},
    {.command = "filetaskinfo", .help = "显示文件写入任务的队列情况", .hint = no_info, .func = &cmd_file_task_info, .argtable = NULL},
    {.command = "download", .help = "从网络下载文件", .hint = "Usage: download <ca_id> <url> <save_file>", .func = &cmd_download, .argtable = NULL},
    {.command = "mv", .help = "移动/重命名文件或目录", .hint = "Usage: mv <source> <dest>", .func = &cmd_mv, .argtable = NULL},
    {.command = "ls", .help = "列出目录", .hint = "Usage: ls [path]", .func = &cmd_ls, .argtable = NULL},
    {.command = "cat", .help = "输出文件的内容", .hint = "Usage: cat <path>", .func = &cmd_cat, .argtable = NULL},
    {.command = "rm", .help = "删除文件或目录", .hint = "Usage: rm [-r] <path>", .func = &cmd_rm, .argtable = NULL},
    {.command = "echo", .help = "写入或追加文件", .hint = "Usage: echo <text> [>|>> <file>]", .func = &cmd_echo, .argtable = NULL},
    {.command = "mkdir", .help = "新建文件夹", .hint = "Usage: mkdir <path>", .func = &cmd_mkdir, .argtable = NULL},
    {.command = "setcpuperiod", .help = "修改SYSTEM_CPUPERIOD_SEL的值", .hint = no_info, .func = &cmd_cpufreq_reg, .argtable = NULL}};

// Custom helper to retrieve the hint string for a given command name.
// Returns the hint pointer from the cmds array, or nullptr if not found.
static const char *find_cmd_hint(const char *cmdline)
{
    // Extract the command name (first token) from cmdline, ignoring any arguments.
    // Find the first whitespace character.
    const char *space = strchr(cmdline, ' ');
    size_t nameLen = space ? (size_t)(space - cmdline) : strlen(cmdline);

    // Iterate over the static cmds array defined above.
    size_t cmdCount = sizeof(cmds) / sizeof(cmds[0]);
    for (size_t i = 0; i < cmdCount; ++i)
    {
        // Compare only the command name length.
        if (strlen(cmds[i].command) == nameLen &&
            strncmp(cmds[i].command, cmdline, nameLen) == 0)
        {
            return cmds[i].hint;
        }
    }
    return nullptr;
}
void CMD::register_commands()
{
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i)
    {
        esp_console_cmd_register(&cmds[i]);
    }
}