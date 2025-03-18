#pragma once
#include "A_Config.h"
// 缓冲区大小
#define COMMAND_BUFFER_SIZE 1024
// 命令头标识
#define COMMAND_HEADER '#'
// 命令结束符
#define COMMAND_TERMINATOR '*'

//命令列表
#define set_cpu_freq        "set_cpu_freq"
#define set_long_press      "set_long_press"
#define get_runtime         "get_runtime"
#define get_cpu_usage       "cpu_usage"
#define erase_nvs           "erase_nvs"
#define littlefs_format     "littlefs_format"
#define littlefs_info       "littlefs_info"
#define get_bat_info        "battery_info"
#define free_heap_size      "free_heap_size"
#define esp_light_sleep     "light_sleep"
#define esp_chip_info_      "chip_info"
#define esp_restart_        "restart"


// 全局缓冲区
class CMD
{
private:
    TaskHandle_t *cmd_task_handle = NULL;
public:
    char cmdBuffer[COMMAND_BUFFER_SIZE];
    void begin();
    void end();
    void parseCommand(const char* command);
};
extern CMD cmd;