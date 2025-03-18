#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class TaskMonitor {
public:
    // 初始化监控（必须在setup()中调用）
    static void begin(uint32_t sampleWindowMs = 1000);

    // 更新统计数据（需在loop()中定期调用）
    static void update();

    // 打印所有任务的CPU利用率
    static void printStats();

    // 获取指定任务的CPU利用率（通过任务名）
    static float getTaskUsage(const char* taskName);

private:
    struct TaskInfo {
        char name[configMAX_TASK_NAME_LEN];
        uint32_t runtime;
        float usage;
        bool updated;
    };

    static uint32_t _lastUpdate;
    static uint32_t _sampleWindowMs;
    static uint32_t _totalRuntime;
    static TaskInfo _tasks[configMAX_TASK_NAME_LEN];
    static uint8_t _taskCount;

    static void _updateStats();
    static void _initHighResTimer();
};