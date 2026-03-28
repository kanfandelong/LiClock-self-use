#include "AppManager.h"
#include "USB.h"
#include "USBMSC.h"

USBMSC MSC;

// ----------------------------------------------------------------------
// 屏幕日志显示系统
// ----------------------------------------------------------------------
#define LOG_LINES 13   // 显示的日志行数（总高度14行，固定信息占1行）
#define MAX_LOG_LEN 80 // 每条日志最大长度
#define SCREEN_WIDTH 384
#define SCREEN_HEIGHT 168
#define FONT_HEIGHT 12 // 字体高度（像素）

static char log_buffer[LOG_LINES][MAX_LOG_LEN];
static uint8_t log_index = 0; // 当前要写入的行索引
static uint8_t log_count = 0; // 已存储的日志条数（最多 LOG_LINES）

// 速度统计变量
static uint64_t total_read_bytes = 0;
static uint64_t total_write_bytes = 0;
static uint32_t read_speed = 0;   // KB/s
static uint32_t write_speed = 0;  // KB/s
static uint32_t last_speed_update = 0;

/**
 * @brief 重绘整个屏幕：固定信息（速度）+ 滚动日志
 */
static void redrawDisplay() {
    display.clearDisplay();          // 清屏

    // 固定信息：显示读写速度（第一行）
    u8g2Fonts.setCursor(0, FONT_HEIGHT);
    u8g2Fonts.printf("R: %lu KB/s  W: %lu KB/s", read_speed, write_speed);

    // 绘制滚动日志（从第二行开始）
    int start_row = 2; // 第一行为固定信息，日志从第2行开始（索引1）
    for (int i = 0; i < log_count; i++) {
        int line_index = (log_index - log_count + i + LOG_LINES) % LOG_LINES;
        u8g2Fonts.setCursor(0, (start_row + i) * FONT_HEIGHT);
        u8g2Fonts.printf("%s", log_buffer[line_index]);
    }

    display.display(); // 非阻塞刷新
}

/**
 * @brief 添加一条日志到滚动缓冲区并刷新显示
 */
static void addLog(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buffer[log_index], MAX_LOG_LEN, fmt, args);
    va_end(args);

    log_index = (log_index + 1) % LOG_LINES;
    if (log_count < LOG_LINES)
        log_count++;
    redrawDisplay();
}

// ----------------------------------------------------------------------
// MSC 回调函数（使用 SDMMC 读写）
// ----------------------------------------------------------------------

/**
 * @brief 从 SD 卡读取数据（扇区对齐）
 */
static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    // 累加读取字节数
    total_read_bytes += bufsize;

    // 检查扇区对齐
    if (offset != 0 || (bufsize % 512) != 0) {
        addLog("ERROR: non-aligned read");
        return -1;
    }

    uint32_t sector_count = bufsize / 512;
    esp_err_t err = SD_MMC.readRaw(buffer, lba, sector_count);
    if (err != ESP_OK) {
        addLog("SD read failed: %d", err);
        return -1;
    }
    return bufsize;
}

/**
 * @brief 向 SD 卡写入数据（扇区对齐）
 */
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    // 累加写入字节数
    total_write_bytes += bufsize;

    if (offset != 0 || (bufsize % 512) != 0) {
        addLog("ERROR: non-aligned write");
        return -1;
    }

    uint32_t sector_count = bufsize / 512;
    esp_err_t err = SD_MMC.writeRaw(buffer, lba, sector_count);
    if (err != ESP_OK) {
        addLog("SD write failed: %d", err);
        return -1;
    }
    return bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    addLog("START/STOP: power=%u start=%u eject=%u", power_condition, start, load_eject);
    return true;
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == ARDUINO_USB_EVENTS) {
        arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
        switch (event_id) {
        case ARDUINO_USB_STARTED_EVENT:
            addLog("USB PLUGGED");
            break;
        case ARDUINO_USB_STOPPED_EVENT:
            addLog("USB UNPLUGGED");
            break;
        case ARDUINO_USB_SUSPEND_EVENT:
            addLog("USB SUSPENDED: rw=%u", data->suspend.remote_wakeup_en);
            break;
        case ARDUINO_USB_RESUME_EVENT:
            addLog("USB RESUMED");
            break;
        default:
            break;
        }
    }
}

// ----------------------------------------------------------------------
// AppMSC 类定义
// ----------------------------------------------------------------------
class AppMSC : public AppBase {
public:
    AppMSC() {
        name = "msc";
        title = "USB-MSC";
        description = "模拟 USB 磁盘";
        image = NULL;
        peripherals_requested = PERIPHERALS_SD_BIT;
    }
    void setup();
    void set();
};

static AppMSC app; // 静态实例

void AppMSC::set() {
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), false);
}

void AppMSC::setup() {
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.end();
#endif

    // 初始化屏幕
    display.clearDisplay();
    u8g2Fonts.setFontMode(1);

    addLog("MSC App Starting...");
    addLog("Storage read/write pending");

    // 速度统计初始化
    last_speed_update = millis();
    total_read_bytes = 0;
    total_write_bytes = 0;
    read_speed = 0;
    write_speed = 0;

    // 注册 USB 事件回调
    USB.onEvent(usbEventCallback);

    // 设置 MSC 描述符
    MSC.vendorID("");
    MSC.productID("LiClock-S3");
    MSC.productRevision("2116");

    // 注册 MSC 回调
    MSC.onStartStop(onStartStop);
    MSC.onRead(onRead);
    MSC.onWrite(onWrite);

    // 获取 SD 卡参数
    sdmmc_card_t *card = SD_MMC.get();
    const uint32_t DISK_SECTOR_COUNT = card->csd.capacity;   // 总扇区数
    const uint16_t DISK_SECTOR_SIZE = card->csd.sector_size; // 扇区大小（通常512）

    // 启动 MSC
    MSC.mediaPresent(true);
    MSC.begin(DISK_SECTOR_COUNT, DISK_SECTOR_SIZE);

    // 启动 USB
    USB.begin();

    addLog("USB MSC Ready");
    redrawDisplay();

    // 主循环：更新速度显示，等待按键退出
    bool end = false;
    while (!end) {
        uint32_t now = millis();
        uint32_t delta = now - last_speed_update;
        if (delta >= 300) {
            // 计算速度（KB/s）
            read_speed = (uint32_t)(total_read_bytes * 1000 / delta / 1024);
            write_speed = (uint32_t)(total_write_bytes * 1000 / delta / 1024);
            // 清零累计
            total_read_bytes = 0;
            total_write_bytes = 0;
            last_speed_update = now;
            redrawDisplay(); // 刷新显示速度
        }

        if (hal.btnl.isPressing()) {
            while (hal.btnl.isPressing())
                delay(20);
            end = true;
            break;
        }
        delay(20);
    }
}