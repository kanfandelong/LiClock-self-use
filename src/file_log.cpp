#include "A_Config.h"

typedef struct
{
    char *buf;
    int len;
} multi_thread_params_t;

// 全局变量
static File file_log;
static TaskHandle_t log_task_handle = NULL;
static QueueHandle_t multi_thread_queue = NULL;
static SemaphoreHandle_t log_mutex = NULL;

// 刷新日志到文件
static void log_flush(char *buf, int len)
{
    if (!file_log)
    {
        file_log = LittleFS.open("/System/log.txt", "a");
        if (!file_log)
        {
            // 使用串口输出错误，避免递归调用log_write
            log_e("[ERROR] 无法打开日志文件");
            if (len >= 64)
                free(buf);
            return;
        }
        file_log.setBufferSize(4096);
    }
    
    if (file_log)
    {
        file_log.write((uint8_t*)buf, len);
        if (len >= 64)
            free(buf);
            
        if (!hal.pref.getBool("fast_boot", false))
            file_log.flush();
    }
}

// 日志刷新任务
static void flush_log_task(void *params)
{
    multi_thread_params_t multi_thread_params;
    
    while (1)
    {
        if (xQueueReceive(multi_thread_queue, &multi_thread_params, portMAX_DELAY) == pdTRUE)
        {
            log_flush(multi_thread_params.buf, multi_thread_params.len);
        }
    }
    vsnprintf(temp, len+1, format, arg);
    Serial.printf("%s", temp);
    
    return len;
}

// 初始化日志系统
bool log_system_init()
{
    if (log_mutex == NULL)
    {
        log_mutex = xSemaphoreCreateMutex();
        if (log_mutex == NULL)
            return false;
    }
    
    if (multi_thread_queue == NULL)
    {
        multi_thread_queue = xQueueCreate(10, sizeof(multi_thread_params_t));
        if (multi_thread_queue == NULL)
            return false;
    }
    
    if (log_task_handle == NULL)
    {
        // 创建任务前确保队列已创建
        if (xTaskCreate(flush_log_task, 
                       "log_flush", 
                       4096,  // 增加堆栈大小
                       NULL, 
                       2,     // 提高优先级
                       &log_task_handle) != pdPASS)
        {
            return false;
        }
    }
    
    return true;
}

// 格式化日志并输出
int log_printfv(const char *format, va_list arg)
{
    va_list arg_copy;
    va_copy(arg_copy, arg);
    
    // 计算所需长度
    int len = vsnprintf(NULL, 0, format, arg_copy);
    va_end(arg_copy);
    
    if (len < 0)
        return 0;
    
    // 分配缓冲区（包含终止符）
    char *buffer = (char *)malloc(len + 1);
    if (buffer == NULL)
    {
        log_e("日志内存分配失败");
        return 0;
    }
    
    // 格式化字符串
    vsnprintf(buffer, len + 1, format, arg);
    
    // 输出到串口
    Serial.write(buffer, len);
    
    // 如果需要写入文件
    if (hal.pref.getBool("sys_log", true))
    {
        // 发送到队列
        if (log_task_handle != NULL && multi_thread_queue != NULL)
        {
            multi_thread_params_t params;
            params.buf = buffer;
            params.len = len;
            
            if (xQueueSend(multi_thread_queue, &params, 100 / portTICK_PERIOD_MS) != pdTRUE)
            {
                // 队列满，直接释放
                log_w("日志队列满，丢弃日志");
                free(buffer);
            }
        }
        else
        {
            // 任务未初始化，直接释放
            free(buffer);
        }
    }
    else
    {
        // 不写入文件，直接释放
        free(buffer);
    }
    
    return len;
}

// 主要的日志写入函数
void log_write(const char *fmt, ...)
{
    // 初始化检查
    if (log_task_handle == NULL)
    {
        if (!log_system_init())
        {
            // 初始化失败，直接输出到串口
            va_list arg;
            va_start(arg, fmt);
            char temp_buf[128];
            vsnprintf(temp_buf, sizeof(temp_buf), fmt, arg);
            va_end(arg);
            Serial.print(temp_buf);
            return;
        }
    }
    
    // 获取互斥锁（如果需要线程安全）
    if (log_mutex != NULL)
    {
        xSemaphoreTake(log_mutex, portMAX_DELAY);
    }
    
    va_list arg;
    va_start(arg, fmt);
    log_printfv(fmt, arg);
    va_end(arg);
    
    if (log_mutex != NULL)
    {
        xSemaphoreGive(log_mutex);
    }
}

// 关闭日志系统
void log_system_deinit()
{
    if (file_log)
    {
        file_log.close();
    }
    
    if (log_task_handle != NULL)
    {
        vTaskDelete(log_task_handle);
        log_task_handle = NULL;
    }
    
    if (multi_thread_queue != NULL)
    {
        // 清空队列
        multi_thread_params_t params;
        while (xQueueReceive(multi_thread_queue, &params, 0) == pdTRUE)
        {
            if (params.len >= 64)
                free(params.buf);
        }
        vQueueDelete(multi_thread_queue);
        multi_thread_queue = NULL;
    }
    
    if (log_mutex != NULL)
    {
        vSemaphoreDelete(log_mutex);
        log_mutex = NULL;
    }
}