#include "A_Config.h"


// 全局日志缓冲区
File file_log;
char * temp;
bool flush_ok = false;
bool need_free = false;

// 刷新日志到文件
void log_flush()
{
    if (!file_log){
        file_log = LittleFS.open("/System/log.txt", "a");
        file_log.setBufferSize(4096);
    }
    if (file_log)
    {
        file_log.printf("%s", temp);
        if (need_free)
            free(temp);
        if (!hal.pref.getBool("fast_boot"))
            file_log.flush();
    }
    else
    {
        Serial.println("无法打开日志文件");
    }
}

void flush_log(void *){
    flush_ok = false;
    log_flush();
    flush_ok = true;
    vTaskDelete(NULL);
}
int log_printfv(const char *format, va_list arg)
{
    static char loc_buf[64];
    temp = loc_buf;
    uint32_t len;
    va_list copy;
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    need_free = false;
    if(len >= sizeof(loc_buf)){
        need_free = true;
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len+1, format, arg);
    Serial.printf("%s", temp);
    
    return len;
}

void log_write(const char *fmt, ...)
{
    int len;
    va_list arg;
    va_start(arg, fmt);
    len = log_printfv(fmt, arg);
    va_end(arg);
    if (!hal.pref.getBool("sys_log"))
        return;
    log_flush();
}
