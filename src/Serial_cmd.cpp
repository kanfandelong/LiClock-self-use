#include "Serial_cmd.h"
#include <nvs_flash.h>
#include "chip-debug-report.h"

extern SPIClass SDSPI;
CMD cmd;
char task_list[1024];
bool stop_fileserver = false; 
extern bool serverRunning;
void fileserver_task(void *){
    DNSServer dnsServer;
    bool wifi = hal.autoConnectWiFi(false);
    String passwd = String((esp_random() % 1000000000L) + 10000000L); // 生成随机密码
    String str = "WIFI:T:WPA2;S:WeatherClock;P:" + passwd + ";;", str1;
    if (wifi){
        str1 = WiFi.localIP().toString();
    }else{
        WiFi.softAP("WeatherClock", passwd.c_str());
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
        str1 = "192.168.4.1";
        log_i("WiFi pass: %s\n", passwd.c_str());
    }
    log_i("WiFi IP: %s\n", str1.c_str());
    beginFileServer();
    while(1){
        if (stop_fileserver){
            if (!wifi)
                dnsServer.stop();
            server.end();
            serverRunning = false;
            hal.can_sleep  = true;
            hal.can_light_sleep  = true;
            vTaskDelete(NULL);
        }else
            vTaskDelay(100);
    }
}

void cmd_task(void *) {
    size_t bufIndex = 0;
    memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
    CYAN;
    HEADER_COLOR;
    Serial.println("LiClock Serial Tool");
    Serial.println("Type '#help*' for available commands");
    RESET_COLOR;
    RESET;
    while(1) {
        // 读取串口数据
        while (Serial.available() > 0) {
            char c = Serial.read();
  
            if (c == COMMAND_TERMINATOR || c == '\n') {
                cmd.cmdBuffer[bufIndex] = '\0';
                GREEN;
                if (c == COMMAND_TERMINATOR)
                    Serial.printf("[DEBUG] Raw command: %s*\n", cmd.cmdBuffer); // 调试日志
                else 
                    Serial.printf("[DEBUG] Raw command: %s\"n\n", cmd.cmdBuffer);
                RESET;
                cmd.parseCommand(cmd.cmdBuffer);
                bufIndex = 0;                   // 重置bufIndex
                memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
                continue;
            }
            
            if (c == '\r') continue; // 忽略回车符
            
            if (bufIndex < COMMAND_BUFFER_SIZE - 1) {
                cmd.cmdBuffer[bufIndex++] = c;
            } else {
                bufIndex = 0;
                memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
                RED;
                Serial.println("Error: Buffer overflow");
                RESET;
            }
        }
  
        vTaskSuspend(NULL);
    }
}
void IRAM_ATTR serialRxCallback() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 唤醒 cmd_task
    cmd.run();

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
void CMD::begin(){
    xTaskCreate(cmd_task, "cmd_task", 4096, NULL, 4, &cmd_task_handle); // 创建 cmd_task，且不指定核心
    Serial.onReceive(serialRxCallback, true);
}
void CMD::stop(){
    vTaskSuspend(cmd_task_handle);  // 挂起串口指令任务
}
void CMD::run(){ 
    vTaskResume(cmd_task_handle);
}
void CMD::end(){
    if (cmd_task_handle != NULL){
        vTaskSuspend(cmd_task_handle);  // 挂起串口指令任务
        vTaskDelete(cmd_task_handle);   // 删除串口指令任务
        cmd_task_handle = NULL;
    }
}
void CMD::printHelp(){
    HEADER_COLOR;
    Serial.println("LiClock Serial Tool");
    Serial.println("===================================");
    RESET_COLOR;
    
    Serial.println("This tool is designed for firmware development, debugging, and recovery operations.");
    Serial.println("Command Format: #command[parameter]*");
    
    HEADER_COLOR;
    Serial.println("\nAVAILABLE COMMANDS:");
    Serial.println("===================");
    RESET_COLOR;
    
    // 系统命令
    HEADER_COLOR;
    Serial.println("\n[SYSTEM COMMANDS]");
    RESET_COLOR;
    Serial.printf("  %-20s - %s\n", help, "Display this help information");
    Serial.printf("  %-20s - %s\n", esp_light_sleep, "Put device into light sleep (wake by button)");
    Serial.printf("  %-20s - %s\n", ((String)esp_light_sleep + "[timeout]").c_str(), "Put device into light sleep with timeout");
    Serial.printf("  %-20s - %s\n", esp_restart_, "Restart the device");
    Serial.printf("  %-20s - %s\n", free_heap_size, "Display free heap memory");
    Serial.printf("  %-20s - %s\n", get_runtime, "Display device runtime");
    Serial.printf("  %-20s - %s\n", get_bat_info, "Display battery information");
    Serial.printf("  %-20s - %s\n", file_server_begin, "Start file server");
    Serial.printf("  %-20s - %s\n", file_server_end, "Stop file server");
    Serial.printf("  %-20s - %s\n", ((String)set_display_debug + "[0/1]").c_str(), "Enable/disable display driver debug output");
    Serial.printf("  %-20s - %s\n", esp_partition_info, "Display partition table information");

    // 硬件控制
    HEADER_COLOR;
    Serial.println("\n[HARDWARE COMMANDS]");
    RESET_COLOR;
    Serial.printf("  %-20s - %s\n", get_cpu_usage, "Display CPU usage (ESP-IDF only)");
    Serial.printf("  %-20s - %s\n", set_cpu_freq, "Get current CPU frequency");
    Serial.printf("  %-20s - %s\n", ((String)set_cpu_freq + "[freq]").c_str(), "Set CPU frequency (takes effect immediately)");
    Serial.printf("  %-20s - %s\n", ((String)config_cpu_freq + "[freq]").c_str(), "Save CPU frequency to settings (takes effect after reboot)");
    Serial.println("    Valid frequencies: 240, 160, 80, 40, 20, 10 (MHz)");
    Serial.printf("  %-20s - %s\n", esp_chip_info_, "Display chip information");
    Serial.printf("  %-20s - %s\n", ((String)set_display + "[gray]").c_str(), "Force set display gray level");
    Serial.printf("  %-20s - %s\n", ((String)set_display_PLL + "[value]").c_str(), "Set display PLL clock (resets after ESP32 reboot)");
    Serial.printf("  %-20s - %s\n", ((String)cfg_display_PLL + "[value]").c_str(), "Set display PLL clock and save to NVS");

    // 文件系统
    HEADER_COLOR;
    Serial.println("\n[FILESYSTEM COMMANDS]");
    RESET_COLOR;
    Serial.printf("  %-20s - %s\n", littlefs_format, "Format LittleFS filesystem");
    Serial.printf("  %-20s - %s\n", littlefs_info, "Display filesystem information");
    Serial.printf("  %-20s - %s\n", erase_nvs, "Erase NVS storage");
    Serial.printf("  %-20s - %s\n", getnvs, "read NVS Key");
    Serial.printf("  %-20s - %s\n", putnvs, "write NVS key");
    Serial.printf("  %-20s - %s\n", format_tf, "Format TF card");

    // 参数设置
    HEADER_COLOR;
    Serial.println("\n[SETTINGS COMMANDS]");
    RESET_COLOR;
    Serial.printf("  %-20s - %s\n", set_long_press, "Get long press threshold");
    Serial.printf("  %-20s - %s\n", ((String)set_long_press + "[time]").c_str(), "Set long press threshold (ms)");
    Serial.printf("  %-20s - %s\n", set_boot_app, "Set default boot app to clock");
    Serial.printf("  %-20s - %s\n", temp_log, "Get temperature logging status");
    Serial.printf("  %-20s - %s\n", ((String)temp_log + "[0/1]").c_str(), "Enable/disable temperature logging");

    HEADER_COLOR;
    Serial.println("\nEXAMPLES:");
    RESET_COLOR;
    Serial.println("  #cpufreq[240]*      - Set CPU frequency to 240MHz");
    Serial.println("  #chipinfo*          - Display chip information");
    Serial.println("  #help*              - Show this help");
    
    HEADER_COLOR;
    Serial.println("\nFor more information about a specific command, type the command without parameters.");
    RESET_COLOR;
}


int parse_command_simple(const char *command, char *cmd, char params[][32], int max_params) {
    const char *p = command;
    int param_count = 0;
    
    // 跳过 #
    if (*p == '#') p++;
    
    // 提取命令 (直到 [ 或字符串结束)
    char *cmd_ptr = cmd;
    while (*p && *p != '[') {
        *cmd_ptr++ = *p++;
    }
    *cmd_ptr = '\0';
    
    // 提取参数
    while (*p && param_count < max_params) {
        if (*p == '[') {
            p++;  // 跳过 [
            char *param_ptr = params[param_count];
            
            // 提取参数内容直到 ]
            while (*p && *p != ']') {
                *param_ptr++ = *p++;
            }
            *param_ptr = '\0';
            
            if (*p == ']') {
                p++;  // 跳过 ]
                param_count++;
            }
        } else {
            p++;  // 跳过其他字符
        }
    }
    
    return param_count;
}

// 命令解析函数
void CMD::parseCommand(const char* command) {
    // 检查命令头
    if (command[0] != COMMAND_HEADER) {
        PRINT_ERROR("Invalid command header");
        Serial.println("Command Format: #command[parameter]*");
        PRINT_INFO("Use '#help*' for available commands");
        return;
    }
  
    // 指令解析
    char cmd[32] = {0};
    char param[5][32] = {0};
    // int parsed = sscanf(command, "#%[^[][%[^]]]", cmd, param);
    int parsed = parse_command_simple(command, cmd, param, 5);
  
    // 命令处理
    
    if (strcmp(cmd, set_cpu_freq) == 0) {
        if (parsed == 1){
            int freq = atoi(param[0]);
            Serial.end();
            if (setCpuFrequencyMhz(freq)){
                Serial.begin(hal.pref.getUInt("uart_baud", 115200));
                Serial.setDebugOutput(true);
                PRINT_SUCCESS("CPU frequency set successfully");
                Serial.printf("New frequency: %d MHz\n", freq);
            } else {
                Serial.begin(hal.pref.getUInt("uart_baud", 115200));
                Serial.setDebugOutput(true);
                PRINT_ERROR("Failed to set CPU frequency");
                PRINT_INFO("Valid frequencies: 240, 160, 80, 40, 20, 10");
            }
        } else {
            Serial.printf("Current CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
        }
    } else if (strcmp(cmd, set_long_press) == 0) {
        if (parsed == 1) {
            int value = atoi(param[0]);
            hal.pref.putInt("lpt", value);
            PRINT_SUCCESS("Long press threshold updated");
            Serial.printf("New threshold: %d ms\n", value * 10);
        } else {
            Serial.printf("Current long press threshold: %d ms\n", hal.pref.getInt("lpt", 20) * 10);
        }
    } else if (strcmp(cmd, config_cpu_freq) == 0) {
        if (parsed == 1) {
            int freq = atoi(param[0]);
            hal.pref.putInt("CpuFreq", freq);
            PRINT_SUCCESS("CPU frequency configuration saved");
            Serial.printf("Frequency will be set to %d MHz after reboot\n", freq);
        } else {
            PRINT_ERROR("Missing frequency parameter");
            PRINT_INFO("Usage: #cfgcpufreq[frequency]*");
        }
    } else if (strcmp(cmd, set_display) == 0) {
        if (parsed == 1) {
            int value = atoi(param[0]);
            hal.pref.putInt("dlsplay", value);
            PRINT_SUCCESS("Display gray level set");
        } else {
            PRINT_ERROR("Missing parameter");
            PRINT_INFO("Usage: #displaygray[level]*");
        }
    } else if (strcmp(cmd, set_display_PLL) == 0) {
        if (parsed == 1) {
            int value = atoi(param[0]);
            // display.epd2.PLL_set(value);
            PRINT_SUCCESS("Display PLL clock set temporarily");
            Serial.println("Note: This setting will reset after ESP32 reboot");
        } else {
            PRINT_ERROR("Missing PLL value parameter");
            PRINT_INFO("Usage: #PLL[value]*");
        }
    } else if (strcmp(cmd, cfg_display_PLL) == 0) {
        if (parsed == 1) {
            int value = atoi(param[0]);
            hal.pref.putUInt("pllset", value);
            // display.epd2.PLL_set(value);
            PRINT_SUCCESS("Display PLL clock configured and saved");
        } else {
            PRINT_ERROR("Missing PLL value parameter");
            PRINT_INFO("Usage: #cfgPLL[value]*");
        }
    } else if (strcmp(cmd, set_display_debug) == 0) {
        if (parsed == 1) {
            int value = atoi(param[0]);
            hal.pref.putInt("display_debug", value);
            PRINT_SUCCESS("Display debug mode updated");
            Serial.printf("Debug mode: %s\n", value ? "ENABLED" : "DISABLED");
        } else {
            PRINT_ERROR("Missing parameter");
            PRINT_INFO("Usage: #display_debug[0/1]*");
        }
    } else if (strcmp(cmd, temp_log) == 0) {
        if (parsed == 1) {
            int value = atoi(param[0]);
            hal.pref.putBool("temp_log", (bool)value);
            PRINT_SUCCESS("Temperature logging updated");
            Serial.printf("Temperature logging: %s\n", value ? "ENABLED" : "DISABLED");
        } else {
            bool enabled = hal.pref.getBool("temp_log");
            Serial.printf("Temperature logging: %s\n", enabled ? "ENABLED" : "DISABLED");
        }
    } else if (strcmp(cmd, erase_nvs) == 0) {
        WARNING_COLOR;
        Serial.println("WARNING: This will erase all NVS data!");
        RESET_COLOR;
        if (nvs_flash_erase() == ESP_OK) {
            PRINT_SUCCESS("NVS erased successfully");
            PRINT_INFO("Device will restart in 3 seconds...");
            delay(3000);
            ESP.restart();
        } else {
            PRINT_ERROR("Failed to erase NVS");
        }
    } else if (strcmp(cmd, littlefs_format) == 0) {
        WARNING_COLOR;
        Serial.println("WARNING: This will format LittleFS and erase all data!");
        RESET_COLOR;
        LittleFS.end();
        if (LittleFS.format()){
            if (LittleFS.begin()) {
                PRINT_SUCCESS("LittleFS formatted successfully");
            } else {
                PRINT_ERROR("Failed to remount LittleFS after format");
            }
        } else {
            PRINT_ERROR("Failed to format LittleFS");
        }
    } else if (strcmp(cmd, format_tf) == 0) {
        // 需要加载TF卡
        // 首先测试TF卡是否存在
        if (digitalRead(PIN_SD_CARDDETECT) != 1)
        {
            info("加载TF卡");
            gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
            digitalWrite(PIN_SDVDD_CTRL, 0);
            gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
            delay(50);
            uint32_t freq = (uint32_t)hal.pref.getInt("sd_clk_freq" , 3500000);
            info("设置TF卡频率:%d HZ\n", freq); 
            if (SD.begin(PIN_SD_CS, SDSPI, freq, "/sd", 5, true) == false)
            {
                delay(100);
                warn("TF卡挂载失败,尝试重新挂载");
                if (SD.begin(PIN_SD_CS, SDSPI, freq, "/sd", 5, true) == false)
                {
                    GUI::msgbox("错误", "存在TF卡，但无法挂载");
                    SD.end();
                    gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
                    digitalWrite(PIN_SDVDD_CTRL, 1);
                    gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
                }
            }
        }else{
            log_w("[外设] 未插入TF卡");
        }
    } else if (strcmp(cmd, littlefs_info) == 0) {
        size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
        HEADER_COLOR;
        Serial.println("LITTLEFS FILESYSTEM INFORMATION:");
        RESET_COLOR;
        Serial.printf("  Total space: %d KB\n", total / 1024);
        Serial.printf("  Used space:  %d KB (%.02f%%)\n", used / 1024, (float)used / (float)total * 100.0);
        Serial.printf("  Free space:  %d KB\n", (total - used) / 1024);
    } else if (strcmp(cmd, free_heap_size) == 0) {
        uint32_t heap = ESP.getHeapSize(), free_heap = ESP.getFreeHeap();
        HEADER_COLOR;
        Serial.println("MEMORY INFORMATION:");
        RESET_COLOR;
        Serial.printf("  Total heap:  %.2f KB\n", (float)heap / 1024.0);
        Serial.printf("  Free heap:   %.2f KB\n", (float)free_heap / 1024.0);
        Serial.printf("  Usage:       %.02f%%\n", ((float)heap - (float)free_heap) / (float)heap * 100.0);
    } else if (strcmp(cmd, esp_chip_info_) == 0) {
        HEADER_COLOR;
        Serial.println("CHIP INFORMATION:");
        RESET_COLOR;
        Serial.printf("  Model:       %s\n", ESP.getChipModel());
        Serial.printf("  Revision:    %u\n", ESP.getChipRevision());
        Serial.printf("  Cores:       %u\n", ESP.getChipCores());
        
        uint64_t chipmacid = ESP.getEfuseMac();
        uint8_t* mac = (uint8_t*)&chipmacid;
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.printf("  MAC:         %s\n", macStr);
        Serial.printf("  Flash size:  %d MB\n", ESP.getFlashChipSize() / 1048576);
        
        uint32_t flash_id;
        uint64_t flash_unique_id;
        esp_flash_read_id(esp_flash_default_chip, &flash_id);
        esp_flash_read_unique_chip_id(esp_flash_default_chip, &flash_unique_id);
        Serial.printf("  Flash ID:    %04x\n", flash_id);
        Serial.printf("  Unique ID:   %016llx\n", flash_unique_id);
    } else if (strcmp(cmd, esp_restart_) == 0) {
        PRINT_INFO("Restarting device...");
        hal.pref.end();
        LittleFS.end();
        delay(1000);
        ESP.restart();
    } else if (strcmp(cmd, get_runtime) == 0) {
        long timeMillis = millis();
        long hours = timeMillis / 3600000;
        long remaining = timeMillis % 3600000;
        long minutes = remaining / 60000;
        remaining %= 60000;
        long seconds = remaining / 1000;
        long tenths = (remaining % 1000) / 100;
        Serial.printf("Device runtime: %4d:%02d:%02d.%d\n", hours, minutes, seconds, tenths);
    } else if (strcmp(cmd, get_bat_info) == 0){
        hal.printBatteryInfo();
    } else if (strcmp(cmd, get_cpu_usage) == 0) { 
        PRINT_WARNING("CPU usage monitoring only available in ESP-IDF environment");
    } else if (strcmp(cmd, set_boot_app) == 0) {
        hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
        PRINT_SUCCESS("Default boot application set to 'clock'");
    } else if (strcmp(cmd, help) == 0) {
        printHelp();
    } else if (strcmp(cmd, esp_light_sleep) == 0) {
        int value = 0;
        if (parsed == 1) {
            value = atoi(param[0]);
            Serial.printf("Entering light sleep with timeout: %d ms\n", value);
        } else {
            Serial.println("Entering light sleep (press button to wake)");
        }
        hal.wait_input(value);
    } else if (strcmp(cmd, file_server_begin) == 0) {
        stop_fileserver = false;
        hal.can_sleep  = false;
        hal.can_light_sleep  = false;
        xTaskCreatePinnedToCore(fileserver_task, "fileserver", 8192, NULL, 1, NULL, 0);
        PRINT_SUCCESS("File server started");
    } else if (strcmp(cmd, file_server_end) == 0) {
        stop_fileserver = true;
        delay(200);
        PRINT_SUCCESS("File server stopped");
    } else if (strcmp(cmd, esp_partition_info) == 0) {
        INFO_COLOR;
        Serial.println("PARTITION INFORMATION:");
        RESET_COLOR;
        printPartitionsInfo();
    } else if (strcmp(cmd, putnvs) == 0) {
        if (parsed == 3)
        {
            int value = atoi(param[1]);
            int error = 0;
            if (strcmp(param[2], "bool") == 0)
                error = hal.pref.putBool(param[0], value);
            else if (strcmp(param[2], "int") == 0)
                error = hal.pref.putInt(param[0], value);
            else if (strcmp(param[2], "uint") == 0)
                error = hal.pref.putUInt(param[0], value);
            else if (strcmp(param[2], "u8") == 0)
                error = hal.pref.putUChar(param[0], value);
            if (error = 0){
                PRINT_ERROR("写入失败");
            }
        }
        else
            PRINT_ERROR("参数不足");
    } else if (strcmp(cmd, getnvs) == 0) {
        if (parsed == 2)
        {
            int value;
            if (strcmp(param[1], "bool") == 0)
                value = hal.pref.getBool(param[0]);
            else if (strcmp(param[1], "int") == 0)
                value = hal.pref.getInt(param[0]);
            else if (strcmp(param[1], "uint") == 0)
                value = hal.pref.getUInt(param[0]);
            Serial.printf("%s: %ld\n", param[0], value);
        }
        else
            PRINT_ERROR("参数不足");
    } else if (strcmp(cmd, removenvs) == 0) {
        if (parsed == 1)
        {
            bool value;
            value = hal.pref.getBool(param[0]);
            Serial.printf("%s: %s\n", param[0], value ? "true" : "false");
        }
        else
            PRINT_ERROR("参数不足");
    } else {
        PRINT_ERROR("Unknown command");
        PRINT_INFO("Use '#help*' to see available commands");
    }
}