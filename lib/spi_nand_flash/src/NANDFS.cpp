#include "NANDFS.h"
#include "esp_vfs_fat_nand.h"
#include "vfs_api.h"
#include "ff.h"
#include "esp_log.h"

static const char* TAG = "NANDFS";

NANDFS::NANDFS()
    : fs::FS(FSImplPtr(new VFSImpl()))
    , _pin_clk(-1)
    , _pin_mosi(-1)
    , _pin_miso(-1)
    , _pin_cs(-1)
    , _pin_wp(-1)
    , _pin_hold(-1)
    , _pin_power(-1)
    , _spi(nullptr)
    , _nand_device(nullptr)
    , _mounted(false)
    , _mountpoint("")
{
}

bool NANDFS::setPins(int clk, int mosi, int miso, int cs, int wp, int hold, int power)
{
    if (_mounted) {
        ESP_LOGE(TAG, "setPins must be called before begin()");
        return false;
    }
    // 检查必要引脚是否有效
    if (clk < 0 || mosi < 0 || miso < 0 || cs < 0) {
        ESP_LOGE(TAG, "Invalid pins: clk=%d, mosi=%d, miso=%d, cs=%d", clk, mosi, miso, cs);
        return false;
    }
    _pin_clk = clk;
    _pin_mosi = mosi;
    _pin_miso = miso;
    _pin_cs = cs;
    _pin_wp = wp;
    _pin_hold = hold;
    _pin_power = power;
    return true;
}

bool NANDFS::_initHardware()
{
    // 检查引脚是否已设置
    if (_pin_clk == -1 || _pin_mosi == -1 || _pin_miso == -1 || _pin_cs == -1) {
        ESP_LOGE(TAG, "Pins not set. Call setPins() first.");
        return false;
    }

    // 电源控制
    if (_pin_power >= 0) {
        gpio_hold_dis((gpio_num_t)_pin_power);
        digitalWrite(_pin_power, HIGH);
        gpio_hold_en((gpio_num_t)_pin_power);
        delay(10);
    }

    // 配置 SPI 总线
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = _pin_mosi;
    bus_cfg.miso_io_num = _pin_miso;
    bus_cfg.sclk_io_num = _pin_clk;
    bus_cfg.quadwp_io_num = _pin_wp;
    bus_cfg.quadhd_io_num = _pin_hold;
    bus_cfg.max_transfer_sz = 4096 * 2;

    // 使用默认 SPI 主机 (可改为配置参数，这里固定使用 SPI2_HOST)
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // 添加 SPI 设备
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = 40 * 1000 * 1000;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = _pin_cs;
    dev_cfg.queue_size = 10;
    dev_cfg.flags = SPI_DEVICE_HALFDUPLEX;

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return false;
    }

    // 初始化 NAND 设备
    spi_nand_flash_config_t nand_cfg = {};
    nand_cfg.device_handle = _spi;
    nand_cfg.io_mode = SPI_NAND_IO_MODE_QIO;  // 可根据芯片支持调整
    nand_cfg.flags = SPI_DEVICE_HALFDUPLEX;

    ret = spi_nand_flash_init_device(&nand_cfg, &_nand_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NAND device init failed: %s", esp_err_to_name(ret));
        spi_bus_remove_device(_spi);
        spi_bus_free(SPI2_HOST);
        _spi = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "NAND hardware initialized");
    return true;
}

void NANDFS::_deinitHardware()
{
    if (_nand_device) {
        spi_nand_flash_deinit_device(_nand_device);
        _nand_device = nullptr;
    }
    if (_spi) {
        spi_bus_remove_device(_spi);
        _spi = nullptr;
        spi_bus_free(SPI2_HOST);
    }
    if (_pin_power >= 0) {
        gpio_hold_dis((gpio_num_t)_pin_power);
        digitalWrite(_pin_power, LOW);
        gpio_hold_en((gpio_num_t)_pin_power);
    }
    ESP_LOGI(TAG, "NAND hardware deinitialized");
}

bool NANDFS::begin(const char* mountpoint, bool format_if_mount_failed, uint8_t max_files)
{
    if (_mounted) {
        ESP_LOGW(TAG, "NAND already mounted at %s", _mountpoint.c_str());
        return true;
    }

    if (!_initHardware()) {
        return false;
    }

    esp_vfs_fat_mount_config_t mount_cfg = {};
    mount_cfg.max_files = max_files;
    mount_cfg.format_if_mount_failed = format_if_mount_failed;
    mount_cfg.allocation_unit_size = 16 * 1024;

    esp_err_t ret = esp_vfs_fat_nand_mount(mountpoint, _nand_device, &mount_cfg);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Formatting was %s.",
                     format_if_mount_failed ? "enabled" : "disabled");
        } else {
            ESP_LOGE(TAG, "Mount failed: %s", esp_err_to_name(ret));
        }
        _deinitHardware();
        return false;
    }

    _impl->mountpoint(mountpoint);
    _mountpoint = mountpoint;
    _mounted = true;
    ESP_LOGI(TAG, "NAND mounted at %s", mountpoint);
    return true;
}

void NANDFS::end()
{
    if (!_mounted) return;
    esp_vfs_fat_nand_unmount(_mountpoint.c_str(), _nand_device);
    _impl->mountpoint(nullptr);
    _mounted = false;
    _mountpoint = "";
    _deinitHardware();
    ESP_LOGI(TAG, "NAND unmounted");
}

uint64_t NANDFS::totalBytes()
{
    if (!_mounted) return 0;
    FATFS* fsinfo;
    DWORD fre_clust;
    if (f_getfree(_mountpoint.c_str(), &fre_clust, &fsinfo) != 0) return 0;
    uint64_t size = ((uint64_t)(fsinfo->csize)) * (fsinfo->n_fatent - 2);
#if _MAX_SS != 512
    size *= fsinfo->ssize;
#else
    size *= 512;
#endif
    return size;
}

uint64_t NANDFS::usedBytes()
{
    if (!_mounted) return 0;
    FATFS* fsinfo;
    DWORD fre_clust;
    if (f_getfree(_mountpoint.c_str(), &fre_clust, &fsinfo) != 0) return 0;
    uint64_t size = ((uint64_t)(fsinfo->csize)) * ((fsinfo->n_fatent - 2) - (fsinfo->free_clst));
#if _MAX_SS != 512
    size *= fsinfo->ssize;
#else
    size *= 512;
#endif
    return size;
}

// 全局实例
NANDFS NAND;