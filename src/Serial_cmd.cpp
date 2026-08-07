#include "Serial_cmd.h"
#include <nvs_flash.h>
#include "my_chip-debug-report.h"
#include "ulp_riscv.h"
#include <cstring>

// Forward declaration for custom hint lookup
static const char *find_cmd_hint(const char *cmdName);
static void on_wifi_task(void *pvParameters);
static TaskHandle_t console_task_handle = NULL;
extern QueueHandle_t multi_thread_queue;
bool stop_fileserver = false;
extern bool serverRunning;
CMD cmd;

// ============ 新增头文件 ============
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include "rom/ets_sys.h" // ets_install_putc2 (如果还没包含)
#include <atomic>

// ===================== WebSocket 控制台配置 =====================
#define WS_LOG_RING_SIZE (32 * 1024) // 32 KB 环形缓冲区，必须是 2 的 N 次幂
#define WS_CMD_QUEUE_SIZE 4
#define WS_CMD_MAX_LEN 512 // 单条命令最大长度

// ===================== WebSocket 控制台全局变量 =====================
static AsyncWebServer *wsServer = nullptr;
static AsyncWebSocket *wsSocket = nullptr;
static char *wsRingBuf = nullptr;
static std::atomic<uint32_t> wsWriteIdx{0};
static std::atomic<uint32_t> wsReadIdx{0};
static TaskHandle_t wsSenderTaskHandle = nullptr;
static TaskHandle_t wsCmdTaskHandle = nullptr;
static QueueHandle_t wsCmdQueue = nullptr;
static bool wsConsoleActive = false;

// 任务函数
static void console_task(void *pvParameters)
{
    size_t bufIndex = 0;
    memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
    // 使用宏定义统一日志输出颜色
    PRINT_INFO("LiClock Serial Tool");
    PRINT_INFO("Type 'help' for available commands");
    while (1)
    {
        while (uart->available() > 0)
        {
            char c = uart->read();

            if (c == '\n' || c == '\r' || c == '*')
            {
                if (bufIndex == 0)
                    continue; // 忽略空行
                cmd.cmdBuffer[bufIndex] = '\0';
                // 执行命令
                int ret;
                esp_err_t err = esp_console_run(cmd.cmdBuffer, &ret);
                if (err == ESP_ERR_NOT_FOUND)
                {
                    PRINT_ERROR("Command not found");
                }
                else if (err == ESP_OK)
                {
                    if (ret == 0)
                    {
                        log_printf("\x04\x04");
                    }
                    if (ret == 1)
                    {
                        PRINT_ERROR("参数错误");
                        // Use custom hint lookup instead of esp_console_get_hint
                        const char *hint_str = find_cmd_hint(cmd.cmdBuffer);
                        if (hint_str)
                        {
                            // Show the hint as normal info (no error prefix)
                            ERROR_COLOR;
                            log_printf("%s\n", hint_str);
                            RESET_COLOR;
                        }
                    }
                    if (ret == 2)
                    {
                        PRINT_ERROR("命令 '%s' 执行失败", cmd.cmdBuffer);
                    }
                }
                else if (err != ESP_OK)
                {
                    PRINT_ERROR("%s", esp_err_to_name(err));
                }
                bufIndex = 0;
                memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
            }
            else
            {
                if (bufIndex < COMMAND_BUFFER_SIZE - 1)
                {
                    cmd.cmdBuffer[bufIndex++] = c;
                }
                else
                {
                    // 缓冲区溢出处理
                    bufIndex = 0;
                    memset(cmd.cmdBuffer, 0, sizeof(cmd.cmdBuffer));
                    RED;
                    PRINT_ERROR("Error: Buffer overflow");
                }
            }
        }
        // 没有数据时挂起任务，等待中断唤醒
        vTaskSuspend(NULL);
    }
}

void fileserver_task(void *)
{
    DNSServer dnsServer;
    bool wifi = hal.autoConnectWiFi(false);
    String passwd = String((esp_random() % 1000000000L) + 10000000L); // 生成随机密码
    String str = "WIFI:T:WPA2;S:WeatherClock;P:" + passwd + ";;", str1;
    if (wifi)
    {
        str1 = WiFi.localIP().toString();
    }
    else
    {
        WiFi.softAP("WeatherClock", passwd.c_str());
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
        str1 = "192.168.4.1";
        log_i("WiFi pass: %s", passwd.c_str());
    }
    log_i("WiFi IP: %s", str1.c_str());
    beginFileServer();
    while (1)
    {
        if (stop_fileserver)
        {
            if (!wifi)
                dnsServer.stop();
            server.end();
            serverRunning = false;
            hal.can_sleep = true;
            hal.can_light_sleep = true;
            vTaskDelete(NULL);
        }
        else
            vTaskDelay(100);
    }
}

void IRAM_ATTR serialRxCallback()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (console_task_handle != NULL)
    {
        xHigherPriorityTaskWoken = xTaskResumeFromISR(console_task_handle);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void CMD::begin()
{
    cmdBuffer = (char *)ps_malloc(COMMAND_BUFFER_SIZE);
    // 初始化 esp_console
    esp_console_config_t console_config = {
        .max_cmdline_length = 8192,
        .max_cmdline_args = 32,
        .heap_alloc_caps = MALLOC_CAP_SPIRAM,
        .hint_color = 39,
        .hint_bold = 0};
    esp_console_init(&console_config);

    // 注册内置 help 命令
    esp_console_register_help_command();

    // 注册自定义命令
    register_commands();

    // 创建控制台任务
    xTaskCreatePinnedToCore(console_task, "console_task", 8192, NULL, 5, &console_task_handle, 1);
#ifndef USE_CDC
    uart->onReceive(serialRxCallback, true);
#endif
    is_run = true;
}

void CMD::SetCallback()
{
#ifndef USE_CDC
    uart->onReceive(serialRxCallback, true);
#endif
}

void CMD::stop()
{
    if (console_task_handle != NULL)
        vTaskSuspend(console_task_handle);
}

void CMD::run()
{
    if (console_task_handle != NULL)
        xTaskResumeFromISR(console_task_handle);
}

void CMD::end()
{
    if (console_task_handle != NULL)
    {
        vTaskSuspend(console_task_handle);
        vTaskDelete(console_task_handle);
        console_task_handle = NULL;
    }
    if (is_run)
    {
        esp_console_deinit();
    }
}

// 此处定义一下命令处理函数的返回值的对应原因
// 0: 命令成功
// 1: 命令参数错误
// 2: 命令执行失败

// ===================== putc2 钩子（ISR 安全） =====================
static void IRAM_ATTR ws_putc2(char c)
{
    if (!wsRingBuf || !wsConsoleActive)
        return;

    uint32_t w = wsWriteIdx.load(std::memory_order_relaxed);
    uint32_t next = (w + 1) & (WS_LOG_RING_SIZE - 1);

    if (next == wsReadIdx.load(std::memory_order_acquire))
    {
        return; // 缓冲区满，丢弃
    }
    wsRingBuf[w] = c;
    wsWriteIdx.store(next, std::memory_order_release);

    // 每写入一个换行就通知发送任务（ISR 安全）
    if (c == '\n' && wsSenderTaskHandle)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(wsSenderTaskHandle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}

void reinstall_ws_putc2()
{
    if (wsConsoleActive)
        ets_install_putc2(ws_putc2);
}

// ===================== 日志发送任务 =====================
static void ws_sender_task(void *param)
{
    String line;
    TickType_t lastSendTick = xTaskGetTickCount(); // 上一次发送时间戳（用于残留行超时）

    while (wsConsoleActive)
    {
        // 等待通知（换行到来）或 200ms 超时
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));

        // 读取环形缓冲区中所有可读字符
        uint32_t r = wsReadIdx.load(std::memory_order_relaxed);
        uint32_t w = wsWriteIdx.load(std::memory_order_acquire);
        while (r != w)
        {
            char c = wsRingBuf[r];
            r = (r + 1) & (WS_LOG_RING_SIZE - 1);

            if (c == '\n')
            {
                // 完整行：发送当前累积的行
                if (wsSocket && wsSocket->count() > 0)
                {
                    wsSocket->textAll(line);
                }
                line = "";
                lastSendTick = xTaskGetTickCount(); // 更新时间戳
            }
            else
            {
                line += c;
            }
        }
        wsReadIdx.store(r, std::memory_order_release);

        // 若一行长时间没有换行符，超时后强制发送残留部分
        if (line.length() > 0)
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - lastSendTick) >= pdMS_TO_TICKS(200))
            {
                if (wsSocket && wsSocket->count() > 0)
                {
                    wsSocket->textAll(line);
                }
                line = "";
                lastSendTick = now;
            }
        }
    }

    // 任务退出前，发送最后可能残留的行
    if (line.length() > 0 && wsSocket && wsSocket->count() > 0)
    {
        wsSocket->textAll(line);
    }
    vTaskDelete(nullptr);
}

// ===================== 命令处理任务 =====================
static void ws_cmd_task(void *param)
{
    char cmdBuf[WS_CMD_MAX_LEN];
    while (wsConsoleActive)
    {
        if (xQueueReceive(wsCmdQueue, cmdBuf, portMAX_DELAY) == pdTRUE)
        {
            int ret;
            esp_err_t err = esp_console_run(cmdBuf, &ret);
            if (err == ESP_ERR_NOT_FOUND)
            {
                log_e("Command not found: %s", cmdBuf);
            }
            else if (err == ESP_OK)
            {
                if (ret == 1)
                {
                    log_e("Parameter error for: %s", cmdBuf);
                }
                else if (ret == 2)
                {
                    log_e("Command execution failed: %s", cmdBuf);
                }
            }
            else
            {
                log_e("Error executing '%s': %s", cmdBuf, esp_err_to_name(err));
            }
        }
    }
    vTaskDelete(nullptr);
}

// ===================== WebSocket 事件处理 =====================
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        log_i("WS console client connected: %u", client->id());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        log_i("WS console client disconnected: %u", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_TEXT)
        {
            // 构造命令字符串
            String msg;
            msg.reserve(len + 1);
            memcpy(msg.begin(), (char *)data, len);
            msg.begin()[len] = '\0';

            if (wsCmdQueue && msg.length() < WS_CMD_MAX_LEN)
            {
                char cmd[WS_CMD_MAX_LEN];
                strncpy(cmd, msg.c_str(), sizeof(cmd));
                cmd[sizeof(cmd) - 1] = '\0';
                xQueueSend(wsCmdQueue, cmd, 0);
            }
        }
    }
}

// ===================== wsconsole 命令实现 =====================
static int cmd_wsconsole(int argc, char **argv)
{
    if (argc < 2)
    {
        PRINT_INFO("WebSocket console is %s", wsConsoleActive ? "RUNNING" : "STOPPED");
        return 0;
    }

    const char *action = argv[1];

    if (strcmp(action, "start") == 0)
    {
        if (wsConsoleActive)
        {
            PRINT_ERROR("WebSocket console already running");
            return 2;
        }
        if (!WiFi.isConnected())
        {
            xTaskCreatePinnedToCore(on_wifi_task, "on_wifi_task", 8192, NULL, 1, NULL, 0);

            unsigned long start = millis();
            const unsigned long timeout = 30000; // 30 秒超时
            while (!WiFi.isConnected() && (millis() - start) < timeout)
            {
                delay(100);
                log_printf("\r|");
                delay(100);
                log_printf("\r/");
                delay(100);
                log_printf("\r-");
                delay(100);
                log_printf("\r\\");
            }

            if (!WiFi.isConnected())
            {
                PRINT_ERROR("WiFi connection timeout!");
                return 2;
            }
            log_printf("\r\n"); // 清除旋转动画
            PRINT_INFO("WiFi connected.");
        }
        // 1. 挂起串口控制台（避免并发执行 esp_console_run）
        // cmd.stop();

        // 2. 分配环形缓冲区
        wsRingBuf = (char *)heap_caps_malloc(WS_LOG_RING_SIZE, MALLOC_CAP_SPIRAM);
        if (!wsRingBuf)
        {
            wsRingBuf = (char *)malloc(WS_LOG_RING_SIZE);
            if (!wsRingBuf)
            {
                PRINT_ERROR("Failed to allocate WS ring buffer");
                cmd.run(); // 恢复串口
                return 2;
            }
            PRINT_WARNING("PSRAM allocation failed, using internal DRAM for WS ring");
        }
        wsWriteIdx = 0;
        wsReadIdx = 0;

        // 3. 启动 WebSocket 服务器
        wsServer = new AsyncWebServer(81);
        wsSocket = new AsyncWebSocket("/ws");
        wsSocket->onEvent(onWsEvent);
        wsServer->addHandler(wsSocket);
        wsServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                     {
            const char *buildTime = __DATE__ " " __TIME__ " GMT";
                  if (request->header("If-Modified-Since").equals(buildTime))
                  {
                      request->send(304);
                  }
                  else
                  {
                    if (LittleFS.exists("/System/console.html.gz")) {
                        File file = LittleFS.open("/System/console.html.gz", "r");
                        time_t lastWrite = file.getLastWrite(); // 获取UTC时间戳
                        file.close();
                  
                        struct tm tm;
                        gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构
                        
                        char timeStr[64];
                        strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                        buildTime = timeStr;
                        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/System/console.html.gz", "text/html", false);
                        response->addHeader("Content-Encoding", "gzip");
                        response->addHeader("Last-Modified", buildTime);
                        request->send(response);
                    }
                    else
                        request->send(404, "text/plain", "file not found");
                  } });
        wsServer->begin();
        PRINT_INFO("WebSocket server started on port 81, path /ws");

        // 4. 创建命令队列
        wsCmdQueue = xQueueCreate(WS_CMD_QUEUE_SIZE, WS_CMD_MAX_LEN);
        if (!wsCmdQueue)
        {
            PRINT_ERROR("Failed to create command queue");
            delete wsSocket;
            wsSocket = nullptr;
            delete wsServer;
            wsServer = nullptr;
            free(wsRingBuf);
            wsRingBuf = nullptr;
            return 2;
        }

        // 5. 安装 putc2 钩子，并标记为活跃
        ets_install_putc2(ws_putc2);
        wsConsoleActive = true;

        // 6. 创建日志发送和命令处理任务
        xTaskCreatePinnedToCore(ws_sender_task, "ws_sender", 4096, nullptr, 2, &wsSenderTaskHandle, 1);
        xTaskCreatePinnedToCore(ws_cmd_task, "ws_cmd", 4096, nullptr, 3, &wsCmdTaskHandle, 1);

        PRINT_SUCCESS("WebSocket console started. Connect to ws://<IP>:81/ws");
        return 0;
    }
    else if (strcmp(action, "stop") == 0)
    {
        if (!wsConsoleActive)
        {
            PRINT_ERROR("WebSocket console is not running");
            return 2;
        }

        // 1. 卸载 WS 钩子并通知任务退出
        ets_install_putc2(nullptr);
        wsConsoleActive = false;

        // 2. 等待任务结束
        if (wsSenderTaskHandle)
        {
            while (eTaskGetState(wsSenderTaskHandle) != eDeleted)
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            wsSenderTaskHandle = nullptr;
        }
        if (wsCmdTaskHandle)
        {
            while (eTaskGetState(wsCmdTaskHandle) != eDeleted)
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            wsCmdTaskHandle = nullptr;
        }

        // 3. 释放资源
        if (wsCmdQueue)
        {
            vQueueDelete(wsCmdQueue);
            wsCmdQueue = nullptr;
        }
        if (wsSocket)
        {
            wsSocket->closeAll();
            delete wsSocket;
            wsSocket = nullptr;
        }
        if (wsServer)
        {
            wsServer->end();
            delete wsServer;
            wsServer = nullptr;
        }
        free(wsRingBuf);
        wsRingBuf = nullptr;

        // 4. 恢复原有的 putc2 钩子（根据文件日志配置）
        reinstall_putc2();

        PRINT_SUCCESS("WebSocket console stopped, serial console restored");
        return 0;
    }
    else
    {
        return 1; // 参数错误，自动显示帮助
    }
}

static int cmd_cpufreq(int argc, char **argv)
{
    if (argc == 1)
    {
        SUCCESS_COLOR;
        PRINT_INFO("Current CPU frequency: %u MHz", ESP.getCpuFreqMHz());
        RESET_COLOR;
        return 0;
    }
    else if (argc == 2)
    {
        int freq = atoi(argv[1]);
        // 检查频率有效性
        bool valid = (freq == 240 || freq == 160 || freq == 80);
        if (!valid)
        {
            return 1;
        }
        hal.cheak_freq(freq, true);
        return 0;
    }
    else
    {
        // Usage hint provided via command registration
        return 1;
    }
}

static int cmd_longpress(int argc, char **argv)
{
    if (argc == 1)
    {
        PRINT_INFO("Current long press threshold: %d ms", hal.pref.getInt("lpt", 20) * 10);
        return 0;
    }
    else if (argc == 2)
    {
        int value = atoi(argv[1]);
        hal.pref.putInt("lpt", value);
        PRINT_INFO("Long press threshold updated to %d ms", value * 10);
        return 0;
    }
    else
    {
        // Usage hint provided via command registration
        return 1;
    }
}

static int cmd_cfgcpufreq(int argc, char **argv)
{
    if (argc == 2)
    {
        int freq = atoi(argv[1]);
        hal.pref.putInt("CpuFreq", freq);
        PRINT_INFO("CPU frequency configuration saved to %d MHz (will take effect after reboot)", freq);
        return 0;
    }
    else
    {
        // Usage hint provided via command registration
        return 1;
    }
}

static int cmd_erasenvs(int argc, char **argv)
{
    PRINT_WARNING("WARNING: This will erase all NVS data!");
    if (nvs_flash_erase() == ESP_OK)
    {
        PRINT_SUCCESS("NVS erased successfully");
        PRINT_INFO("Device will restart in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    else
    {
        // Execution failure
        PRINT_ERROR("Failed to erase NVS");
        return 2;
    }
    return 0;
}

static int cmd_lfsformat(int argc, char **argv)
{
    PRINT_WARNING("WARNING: This will format LittleFS and erase all data!");
    LittleFS.end();
    if (LittleFS.format())
    {
        if (LittleFS.begin())
        {
            PRINT_SUCCESS("LittleFS formatted successfully");
        }
        else
        {
            // Execution failure: could not remount after format
            PRINT_ERROR("Failed to remount LittleFS after format");
            return 2;
        }
    }
    else
    {
        // Execution failure: format failed
        PRINT_ERROR("Failed to format LittleFS");
        return 2;
    }
    return 0;
}

static int cmd_lfsinfo(int argc, char **argv)
{
    size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    PRINT_INFO("LITTLEFS FILESYSTEM INFORMATION:");
    PRINT_INFO("  Total space: %5d KB", total / 1024);
    PRINT_INFO("  Used space:  %5d KB (%.02f%%)", used / 1024, (float)used / (float)total * 100.0);
    PRINT_INFO("  Free space:  %5d KB", (total - used) / 1024);
    return 0;
}

static int cmd_heap(int argc, char **argv)
{
    printMemCapsInfo(INTERNAL);
    if (psramFound())
    {
        printMemCapsInfo(SPIRAM);
    }
    return 0;
}

// 缓冲区大小 (128KB)
#define DUMP_BUFFER_SIZE (128 * 1024)

// 全局缓冲区指针和写入位置
static char *s_dump_buffer = nullptr;
static size_t s_dump_write_idx = 0;
static size_t s_dump_buffer_size = 0;
static bool s_dump_overflow = false;

static void IRAM_ATTR custom_dump_putc(char c)
{
    if (!s_dump_buffer)
        return;

    if (s_dump_write_idx < s_dump_buffer_size - 1)
    {
        s_dump_buffer[s_dump_write_idx++] = c;
    }
    else
    {
        s_dump_overflow = true;
        // 缓冲区满，停止写入（或可环形覆盖，但一般丢弃）
    }
}

/**
 * @brief 对指定 caps 执行 heap_caps_dump，将结果保存到文件中
 * @param caps   内存能力掩码
 * @param file_path  输出文件路径（例如 "/littlefs/heapdump.txt" 或 "/sd/dump.txt"）
 * @return true 成功, false 失败
 */
bool heap_caps_dump_to_file(uint32_t caps, const char *file_path)
{
    // 1. 分配 PSRAM 缓冲区
    s_dump_buffer = (char *)heap_caps_malloc(DUMP_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_dump_buffer)
    {
        // 尝试降级到内部 DRAM
        s_dump_buffer = (char *)malloc(DUMP_BUFFER_SIZE);
        if (!s_dump_buffer)
        {
            log_e("Failed to allocate dump buffer");
            return false;
        }
        log_w("Using internal DRAM for dump buffer");
    }
    s_dump_buffer_size = DUMP_BUFFER_SIZE;
    s_dump_write_idx = 0;
    s_dump_overflow = false;
    s_dump_buffer[0] = '\0';

    // 2. 临时禁用看门狗（虽然理论上临界区时间变短，但为保险仍禁用任务看门狗）
    disableCore1WDT();
    // 注意：heap_caps_dump 内部有临界区，但我们已经大幅缩短了输出时间

    // 3. 劫持 putc1
    ets_install_putc1(custom_dump_putc);

    // 4. 执行 dump
    heap_caps_dump(caps);

    // 5. 恢复环境
    enableCore1WDT();

    // 重载所有钩子
    uart->setDebugOutput(true);
    reinstall_putc2();
    reinstall_ws_putc2();

    // 6. 将缓冲区内容写入文件
    bool write_success = false;
    FILE *f = fopen(file_path, "w");
    if (f)
    {
        size_t written = fwrite(s_dump_buffer, 1, s_dump_write_idx, f);
        if (written == s_dump_write_idx)
        {
            log_i("Heap dump written to %s (%u bytes)", file_path, s_dump_write_idx);
            write_success = true;
        }
        else
        {
            log_e("Write error: only %u of %u bytes written", written, s_dump_write_idx);
        }
        fclose(f);
    }
    else
    {
        log_e("Cannot open file: %s", file_path);
    }

    // 7. 处理溢出警告
    if (s_dump_overflow)
    {
        log_w("Dump buffer overflow! Increase DUMP_BUFFER_SIZE.");
    }

    // 8. 释放缓冲区
    free(s_dump_buffer);
    s_dump_buffer = nullptr;

    return write_success;
}

// ==================== heapdump 命令 ====================
// 用法：heapdump <type>
// 支持的 type:
//   internal, spiram, dma, exec, 8bit, default, retention, rt-cram
//   也可以直接输入十六进制数值，如 0x8 (MALLOC_CAP_DMA)
static int cmd_heapdump(int argc, char **argv)
{
    if (argc != 3)
    {
        return 1; // 显示帮助
    }

    // 参数1：caps 类型（字符串或数字）
    const char *type_str = argv[1];
    uint32_t caps = 0;

    // 字符串解析（同上文）
    if (strcmp(type_str, "internal") == 0)
        caps = MALLOC_CAP_INTERNAL;
    else if (strcmp(type_str, "spiram") == 0)
        caps = MALLOC_CAP_SPIRAM;
    else if (strcmp(type_str, "dma") == 0)
        caps = MALLOC_CAP_DMA;
    else if (strcmp(type_str, "exec") == 0)
        caps = MALLOC_CAP_EXEC;
    else if (strcmp(type_str, "8bit") == 0)
        caps = MALLOC_CAP_8BIT;
    else if (strcmp(type_str, "default") == 0)
        caps = MALLOC_CAP_DEFAULT;
    else if (strcmp(type_str, "retention") == 0)
        caps = MALLOC_CAP_RETENTION;
    else if (strcmp(type_str, "rtcram") == 0)
        caps = MALLOC_CAP_RTCRAM;
    else
    {
        char *endptr;
        unsigned long val = strtoul(type_str, &endptr, 0);
        if (*endptr != '\0')
        {
            PRINT_ERROR("Unknown heap type: %s", type_str);
            return 1;
        }
        caps = (uint32_t)val;
    }

    // 参数2：输出文件路径
    const char *out_file = argv[2];
    if (!out_file || !out_file[0])
    {
        PRINT_ERROR("Invalid file path");
        return 1;
    }

    PRINT_INFO("Dumping heap caps=0x%08X to %s", caps, out_file);
    bool ok = heap_caps_dump_to_file(caps, out_file);
    if (ok)
    {
        PRINT_SUCCESS("Dump completed");
    }
    else
    {
        PRINT_ERROR("Dump failed");
        return 2;
    }
    return 0;
}

static int cmd_chipinfo(int argc, char **argv)
{
    PRINT_INFO("CHIP INFORMATION:");
    PRINT_INFO("  Model:             %s", ESP.getChipModel());
    PRINT_INFO("  Revision:          %u", ESP.getChipRevision());
    PRINT_INFO("  Cores:             %u", ESP.getChipCores());
    uint64_t chipmacid = ESP.getEfuseMac();
    uint8_t *mac = (uint8_t *)&chipmacid;
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    PRINT_INFO("  MAC:               %s", macStr);
    uint64_t unique_id[2];
    esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, unique_id, 128);
    uint8_t *chip_uid[2];
    chip_uid[0] = (uint8_t *)&unique_id[0];
    chip_uid[1] = (uint8_t *)&unique_id[1];
    PRINT_INFO("  Unique ID:         %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               chip_uid[0][0], chip_uid[0][1], chip_uid[0][2], chip_uid[0][3],
               chip_uid[0][4], chip_uid[0][5], chip_uid[0][6], chip_uid[0][7],
               chip_uid[1][0], chip_uid[1][1], chip_uid[1][2], chip_uid[1][3],
               chip_uid[1][4], chip_uid[1][5], chip_uid[1][6], chip_uid[1][7]);
    PRINT_INFO("  Flash size:        %d MB", ESP.getFlashChipSize() / 1048576);
    uint32_t flash_id;
    uint64_t flash_unique_id;
    esp_flash_read_id(esp_flash_default_chip, &flash_id);
    esp_flash_read_unique_chip_id(esp_flash_default_chip, &flash_unique_id);
    PRINT_INFO("  Flash ID:          %04x", flash_id);
    uint8_t *fuid = (uint8_t *)&flash_unique_id;
    PRINT_INFO("  Unique ID (Flash): %02x%02x%02x%02x%02x%02x%02x%02x",
               fuid[0], fuid[1], fuid[2], fuid[3],
               fuid[4], fuid[5], fuid[6], fuid[7]);
    return 0;
}

static int cmd_rst(int argc, char **argv)
{
    PRINT_INFO("Restarting device...");
    // hal.pref.end(); // 避免panic时回退
    LittleFS.end();
    delay(1000);
    ESP.restart();
    return 0;
}

static int cmd_runtime(int argc, char **argv)
{
    long timeMillis = millis();
    long hours = timeMillis / 3600000;
    long remaining = timeMillis % 3600000;
    long minutes = remaining / 60000;
    remaining %= 60000;
    long seconds = remaining / 1000;
    long tenths = (remaining % 1000) / 100;
    PRINT_INFO("Device runtime: %4d:%02d:%02d.%d", hours, minutes, seconds, tenths);
    return 0;
}

static int cmd_batinfo(int argc, char **argv)
{
    hal.printBatteryInfo();
    return 0;
}

// ==================== taskstats 命令（CPU 占用率） ====================
static int cmd_taskstats(int argc, char **argv)
{
#if (configGENERATE_RUN_TIME_STATS == 1)
    const size_t bufferSize = 1024;
    char *buffer = (char *)malloc(bufferSize);
    if (buffer == NULL)
    {
        PRINT_ERROR("Failed to allocate memory for stats");
        return 2;
    }
    vTaskGetRunTimeStats(buffer);
    PRINT_INFO("Task Run Time Statistics:");
    log_printf(buffer); // 直接输出，其中已包含换行格式
    free(buffer);
    return 0;
#else
    PRINT_ERROR("configGENERATE_RUN_TIME_STATS is not enabled in FreeRTOSConfig.h");
    return 2;
#endif
}

static int cmd_tasklist(int argc, char **argv)
{
#if (configUSE_TRACE_FACILITY == 1 && configUSE_STATS_FORMATTING_FUNCTIONS == 1)
    const size_t bufferSize = 1024;
    char *buffer = (char *)malloc(bufferSize);
    if (buffer == NULL)
    {
        PRINT_ERROR("Failed to allocate memory for task list");
        return 2;
    }
    vTaskList(buffer);
    HEADER_COLOR;
    log_printf("Task List\n");
    RESET_COLOR;
    PRINT_INFO("Task name       State   Prio    Stack   Num     Core");
    log_printf("--------------- ------- ------- ------- ------- ----\n");
    log_printf(buffer);
    free(buffer);
    return 0;
#else
    PRINT_ERROR("vTaskList requires configUSE_TRACE_FACILITY and configUSE_STATS_FORMATTING_FUNCTIONS");
    return 2;
#endif
}

static int cmd_taskload(int argc, char **argv)
{
#if (configGENERATE_RUN_TIME_STATS == 1 && configUSE_TRACE_FACILITY == 1)
    // 可选的窗口长度参数 (秒)，默认 2
    int window_sec = 5;
    if (argc >= 2)
    {
        window_sec = atoi(argv[1]);
        if (window_sec <= 0 || window_sec > 60)
        {
            PRINT_ERROR("Invalid window. Use 1-60 seconds.");
            return 1;
        }
    }
    PRINT_INFO("Measuring CPU load over the last %d seconds...", window_sec);
    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    if (taskCount == 0)
    {
        PRINT_INFO("No tasks running.");
        return 0;
    }

    // 分配快照
    TaskStatus_t *snap1 = (TaskStatus_t *)malloc(taskCount * sizeof(TaskStatus_t));
    if (!snap1)
    {
        free(snap1);
        PRINT_ERROR("Memory allocation failed.");
        return 2;
    }

    uint32_t totalTime1, totalTime2;
    // 第一次快照
    UBaseType_t count1 = uxTaskGetSystemState(snap1, taskCount, &totalTime1);

    // 等待窗口时间 (阻塞自己，让其他任务运行)
    vTaskDelay(pdMS_TO_TICKS(window_sec * 1000));

    // 第二次快照（任务数可能变化，重新获取上限）
    UBaseType_t maxCount2 = uxTaskGetNumberOfTasks();
    TaskStatus_t *snap2 = (TaskStatus_t *)malloc(maxCount2 * sizeof(TaskStatus_t));
    if (!snap2)
    {
        free(snap1);
        PRINT_ERROR("Memory allocation failed.");
        return 2;
    }
    UBaseType_t count2 = uxTaskGetSystemState(snap2, maxCount2, &totalTime2);

    // 计算时间窗口（微秒）
    unsigned long elapsed = totalTime2 - totalTime1;
    if (elapsed == 0)
    {
        PRINT_WARNING("Elapsed time zero, stats unavailable.");
        free(snap1);
        free(snap2);
        return 0;
    }

    // 收集任务名最大长度（对齐）
    size_t maxNameLen = 4;
    for (UBaseType_t i = 0; i < count2; ++i)
    {
        size_t len = strlen(snap2[i].pcTaskName);
        if (len > maxNameLen)
            maxNameLen = len;
    }
    maxNameLen += 2;

    // 打印表头
    log_printf("\n--- CPU Load in last %d seconds ---\n", window_sec);
    log_printf("%-*s %5s %5s %5s %3s %10s %10s %9s\n",
               (int)maxNameLen, "Name",
               "Core", "State", "Prio", "WDT", "Stack", "Time(us)", "Load%");
    log_printf("%-*s ----- ----- ----- --- ---------- ---------- ---------\n",
               (int)maxNameLen, "----");

    const char state[6][2] = {"X", "R", "B", "S", "D", "?"};
    const int state_count = sizeof(state) / sizeof(state[0]);
    // 遍历第二次快照，在第一次快照中查找同名且同任务号的任务
    for (UBaseType_t j = 0; j < count2; ++j)
    {
        const TaskStatus_t &t2 = snap2[j];
        // 在快照1中查找匹配的任务句柄（保证是同一个任务）
        unsigned long runTime1 = 0;
        bool found = false;
        for (UBaseType_t i = 0; i < count1; ++i)
        {
            if (snap1[i].xHandle == t2.xHandle)
            {
                runTime1 = snap1[i].ulRunTimeCounter;
                found = true;
                break;
            }
        }
        if (!found)
        {
            // 任务在窗口期间创建，运行时间从0开始
            runTime1 = 0;
        }

        unsigned long delta = t2.ulRunTimeCounter - runTime1;
        float percent = ((float)delta * 100.0f) / (float)elapsed;

        const char *classColor;

        if (strncmp(t2.pcTaskName, "IDLE", 4) == 0)
            classColor = "\033[36m"; // 青色
        else if (percent >= 90.0f)
            classColor = "\033[91m"; // 亮红色
        else if (percent >= 70.0f)
            classColor = "\033[31m"; // 红色
        else if (percent >= 30.0f)
            classColor = "\033[33m"; // 黄色
        else if (percent >= 5.0f)
            classColor = "\033[32m"; // 绿色
        else if (percent >= 1.0f)
            classColor = "\033[37m"; // 白色
        else
            classColor = "\033[90m"; // 灰色（<1%）

        const char *state_str = (t2.eCurrentState < state_count) ? state[t2.eCurrentState] : "?";

        esp_err_t wdt_ret = esp_task_wdt_status(t2.xHandle);
        const char *wdt_str = "  -"; // 默认：未初始化或无效
        if (wdt_ret == ESP_OK)
        {
            wdt_str = "  √"; // 已订阅
        }
        else if (wdt_ret == ESP_ERR_NOT_FOUND)
        {
            wdt_str = "  ✗"; // 未订阅
        }

        int core = (t2.xCoreID == tskNO_AFFINITY) ? -1 : (int)t2.xCoreID;

        log_printf("%s%-*s %5d %5s %2u/%2u %s %10lu %10lu %8.02f%%\033[0m\n",
                   classColor,
                   (int)maxNameLen, t2.pcTaskName,
                   core,
                   state_str,
                   t2.uxBasePriority,
                   t2.uxCurrentPriority,
                   wdt_str,
                   (unsigned long)t2.usStackHighWaterMark,
                   delta,
                   percent);
    }

    free(snap1);
    free(snap2);
    return 0;
#else
    PRINT_ERROR("Requires configGENERATE_RUN_TIME_STATS and configUSE_TRACE_FACILITY");
    return 2;
#endif
}

static int cmd_taskctrl(int argc, char **argv)
{
#if (INCLUDE_vTaskSuspend == 1 && configUSE_TRACE_FACILITY == 1)
    if (argc < 3)
    {
        return 1;
    }

    const char *action = argv[1];
    const char *task_name = argv[2];

    // 1. 保护系统关键任务（禁止操作 IDLE）
    if (strcmp(task_name, "IDLE") == 0)
    {
        PRINT_ERROR("Cannot control IDLE task (system critical).");
        return 2;
    }

    // 2. 通过任务名获取句柄
    TaskHandle_t handle = xTaskGetHandle(task_name);
    if (handle == NULL)
    {
        PRINT_ERROR("Task '%s' not found.", task_name);
        return 1;
    }

    // 3. 执行动作
    if (strcmp(action, "down") == 0)
    {
        // 防止挂起当前任务（会导致命令卡死且无法恢复）
#if (INCLUDE_xTaskGetCurrentTaskHandle == 1)
        if (handle == xTaskGetCurrentTaskHandle())
        {
            PRINT_ERROR("Cannot suspend the current command task (would hang forever).");
            return 2;
        }
#endif
        vTaskSuspend(handle);
        PRINT_INFO("Task '%s' suspended.", task_name);
    }
    else if (strcmp(action, "up") == 0)
    {
        vTaskResume(handle);
        PRINT_INFO("Task '%s' resumed.", task_name);
    }
    else if (strcmp(action, "del") == 0)
    {
        vTaskDelete(handle);
        PRINT_INFO("Task '%s' delete.", task_name);
    }
    else
    {
        return 1;
    }

    return 0;
#else
    PRINT_ERROR("Requires INCLUDE_vTaskSuspend and configUSE_TRACE_FACILITY to be 1.");
    return 2;
#endif
}

static int cmd_bootapp_clock(int argc, char **argv)
{
    hal.pref.putString(SETTINGS_PARAM_HOME_APP, "clock");
    PRINT_INFO("Default boot application set to 'clock'");
    return 0;
}

static int cmd_lightsleep(int argc, char **argv)
{
    int timeout = 0;
    if (argc == 2)
    {
        timeout = atoi(argv[1]);
        PRINT_INFO("Entering light sleep with timeout: %d ms", timeout);
    }
    else
    {
        PRINT_INFO("Entering light sleep (press button to wake)");
    }
    hal.wait_input(timeout);
    return 0;
}

static int cmd_fserverbegin(int argc, char **argv)
{
    stop_fileserver = false;
    hal.can_sleep = false;
    hal.can_light_sleep = false;
    xTaskCreatePinnedToCore(fileserver_task, "fileserver", 8192, NULL, 1, NULL, 0);
    PRINT_INFO("File server started");
    return 0;
}

static int cmd_fserverend(int argc, char **argv)
{
    stop_fileserver = true;
    delay(200);
    PRINT_INFO("File server stopped");
    return 0;
}

static int cmd_partitioninfo(int argc, char **argv)
{
    PRINT_INFO("PARTITION INFORMATION:");
    printPartitionsInfo();
    return 0;
}

// 辅助函数：将字符串解析为布尔值
static bool parseBool(const char *str, bool &value)
{
    if (strcasecmp(str, "true") == 0 || strcasecmp(str, "1") == 0 ||
        strcasecmp(str, "yes") == 0 || strcasecmp(str, "on") == 0)
    {
        value = true;
        return true;
    }
    if (strcasecmp(str, "false") == 0 || strcasecmp(str, "0") == 0 ||
        strcasecmp(str, "no") == 0 || strcasecmp(str, "off") == 0)
    {
        value = false;
        return true;
    }
    return false;
}

// 辅助函数：将字符串解析为整数，并检查是否完全消耗
template <typename T>
static bool parseInteger(const char *str, T &value, int base = 0)
{
    char *end;
    long long val = strtoll(str, &end, base);
    if (*end != '\0' || end == str)
        return false; // 存在非法字符或没有数字
    value = static_cast<T>(val);
    // 可选：检查范围是否溢出（可根据需要添加）
    return true;
}

// 辅助函数：将字符串解析为无符号整数
template <typename T>
static bool parseUnsigned(const char *str, T &value, int base = 0)
{
    char *end;
    unsigned long long val = strtoull(str, &end, base);
    if (*end != '\0' || end == str)
        return false;
    value = static_cast<T>(val);
    return true;
}

// 辅助函数：将字符串解析为浮点数
static bool parseFloat(const char *str, float &value)
{
    char *end;
    double val = strtod(str, &end);
    if (*end != '\0' || end == str)
        return false;
    value = static_cast<float>(val);
    return true;
}

static bool parseDouble(const char *str, double &value)
{
    char *end;
    value = strtod(str, &end);
    return (*end == '\0' && end != str);
}

static size_t parseHexToBytes(const char *hexStr, uint8_t *out, size_t maxLen)
{
    if (!hexStr || !out || maxLen == 0)
        return 0;

    // 跳过可选的 "0x" 或 "0X" 前缀
    if (hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X'))
        hexStr += 2;

    size_t len = strlen(hexStr);
    // 十六进制字符串长度必须是偶数
    if (len % 2 != 0)
        return 0;

    size_t byteCount = len / 2;
    if (byteCount > maxLen)
        return 0; // 缓冲区不够

    for (size_t i = 0; i < byteCount; ++i)
    {
        char high = hexStr[2 * i];
        char low = hexStr[2 * i + 1];

        // 检查是否为合法的十六进制字符
        if (!isxdigit((unsigned char)high) || !isxdigit((unsigned char)low))
            return 0;

        unsigned char h = (unsigned char)(high >= 'a' ? high - 'a' + 10 : high >= 'A' ? high - 'A' + 10
                                                                                      : high - '0');
        unsigned char l = (unsigned char)(low >= 'a' ? low - 'a' + 10 : low >= 'A' ? low - 'A' + 10
                                                                                   : low - '0');
        out[i] = (h << 4) | l;
    }
    return byteCount;
}

// ==================== putnvs 命令 ====================
static int cmd_putnvs(int argc, char **argv)
{
    // Expected usage:
    //   putnvs <key> <value> [type]   (type optional)
    if (argc < 3 || argc > 4)
    {
        return 1; // 参数个数错误
    }

    const char *key = argv[1];
    const char *valueStr = argv[2];
    const char *type = (argc == 4) ? argv[3] : nullptr;
    bool ok = false;

    // ----- 情况1：未指定类型，自动检测 -----
    if (type == nullptr)
    {
        PreferenceType pt = hal.pref.getType(key);
        if (pt == PT_INVALID)
        {
            PRINT_ERROR("Key '%s' not found, cannot auto-detect type", key);
            return 1;
        }
        // 如果已有类型是 BLOB，无法自动写入（可能是 float/double 或原始二进制）
        if (pt == PT_BLOB)
        {
            PRINT_ERROR("Key '%s' is a BLOB (float/double/blob). Please specify type explicitly.", key);
            return 1;
        }

        // 根据已有类型进行写入
        switch (pt)
        {
        case PT_I8:
        {
            int8_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putChar(key, val);
            break;
        }
        case PT_U8:
        {
            uint8_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUChar(key, val);
            break;
        }
        case PT_I16:
        {
            int16_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putShort(key, val);
            break;
        }
        case PT_U16:
        {
            uint16_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUShort(key, val);
            break;
        }
        case PT_I32:
        {
            int32_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putInt(key, val);
            break;
        }
        case PT_U32:
        {
            uint32_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUInt(key, val);
            break;
        }
        case PT_I64:
        {
            int64_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putLong64(key, val);
            break;
        }
        case PT_U64:
        {
            uint64_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putULong64(key, val);
            break;
        }
        case PT_STR:
        {
            // 字符串直接使用原值，无需转换
            ok = hal.pref.putString(key, valueStr);
            break;
        }
        case PT_BLOB: // 已在前面过滤
        default:
            // 不会到达这里
            return 1;
        }
    }
    // ----- 情况2：指定类型 -----
    else
    {
        // 处理布尔类型（支持字符串和数字）
        if (strcmp(type, "bool") == 0)
        {
            bool val;
            if (!parseBool(valueStr, val))
            {
                PRINT_ERROR("Invalid bool value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putBool(key, val);
        }
        else if (strcmp(type, "int") == 0 || strcmp(type, "i32") == 0)
        {
            int32_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putInt(key, val);
        }
        else if (strcmp(type, "uint") == 0 || strcmp(type, "u32") == 0)
        {
            uint32_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint32 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUInt(key, val);
        }
        else if (strcmp(type, "i8") == 0)
        {
            int8_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putChar(key, val);
        }
        else if (strcmp(type, "u8") == 0)
        {
            uint8_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint8 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUChar(key, val);
        }
        else if (strcmp(type, "i16") == 0)
        {
            int16_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putShort(key, val);
        }
        else if (strcmp(type, "u16") == 0)
        {
            uint16_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint16 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putUShort(key, val);
        }
        else if (strcmp(type, "i64") == 0)
        {
            int64_t val;
            if (!parseInteger(valueStr, val))
            {
                PRINT_ERROR("Invalid int64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putLong64(key, val);
        }
        else if (strcmp(type, "u64") == 0)
        {
            uint64_t val;
            if (!parseUnsigned(valueStr, val))
            {
                PRINT_ERROR("Invalid uint64 value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putULong64(key, val);
        }
        else if (strcmp(type, "float") == 0)
        {
            float val;
            if (!parseFloat(valueStr, val))
            {
                PRINT_ERROR("Invalid float value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putFloat(key, val);
        }
        else if (strcmp(type, "double") == 0)
        {
            double val;
            if (!parseDouble(valueStr, val))
            {
                PRINT_ERROR("Invalid double value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putDouble(key, val);
        }
        else if (strcmp(type, "string") == 0)
        {
            // 字符串直接使用原值
            ok = hal.pref.putString(key, valueStr);
        }
        else if (strcmp(type, "blob") == 0)
        {
            // 选择一个合理的最大 BLOB 长度，防止栈溢出
            const size_t MAX_BLOB_LEN = 512; // 可根据实际需求调整
            uint8_t buffer[MAX_BLOB_LEN];

            size_t len = parseHexToBytes(valueStr, buffer, MAX_BLOB_LEN);
            if (len == 0)
            {
                PRINT_ERROR("Invalid hex blob value: %s", valueStr);
                return 1;
            }
            ok = hal.pref.putBytes(key, buffer, len);
        }
        else
        {
            PRINT_ERROR("Unsupported type: %s", type);
            return 1;
        }
    }

    if (ok)
    {
        PRINT_SUCCESS("successful write NVS key \"%s\"", key);
        return 0;
    }
    else
    {
        PRINT_ERROR("fail write NVS key \"%s\"", key);
        return 2;
    }
}

// ==================== getnvs 命令 ====================
static int cmd_getnvs(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        return 1; // 参数错误
    }

    const char *key = argv[1];
    const char *type = (argc == 3) ? argv[2] : nullptr;

    // 先检查键是否存在
    if (hal.pref.getType(key) == PT_INVALID)
    {
        PRINT_ERROR("Key '%s' not found", key);
        return 1;
    }

    // ----- 指定类型读取（目前仅支持 float/double） -----
    if (type != nullptr)
    {
        if (strcmp(type, "float") == 0)
        {
            // 验证长度是否为4字节
            size_t len = hal.pref.getBytesLength(key);
            if (len != sizeof(float))
            {
                PRINT_ERROR("Key '%s' is not a float (actual size %u bytes)", key, len);
                return 1;
            }
            float val = hal.pref.getFloat(key);
            PRINT_INFO("%s (float): %f", key, val);
        }
        else if (strcmp(type, "double") == 0)
        {
            size_t len = hal.pref.getBytesLength(key);
            if (len != sizeof(double))
            {
                PRINT_ERROR("Key '%s' is not a double (actual size %u bytes)", key, len);
                return 1;
            }
            double val = hal.pref.getDouble(key);
            PRINT_INFO("%s (double): %lf", key, val);
        }
        else
        {
            PRINT_ERROR("Specified type '%s' not supported for reading", type);
            return 1;
        }
        return 0;
    }

    // ----- 未指定类型，自动检测并输出 -----
    PreferenceType pt = hal.pref.getType(key);
    switch (pt)
    {
    case PT_I8:
    {
        int8_t val = hal.pref.getChar(key);
        PRINT_INFO("%s (int8): %d", key, val);
        break;
    }
    case PT_U8:
    {
        uint8_t val = hal.pref.getUChar(key);
        PRINT_INFO("%s (uint8): %u", key, val);
        break;
    }
    case PT_I16:
    {
        int16_t val = hal.pref.getShort(key);
        PRINT_INFO("%s (int16): %d", key, val);
        break;
    }
    case PT_U16:
    {
        uint16_t val = hal.pref.getUShort(key);
        PRINT_INFO("%s (uint16): %u", key, val);
        break;
    }
    case PT_I32:
    {
        int32_t val = hal.pref.getInt(key);
        PRINT_INFO("%s (int32): %d", key, val);
        break;
    }
    case PT_U32:
    {
        uint32_t val = hal.pref.getUInt(key);
        PRINT_INFO("%s (uint32): %u", key, val);
        break;
    }
    case PT_I64:
    {
        int64_t val = hal.pref.getLong64(key);
        PRINT_INFO("%s (int64): %lld", key, (long long)val);
        break;
    }
    case PT_U64:
    {
        uint64_t val = hal.pref.getULong64(key);
        PRINT_INFO("%s (uint64): %llu", key, (unsigned long long)val);
        break;
    }
    case PT_STR:
    {
        String val = hal.pref.getString(key);
        PRINT_INFO("%s (string): \"%s\"", key, val.c_str());
        break;
    }
    case PT_BLOB:
    {
        size_t len = hal.pref.getBytesLength(key);
        char buffer[len + 1];
        hal.pref.getBytes(key, buffer, len + 1);
        PRINT_INFO("%s (blob): length %u bytes", key, len);
        INFO_COLOR;
        for (size_t i = 0; i < len; ++i)
        {
            log_printf("0x%02X ", (uint8_t)buffer[i]);
        }
        RESET_COLOR;
        break;
    }
    default:
        PRINT_ERROR("Key '%s' has unknown type", key);
        return 1;
    }
    return 0;
}

// 删除 NVS 键值对命令
static int cmd_rmnvs(int argc, char **argv)
{
    // Usage: rmnvs <key>
    if (argc != 2)
    {
        // 参数个数错误，返回1会自动显示帮助
        return 1;
    }

    const char *key = argv[1];

    // 检查键是否存在
    if (hal.pref.getType(key) == PT_INVALID)
    {
        PRINT_ERROR("Key '%s' not found", key);
        return 2; // 执行失败
    }

    // 执行删除
    bool ok = hal.pref.remove(key);
    if (ok)
    {
        PRINT_SUCCESS("NVS key '%s' removed", key);
        return 0;
    }
    else
    {
        PRINT_ERROR("Failed to remove key '%s'", key);
        return 2;
    }
}

static int cmd_display_debug(int argc, char **argv)
{
    if (argc == 2)
    {
        bool en = atoi(argv[1]);
        bool valid = (en == 1 || en == 0);
        if (!valid)
        {
            return 1;
        }
        display.debug_log((bool)en);
        return 0;
    }
    else
        return 1;
    return 0;
}

static int cmd_file_task_info(int argc, char **argv)
{
    if (multi_thread_queue != NULL)
    {
        UBaseType_t count = uxQueueMessagesWaiting(multi_thread_queue);
        PRINT_INFO("当前文件写入任务队列深度:%lu", count);
    }
    else
        PRINT_WARNING("当前文件写入任务队列为空");
    return 0;
}

#include "soc/soc.h"
#include "soc/system_reg.h"

static int cmd_cpufreq_reg(int argc, char **argv)
{
    if (argc == 2)
    {
        uint8_t val = atoi(argv[1]);
        bool valid = (val < 4);
        if (!valid)
        {
            return 1;
        }
        REG_SET_FIELD(SYSTEM_CPU_PER_CONF_REG, SYSTEM_CPUPERIOD_SEL, val);
        int64_t start_us = esp_timer_get_time();
        for (int i = 0; i < 3000; ++i)
        {
            _NOP();
        }
        int64_t end_us = esp_timer_get_time();
        int64_t elapsed_us = end_us - start_us;
        PRINT_INFO("NOP latency: %lld us\n", elapsed_us);
        return 0;
    }
    else
        return 1;
    return 0;
}

static void on_wifi_task(void *pvParameters)
{
    hal.autoConnectWiFi(false);
    vTaskDelete(NULL);
}

static String urlEncode(const String &str)
{
    String encoded = "";
    char hex[17] = "0123456789ABCDEF"; // hex字符范围

    for (size_t i = 0; i < str.length(); i++)
    {
        unsigned char c = str[i]; // 取出每个字节

        // 保留无需编码的字符：字母、数字、-_.~:/
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ':' || c == '/' || c == '=' || c == '?')
        {
            encoded += (char)c;
        }
        else
        {
            // 其他字符转换为 %XX 格式
            encoded += '%';
            encoded += hex[(c >> 4) & 0xF];
            encoded += hex[c & 0xF];
        }
    }

    return encoded;
}

static int cmd_download(int argc, char **argv)
{
    if (argc != 4 && argc != 5)
        return 1;

    bool encodeUrl = false;
    int argOffset = 0;

    if (argc == 5)
    {
        if (strcmp(argv[1], "-e") != 0)
            return 1;
        encodeUrl = true;
        argOffset = 1;
    }

    char *ca_id = argv[1 + argOffset];
    String url = argv[2 + argOffset];
    String save_file = argv[3 + argOffset];
    String _url = encodeUrl ? urlEncode(url) : url;

    // 2. 获取 CA 证书（可能为空字符串）
    String CAcert = hal.get_CAcert(ca_id);

    // 3. 确保 WiFi 已连接
    if (!WiFi.isConnected())
    {
        xTaskCreatePinnedToCore(on_wifi_task, "on_wifi_task", 8192, NULL, 1, NULL, 0);

        unsigned long start = millis();
        const unsigned long timeout = 30000; // 30 秒超时
        while (!WiFi.isConnected() && (millis() - start) < timeout)
        {
            delay(100);
            log_printf("\r|");
            delay(100);
            log_printf("\r/");
            delay(100);
            log_printf("\r-");
            delay(100);
            log_printf("\r\\");
        }

        if (!WiFi.isConnected())
        {
            PRINT_ERROR("WiFi connection timeout!");
            return 2;
        }
        log_printf("\r\n"); // 清除旋转动画
        PRINT_INFO("WiFi connected.");
    }

    WiFi.setSleep(false);

    // 4. 初始化 HTTP 客户端
    HTTPClient http;
    WiFiClientSecure client;

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    bool httpStarted = false;

    if (CAcert.length() == 0)
    {
        client.setInsecure();
        httpStarted = http.begin(client, _url);
    }
    else
        httpStarted = http.begin(_url, CAcert.c_str());

    if (!httpStarted)
    {
        PRINT_ERROR("HTTP begin failed!");
        return 2;
    }

    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
    http.addHeader("Accept", "*/*");
    http.addHeader("Connection", "keep-alive");

    // 5. 发送 GET 请求
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        PRINT_ERROR("HTTP GET failed, code: %d", httpCode);
        http.end();
        return 2;
    }

    // 6. 获取文件总大小（可能为 -1 表示未知）
    int totalSize = http.getSize(); // HTTPClient 的 getSize() 返回 Content-Length
    if (totalSize <= 0)
    {
        PRINT_WARNING("Cannot determine file size, progress will not be shown.");
    }

    // 7. 打开目标文件
    File file = hal.open(save_file, FILE_WRITE);
    if (!file)
    {
        PRINT_ERROR("Failed to open file: %s", save_file.c_str());
        http.end();
        return 2;
    }

    // 8. 读取数据流并写入文件，同时显示进度
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t totalWritten = 0;
    int lastPercent = -1;
    const int progressBarWidth = 50; // 进度条宽度（字符数）
    long beginTime = millis();

    while (http.connected() && (stream->available() > 0 || totalWritten < (size_t)totalSize))
    {
        int len = stream->readBytes(buffer, sizeof(buffer));
        if (len > 0)
        {
            size_t written = file.write(buffer, len);
            if (written != (size_t)len)
            {
                PRINT_ERROR("File write error!");
                file.close();
                http.end();
                return 2;
            }
            totalWritten += written;

            // 计算并显示进度（仅当总大小已知时）
            if (totalSize > 0)
            {
                int percent = (int)((totalWritten * 100) / totalSize);
                if (percent != lastPercent)
                {
                    lastPercent = percent;
                    int fill = (percent * progressBarWidth) / 100;
                    log_printf("\r["); // 开始打印进度条
                    for (int i = 0; i < fill; i++)
                        log_printf("█"); // 打印已下载部分
                    for (int i = fill; i < progressBarWidth; i++)
                        log_printf("░");            // 打印未下载部分
                    log_printf("] %3d%%", percent); // 完成百分比
                }
            }
        }
        else
        {
            // 无数据但连接仍在，稍等再试（避免死循环）
            yield();
        }
    }

    // 确保所有数据都已读完（对于 chunked 或未知大小的情况）
    while (stream->available())
    {
        int len = stream->readBytes(buffer, sizeof(buffer));
        if (len > 0)
        {
            size_t written = file.write(buffer, len);
            if (written != (size_t)len)
            {
                PRINT_ERROR("File write error!");
                file.close();
                http.end();
                return 2;
            }
            totalWritten += written;
        }
    }

    // 9. 关闭文件和 HTTP 连接
    file.close();
    http.end();

    long elapsedTime = millis() - beginTime;
    PRINT_INFO("\nDownload completed in %.2f seconds, speed %.2f KB/s", elapsedTime / 1000.0, totalWritten / (elapsedTime / 1000.0) / 1024.0);
    // 换行，使下一个输出不覆盖进度条
    log_printf("\n");

    // 10. 输出结果
    if (totalSize > 0 && totalWritten == (size_t)totalSize)
    {
        PRINT_SUCCESS("Downloaded %u bytes to %s", totalWritten, save_file.c_str());
        return 0;
    }
    else if (totalSize <= 0)
    {
        PRINT_SUCCESS("Downloaded %u bytes to %s (size unknown)", totalWritten, save_file.c_str());
        return 0;
    }
    else
    {
        PRINT_ERROR("Download incomplete: expected %d bytes, got %u", totalSize, totalWritten);
        return 2;
    }
}

// ==================== 辅助函数 ====================

// 打印指定目录的内容（不递归）
static void list_dir(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
    {
        PRINT_ERROR("Cannot open directory: %s", path);
        return;
    }

    log_printf("Directory: %s\n", path);

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // 跳过当前目录和父目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // 构建完整路径以便获取详细信息
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0)
        {
            PRINT_WARNING("Cannot stat: %s", full_path);
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            log_printf("\033[36m[DIR] %s\033[0m\n", entry->d_name);
        }
        else if (S_ISREG(st.st_mode))
        {
            log_printf("[FILE] %s (%u bytes)\n", entry->d_name, (unsigned int)st.st_size);
        }
        else
        {
            log_printf("[UNKN] %s\n", entry->d_name);
        }
    }

    closedir(dir);
}
// ==================== 辅助函数 ====================

// 去除路径末尾的 '/'（根目录 "/" 除外）
static void strip_trailing_slash(char *path)
{
    if (!path || path[0] == '\0')
        return;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/')
    {
        path[len - 1] = '\0';
        len--;
    }
}

// 递归创建目录（类似 mkdir -p）
static int mkdir_p(const char *dir)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    strip_trailing_slash(tmp); // 确保干净
    size_t len = strlen(tmp);

    // 去掉末尾的斜杠（如果有）之后逐级检查
    for (size_t i = 1; i < len; i++)
    { // 从 "/" 后开始
        if (tmp[i] == '/')
        {
            tmp[i] = '\0';
            struct stat st;
            if (stat(tmp, &st) == 0)
            {
                if (!S_ISDIR(st.st_mode))
                {
                    PRINT_ERROR("Component '%s' exists but is not a directory", tmp);
                    return -1;
                }
            }
            else
            {
                if (mkdir(tmp, 0755) != 0)
                {
                    PRINT_ERROR("mkdir '%s' failed: errno %d", tmp, errno);
                    return -1;
                }
            }
            tmp[i] = '/';
        }
    }
    // 最后创建完整路径
    struct stat st;
    if (stat(tmp, &st) == 0)
    {
        if (!S_ISDIR(st.st_mode))
        {
            PRINT_ERROR("'%s' exists but is not a directory", tmp);
            return -1;
        }
    }
    else
    {
        if (mkdir(tmp, 0755) != 0)
        {
            PRINT_ERROR("mkdir '%s' failed: errno %d", tmp, errno);
            return -1;
        }
    }
    return 0;
}

static int remove_directory_recursive(const char *path, bool verbose)
{
    DIR *dir = opendir(path);
    if (!dir)
        return -1;

    struct dirent *entry;
    char full_path[1024];

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1)
        {
            closedir(dir);
            return -1;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (remove_directory_recursive(full_path, verbose) != 0)
            {
                closedir(dir);
                return -1;
            }
        }
        else
        {
            if (remove(full_path) == 0)
            {
                if (verbose)
                    PRINT_INFO("removed file '%s'", full_path);
            }
            else
            {
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);

    if (rmdir(path) == 0)
    {
        if (verbose)
            PRINT_INFO("removed directory '%s'", path);
        return 0;
    }
    return -1;
}

// ==================== cp 辅助函数 ====================

// 复制单个文件，src -> dst，返回 0 成功
static int cp_file(const char *src, const char *dst, bool force, bool verbose)
{
    // 检查目标是否已存在，若存在且非 force，则报错
    struct stat st_dst;
    if (stat(dst, &st_dst) == 0)
    {
        if (!force)
        {
            PRINT_ERROR("File '%s' already exists. Use -f to overwrite.", dst);
            return -1;
        }
        // 删除已存在的目标文件，以便覆盖（remove 后立即创建）
        if (remove(dst) != 0)
        {
            PRINT_ERROR("Cannot remove existing file '%s'", dst);
            return -1;
        }
    }

    FILE *fsrc = fopen(src, "rb");
    if (!fsrc)
    {
        PRINT_ERROR("Cannot open source file: %s", src);
        return -1;
    }

    FILE *fdst = fopen(dst, "wb");
    if (!fdst)
    {
        PRINT_ERROR("Cannot create destination file: %s", dst);
        fclose(fsrc);
        return -1;
    }

    const size_t buf_size = 512;
    uint8_t buf[buf_size];
    size_t total = 0;
    size_t len;
    int ret = 0;
    while ((len = fread(buf, 1, buf_size, fsrc)) > 0)
    {
        if (fwrite(buf, 1, len, fdst) != len)
        {
            PRINT_ERROR("Write error to '%s'", dst);
            ret = -1;
            break;
        }
        total += len;
    }
    if (ferror(fsrc))
    {
        PRINT_ERROR("Read error from '%s'", src);
        ret = -1;
    }

    fclose(fsrc);
    fclose(fdst);

    if (ret == 0 && verbose)
    {
        PRINT_INFO("'%s' -> '%s' (%u bytes)", src, dst, total);
    }
    return ret;
}

// 递归复制目录，src_dir -> dst_dir，dst_dir 必须是不存在或允许覆盖的目录
static int cp_dir(const char *src_dir, const char *dst_dir, bool force, bool verbose)
{
    // 确保目标目录存在（若不存在则创建）
    struct stat st;
    if (stat(dst_dir, &st) != 0)
    {
        if (mkdir(dst_dir, 0755) != 0)
        {
            PRINT_ERROR("Cannot create target directory '%s'", dst_dir);
            return -1;
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {
        PRINT_ERROR("Target '%s' exists but is not a directory", dst_dir);
        return -1;
    }

    DIR *dir = opendir(src_dir);
    if (!dir)
    {
        PRINT_ERROR("Cannot open source directory: %s", src_dir);
        return -1;
    }

    struct dirent *entry;
    char src_path[512], dst_path[512];
    int ret = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat st_entry;
        if (stat(src_path, &st_entry) != 0)
        {
            PRINT_ERROR("Cannot stat '%s'", src_path);
            ret = -1;
            continue;
        }

        if (S_ISDIR(st_entry.st_mode))
        {
            if (cp_dir(src_path, dst_path, force, verbose) != 0)
            {
                ret = -1;
            }
        }
        else
        {
            if (cp_file(src_path, dst_path, force, verbose) != 0)
            {
                ret = -1;
            }
        }
    }
    closedir(dir);
    return ret;
}

// 候选挂载点列表（可按实际挂载情况增减）
static const char *CANDIDATE_MOUNTS[] = {
    "/littlefs",
    "/sd",
    "/sdcard",
    "/nand",
    "/spiffs",
    "/ffat",
    // 可在此添加其他可能挂载点
};
#define CANDIDATE_COUNT (sizeof(CANDIDATE_MOUNTS) / sizeof(CANDIDATE_MOUNTS[0]))

#define MAX_MOUNT_POINTS 4
static char mount_points[MAX_MOUNT_POINTS][64];
static int num_mount_points = 0;

static void scan_mount_points(void)
{
    num_mount_points = 0;
    struct stat root_st;
    bool root_valid = (stat("/", &root_st) == 0);

    for (int i = 0; i < CANDIDATE_COUNT && num_mount_points < MAX_MOUNT_POINTS; i++)
    {
        struct stat st;
        if (stat(CANDIDATE_MOUNTS[i], &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        // 如果根目录不可 stat，则直接接受；否则比较设备号
        if (!root_valid || st.st_dev != root_st.st_dev)
        {
            strncpy(mount_points[num_mount_points], CANDIDATE_MOUNTS[i],
                    sizeof(mount_points[0]) - 1);
            mount_points[num_mount_points][sizeof(mount_points[0]) - 1] = '\0';
            num_mount_points++;
        }
    }
}

// ==================== du 辅助函数 ====================

// 将字节转换为人类可读字符串（结果存入 buf，buf 大小至少 16）
static void human_readable_size(uint64_t bytes, char *buf, size_t buf_size)
{
    const char *units[] = {"B", "K", "M", "G"};
    int unit = 0;
    double size = (double)bytes;
    while (size >= 1024.0 && unit < 3)
    {
        size /= 1024.0;
        unit++;
    }
    snprintf(buf, buf_size, "%.1f%s", size, units[unit]);
}

// 递归计算目录大小（字节），如果 verbose 且 !summarize，则打印每个子目录的大小
// base_path: 用于显示的路径前缀（保持和传入时一致）
// 返回总字节数，出错返回 0
static uint64_t du_dir(const char *path, bool human, bool summarize)
{
    DIR *dir = opendir(path);
    if (!dir)
    {
        PRINT_ERROR("Cannot open directory: %s", path);
        return 0;
    }

    struct dirent *entry;
    char full_path[512];
    uint64_t total = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            // 递归子目录，并传入相同的 summarize 标志
            uint64_t sub_size = du_dir(full_path, human, summarize);
            total += sub_size;

            // 非汇总模式：打印子目录的大小（这里就是 Linux du 的行为）
            if (!summarize)
            {
                char size_str[16];
                if (human)
                {
                    human_readable_size(sub_size, size_str, sizeof(size_str));
                }
                else
                {
                    snprintf(size_str, sizeof(size_str), "%" PRIu64, sub_size);
                }
                PRINT_INFO("%-8s %s", size_str, full_path);
            }
        }
        else
        {
            total += st.st_size;
            // 文件大小不单独打印（du 只显示目录累计大小）
        }
    }
    closedir(dir);

    // 注意：不打印 path 本身，由外部 cmd_du 决定何时打印顶层目录
    return total;
}

// 处理转义序列：将 src 中的 \n, \t, \\, \" 等替换为实际字符，结果存入 dst，dst_size 为缓冲区大小
static void expand_escapes(const char *src, char *dst, size_t dst_size)
{
    if (src == NULL || dst == NULL || dst_size == 0)
    {
        if (dst && dst_size > 0)
        {
            dst[0] = '\0';
        }
        return;
    }
    size_t i = 0, j = 0;
    while (src[i] != '\0' && j < dst_size - 1)
    {
        if (src[i] == '\\' && src[i + 1] != '\0')
        {
            i++; // 跳过反斜杠
            switch (src[i])
            {
            case 'n':
                dst[j++] = '\n';
                break;
            case 't':
                dst[j++] = '\t';
                break;
            case '\\':
                dst[j++] = '\\';
                break;
            case '\"':
                dst[j++] = '\"';
                break;
            case '\'':
                dst[j++] = '\'';
                break;
            default: // 非转义字符，保留反斜杠和原字符
                dst[j++] = '\\';
                dst[j++] = src[i];
                break;
            }
            i++;
        }
        else
        {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

// ==================== ls 命令 ====================
// 用法：ls [path1] [path2] ...
static int cmd_ls(int argc, char **argv)
{
    if (argc == 1)
    {
        scan_mount_points();
        if (num_mount_points == 0)
        {
            // 没有任何挂载点，尝试列出根目录
            list_dir("/");
        }
        else
        {
            for (int i = 0; i < num_mount_points; i++)
            {
                list_dir(mount_points[i]);
            }
        }
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        strip_trailing_slash(argv[i]);
        list_dir(argv[i]);
    }
    return 0;
}

// ==================== echo 命令 ====================
// 用法：
//   echo [text ...]                      - 输出到串口（带换行）
//   echo [text ...] > <file>             - 覆盖写入
//   echo [text ...] >> <file>            - 追加写入
static int cmd_echo(int argc, char **argv)
{
    if (argc < 2)
    {
        log_printf("\n");
        return 0;
    }

    // --- 1. 解析选项和重定向 ---
    bool enable_escapes = false;
    int text_start = 1;
    if (strcmp(argv[1], "-e") == 0)
    {
        enable_escapes = true;
        text_start = 2;
    }

    int redir_idx = -1;
    const char *mode = NULL;
    for (int i = text_start; i < argc; i++)
    {
        if (strcmp(argv[i], ">") == 0)
        {
            redir_idx = i;
            mode = "w";
            break;
        }
        else if (strcmp(argv[i], ">>") == 0)
        {
            redir_idx = i;
            mode = "a";
            break;
        }
    }

    // --- 2. 计算 raw_text 所需总长度（不含终止符） ---
    size_t raw_len = 0;
    int text_end = (redir_idx > 0) ? redir_idx : argc;
    for (int i = text_start; i < text_end; i++)
    {
        if (i > text_start)
            raw_len++; // 空格
        raw_len += strlen(argv[i]);
    }

    // 动态分配 raw_text
    char *raw_text = (char *)malloc(raw_len + 1);
    if (!raw_text)
    {
        PRINT_ERROR("Out of memory for raw_text");
        return 1;
    }

    // 安全拼接
    raw_text[0] = '\0';
    size_t pos = 0;
    for (int i = text_start; i < text_end; i++)
    {
        if (i > text_start)
        {
            raw_text[pos++] = ' ';
            raw_text[pos] = '\0';
        }
        size_t arg_len = strlen(argv[i]);
        if (pos + arg_len > raw_len)
        { // 理论不会，但保险
            PRINT_ERROR("Internal buffer overrun");
            free(raw_text);
            return 1;
        }
        memcpy(raw_text + pos, argv[i], arg_len + 1); // 复制包括'\0'
        pos += arg_len;
    }

    // --- 3. 处理转义 ---
    // 转义展开后长度只会减小（两个字符→一个字符），所以 final_text 分配与 raw_text 相同大小即可
    char *final_text = (char *)malloc(raw_len + 1);
    if (!final_text)
    {
        PRINT_ERROR("Out of memory for final_text");
        free(raw_text);
        return 1;
    }

    if (enable_escapes)
    {
        expand_escapes(raw_text, final_text, raw_len + 1);
    }
    else
    {
        // 直接复制（raw_text 必定以 '\0' 结尾）
        memcpy(final_text, raw_text, raw_len + 1);
    }

    // --- 4. 添加换行符（除非重定向，通常也加，但按 Linux 习惯 echo 默认换行）---
    size_t final_len = strlen(final_text);
    if (final_len + 2 <= raw_len + 1)
    {
        final_text[final_len] = '\n';
        final_text[final_len + 1] = '\0';
    }
    else
    {
        // 空间不够，放弃换行（或截断），这里直接不追加
        // 但更好是重新分配更大的缓冲区，但为了简单，我们可以在末尾加换行前检查
        // 如果不够，就重新分配
        char *new_final = (char *)realloc(final_text, final_len + 2);
        if (new_final)
        {
            final_text = new_final;
            final_text[final_len] = '\n';
            final_text[final_len + 1] = '\0';
        }
        else
        {
            // 无法扩展，就保持原样，但会丢失换行
            PRINT_ERROR("Can't append newline, output may be incomplete");
        }
    }

    // --- 5. 输出 ---
    int ret = 0;
    if (redir_idx > 0)
    {
        if (redir_idx + 1 >= argc)
        {
            PRINT_ERROR("Missing file operand after '%s'", argv[redir_idx]);
            ret = 1;
            goto cleanup;
        }
        const char *file_path = argv[redir_idx + 1];
        // 处理路径（复制到临时缓冲区，因为 strip_trailing_slash 需要可修改）
        char *file_buf = (char *)malloc(strlen(file_path) + 1);
        if (!file_buf)
        {
            PRINT_ERROR("Out of memory for file path");
            ret = 1;
            goto cleanup;
        }
        strcpy(file_buf, file_path);
        strip_trailing_slash(file_buf);

        FILE *f = fopen(file_buf, mode);
        if (!f)
        {
            PRINT_ERROR("Failed to open file: %s", file_buf);
            free(file_buf);
            ret = 2;
            goto cleanup;
        }
        int written = fprintf(f, "%s", final_text);
        fclose(f);
        free(file_buf);
        if (written < 0 || (size_t)written != strlen(final_text))
        {
            PRINT_ERROR("Write error to file: %s", file_buf);
            ret = 2;
            goto cleanup;
        }
        PRINT_SUCCESS("Written %u bytes to %s", written, file_buf);
    }
    else
    {
        // 输出到控制台
        log_printf("%s", final_text);
    }

cleanup:
    free(raw_text);
    free(final_text);
    return ret;
}

// ==================== cat 命令 ====================
// 用法：cat <file1> [file2 ...]
static int cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        return 1;
    }

    const size_t buf_size = 4096; // 增大缓冲区，减少系统调用
    uint8_t buffer[buf_size];

    // 可选：禁用 stdio 缓冲，让输出立即发送（对实时性要求高时可启用）
    // setbuf(stdout, NULL);

    for (int i = 1; i < argc; i++)
    {
        strip_trailing_slash(argv[i]);
        const char *path = argv[i];

        FILE *f = fopen(path, "rb"); // 二进制模式，避免 CR/LF 转换
        if (!f)
        {
            PRINT_ERROR("Cannot open file: %s", path);
            continue;
        }

        size_t total = 0;
        size_t len;
        while ((len = fread(buffer, 1, buf_size, f)) > 0)
        {
            size_t written = fwrite(buffer, 1, len, stdout);
            if (written != len)
            {
                PRINT_ERROR("Write error while reading %s", path);
                break;
            }
            total += len;
        }
        fclose(f);

        if (total == 0)
        {
            PRINT_WARNING("File is empty: %s", path);
        }
    }

    // 确保所有数据已发送（通常 fclose(stdout) 不会被调用，这里显式刷新）
    fflush(stdout);
    return 0;
}

// ==================== mkdir 命令 ====================
// 用法：mkdir [-p] <dir1> [dir2 ...]
static int cmd_mkdir(int argc, char **argv)
{
    bool create_parents = false;
    int first_dir = 1;

    // 解析 -p 选项
    if (argc >= 2 && strcmp(argv[1], "-p") == 0)
    {
        create_parents = true;
        first_dir = 2;
    }

    if (first_dir >= argc)
    {
        PRINT_ERROR("Missing directory operand");
        return 1;
    }

    for (int i = first_dir; i < argc; i++)
    {
        strip_trailing_slash(argv[i]);
        const char *path = argv[i];

        if (create_parents)
        {
            if (mkdir_p(path) != 0)
            {
                PRINT_ERROR("Failed to create directory (with parents): %s", path);
                return 2;
            }
            PRINT_SUCCESS("Directory created: %s", path);
        }
        else
        {
            struct stat st;
            if (stat(path, &st) == 0)
            {
                PRINT_ERROR("Path already exists: %s", path);
                return 2;
            }
            if (mkdir(path, 0755) == 0)
            {
                PRINT_SUCCESS("Directory created: %s", path);
            }
            else
            {
                PRINT_ERROR("Failed to create directory: %s (errno: %d)", path, errno);
                return 2;
            }
        }
    }
    return 0;
}

// ==================== rm 命令 ====================
// 用法：rm [-r] [-f] [-v] <target1> [target2 ...]
static int cmd_rm(int argc, char **argv)
{
    bool recursive = false;
    bool force = false;
    bool verbose = false;
    int first_target = 1;

    // 解析选项组合，如 -rf, -frv, -v -r -f 等
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            for (int j = 1; argv[i][j] != '\0'; j++)
            {
                switch (argv[i][j])
                {
                case 'r':
                    recursive = true;
                    break;
                case 'f':
                    force = true;
                    break;
                case 'v':
                    verbose = true;
                    break;
                default:
                    PRINT_ERROR("Unknown option '-%c'", argv[i][j]);
                    return 1;
                }
            }
            first_target = i + 1;
        }
        else
        {
            break; // 遇到非选项参数，后面的都是目标
        }
    }

    if (first_target >= argc)
    {
        PRINT_ERROR("Missing operand");
        return 1;
    }

    int ret = 0;
    for (int i = first_target; i < argc; i++)
    {
        strip_trailing_slash(argv[i]);
        const char *path = argv[i];

        struct stat st;
        if (stat(path, &st) != 0)
        {
            if (!force)
            {
                PRINT_ERROR("Path does not exist: %s", path);
                ret = 2;
            }
            else
            {
                if (verbose)
                    PRINT_INFO("(ignored missing) %s", path);
            }
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (recursive)
            {
                if (remove_directory_recursive(path, verbose) == 0)
                {
                    PRINT_SUCCESS("Removed directory recursively: %s", path);
                }
                else
                {
                    if (!force)
                    {
                        PRINT_ERROR("Failed to remove directory: %s (errno: %d)", path, errno);
                        ret = 2;
                    }
                    else
                    {
                        if (verbose)
                            PRINT_INFO("(force) failed to remove %s, continuing", path);
                    }
                }
            }
            else
            {
                if (!force)
                {
                    PRINT_ERROR("'%s' is a directory. Use -r to remove it.", path);
                    ret = 2;
                }
                else
                {
                    if (verbose)
                        PRINT_INFO("(force) skipping directory %s", path);
                }
            }
        }
        else
        {
            // 普通文件
            if (remove(path) == 0)
            {
                if (verbose)
                    PRINT_INFO("removed file '%s'", path);
                PRINT_SUCCESS("Removed file: %s", path);
            }
            else
            {
                if (!force)
                {
                    PRINT_ERROR("Failed to remove file: %s (errno: %d)", path, errno);
                    ret = 2;
                }
                else
                {
                    if (verbose)
                        PRINT_INFO("(force) failed to remove %s, continuing", path);
                }
            }
        }
    }
    return ret;
}

// ==================== mv 命令 ====================
// 用法：mv <source> <dest>
// 改进：若目标为已存在的目录，则将源移动进该目录。
static int cmd_mv(int argc, char **argv)
{
    if (argc != 3)
    {
        return 1;
    }

    char src[256], dst[256];
    strncpy(src, argv[1], sizeof(src) - 1);
    strncpy(dst, argv[2], sizeof(dst) - 1);
    src[sizeof(src) - 1] = '\0';
    dst[sizeof(dst) - 1] = '\0';
    strip_trailing_slash(src);
    strip_trailing_slash(dst);

    struct stat st_src;
    if (stat(src, &st_src) != 0)
    {
        PRINT_ERROR("Source does not exist: %s", src);
        return 2;
    }

    // 检查目标状态
    struct stat st_dst;
    int dst_exists = (stat(dst, &st_dst) == 0);

    if (dst_exists && S_ISDIR(st_dst.st_mode))
    {
        // 目标是一个已存在的目录：移动源到该目录下，保持原名
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;
        char new_dst[512];
        snprintf(new_dst, sizeof(new_dst), "%s/%s", dst, base);
        // 检查新路径是否已存在
        if (stat(new_dst, &st_dst) == 0)
        {
            PRINT_ERROR("Destination already exists: %s", new_dst);
            return 2;
        }
        if (rename(src, new_dst) == 0)
        {
            PRINT_SUCCESS("Moved/Renamed: %s -> %s", src, new_dst);
            return 0;
        }
        else
        {
            if (errno == EXDEV)
            {
                PRINT_ERROR("Move failed: cross-device rename not allowed (%s -> %s)", src, new_dst);
            }
            else
            {
                PRINT_ERROR("Move failed: %s -> %s (errno: %d)", src, new_dst, errno);
            }
            return 2;
        }
    }
    else
    {
        // 目标不存在或为文件（根据 Linux 行为，若目标文件存在则覆盖，这里直接允许 rename）
        // 为防止意外覆盖，可以检查 src 和 dst 是否相同
        if (strcmp(src, dst) == 0)
        {
            PRINT_ERROR("Source and destination are the same: %s", src);
            return 2;
        }
        if (rename(src, dst) == 0)
        {
            PRINT_SUCCESS("Moved/Renamed: %s -> %s", src, dst);
            return 0;
        }
        else
        {
            if (errno == EXDEV)
            {
                PRINT_ERROR("Move failed: cross-device rename not allowed (%s -> %s)", src, dst);
            }
            else
            {
                PRINT_ERROR("Move failed: %s -> %s (errno: %d)", src, dst, errno);
            }
            return 2;
        }
    }
}

// ==================== cp 命令 ====================
static int cmd_cp(int argc, char **argv)
{
    bool recursive = false;
    bool force = false;
    bool verbose = false;
    int first_src = 1;

    // 解析选项： -r, -f, -v 及其组合（如 -rfv, -r -f 等）
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            for (int j = 1; argv[i][j] != '\0'; j++)
            {
                switch (argv[i][j])
                {
                case 'r':
                    recursive = true;
                    break;
                case 'f':
                    force = true;
                    break;
                case 'v':
                    verbose = true;
                    break;
                default:
                    PRINT_ERROR("Unknown option '-%c'", argv[i][j]);
                    return 1;
                }
            }
            first_src = i + 1; // 下一个参数才是源
        }
        else
        {
            break; // 遇到非选项参数，停止解析选项
        }
    }

    // 至少需要两个参数：源和目标
    if (argc - first_src < 2)
    {
        PRINT_ERROR("Usage: cp [-r] [-f] [-v] <source...> <dest>");
        return 1;
    }

    // 最后一个参数是目标
    const char *dest = argv[argc - 1];
    char dest_buf[512];
    strncpy(dest_buf, dest, sizeof(dest_buf) - 1);
    dest_buf[sizeof(dest_buf) - 1] = '\0';
    strip_trailing_slash(dest_buf); // 去除末尾 '/'

    // 判断目标是否存在、是否为目录
    struct stat st_dest;
    bool dest_exists = (stat(dest_buf, &st_dest) == 0);
    bool dest_is_dir = dest_exists && S_ISDIR(st_dest.st_mode);

    // 多个源文件时，目标必须是已存在的目录
    int num_sources = argc - 1 - first_src;
    if (num_sources > 1 && !(dest_exists && dest_is_dir))
    {
        PRINT_ERROR("Target '%s' is not a directory (multiple sources require a directory target)", dest_buf);
        return 1;
    }

    int ret = 0;
    // 遍历所有源
    for (int i = first_src; i < argc - 1; i++)
    {
        char src_buf[512];
        strncpy(src_buf, argv[i], sizeof(src_buf) - 1);
        src_buf[sizeof(src_buf) - 1] = '\0';
        strip_trailing_slash(src_buf);

        struct stat st_src;
        if (stat(src_buf, &st_src) != 0)
        {
            PRINT_ERROR("Source '%s' does not exist", src_buf);
            ret = 1;
            continue;
        }

        // 构造实际目标路径
        char actual_dst[512];
        if (dest_is_dir)
        {
            // 目标为目录：将源的文件/目录名拼接到目标路径下
            const char *base = strrchr(src_buf, '/');
            base = base ? base + 1 : src_buf;
            snprintf(actual_dst, sizeof(actual_dst), "%s/%s", dest_buf, base);
        }
        else
        {
            // 目标非目录（且源只有一个）
            strncpy(actual_dst, dest_buf, sizeof(actual_dst) - 1);
            actual_dst[sizeof(actual_dst) - 1] = '\0';
        }

        if (S_ISDIR(st_src.st_mode))
        {
            if (!recursive)
            {
                PRINT_ERROR("'%s' is a directory (use -r to copy recursively)", src_buf);
                ret = 1;
                continue;
            }
            if (cp_dir(src_buf, actual_dst, force, verbose) != 0)
            {
                PRINT_ERROR("Failed to copy directory '%s'", src_buf);
                ret = 1;
            }
            else
            {
                if (verbose || !force) // 通常 cp -v 才打印，但成功消息保留
                    PRINT_SUCCESS("Copied directory '%s' -> '%s'", src_buf, actual_dst);
            }
        }
        else
        {
            // 普通文件
            if (cp_file(src_buf, actual_dst, force, verbose) != 0)
            {
                PRINT_ERROR("Failed to copy file '%s'", src_buf);
                ret = 1;
            }
            else
            {
                if (verbose)
                    PRINT_SUCCESS("Copied file '%s' -> '%s'", src_buf, actual_dst);
            }
        }
    }

    return ret;
}

// ==================== du 命令 ====================
static int cmd_du(int argc, char **argv)
{
    bool human = false;
    bool summarize = false;
    int first_path = 1;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            for (int j = 1; argv[i][j] != '\0'; j++)
            {
                switch (argv[i][j])
                {
                case 'h':
                    human = true;
                    break;
                case 's':
                    summarize = true;
                    break;
                default:
                    PRINT_ERROR("Unknown option '-%c'", argv[i][j]);
                    return 1;
                }
            }
            first_path = i + 1;
        }
        else
        {
            break;
        }
    }

    // 无参数：自动扫描挂载点，以汇总模式显示每个挂载点总大小
    if (first_path >= argc)
    {
        scan_mount_points();
        if (num_mount_points == 0)
        {
            PRINT_ERROR("No mount points found. Please specify path.");
            return 1;
        }

        for (int i = 0; i < num_mount_points; i++)
        {
            char path_buf[64];
            strncpy(path_buf, mount_points[i], sizeof(path_buf) - 1);
            path_buf[sizeof(path_buf) - 1] = '\0';
            strip_trailing_slash(path_buf);

            uint64_t total = du_dir(path_buf, human, true); // 强制汇总模式
            char size_str[16];
            if (human)
            {
                human_readable_size(total, size_str, sizeof(size_str));
            }
            else
            {
                snprintf(size_str, sizeof(size_str), "%" PRIu64, total);
            }
            PRINT_INFO("%-8s %s", size_str, path_buf);
        }
        return 0;
    }

    // 有参数：处理每个路径
    int ret = 0;
    for (int i = first_path; i < argc; i++)
    {
        char path_buf[256];
        strncpy(path_buf, argv[i], sizeof(path_buf) - 1);
        path_buf[sizeof(path_buf) - 1] = '\0';
        strip_trailing_slash(path_buf);

        struct stat st;
        if (stat(path_buf, &st) != 0)
        {
            PRINT_ERROR("Path does not exist: %s", path_buf);
            ret = 1;
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            uint64_t total = du_dir(path_buf, human, summarize);

            // 顶层目录的大小由这里统一打印（du_dir 内部只打印子目录）
            char size_str[16];
            if (human)
            {
                human_readable_size(total, size_str, sizeof(size_str));
            }
            else
            {
                snprintf(size_str, sizeof(size_str), "%" PRIu64, total);
            }
            PRINT_INFO("%-8s %s", size_str, path_buf);
        }
        else
        {
            // 普通文件：直接显示大小
            char size_str[16];
            if (human)
            {
                human_readable_size((uint64_t)st.st_size, size_str, sizeof(size_str));
            }
            else
            {
                snprintf(size_str, sizeof(size_str), "%" PRIu64, (uint64_t)st.st_size);
            }
            PRINT_INFO("%-8s %s", size_str, path_buf);
        }
    }

    return ret;
}

// 将 32 位无符号整数转换为二进制字符串（格式：每组4位用空格分隔，提高可读性）
static void uint32_to_bin_str(uint32_t val, char *buf, size_t buf_size)
{
    if (buf_size < 40)
        return; // 确保缓冲区足够（32位+7空格+结尾=40）
    int idx = 0;
    for (int i = 31; i >= 0; i--)
    {
        buf[idx++] = (val & (1 << i)) ? '1' : '0';
        if (i % 4 == 0 && i != 0)
        {
            buf[idx++] = ' '; // 每4位加空格
        }
    }
    buf[idx] = '\0';
}

// ==================== espreg 命令 ====================
// 用法：
//   espreg -r <address> [-bit <bit>]
//   espreg -w <address> [-bit <bit>] <value>
static int cmd_espreg(int argc, char **argv)
{
    if (argc < 3)
    {
        return 1; // 参数不足
    }

    // 解析操作类型
    bool is_read = false;
    if (strcmp(argv[1], "-r") == 0)
    {
        is_read = true;
    }
    else if (strcmp(argv[1], "-w") == 0)
    {
        is_read = false;
    }
    else
    {
        PRINT_ERROR("Invalid operation: must be -r or -w");
        return 1;
    }

    // 解析地址
    char *endptr;
    uint32_t addr = strtoul(argv[2], &endptr, 0);
    if (*endptr != '\0')
    {
        PRINT_ERROR("Invalid address: %s", argv[2]);
        return 1;
    }
    // 地址对齐检查（警告但继续）
    if (addr & 0x3)
    {
        PRINT_WARNING("Address 0x%08X is not 32-bit aligned, access may cause exception", addr);
    }

    // 解析可选参数 -bit
    int bit_pos = -1;
    int value_arg_idx = 3;
    if (argc >= 5 && strcmp(argv[3], "-bit") == 0)
    {
        bit_pos = atoi(argv[4]);
        if (bit_pos < 0 || bit_pos > 31)
        {
            PRINT_ERROR("Bit position must be between 0 and 31");
            return 1;
        }
        value_arg_idx = 5;
    }

    // 读操作
    if (is_read)
    {
        if (argc != value_arg_idx)
        {
            return 1; // 多余参数
        }
        uint32_t val = REG_READ(addr);
        if (bit_pos >= 0)
        {
            int bit_val = (val >> bit_pos) & 1;
            PRINT_INFO("Register 0x%08X bit %d = %d", addr, bit_pos, bit_val);
        }
        else
        {
            char bin_str[64];
            uint32_to_bin_str(val, bin_str, sizeof(bin_str));
            PRINT_INFO("Register 0x%08X = 0x%08X (bin: %s)", addr, val, bin_str);
        }
        return 0;
    }

    // 写操作
    if (!is_read)
    {
        if (argc != value_arg_idx + 1)
        {
            return 1; // 缺少 value 参数
        }
        uint32_t new_val = strtoul(argv[value_arg_idx], &endptr, 0);
        if (*endptr != '\0')
        {
            PRINT_ERROR("Invalid value: %s", argv[value_arg_idx]);
            return 1;
        }

        uint32_t old_val = REG_READ(addr);
        uint32_t write_val;

        if (bit_pos >= 0)
        {
            if (new_val != 0 && new_val != 1)
            {
                PRINT_ERROR("Bit value must be 0 or 1 when using -bit");
                return 1;
            }
            write_val = old_val;
            if (new_val)
            {
                write_val |= (1 << bit_pos);
            }
            else
            {
                write_val &= ~(1 << bit_pos);
            }
            PRINT_INFO("Modifying bit %d of register 0x%08X from %d to %d",
                       bit_pos, addr, (old_val >> bit_pos) & 1, (int)new_val);
        }
        else
        {
            write_val = new_val;
            PRINT_INFO("Writing 0x%08X to register 0x%08X (old: 0x%08X)",
                       write_val, addr, old_val);
        }

        REG_WRITE(addr, write_val);
        uint32_t verify_val = REG_READ(addr);
        if (verify_val == write_val)
        {
            PRINT_SUCCESS("Register updated successfully");
        }
        else
        {
            PRINT_ERROR("Register write verification failed! Read back 0x%08X", verify_val);
            return 2;
        }
        return 0;
    }

    return 0;
}

// ==================== display 命令（底层调试） ====================
// 用法：display <cmd> [data...]
//   cmd  : 命令字节（支持十进制或十六进制，如 42 或 0x2A）
//   data : 可选的若干个数据字节（同样支持十进制或十六进制）

static int cmd_display(int argc, char **argv)
{
    if (argc < 2)
    {
        return 1; // 参数不足，显示帮助
    }

    // 1. 解析命令字节
    char *endptr;
    unsigned long cmd_val = strtoul(argv[1], &endptr, 0);
    if (*endptr != '\0' || cmd_val > 0xFF)
    {
        PRINT_ERROR("Invalid command byte: %s (must be 0-255)", argv[1]);
        return 1;
    }
    uint8_t cmd_byte = (uint8_t)cmd_val;

    // 2. 准备数据字节数组（最多 argc-2 个）
    int data_count = argc - 2;
    uint8_t *data_bytes = nullptr;
    if (data_count > 0)
    {
        data_bytes = (uint8_t *)malloc(data_count);
        if (!data_bytes)
        {
            PRINT_ERROR("Failed to allocate memory for data bytes");
            return 2;
        }
    }

    // 3. 解析每个数据字节
    for (int i = 0; i < data_count; i++)
    {
        unsigned long val = strtoul(argv[2 + i], &endptr, 0);
        if (*endptr != '\0' || val > 0xFF)
        {
            PRINT_ERROR("Invalid data byte at position %d: %s (must be 0-255)", i + 1, argv[2 + i]);
            if (data_bytes)
                free(data_bytes);
            return 1;
        }
        data_bytes[i] = (uint8_t)val;
    }

    // 4. 加锁并发送命令/数据
    display.spi_lock(); // 假设 display 对象提供了公共的锁方法

    PRINT_INFO("Sending command: 0x%02X (%d)", cmd_byte, cmd_byte);
    display.sendCommand(cmd_byte);

    for (int i = 0; i < data_count; i++)
    {
        PRINT_INFO("  data[%d]: 0x%02X (%d)", i, data_bytes[i], data_bytes[i]);
        display.sendData(data_bytes[i]);
    }

    display.spi_unlock();

    if (data_bytes)
        free(data_bytes);

    PRINT_SUCCESS("Command sent successfully");
    return 0;
}

static int cmd_fontinfo(int argc, char **argv)
{
    u8g2_font_t *u8g2 = u8g2Fonts.getU8g2();
    if (!u8g2)
    {
        PRINT_ERROR("No active u8g2 font object");
        return 2;
    }

    const u8g2_font_info_t *info = &u8g2->font_info;
    PRINT_INFO("=== U8g2 Font Information ===");
    PRINT_INFO("Font pointer        : 0x%p", u8g2->font);
    PRINT_INFO("Glyph count         : %5u", info->glyph_cnt);
    PRINT_INFO("BBX mode            : %5u", info->bbx_mode);
    PRINT_INFO("Bits per 0          : %5u", info->bits_per_0);
    PRINT_INFO("Bits per 1          : %5u", info->bits_per_1);
    PRINT_INFO("Bits per char width : %5u", info->bits_per_char_width);
    PRINT_INFO("Bits per char height: %5u", info->bits_per_char_height);
    PRINT_INFO("Bits per char x     : %5u", info->bits_per_char_x);
    PRINT_INFO("Bits per char y     : %5u", info->bits_per_char_y);
    PRINT_INFO("Bits per delta x    : %5u", info->bits_per_delta_x);
    PRINT_INFO("Max char width      : %5d", info->max_char_width);
    PRINT_INFO("Max char height     : %5d", info->max_char_height);
    PRINT_INFO("X offset            : %5d", info->x_offset);
    PRINT_INFO("Y offset            : %5d", info->y_offset);
    PRINT_INFO("Ascent 'A'          : %5d", info->ascent_A);
    PRINT_INFO("Descent 'g'         : %5d", info->descent_g);
    PRINT_INFO("Ascent paragraph    : %5d", info->ascent_para);
    PRINT_INFO("Descent paragraph   : %5d", info->descent_para);
    PRINT_INFO("Start pos upper A   : 0x%04X", info->start_pos_upper_A);
    PRINT_INFO("Start pos lower a   : 0x%04X", info->start_pos_lower_a);
    PRINT_INFO("Start pos unicode   : 0x%04X", info->start_pos_unicode);

    // 额外打印解码结构的部分关键信息（可选）
    PRINT_INFO("Decode transparency: %s", u8g2->font_decode.is_transparent ? "yes" : "no");
    PRINT_INFO("Decode direction   : %d", u8g2->font_decode.dir);

    return 0;
}

static int cmd_buildfontwidthtable(int argc, char **argv)
{
    uint8_t *width_table = (uint8_t *)heap_caps_malloc(sizeof(uint8_t[0xFFFF]), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    PRINT_INFO("Font width table built in PSRAM at address: %p", width_table);
    u8g2Fonts.BuildfontWidthTable(width_table, 0xFFFF);
    String width_table_path = hal.pref.getString("system_font", "default") + ".width_table";
    File file = hal.open(width_table_path.c_str(), "w");
    if (file)
    {
        file.write(width_table, sizeof(uint8_t[0xFFFF]));
        file.close();
        PRINT_SUCCESS("Font width table saved to %s", width_table_path.c_str());
    }
    else
    {
        PRINT_ERROR("Failed to save font width table to file: %s", width_table_path.c_str());
    }
    return 0;
}

extern void lua_execute(const char *filename, int argc, const char **argv);

static int cmd_luarun(int argc, char **argv)
{
    // 至少需要一个参数：文件名
    if (argc < 2)
    {
        return 1; // 返回 1 触发帮助信息
    }

    const char *filename = argv[1];

    // 将命令行参数（跳过文件名本身）传递给 Lua
    // argc - 2 是实际参数个数，argv + 2 是参数数组起始位置
    lua_execute(filename, argc - 2, (const char **)(argv + 2));

    // 注意：lua_execute 内部已打印错误，这里直接返回成功
    return 0;
}

extern void printAppList();

static int cmd_applist(int argc, char **argv)
{
    printAppList();
    return 0;
}

static int cmd_about(int argc, char **argv)
{
    assert(false);
    return 0;
}

static int cmd_uart_baudset(int argc, char **argv)
{
    if (argc < 2)
    {
        return 1; // 返回 1 触发帮助信息
    }
    uint32_t baud, last_baud;
    parseUnsigned(argv[1], baud);
    last_baud = uart->baudRate();
    uart->updateBaudRate(baud);
    log_i("UART baud rate changed %lu -> %lu", last_baud, baud);
    // uart->end();
    // uart->setRxBufferSize(4096);
    // uart->begin(band);
    // uart->setDebugOutput(true);
    // cmd.SetCallback();
    // reinstall_putc2();
    // reinstall_ws_putc2();
    return 0;
}

// ESP32-S3 RTC SLOW Memory 地址范围（ULP 所用）
#define RTC_SLOW_MEM_START 0x50000000
#define RTC_SLOW_MEM_END 0x50001FFF // 8KB
#define RTC_SLOW_MEM_SIZE (8 * 1024)

/**
 * 检查地址是否在 RTC SLOW Memory 范围内
 */
static bool is_rtc_slow_addr(uint32_t addr)
{
    return (addr >= RTC_SLOW_MEM_START && addr <= RTC_SLOW_MEM_END);
}

/**
 * 以 hex 形式打印 RTC SLOW 内存区域
 */
static void dump_rtc_slow_memory(uint32_t start_addr, size_t length)
{
    const uint8_t *base = (const uint8_t *)start_addr;
    size_t offset = 0;

    while (offset < length)
    {
        // 地址列
        log_printf("%s0x%08X:%s ", INFO_COLOR, start_addr + offset, RESET_COLOR);

        // 打印 16 个 hex 字节
        for (int i = 0; i < 16; i++)
        {
            if (offset + i < length)
            {
                log_printf("%02X ", base[offset + i]);
            }
            else
            {
                log_printf("   "); // 不足时补空格
            }
            if (i == 7)
            {
                log_printf(" "); // 中间分隔
            }
        }

        // ASCII 表示
        log_printf(" ");
        for (int i = 0; i < 16 && offset + i < length; i++)
        {
            uint8_t c = base[offset + i];
            log_printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
        }
        log_printf("\n");
        offset += 16;
    }
}

/**
 * ULP dump 子命令处理
 * 用法: ulp dump <address> <length>
 *   address : RTC SLOW Memory 地址（十六进制，如 0x50000100）
 *   length  : 要 dump 的字节数（十进制）
 * 返回值: 0-成功, 1-参数错误, 2-执行失败
 */
static int cmd_ulp_dump(int argc, char **argv)
{
    if (argc < 3)
    {
        PRINT_ERROR("Missing arguments. Usage: ulp dump <address> <length>");
        return 1;
    }

    // 解析 address
    char *endptr;
    uint32_t start_addr = strtoul(argv[2], &endptr, 0);
    if (*endptr != '\0')
    {
        PRINT_ERROR("Invalid address: %s", argv[2]);
        return 1;
    }

    // 解析 length（十进制）
    size_t length = strtoul(argv[3], &endptr, 10);
    if (*endptr != '\0')
    {
        PRINT_ERROR("Invalid length: %s", argv[3]);
        return 1;
    }

    if (length == 0)
    {
        PRINT_ERROR("Length must be > 0");
        return 1;
    }

    // 地址有效性检查
    if (!is_rtc_slow_addr(start_addr))
    {
        PRINT_ERROR("Address 0x%08X is outside RTC SLOW Memory (0x%08X - 0x%08X)",
                    start_addr, RTC_SLOW_MEM_START, RTC_SLOW_MEM_END);
        return 2;
    }

    // 长度边界检查（不能超出 RTC SLOW 末尾）
    if (start_addr + length - 1 > RTC_SLOW_MEM_END)
    {
        PRINT_ERROR("Requested range 0x%08X + %zu bytes exceeds RTC SLOW Memory (size %d KB)",
                    start_addr, length, RTC_SLOW_MEM_SIZE / 1024);
        return 2;
    }

    // 执行 dump
    PRINT_INFO("Dumping RTC SLOW Memory from 0x%08X, %zu bytes:", start_addr, length);
    dump_rtc_slow_memory(start_addr, length);
    return 0;
}

/**
 * ULP 命令处理函数（主命令）
 * 用法: ulp run | stop | reset | halt | load <filepath>
 * 返回值:
 *   0 - 成功
 *   1 - 参数错误
 *   2 - 执行失败
 */
int ulp_cmd(int argc, char **argv)
{
    // 检查是否至少有一个子命令参数
    if (argc < 2)
    {
        PRINT_ERROR("Missing subcommand. Usage: ulp run|stop|reset|halt|load <filepath>");
        return 1;
    }

    const char *sub = argv[1];
    esp_err_t err;

    // ---------- run ----------
    if (strcmp(sub, "run") == 0)
    {
        err = ulp_riscv_run();
        if (err != ESP_OK)
        {
            PRINT_ERROR("ulp_riscv_run failed: %s", esp_err_to_name(err));
            return 2;
        }
        PRINT_SUCCESS("ULP RISC-V started");
        return 0;
    }

    // ---------- stop ----------
    if (strcmp(sub, "stop") == 0)
    {
        ulp_riscv_timer_stop();
        PRINT_INFO("ULP timer stopped");
        return 0;
    }

    // ---------- reset ----------
    if (strcmp(sub, "reset") == 0)
    {
        ulp_riscv_reset();
        PRINT_INFO("ULP reset");
        return 0;
    }

    // ---------- halt ----------
    if (strcmp(sub, "halt") == 0)
    {
        ulp_riscv_halt();
        PRINT_INFO("ULP halted");
        return 0;
    }

    // ---------- dump ----------
    if (strcmp(sub, "dump") == 0)
    {
        return cmd_ulp_dump(argc, argv); // 注意：传入完整 argc/argv
    }

    // ---------- load ----------
    if (strcmp(sub, "load") == 0)
    {
        // 检查是否提供了文件路径
        if (argc < 3)
        {
            PRINT_ERROR("Missing filepath for load subcommand");
            return 1;
        }
        const char *filepath = argv[2];

        // 打开文件
        int fd = open(filepath, O_RDONLY);
        if (fd < 0)
        {
            PRINT_ERROR("Cannot open file: %s", filepath);
            return 2;
        }

        // 获取文件大小
        struct stat st;
        if (fstat(fd, &st) != 0)
        {
            PRINT_ERROR("fstat failed for %s", filepath);
            close(fd);
            return 2;
        }
        size_t size = st.st_size;
        if (size == 0)
        {
            PRINT_ERROR("File is empty: %s", filepath);
            close(fd);
            return 2;
        }

        // 分配内存读取文件
        uint8_t *buffer = (uint8_t *)malloc(size);
        if (!buffer)
        {
            PRINT_ERROR("Out of memory (size %zu)", size);
            close(fd);
            return 2;
        }

        ssize_t read_bytes = read(fd, buffer, size);
        close(fd);
        if (read_bytes != (ssize_t)size)
        {
            PRINT_ERROR("Read error: only %ld of %zu bytes read from %s", read_bytes, size, filepath);
            free(buffer);
            return 2;
        }

        // 加载二进制到 ULP
        err = ulp_riscv_load_binary(buffer, size);
        free(buffer);
        if (err != ESP_OK)
        {
            PRINT_ERROR("ulp_riscv_load_binary failed: %s", esp_err_to_name(err));
            return 2;
        }

        PRINT_SUCCESS("ULP binary loaded from %s (%zu bytes)", filepath, size);
        return 0;
    }

    // ---------- 未知子命令 ----------
    PRINT_ERROR("Unknown subcommand: '%s'. Valid: run, stop, reset, halt, load", sub);
    return 1;
}

/**
 * @brief hexdump 命令实现
 * 用法: hexdump <file_path> [-o offset] [-n length]
 *   - file_path : 文件路径（必需）
 *   - -o offset : 起始偏移（可选，默认 0，支持十进制或十六进制如 0x100）
 *   - -n length : 要打印的字节数（可选，默认打印到文件末尾）
 *
 * 示例:
 *   hexdump file.bin               # 打印整个文件
 *   hexdump file.bin -o 0x100      # 从 0x100 开始打印到末尾
 *   hexdump file.bin -n 256        # 从 0 开始打印 256 字节
 *   hexdump file.bin -o 0x100 -n 128 # 从 0x100 开始打印 128 字节
 */
static int cmd_hexdump(int argc, char **argv)
{
    if (argc < 2) {
        PRINT_ERROR("Missing file path");
        return 1;
    }

    const char *filepath = NULL;
    size_t offset = 0;
    size_t length = 0;           // 0 表示打印到文件末尾
    bool has_length = false;     // 是否明确指定了长度

    // 手动解析命令行选项（支持 -o 和 -n）
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-o") == 0) {
            if (++i >= argc) {
                PRINT_ERROR("Missing offset after -o");
                return 1;
            }
            char *endptr;
            offset = strtoul(argv[i], &endptr, 0);
            if (*endptr != '\0') {
                PRINT_ERROR("Invalid offset: %s", argv[i]);
                return 1;
            }
        } else if (strcmp(arg, "-n") == 0) {
            if (++i >= argc) {
                PRINT_ERROR("Missing length after -n");
                return 1;
            }
            char *endptr;
            length = strtoul(argv[i], &endptr, 0);
            if (*endptr != '\0' || length == 0) {
                PRINT_ERROR("Invalid length (must be > 0): %s", argv[i]);
                return 1;
            }
            has_length = true;
        } else {
            // 非选项参数视为文件路径（只允许一个）
            if (filepath == NULL) {
                filepath = arg;
            } else {
                PRINT_ERROR("Unexpected extra argument: %s", arg);
                return 1;
            }
        }
    }

    if (filepath == NULL) {
        PRINT_ERROR("File path not specified");
        return 1;
    }

    // 打开文件
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        PRINT_ERROR("Failed to open file '%s': %s", filepath, strerror(errno));
        return 2;
    }

    // 分配大缓冲区（使用 SPIRAM 和 DMA 能力）
    char *buffer = NULL;
    buffer = (char *)heap_caps_aligned_alloc(32, 1024 * 8, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (buffer) {
        setvbuf(f, buffer, _IOFBF, 1024 * 8);
    }

    // 获取文件大小
    if (fseek(f, 0, SEEK_END) != 0) {
        PRINT_ERROR("Failed to seek to end: %s", strerror(errno));
        fclose(f);
        if (buffer) heap_caps_free(buffer);
        return 2;
    }
    long file_size = ftell(f);
    if (file_size < 0) {
        PRINT_ERROR("Failed to get file size");
        fclose(f);
        if (buffer) heap_caps_free(buffer);
        return 2;
    }
    rewind(f);

    // 检查偏移是否超出文件大小
    if (offset > (size_t)file_size) {
        PRINT_ERROR("Offset 0x%08zx exceeds file size 0x%08lx", offset, file_size);
        fclose(f);
        if (buffer) heap_caps_free(buffer);
        return 2;
    }

    // 如果未指定长度，则打印到文件末尾
    if (!has_length) {
        length = (size_t)(file_size - offset);
    } else {
        // 限制长度不超过文件剩余部分（避免无效读取）
        size_t remaining = (size_t)(file_size - offset);
        if (length > remaining) {
            length = remaining;
        }
    }

    // 跳转到偏移
    if (fseek(f, offset, SEEK_SET) != 0) {
        PRINT_ERROR("Failed to seek to offset 0x%08zx: %s", offset, strerror(errno));
        fclose(f);
        if (buffer) heap_caps_free(buffer);
        return 2;
    }

    // 逐行读取并打印（每次最多 16 字节）
    size_t remaining = length;
    size_t addr = offset;
    uint8_t buf[16];

    while (remaining > 0) {
        size_t read_len = (remaining > 16) ? 16 : remaining;
        size_t actual = fread(buf, 1, read_len, f);
        if (actual == 0) {
            if (feof(f)) {
                break; // 文件结束
            } else {
                PRINT_ERROR("Read error at offset 0x%08zx", addr);
                fclose(f);
                if (buffer) heap_caps_free(buffer);
                return 2;
            }
        }

        // 打印地址列
        log_printf("%s0x%08X:%s ", INFO_COLOR, (uint32_t)addr, RESET_COLOR);

        // 打印 16 个 hex 字节
        for (int i = 0; i < 16; i++) {
            if (i < actual) {
                log_printf("%02X ", buf[i]);
            } else {
                log_printf("   "); // 不足补空格
            }
            if (i == 7) {
                log_printf(" "); // 中间分隔
            }
        }

        // 打印 ASCII 表示
        log_printf(" ");
        for (size_t i = 0; i < actual; i++) {
            uint8_t c = buf[i];
            log_printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
        }
        log_printf("\n");

        // 更新计数
        remaining -= actual;
        addr += actual;

        // 如果读取的字节少于请求的（文件提前结束），退出循环
        if (actual < read_len) {
            break;
        }
    }

    fclose(f);
    if (buffer) heap_caps_free(buffer);
    return 0;
}

// 命令注册表
// Register all console commands defined in this file
static const char *no_info = "";
static const esp_console_cmd_t cmds[] = {
    {.command = "cpufreq", .help = "获取当前CPU频率或设置频率", .hint = "Usage: cpufreq [freq]\n  freq: 240,160,80", .func = &cmd_cpufreq, .argtable = NULL},
    {.command = "longpress", .help = "设置长按检测时间（X10ms）", .hint = "Usage: longpress [value]", .func = &cmd_longpress, .argtable = NULL},
    {.command = "heap", .help = "显示堆内存使用情况", .hint = no_info, .func = &cmd_heap, .argtable = NULL},
    {.command = "heapdump",
     .help = "Dump heap structure to file (PSRAM buffer 128KB)",
     .hint = "Usage: heapdump <type> <file>\n"
             "  type: internal, spiram, dma, exec, 8bit, default, retention, rtcram\n"
             "        or numeric caps mask\n"
             "  file: output path, e.g. /littlefs/heap.txt",
     .func = &cmd_heapdump,
     .argtable = NULL},
    {.command = "cfgcpufreq", .help = "配置CPU频率（重启后生效）", .hint = "Usage: cfgcpufreq <freq>\n  freq: 240,160,80", .func = &cmd_cfgcpufreq, .argtable = NULL},
    {.command = "erasenvs", .help = "擦除所有NVS数据并重启", .hint = "谨慎操作", .func = &cmd_erasenvs, .argtable = NULL},
    {.command = "lfsformat", .help = "格式化LittleFS文件系统", .hint = "谨慎操作", .func = &cmd_lfsformat, .argtable = NULL},
    {.command = "lfsinfo", .help = "显示LittleFS文件系统信息", .hint = no_info, .func = &cmd_lfsinfo, .argtable = NULL},
    {.command = "chipinfo", .help = "显示芯片信息", .hint = no_info, .func = &cmd_chipinfo, .argtable = NULL},
    {.command = "rst", .help = "重启设备", .hint = no_info, .func = &cmd_rst, .argtable = NULL},
    {.command = "runtime", .help = "显示设备运行时间", .hint = no_info, .func = &cmd_runtime, .argtable = NULL},
    {.command = "batinfo", .help = "显示电池信息", .hint = no_info, .func = &cmd_batinfo, .argtable = NULL},
    {.command = "taskstats", .help = "打印各任务CPU占用率（需configGENERATE_RUN_TIME_STATS=1）", .hint = no_info, .func = &cmd_taskstats, .argtable = NULL},
    {.command = "tasklist", .help = "打印任务列表及栈信息（vTaskList）", .hint = no_info, .func = &cmd_tasklist, .argtable = NULL},
    {.command = "taskload", .help = "显示最近N秒的CPU占用率(默认2秒)", .hint = "Usage: taskload [seconds]", .func = &cmd_taskload, .argtable = NULL},
    {.command = "taskctrl", .help = "任务控制", .hint = "Usage: taskctrl <down|up|del> <task_name>", .func = &cmd_taskctrl, .argtable = NULL},
    {.command = "bootapp_clock", .help = "设置默认启动应用为clock", .hint = no_info, .func = &cmd_bootapp_clock, .argtable = NULL},
    {.command = "lightsleep", .help = "进入轻睡眠模式，可选超时", .hint = "Usage: lightsleep [timeout]", .func = &cmd_lightsleep, .argtable = NULL},
    {.command = "fserverbegin", .help = "启动文件服务器", .hint = no_info, .func = &cmd_fserverbegin, .argtable = NULL},
    {.command = "fserverend", .help = "停止文件服务器", .hint = no_info, .func = &cmd_fserverend, .argtable = NULL},
    {.command = "partitioninfo", .help = "显示分区信息", .hint = no_info, .func = &cmd_partitioninfo, .argtable = NULL},
    {.command = "putnvs", .help = "写入NVS键值", .hint = "Usage: putnvs <key> <value> [type]\n  type: bool, int, uint, i8, u8, i16, u16, i32, u32, i64, u64, float, double, string (omit for auto-detect)", .func = &cmd_putnvs, .argtable = NULL},
    {.command = "getnvs", .help = "读取NVS键值", .hint = "Usage: getnvs <key> [type]\n  type: bool, int, uint, i8, u8, i16, u16, i32, u32, i64, u64, float, double, string (omit for auto-detect)", .func = &cmd_getnvs, .argtable = NULL},
    {.command = "rmnvs", .help = "删除NVS键值对", .hint = "Usage: rmnvs <key>", .func = &cmd_rmnvs, .argtable = NULL},
    {.command = "display_debug", .help = "控制屏幕驱动debug输出", .hint = no_info, .func = &cmd_display_debug, .argtable = NULL},
    {.command = "filetaskinfo", .help = "显示文件写入任务的队列情况", .hint = no_info, .func = &cmd_file_task_info, .argtable = NULL},
    {.command = "download", .help = "从网络下载文件", .hint = "Usage: download <ca_id> <url> <save_file>", .func = &cmd_download, .argtable = NULL},
    {.command = "mv", .help = "移动/重命名文件或目录", .hint = "Usage: mv <source> <dest>", .func = &cmd_mv, .argtable = NULL},
    {.command = "ls", .help = "列出目录", .hint = "Usage: ls [path]", .func = &cmd_ls, .argtable = NULL},
    {.command = "cat", .help = "输出文件的内容", .hint = "Usage: cat <path>", .func = &cmd_cat, .argtable = NULL},
    {.command = "rm", .help = "删除文件或目录", .hint = "Usage: rm [-r] <path>", .func = &cmd_rm, .argtable = NULL},
    {.command = "echo", .help = "写入或追加文件", .hint = "Usage: echo <text> [>|>> <file>]", .func = &cmd_echo, .argtable = NULL},
    {.command = "mkdir", .help = "新建文件夹", .hint = "Usage: mkdir <path>", .func = &cmd_mkdir, .argtable = NULL},
    {.command = "cp", .help = "复制文件或文件夹", .hint = "cp [-r] [-f] [-v] <source...> <dest>", .func = &cmd_cp, .argtable = NULL},
    {.command = "du", .help = "显示文件夹的大小", .hint = "du [-h] [-s] [<path> ...]", .func = &cmd_du, .argtable = NULL},
    {.command = "espreg", .help = "读取或写入寄存器（支持位操作）", .hint = "Usage: espreg <-r|-w> <address> [-bit <bit>] [value]\n"
                                                                            "  -r: read register or specific bit\n"
                                                                            "  -w: write register or modify specific bit\n"
                                                                            "  address: hex or decimal (e.g., 0x3FF44020)\n"
                                                                            "  -bit: optional bit position (0-31)\n"
                                                                            "  value: for -w: full register value, or 0/1 when -bit is used",
     .func = &cmd_espreg,
     .argtable = NULL},
    {.command = "wsconsole",
     .help = "Start/stop WebSocket console (port 81)",
     .hint = "Usage: wsconsole <start|stop>",
     .func = &cmd_wsconsole,
     .argtable = NULL},
    {.command = "display",
     .help = "Send raw command/data to display (debugging)",
     .hint = "Usage: display <cmd> [data...]\n"
             "  cmd  : command byte (decimal or hex, e.g. 42 or 0x2A)\n"
             "  data : optional data bytes (multiple values allowed)",
     .func = &cmd_display,
     .argtable = NULL},
    {.command = "fontinfo", .help = "显示当前u8g2字体的详细信息", .hint = no_info, .func = &cmd_fontinfo, .argtable = NULL},
    {.command = "buildfontwidthtable", .help = "构建当前字体的宽度表并保存到文件", .hint = no_info, .func = &cmd_buildfontwidthtable, .argtable = NULL},
    {.command = "luarun",
     .help = "执行 Lua 脚本文件，支持传递参数",
     .hint = "Usage: luarun <filename> [arg1] [arg2] ...",
     .func = &cmd_luarun,
     .argtable = NULL},
    {.command = "applist", .help = "显示所有已注册的app", .hint = no_info, .func = &cmd_applist, .argtable = NULL},
    {.command = "about", .help = "中断当前的运行", .hint = no_info, .func = &cmd_about, .argtable = NULL},
    {.command = "setbaud", .help = "重置串口波特率", .hint = "Usage: setbaud [cmd]", .func = &cmd_uart_baudset, .argtable = NULL},
    {.command = "setcpuperiod", .help = "修改SYSTEM_CPUPERIOD_SEL的值", .hint = no_info, .func = &cmd_cpufreq_reg, .argtable = NULL},
    {
        .command = "ulp",
        .help = "ULP RISC-V control commands",
        .hint = "run | stop | reset | halt | load <filepath>",
        .func = ulp_cmd,
    },
    {
        .command = "hexdump",
        .help = "Hexdump a file",
        .hint = "Usage: hexdump <file_path> [offset] [length]\n"
                "  file_path: path to the file\n"
                "  offset   : starting offset (optional, default 0)\n"
                "  length   : number of bytes to dump (optional, default 256)",
        .func = cmd_hexdump,
    }};

// Custom helper to retrieve the hint string for a given command name.
// Returns the hint pointer from the cmds array, or nullptr if not found.
static const char *find_cmd_hint(const char *cmdline)
{
    // Extract the command name (first token) from cmdline, ignoring any arguments.
    // Find the first whitespace character.
    const char *space = strchr(cmdline, ' ');
    size_t nameLen = space ? (size_t)(space - cmdline) : strlen(cmdline);

    // Iterate over the static cmds array defined above.
    size_t cmdCount = sizeof(cmds) / sizeof(cmds[0]);
    for (size_t i = 0; i < cmdCount; ++i)
    {
        // Compare only the command name length.
        if (strlen(cmds[i].command) == nameLen &&
            strncmp(cmds[i].command, cmdline, nameLen) == 0)
        {
            return cmds[i].hint;
        }
    }
    return nullptr;
}
void CMD::register_commands()
{
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i)
    {
        esp_console_cmd_register(&cmds[i]);
    }
}