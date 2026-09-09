#pragma once
#include "A_Config.h"
// 缓冲区大小
#define COMMAND_BUFFER_SIZE 8192 * 4

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