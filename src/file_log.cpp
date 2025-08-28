#include "A_Config.h"


// 全局日志缓冲区
static String logBuffer;
static const size_t LOG_BUFFER_SIZE = 8192; // 8KB缓冲区
File file_log;

// 刷新日志到文件
void log_flush()
{
    if (logBuffer.length() > 0)
    {
        if (!file_log)
            file_log = LittleFS.open("/System/log.txt", "a");
        if (file_log)
        {
            file_log.print(logBuffer);
            if (!hal.pref.getBool("fast_boot"))
                file_log.flush();
            logBuffer = ""; // 清空缓冲区
        }
        else
        {
            Serial.println("无法打开日志文件");
        }
    }
}

// 日志记录函数
void log_write(const char *file, int line, const char *fmt, ...)
{
    if (!hal.pref.getBool("sys_log"))
        return;
    // 检查缓冲区剩余空间，如果不足则刷新
    if (logBuffer.length() > LOG_BUFFER_SIZE * 0.8)
    { // 当缓冲区使用超过80%时刷新
        log_flush();
    }

    // 格式化时间戳和位置信息
    char header[100];
    snprintf(header, sizeof(header), "[%06d][%s:%d] ", esp_log_timestamp(), file, line);
    logBuffer += header;

    // 格式化日志内容
    char content[256]; // 单条日志内容缓冲区
    va_list args;
    va_start(args, fmt);
    vsnprintf(content, sizeof(content), fmt, args);
    va_end(args);

    logBuffer += content;
    logBuffer += "\n";

    // 如果单条日志就很大，接近缓冲区限制，立即刷新
    if (logBuffer.length() > LOG_BUFFER_SIZE * 0.9)
    {
        log_flush();
    }
}
