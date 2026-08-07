// 在 main.cpp 或单独一个文件中

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 声明原始函数（链接器会提供 __real_ 版本）
BaseType_t __real_xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode,
                                          const char * const pcName,
                                          const configSTACK_DEPTH_TYPE usStackDepth,
                                          void * const pvParameters,
                                          UBaseType_t uxPriority,
                                          TaskHandle_t * const pvCreatedTask,
                                          const BaseType_t xCoreID);

// 你的包装函数
BaseType_t __wrap_xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode,
                                          const char * const pcName,
                                          const configSTACK_DEPTH_TYPE usStackDepth,
                                          void * const pvParameters,
                                          UBaseType_t uxPriority,
                                          TaskHandle_t * const pvCreatedTask,
                                          const BaseType_t xCoreID)
{
    // 判断是否为 IPC 任务（名称以 "ipc" 开头）
    if (pcName != NULL && strncmp(pcName, "ipc", 3) == 0) {
        // 设定你期望的栈大小（单位：字，例如 4096 字 = 16KB）
        configSTACK_DEPTH_TYPE newStackSize = 1024 + 512;
        // 调用原始函数，但传入了新的栈大小
        return __real_xTaskCreatePinnedToCore(pvTaskCode, pcName, newStackSize,
                                              pvParameters, uxPriority, pvCreatedTask, xCoreID);
    }
    // 其他任务保持不变
    return __real_xTaskCreatePinnedToCore(pvTaskCode, pcName, usStackDepth,
                                          pvParameters, uxPriority, pvCreatedTask, xCoreID);
}

#ifdef __cplusplus
}
#endif