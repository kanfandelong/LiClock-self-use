#pragma once
#include "A_Config.h"
// 缓冲区大小
#define COMMAND_BUFFER_SIZE 1024
// 命令头标识
#define COMMAND_HEADER '#'
// 命令结束符
#define COMMAND_TERMINATOR '*'

//命令列表
#define help                "help"
#define set_cpu_freq        "cpufreq"
#define set_display         "displaygray"
#define set_display_PLL     "PLL"
#define config_cpu_freq     "configcpufreq"
#define set_long_press      "longpress"
#define get_runtime         "runtime"
#define get_cpu_usage       "cpuusage"
#define set_boot_app        "bootapp->clock"
#define erase_nvs           "erasenvs"
#define littlefs_format     "littlefsformat"
#define littlefs_info       "littlefsinfo"
#define get_bat_info        "batinfo"
#define free_heap_size      "freeheap"
#define esp_light_sleep     "lightsleep"
#define esp_chip_info_      "chipinfo"
#define file_server_begin   "fileserverbegin"
#define file_server_end     "fileserverend"
#define esp_restart_        "rst"
//串口颜色转义码
#define RED     Serial.print("\033[31m")
#define GREEN   Serial.print("\033[32m")
#define YELLOW  Serial.print("\033[33m")
#define BLUE    Serial.print("\033[34m")
#define MAGENTA Serial.print("\033[35m")
#define CYAN    Serial.print("\033[36m")
#define WHITE   Serial.print("\033[37m")
#define RESET   Serial.print("\033[0m")

class CMD
{
private:
    TaskHandle_t *cmd_task_handle = NULL;
public:
    char cmdBuffer[COMMAND_BUFFER_SIZE];
    void begin();
    void end();
    void printHelp();
    void parseCommand(const char* command);
};
extern CMD cmd;