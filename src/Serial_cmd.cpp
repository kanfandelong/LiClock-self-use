#include "Serial_cmd.h"
#include <nvs_flash.h>
CMD cmd;
char task_list[1024];
void cmd_task(void *pvParameters) {
    size_t bufIndex = 0;
    memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
    Serial.println("Serial command task started");
    while(1) {
        // 读取串口数据
        while (Serial.available() > 0) {
            char c = Serial.read();
  
            if (c == COMMAND_TERMINATOR) {
                cmd.cmdBuffer[bufIndex] = '\0';
                Serial.printf("[DEBUG] Raw command: %s\n", cmd.cmdBuffer); // 调试日志
                cmd.parseCommand(cmd.cmdBuffer);
                bufIndex = 0;
                memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
                continue;
            }
            
            if (c == '\r') continue; // 忽略回车符
            
            if (bufIndex < COMMAND_BUFFER_SIZE - 1) {
                cmd.cmdBuffer[bufIndex++] = c;
            } else {
                bufIndex = 0;
                memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
                Serial.println("Error: Buffer overflow");
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
// 命令解析函数
void CMD::parseCommand(const char* command) {
    // 检查命令头
    if (command[0] != COMMAND_HEADER) {
        Serial.println("Error: Invalid command header");
        return;
    }
  
    // 提取指令部分
    char cmd[32] = {0};
    char param[32] = {0};
    int parsed = sscanf(command, "#%[^[][%[^]]]", cmd, param);
  
    // 根据解析结果处理命令
    if (parsed >= 1) {
        // Serial.printf("Command: %s\n", cmd);
        if (parsed == 2) {
            // Serial.printf("Parameter: %s\n", param);
        }
        // 指令处理
        if (strcmp(cmd, set_cpu_freq) == 0 && parsed == 2) {
            int freq = atoi(param);
            if (setCpuFrequencyMhz(freq)){
                Serial.println("CPU frequency set successfully");
                hal.pref.putInt("CpuFreq", freq);
            } else 
                Serial.println("Error: Failed to set CPU frequency");
        } else if (strcmp(cmd, set_long_press) == 0 && parsed == 2) {
            int value = atoi(param);
            hal.pref.putInt("lpt", value);
        } else if (strcmp(cmd, erase_nvs) == 0) {
            nvs_flash_erase();
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
            Serial.printf("Heap fragmentation: %.02f%%\n", ((float)heap - (float)free_heap) / (float)heap * 100.0);
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
        } else 
            Serial.println("Error: Unknown command");
        
    } else {
        Serial.println("Error: Invalid command format");
    }
}