#include "Serial_cmd.h"
#include <nvs_flash.h>
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
        hal.cheak_freq();
        WiFi.softAP("WeatherClock", passwd.c_str());
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
        str1 = "192.168.4.1";
    }
    beginWebServer();
    while(1){
        if (!stop_fileserver){
            if (wifi)
                dnsServer.stop();
            server.end();
            vTaskDelete(NULL);
        }else
            vTaskDelay(100);
    }
}

void cmd_task(void *pvParameters) {
    size_t bufIndex = 0;
    memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
    BLUE;
    Serial.println("LiClock串口工具已启动");
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
  
        delay(10); // 适当释放CPU
    }
}
void CMD::begin(){
    xTaskCreatePinnedToCore(cmd_task, "cmd_task", 4096, NULL, 1, cmd_task_handle, 0);
    esp_sleep_enable_uart_wakeup(0);
}
void CMD::end(){
    //vTaskSuspend(cmd_task_handle);
    vTaskDelete(cmd_task_handle);
}
void CMD::printHelp(){
    Serial.println("===================================");
    Serial.println("欢迎使用LiClock串口工具");
    Serial.println("此工具旨在帮助固件开发和调试，以及进行错误设置后的救砖操作");
    Serial.println("命令格式: #command[param]*");
    Serial.println("\n=== Available Commands ===");
    
    // 系统命令
    Serial.println("\n[System]");
    Serial.printf("%-18s - %s\n", help, "显示帮助信息");
    Serial.printf("%-18s - %s\n", esp_light_sleep, "使设备CPU停止（由按键唤醒）");
    Serial.printf("%-18s - %s\n", ((String)esp_light_sleep + "[]").c_str(), "使设备CPU停止（由按键唤醒）");
    Serial.printf("%-18s - %s\n", esp_restart_, "重启设备");
    Serial.printf("%-18s - %s\n", free_heap_size, "显示剩余内存");

    // 硬件控制
    Serial.println("[Hardware]");
    Serial.printf("%-18s - %s\n", set_cpu_freq, "获取CPU频率（单位：MHz）");
    Serial.printf("%-18s - %s\n", ((String)set_cpu_freq + "[]").c_str(), "设置CPU频率（单位：MHz）,立即生效,注意应在[]中填入参数");
    Serial.printf("%-18s - %s\n", ((String)config_cpu_freq + "[]").c_str(), "保存CPU频率（单位：MHz）到设置,重启后生效,注意应在[]中填入参数");
    Serial.println("CPU频率可选值:240、160、80、40、20、10");
    Serial.printf("%-18s - %s\n", esp_chip_info_, "显示芯片信息");

    // 文件系统
    Serial.println("[Filesystem]");
    Serial.printf("%-18s - %s\n", littlefs_format, "格式化LittleFS");
    Serial.printf("%-18s - %s\n", littlefs_info, "显示存储空间信息");
    Serial.printf("%-18s - %s\n", erase_nvs, "擦除NVS存储");

    // 其他
    Serial.println("[Parameters]");
    Serial.printf("%-18s - %s\n", set_long_press, "获取长按阈值（单位：ms）");
    Serial.printf("%-18s - %s\n", ((String)set_long_press + "[]").c_str(), "设置长按阈值（单位：ms）,注意应在[]中填入参数");
    Serial.printf("%-18s - %s\n", set_boot_app, "修改默认APP为clock");
    Serial.printf("%-18s - %s\n", get_bat_info, "显示电池信息");
    
    Serial.println("Example:");
    Serial.println("#set_cpu_freq[240]*");
    Serial.println("#chip_info*");
}
// 命令解析函数
void CMD::parseCommand(const char* command) {
    // 检查命令头
    if (command[0] != COMMAND_HEADER) {
        RED;
        Serial.println("Error: Invalid command header");
        RESET;
        Serial.println("Command Format: #command[param]*");
        YELLOW;
        Serial.println("use '#help*' to get help");
        RESET;
        return;
    }
  
    // 指令解析
    char cmd[32] = {0};
    char param[32] = {0};
    int parsed = sscanf(command, "#%[^[][%[^]]]", cmd, param);
  
    // 命令处理
    if (parsed >= 1) {
        // Serial.printf("Command: %s\n", cmd);
        if (parsed == 2) {
            // Serial.printf("Parameter: %s\n", param);
        }
        // 指令处理
        if (strcmp(cmd, set_cpu_freq) == 0) {
            if (parsed == 2){
                int freq = atoi(param);
                Serial.end();
                if (setCpuFrequencyMhz(freq)){
                    Serial.begin(115200);
                    Serial.setDebugOutput(true);
                    Serial.println("CPU frequency set successfully");
                } else {
                    Serial.begin(115200);
                    Serial.setDebugOutput(true);
                    Serial.println("Error: Failed to set CPU frequency");
                }
            } else {
                Serial.printf("CPU frequency:%uMhz\n", ESP.getCpuFreqMHz());
            }
        } else if (strcmp(cmd, set_long_press) == 0) {
            if (parsed == 2) {
                int value = atoi(param);
                hal.pref.putInt("lpt", value);
            } else {
                Serial.printf("LongPress wait time:%dms\n", hal.pref.getInt("lpt", 20) * 10);
            }
        } else if (strcmp(cmd, config_cpu_freq) == 0 && parsed == 2) {
            int freq = atoi(param);
            hal.pref.putInt("CpuFreq", freq);
        } else if (strcmp(cmd, set_display) == 0 && parsed == 2) {
            int value = atoi(param);
            hal.pref.putInt("dlsplay", value);
        } else if (strcmp(cmd, set_display_PLL) == 0 && parsed == 2) {
            int value = atoi(param);
            display.epd2.PLL_set(value);
        } else if (strcmp(cmd, cfg_display_PLL) == 0 && parsed == 2) {
            int value = atoi(param);
            hal.pref.putUInt("pllset", value);
            display.epd2.PLL_set(value);
        } else if (strcmp(cmd, set_display_debug) == 0 && parsed == 2) {
            int value = atoi(param);
            hal.pref.putInt("display_debug", value);
        } else if (strcmp(cmd, erase_nvs) == 0) {
            if (nvs_flash_erase() == ESP_OK)
                Serial.println("NVS erased successfully");
            else    
                Serial.println("Error: Failed to erase NVS");
        } else if (strcmp(cmd, littlefs_format) == 0) {
            LittleFS.end();
            if (!LittleFS.format()){
                Serial.println("Error: Failed to format LittleFS");
                return;
            }
            if (!LittleFS.begin())
                Serial.println("Error: Failed to begin LittleFS");
        } else if (strcmp(cmd, littlefs_info) == 0) {
            size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
            Serial.printf("LittleFS total space: %d KB\n", total / 1024);
            Serial.printf("LittleFS used space:  %d KB (%.02f%%)\n", used / 1024, (float)used / (float)total * 100.0);
            Serial.printf("LittleFS free space:  %d KB\n", (total - used) / 1024);
        } else if (strcmp(cmd, free_heap_size) == 0) {
            uint32_t heap = ESP.getHeapSize(), free_heap = ESP.getFreeHeap();
            Serial.printf("All heap size:  %d KB\n", heap / 1024);
            Serial.printf("Free heap size: %d KB\n", free_heap / 1024);
            Serial.printf("内存占用:        %.02f%%\n", ((float)heap - (float)free_heap) / (float)heap * 100.0);
        } else if (strcmp(cmd, esp_chip_info_) == 0) {
            Serial.printf("ChipModel:    %s\n", ESP.getChipModel());
            Serial.printf("ChipRevision: %u\n", ESP.getChipRevision());
            Serial.printf("ChipCores:    %u\n", ESP.getChipCores());
            // 获取MAC地址
            uint64_t chipmacid = ESP.getEfuseMac();

            // 转换为uint8_t数组指针
            uint8_t* mac = (uint8_t*)&chipmacid;

            // 格式化并打印MAC地址
            char macStr[18]; // MAC字符串缓冲区（17字符+终止符）
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            Serial.print("EfuseMac: ");
            Serial.println(macStr);
            Serial.printf("FlashChipSize:%d MB\n", ESP.getFlashChipSize() / 1048576);
            uint32_t flash_id;
            uint64_t flash_unique_id;
            esp_flash_read_id(esp_flash_default_chip, &flash_id);
            esp_flash_read_unique_chip_id(esp_flash_default_chip, &flash_unique_id);
            Serial.printf("flash_id:%04x\n", flash_id);
            Serial.printf("flash_unique_id:%08x\n", flash_unique_id);
        } else if (strcmp(cmd, esp_restart_) == 0) {
            ESP.restart();
        } else if (strcmp(cmd, get_runtime) == 0) {
            long timeMillis = millis();
            long hours = timeMillis / 3600000;      // 计算小时
            long remaining = timeMillis % 3600000;  // 剩余毫秒数
            long minutes = remaining / 60000;       // 计算分钟
            remaining %= 60000;                              // 剩余毫秒数
            long seconds = remaining / 1000;        // 计算秒
            long tenths = (remaining % 1000) / 100; // 计算十分位（0-9）
            Serial.printf("Runtime: %3d:%02d:%02d.%d", hours, minutes, seconds, tenths);
        } else if (strcmp(cmd, get_bat_info) == 0){
            hal.task_bat_info_update();
            hal.printBatteryInfo();
        } else if (strcmp(cmd, get_cpu_usage) == 0) {
            Serial.println("only use in ESP-IDF");
        } else if (strcmp(cmd, set_boot_app) == 0) {
            hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
        } else if (strcmp(cmd, help) == 0) {
            printHelp();
        } else if (strcmp(cmd, esp_light_sleep) == 0) {
            int value = 0;
            if (parsed == 2)
                value = atoi(param);
            hal.wait_input(value);
        } else if (strcmp(cmd, file_server_begin) == 0) {
            xTaskCreatePinnedToCore(fileserver_task, "fileserver", 8192, NULL, 1, NULL, 0);
        } else if (strcmp(cmd, file_server_end) == 0) {
            stop_fileserver = true;
            serverRunning = false;
        } else {
            RED;
            Serial.println("Error: Unknown command");
            YELLOW;
            Serial.println("use '#help*' to get help");
            RESET;
        }
        
    } else {
        RED;
        Serial.println("Error: Invalid command format");
        YELLOW;
        Serial.println("use '#help*' to get help");
        RESET;
    }
}