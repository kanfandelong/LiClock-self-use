#include "Serial_cmd.h"
#include <nvs_flash.h>
#include "chip-debug-report.h"
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

    // 唤醒 cmd_task
    cmd.run();

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void CMD::begin()
{
    cmdBuffer = (char *)ps_malloc(COMMAND_BUFFER_SIZE);
    // 初始化 esp_console
    esp_console_config_t console_config = {
        .max_cmdline_length = 8192,
        .max_cmdline_args = 32,
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
        vTaskResume(console_task_handle);
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
        if (setCpuFrequencyMhz(freq))
        {
            SUCCESS_COLOR;
            PRINT_INFO("CPU frequency set to %d MHz", freq);
            RESET_COLOR;
        }
        else
        {
            PRINT_ERROR("Failed to set CPU frequency");
            return 2;
        }
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
    PRINT_INFO("  Model:       %s", ESP.getChipModel());
    PRINT_INFO("  Revision:    %u", ESP.getChipRevision());
    PRINT_INFO("  Cores:       %u", ESP.getChipCores());
    uint64_t chipmacid = ESP.getEfuseMac();
    uint8_t *mac = (uint8_t *)&chipmacid;
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    PRINT_INFO("  MAC:         %s", macStr);
    uint64_t unique_id[2];
    esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, unique_id, 128);
    uint8_t *chip_uid[2];
    chip_uid[0] = (uint8_t *)&unique_id[0];
    chip_uid[1] = (uint8_t *)&unique_id[1];
    PRINT_INFO("  Unique ID:   %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               chip_uid[0][0], chip_uid[0][1], chip_uid[0][2], chip_uid[0][3],
               chip_uid[0][4], chip_uid[0][5], chip_uid[0][6], chip_uid[0][7],
               chip_uid[1][0], chip_uid[1][1], chip_uid[1][2], chip_uid[1][3],
               chip_uid[1][4], chip_uid[1][5], chip_uid[1][6], chip_uid[1][7]);
    PRINT_INFO("  Flash size:  %d MB", ESP.getFlashChipSize() / 1048576);
    uint32_t flash_id;
    uint64_t flash_unique_id;
    esp_flash_read_id(esp_flash_default_chip, &flash_id);
    esp_flash_read_unique_chip_id(esp_flash_default_chip, &flash_unique_id);
    PRINT_INFO("  Flash ID:    %04x", flash_id);
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
            PRINT_ERROR("Key '%s' is a BLOB (float/double/bytes). Please specify type explicitly.", key);
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

// 检查路径是否有效（以 /littlefs 或 /sd 开头）
static bool is_valid_root_path(const char *path)
{
    return (strncmp(path, "/littlefs/", 10) == 0 || strncmp(path, "/sd/", 4) == 0 ||
            strcmp(path, "/littlefs") == 0 || strcmp(path, "/sd") == 0);
}

// ==================== ls 命令 ====================
// 用法：
//   ls               - 列出根挂载点 /littlefs 和 /sd
//   ls <path>        - 列出指定目录内容（路径必须完整）
static int cmd_ls(int argc, char **argv)
{
    if (argc == 1)
    {
        // 无参数：列出两个挂载点
        list_dir("/littlefs");
        list_dir("/sd");
        return 0;
    }
    else if (argc == 2)
    {
        const char *path = argv[1];
        if (!is_valid_root_path(path))
        {
            PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
            return 1;
        }
        list_dir(path);
        return 0;
    }
    else
    {
        return 1;
    }
}

// ==================== echo 命令 ====================
// 用法：
//   echo <text>                     - 直接打印文本
//   echo <text> > <file>            - 覆盖写入文件
//   echo <text> >> <file>           - 追加写入文件
static int cmd_echo(int argc, char **argv)
{
    if (argc < 2)
    {
        return 1;
    }

    // 情况1：无重定向
    if (argc == 2)
    {
        PRINT_INFO("%s", argv[1]);
        return 0;
    }

    // 情况2：有重定向，必须满足 argc == 4 且 argv[2] 为 ">" 或 ">>"
    if (argc == 4 && (strcmp(argv[2], ">") == 0 || strcmp(argv[2], ">>") == 0))
    {
        const char *mode = (strcmp(argv[2], ">") == 0) ? "w" : "a";
        const char *file_path = argv[3];

        if (!is_valid_root_path(file_path))
        {
            PRINT_ERROR("Invalid file path: must start with /littlefs/ or /sd/");
            return 1;
        }

        File f = hal.open(file_path, mode, true);
        if (!f)
        {
            PRINT_ERROR("Failed to open file: %s", file_path);
            return 2;
        }

        size_t written = f.print(argv[1]);
        f.close();

        if (written != strlen(argv[1]))
        {
            PRINT_ERROR("Write error to file: %s", file_path);
            return 2;
        }

        PRINT_SUCCESS("Written %u bytes to %s", written, file_path);
        return 0;
    }

    return 1;
}

// ==================== cat 命令 ====================
// 用法：cat <file>
static int cmd_cat(int argc, char **argv)
{
    if (argc != 2)
    {
        return 1;
    }
    const char *path = argv[1];
    if (!is_valid_root_path(path))
    {
        PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
        return 1;
    }

    File f = hal.open(path, "r");
    if (!f)
    {
        PRINT_ERROR("Cannot open file: %s", path);
        return 2;
    }

    // 分块读取并输出
    const size_t buf_size = 512;
    uint8_t buffer[buf_size];
    size_t total = 0;
    while (f.available())
    {
        size_t len = f.read(buffer, buf_size);
        if (len > 0)
        {
            log_printf("%.*s", len, buffer);
            total += len;
        }
    }
    f.close();

    if (total == 0)
    {
        PRINT_WARNING("File is empty: %s", path);
    }
    return 0;
}

// ==================== mkdir 命令 ====================
// 用法：mkdir <dir>
static int cmd_mkdir(int argc, char **argv)
{
    if (argc != 2)
    {
        return 1;
    }
    const char *path = argv[1];
    if (!is_valid_root_path(path))
    {
        PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
        return 1;
    }

    if (hal.exists(path))
    {
        PRINT_ERROR("Path already exists: %s", path);
        return 2;
    }

    if (hal.mkdir(path))
    {
        PRINT_SUCCESS("Directory created: %s", path);
        return 0;
    }
    else
    {
        PRINT_ERROR("Failed to create directory: %s", path);
        return 2;
    }
}

// ==================== rm 命令 ====================
// 用法：
//   rm <file>          - 删除文件
//   rm -r <dir>        - 递归删除目录及其内容
//   rm <empty_dir>     - 删除空目录（需要加 -r 或显式使用 rmdir，这里按 Linux 习惯，rm 默认不删除目录）
static int cmd_rm(int argc, char **argv)
{
    bool recursive = false;
    const char *path = nullptr;

    // 解析参数
    if (argc == 2)
    {
        path = argv[1];
    }
    else if (argc == 3 && strcmp(argv[1], "-r") == 0)
    {
        recursive = true;
        path = argv[2];
    }
    else
    {
        return 1;
    }

    if (!is_valid_root_path(path))
    {
        PRINT_ERROR("Invalid path: must start with /littlefs/ or /sd/");
        return 1;
    }

    // 检查路径是否存在
    if (!hal.exists(path))
    {
        PRINT_ERROR("Path does not exist: %s", path);
        return 2;
    }

    // 打开路径判断类型
    File f = hal.open(path, "r");
    if (!f)
    {
        PRINT_ERROR("Cannot access: %s", path);
        return 2;
    }
    bool is_dir = f.isDirectory();
    f.close();

    if (is_dir)
    {
        if (recursive)
        {
            // 递归删除目录
            hal.rm_rf(path);
            PRINT_SUCCESS("Removed directory recursively: %s", path);
            return 0;
        }
        else
        {
            PRINT_ERROR("'%s' is a directory. Use -r to remove it.", path);
            return 2;
        }
    }
    else
    {
        // 文件：直接删除
        if (hal.remove(path))
        {
            PRINT_SUCCESS("Removed file: %s", path);
            return 0;
        }
        else
        {
            PRINT_ERROR("Failed to remove file: %s", path);
            return 2;
        }
    }
}

// ==================== mv 命令 ====================
// 用法：mv <source> <dest>
// 注意：仅支持同一文件系统内的移动/重命名，跨文件系统会报错。
static int cmd_mv(int argc, char **argv)
{
    if (argc != 3)
    {
        return 1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    if (!is_valid_root_path(src) || !is_valid_root_path(dst))
    {
        PRINT_ERROR("Paths must start with /littlefs/ or /sd/");
        return 1;
    }

    // 检查源是否存在
    if (!hal.exists(src))
    {
        PRINT_ERROR("Source does not exist: %s", src);
        return 2;
    }

    // 检查目标是否已存在（避免覆盖，可按需要修改）
    if (hal.exists(dst))
    {
        PRINT_ERROR("Destination already exists: %s", dst);
        return 2;
    }

    // 尝试重命名/移动
    if (hal.rename(src, dst))
    {
        PRINT_SUCCESS("Moved/Renamed: %s -> %s", src, dst);
        return 0;
    }
    else
    {
        // 可能跨文件系统或权限问题
        PRINT_ERROR("Move failed (possibly cross filesystem): %s -> %s", src, dst);
        return 2;
    }
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
    {.command = "setcpuperiod", .help = "修改SYSTEM_CPUPERIOD_SEL的值", .hint = no_info, .func = &cmd_cpufreq_reg, .argtable = NULL}};

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