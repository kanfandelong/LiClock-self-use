#include "CpuUsage.h"
#include "esp_timer.h"

// 静态成员初始化
uint32_t TaskMonitor::_lastUpdate = 0;
uint32_t TaskMonitor::_sampleWindowMs = 1000;
uint32_t TaskMonitor::_totalRuntime = 0;
TaskMonitor::TaskInfo TaskMonitor::_tasks[configMAX_TASK_NAME_LEN];
uint8_t TaskMonitor::_taskCount = 0;

// 高精度定时器初始化
void TaskMonitor::_initHighResTimer() {
    #ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    // 配置运行时间统计时钟（使用CPU周期）
    extern void esp_configure_run_time_counter(void);
    esp_configure_run_time_counter();
    #endif
}

void TaskMonitor::begin(uint32_t sampleWindowMs) {
    _sampleWindowMs = sampleWindowMs;
    _initHighResTimer();
}

void TaskMonitor::update() {
    if (millis() - _lastUpdate >= _sampleWindowMs) {
        _updateStats();
        _lastUpdate = millis();
    }
}

void TaskMonitor::_updateStats() {
    // 获取系统所有任务状态
    TaskStatus_t *taskStatusArray;
    const UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    taskStatusArray = (TaskStatus_t*)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (taskStatusArray != NULL) {
        // 获取详细任务数据
        uint32_t totalRuntime;
        UBaseType_t filled = uxTaskGetSystemState(
            taskStatusArray,
            uxArraySize,
            &totalRuntime
        );

        // 更新总运行时间
        _totalRuntime = totalRuntime;

        // 合并统计信息
        for (UBaseType_t i = 0; i < filled; i++) {
            bool found = false;
            for (uint8_t j = 0; j < _taskCount; j++) {
                if (strcmp(taskStatusArray[i].pcTaskName, _tasks[j].name) == 0) {
                    // 计算时间差（单位：CPU周期）
                    uint32_t delta = taskStatusArray[i].ulRunTimeCounter - _tasks[j].runtime;
                    _tasks[j].usage = (delta * 100.0f) / (totalRuntime * portTICK_PERIOD_MS * 1000);
                    _tasks[j].runtime = taskStatusArray[i].ulRunTimeCounter;
                    _tasks[j].updated = true;
                    found = true;
                    break;
                }
            }

            // 发现新任务
            if (!found && _taskCount < configMAX_TASK_NAME_LEN) {
                strncpy(_tasks[_taskCount].name, taskStatusArray[i].pcTaskName, configMAX_TASK_NAME_LEN);
                _tasks[_taskCount].runtime = taskStatusArray[i].ulRunTimeCounter;
                _tasks[_taskCount].usage = 0;
                _tasks[_taskCount].updated = true;
                _taskCount++;
            }
        }

        // 清理未更新的旧任务
        for (uint8_t j = 0; j < _taskCount; j++) {
            if (!_tasks[j].updated) {
                // 移除已删除的任务
                memmove(&_tasks[j], &_tasks[j+1], (_taskCount - j - 1) * sizeof(TaskInfo));
                _taskCount--;
                j--;
            } else {
                _tasks[j].updated = false;
            }
        }

        vPortFree(taskStatusArray);
    }
}

float TaskMonitor::getTaskUsage(const char* taskName) {
    for (uint8_t j = 0; j < _taskCount; j++) {
        if (strcmp(taskName, _tasks[j].name) == 0) {
            return _tasks[j].usage;
        }
    }
    return -1.0f;
}

void TaskMonitor::printStats() {
    Serial.println("\n===== Task CPU Usage =====");
    for (uint8_t j = 0; j < _taskCount; j++) {
        Serial.printf("%-16s: %6.2f%%\n", _tasks[j].name, _tasks[j].usage);
    }
    Serial.printf("Total Runtime: %.2f ms\n", _totalRuntime * portTICK_PERIOD_MS);
    Serial.println("========================");
}