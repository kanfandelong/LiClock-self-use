#include "A_Config.h"
#include "rom/ets_sys.h"          // ets_install_putc2
#include <LittleFS.h>
#include <atomic>
#include <esp_heap_caps.h>        // heap_caps_malloc

// ===================== 配置 =====================
#define LOG_RING_SIZE          (64 * 1024)    // 64 KB 环形缓冲区（放在 PSRAM）
#define LOG_WRITER_PERIOD_MS   3000            // 写文件任务的轮询周期
#define LOG_FILE_PATH          "/System/log.txt"

// ===================== 全局变量 =====================
static char*            ring_buf   = nullptr;
static std::atomic<uint32_t> write_idx{0};
static std::atomic<uint32_t> read_idx {0};

static File             log_file;
static TaskHandle_t     writer_task = nullptr;
static bool             sys_log_enabled = false;

// ===================== putc2 回调 =====================
static void IRAM_ATTR log_putc2(char c) {
    if (!ring_buf) return;

    uint32_t w = write_idx.load(std::memory_order_relaxed);
    uint32_t next = (w + 1) & (LOG_RING_SIZE - 1); // 环形缓冲区索引回绕

    // 缓冲区满则丢弃，绝不阻塞
    if (next == read_idx.load(std::memory_order_acquire)) {
        return;
    }

    ring_buf[w] = c;
    write_idx.store(next, std::memory_order_release);
}

void reinstall_putc2() {
    if (sys_log_enabled) {
        ets_install_putc2(log_putc2);
    }
}

void open_log_file() {
    if (hal.pref.getString("log_path", "") != "") {
        log_file = hal.open(hal.pref.getString("log_path"), "a");
    } else {
        log_file = LittleFS.open(LOG_FILE_PATH, "a");
    }
    if (log_file) {
        log_file.setBufferSize(4096);
    } else {
        log_e("无法打开日志文件：%s", LOG_FILE_PATH);
    }
}

// ===================== 文件写入任务 =====================
static void log_writer_task_func(void* param) {
    while (sys_log_enabled) {
        uint32_t r = read_idx.load(std::memory_order_relaxed);
        uint32_t w = write_idx.load(std::memory_order_acquire);

        // 计算当前缓冲区中待写入的字节数（考虑回绕）
        uint32_t available = (w >= r) ? (w - r) : (LOG_RING_SIZE - r + w);

        if (available < 56 * 1024) {
            // 数据量不足，不写入，继续休眠
            vTaskDelay(pdMS_TO_TICKS(LOG_WRITER_PERIOD_MS));
            continue;
        }

        while (r != w) {
            // 计算从 r 到 w 的连续字节数（可能回绕）
            uint32_t len = (w > r) ? (w - r) : (LOG_RING_SIZE - r);
            if (log_file) {
                size_t written = log_file.write((const uint8_t*)&ring_buf[r], len);
                if (written != len) {
                    log_w("文件写入不完整，期望 %u 字节，实际 %u 字节", len, written);
                }
                // fast_boot 为 false 时每次写入后立刻 flush
                if (!hal.pref.getBool("fast_boot", false)) {
                    log_file.flush();
                }
            } else {
                // 文件意外关闭，尝试重新打开
                log_w("日志文件句柄丢失，尝试重新打开");
                open_log_file();
            }

            r = (r + len) % LOG_RING_SIZE;
            read_idx.store(r, std::memory_order_release);
            // 重新检查写入指针，防止在处理过程中又来了新数据
            w = write_idx.load(std::memory_order_acquire);
        }

        vTaskDelay(pdMS_TO_TICKS(LOG_WRITER_PERIOD_MS));
    }

    // 退出循环后（deinit 触发），确保剩余数据全部写入
    log_i("日志文件写入任务即将结束，写入剩余数据");
    uint32_t r = read_idx.load(std::memory_order_relaxed);
    uint32_t w = write_idx.load(std::memory_order_acquire);
    while (r != w) {
        uint32_t len = (w > r) ? (w - r) : (LOG_RING_SIZE - r);
        if (log_file) {
            log_file.write((const uint8_t*)&ring_buf[r], len);
        }
        r = (r + len) % LOG_RING_SIZE;
    }
    if (log_file) {
        log_file.flush();
        log_file.close();
    }
    vTaskDelete(nullptr);
}

// ===================== 对外接口 =====================
bool log_system_init() {
    // 不需要文件日志时直接返回成功
    if (!hal.pref.getBool("sys_log", true)) {
        log_i("文件日志已禁用");
        return true;
    }

    // 防止重复初始化
    if (sys_log_enabled || ring_buf) {
        log_w("日志系统已经初始化");
        return true;
    }

    log_i("开始初始化日志系统...");

    // 1. 分配环形缓冲区（优先 PSRAM）
    ring_buf = (char*)heap_caps_malloc(LOG_RING_SIZE, MALLOC_CAP_SPIRAM);
    if (!ring_buf) {
        ring_buf = (char*)malloc(LOG_RING_SIZE);
        if (!ring_buf) {
            log_e("致命错误：无法分配 %d 字节环形缓冲区", LOG_RING_SIZE);
            return false;
        }
        log_w("PSRAM 分配失败，已退至内部 DRAM");
    }

    // 2. 打开日志文件
    open_log_file();
    if (!log_file) {
        log_e("无法打开日志文件：%s", LOG_FILE_PATH);
        free(ring_buf);
        ring_buf = nullptr;
        return false;
    }

    // 3. 创建工作任务
    if (xTaskCreate(log_writer_task_func, "log_wr", 4096, nullptr, 2, &writer_task) != pdPASS) {
        log_e("创建写入任务失败");
        log_file.close();
        free(ring_buf);
        ring_buf = nullptr;
        return false;
    }

    // 4. 安装 putc2 钩子
    ets_install_putc2(log_putc2);
    sys_log_enabled = true;
    log_i("putc2 钩子已安装");
    return true;
}

void log_system_deinit() {
    // 1. 立即停止 putc2 注入
    ets_install_putc2(nullptr);
    sys_log_enabled = false;   // 通知任务退出循环
    log_i("putc2 钩子已卸载");

    // 2. 如果任务存在，等待它自行结束（它会刷盘然后删除自己）
    if (writer_task) {
        log_i("等待写入任务结束...");
        uint32_t wait = 0;
        while (eTaskGetState(writer_task) != eDeleted && wait < 5000) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait += 10;
        }
        if (eTaskGetState(writer_task) != eDeleted) {
            log_w("写入任务未在超时内结束，强制删除");
            vTaskDelete(writer_task);
        }
        writer_task = nullptr;
    }

    // 3. 如果任务没有来得及关闭文件，这里做兜底关闭
    if (log_file) {
        log_file.close();
        log_i("日志文件已关闭");
    }

    // 4. 释放环形缓冲区
    if (ring_buf) {
        free(ring_buf);
        ring_buf = nullptr;
        log_i("环形缓冲区已释放");
    }

}