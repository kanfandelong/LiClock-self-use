#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

BaseType_t __real_xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode,
                                          const char * const pcName,
                                          const configSTACK_DEPTH_TYPE usStackDepth,
                                          void * const pvParameters,
                                          UBaseType_t uxPriority,
                                          TaskHandle_t * const pvCreatedTask,
                                          const BaseType_t xCoreID);

BaseType_t __wrap_xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode,
                                          const char * const pcName,
                                          const configSTACK_DEPTH_TYPE usStackDepth,
                                          void * const pvParameters,
                                          UBaseType_t uxPriority,
                                          TaskHandle_t * const pvCreatedTask,
                                          const BaseType_t xCoreID)
{
    if (pcName != NULL && strncmp(pcName, "ipc", 3) == 0) {
        configSTACK_DEPTH_TYPE newStackSize = 1024 + 512;
        return __real_xTaskCreatePinnedToCore(pvTaskCode, pcName, newStackSize,
                                              pvParameters, uxPriority, pvCreatedTask, xCoreID);
    }
    return __real_xTaskCreatePinnedToCore(pvTaskCode, pcName, usStackDepth,
                                          pvParameters, uxPriority, pvCreatedTask, xCoreID);
}

#ifdef __cplusplus
}
#endif