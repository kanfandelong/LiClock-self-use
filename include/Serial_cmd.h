#pragma once
#include "A_Config.h"
#include "esp_console.h"
// 缓冲区大小
#define COMMAND_BUFFER_SIZE 8192 * 4
// 命令头标识
#define COMMAND_HEADER '#'
// 命令结束符
#define COMMAND_TERMINATOR '*'

//命令列表
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
#define aboutcode           "aboutcode"

#define getnvs              "getnvs"
#define putnvs              "putnvs"
#define removenvs           "removenvs"
//串口颜色转义码
#define RED     log_printf("\033[31m")
#define GREEN   log_printf("\033[32m")
#define YELLOW  log_printf("\033[33m")
#define BLUE    log_printf("\033[34m")
#define MAGENTA log_printf("\033[35m")
#define CYAN    log_printf("\033[36m")
#define WHITE   log_printf("\033[37m")
#define RESET   log_printf("\033[0m")

#define ERROR_COLOR     log_printf("\033[91m")  // 亮红色
#define WARNING_COLOR   log_printf("\033[93m")  // 亮黄色  
#define SUCCESS_COLOR   log_printf("\033[92m")  // 亮绿色
#define INFO_COLOR      log_printf("\033[96m")  // 亮青色
#define HEADER_COLOR    log_printf("\033[95m")  // 亮紫色
#define RESET_COLOR     log_printf("\033[0m")

#define PRINT_ERROR(fmt, ...)           \
    do                                  \
    {                                   \
        ERROR_COLOR;                    \
        log_printf(fmt, ##__VA_ARGS__); \
        log_printf("\n");               \
        RESET_COLOR;                    \
    } while (0)
#define PRINT_WARNING(fmt, ...)         \
    do                                  \
    {                                   \
        WARNING_COLOR;                  \
        log_printf(fmt, ##__VA_ARGS__); \
        log_printf("\n");               \
        RESET_COLOR;                    \
    } while (0)
#define PRINT_SUCCESS(fmt, ...)         \
    do                                  \
    {                                   \
        SUCCESS_COLOR;                  \
        log_printf(fmt, ##__VA_ARGS__); \
        log_printf("\n");               \
        RESET_COLOR;                    \
    } while (0)
#define PRINT_INFO(fmt, ...)            \
    do                                  \
    {                                   \
        INFO_COLOR;                     \
        log_printf(fmt, ##__VA_ARGS__); \
        log_printf("\n");               \
        RESET_COLOR;                    \
    } while (0)

class CMD
{
private:
    void register_commands();
    bool is_run = false;

public:
    char *cmdBuffer;
    void begin();
    void SetCallback();
    void run();
    void stop();
    void end();
};
extern CMD cmd;