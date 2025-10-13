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
#define set_display_debug   "display_debug"
#define set_display_PLL     "PLL"
#define cfg_display_PLL     "cfgPLL"
#define config_cpu_freq     "cfgcpufreq"
#define set_long_press      "longpress"
#define get_runtime         "runtime"
#define get_cpu_usage       "cpuuse"
#define set_boot_app        "bootapp->clock"
#define erase_nvs           "erasenvs"
#define littlefs_format     "lfsformat"
#define littlefs_info       "lfsinfo"
#define get_bat_info        "batinfo"
#define free_heap_size      "heap"
#define esp_light_sleep     "lightsleep"
#define esp_chip_info_      "chipinfo"
#define esp_partition_info  "partitioninfo"
#define file_server_begin   "fserverbegin"
#define file_server_end     "fserverend"
#define esp_restart_        "rst"
#define temp_log            "templog"
#define format_tf           "formattf"

#define getnvs              "getnvs"
#define putnvs              "putnvs"
//串口颜色转义码
#define RED     Serial.print("\033[31m")
#define GREEN   Serial.print("\033[32m")
#define YELLOW  Serial.print("\033[33m")
#define BLUE    Serial.print("\033[34m")
#define MAGENTA Serial.print("\033[35m")
#define CYAN    Serial.print("\033[36m")
#define WHITE   Serial.print("\033[37m")
#define RESET   Serial.print("\033[0m")

#define ERROR_COLOR     Serial.print("\033[91m")  // 亮红色
#define WARNING_COLOR   Serial.print("\033[93m")  // 亮黄色  
#define SUCCESS_COLOR   Serial.print("\033[92m")  // 亮绿色
#define INFO_COLOR      Serial.print("\033[96m")  // 亮青色
#define HEADER_COLOR    Serial.print("\033[95m")  // 亮紫色
#define RESET_COLOR     Serial.print("\033[0m")

#define PRINT_ERROR(msg)     do { ERROR_COLOR; Serial.print("ERROR: "); Serial.println(msg); RESET_COLOR; } while(0)
#define PRINT_WARNING(msg)   do { WARNING_COLOR; Serial.print("WARNING: "); Serial.println(msg); RESET_COLOR; } while(0)
#define PRINT_SUCCESS(msg)   do { SUCCESS_COLOR; Serial.print("SUCCESS: "); Serial.println(msg); RESET_COLOR; } while(0)
#define PRINT_INFO(msg)      do { INFO_COLOR; Serial.println(msg); RESET_COLOR; } while(0)

class CMD
{
private:
    TaskHandle_t cmd_task_handle = NULL;
public:
    char cmdBuffer[COMMAND_BUFFER_SIZE];
    void begin();
    void run();
    void stop();
    void end();
    void printHelp();
    void parseCommand(const char* command);
};
extern CMD cmd;