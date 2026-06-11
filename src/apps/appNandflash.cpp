#include "AppManager.h"
#include "esp_vfs_fat_nand.h"
#include "ff.h"

class AppNandflash : public AppBase
{
private:
    /* data */
public:
    AppNandflash()
    {
        name = "W25N02";
        title = "W25N02";
        description = "W25N02文件系统";
        image = NULL;
    }
    void set();
    void setup();
};
static AppNandflash app;

void AppNandflash::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

void print_nand_space() {
    FATFS *fs;
    DWORD free_clust;
    if (f_getfree("/nand", &free_clust, &fs) != FR_OK) {
        log_e("f_getfree failed");
        return;
    }
    uint64_t total_sectors = (fs->n_fatent - 2) * fs->csize;
    uint64_t free_sectors = free_clust * fs->csize;
    uint64_t sector_size = FF_MAX_SS == 512 ? 512 : fs->ssize;

    uint64_t total_bytes = total_sectors * sector_size;
    uint64_t free_bytes = free_sectors * sector_size;

    log_i("Total: %llu kB, Free: %llu kB", total_bytes / 1024, free_bytes / 1024);
}

void AppNandflash::setup()
{
    esp_err_t ret;
    gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
    digitalWrite(PIN_SDVDD_CTRL, 0);
    gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
    delay(50);

    spi_bus_config_t bus_config = {0};
    bus_config.mosi_io_num = PIN_SD_D0;
    bus_config.miso_io_num = PIN_SD_D1;
    bus_config.sclk_io_num = PIN_SD_SCLK;
    bus_config.quadhd_io_num = PIN_SD_D3;
    bus_config.quadwp_io_num = PIN_SD_D2;
    bus_config.max_transfer_sz = 4096 * 2;

    ret = spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO);
    log_i("spi_bus_initialize: %s", esp_err_to_name(ret));

    spi_device_interface_config_t devcfg = {0};
    devcfg.clock_speed_hz = SPI_MASTER_FREQ_40M;
    devcfg.mode = (uint8_t)0;
    devcfg.spics_io_num = PIN_SD_CMD;
    devcfg.queue_size = 10;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;

    spi_device_handle_t spi;

    ret = spi_bus_add_device(SPI3_HOST, &devcfg, &spi);
    log_i("spi_bus_add_device: %s", esp_err_to_name(ret));

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .io_mode = SPI_NAND_IO_MODE_QIO,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    spi_nand_flash_device_t *nand_flash_device_handle;
    ret = spi_nand_flash_init_device(&nand_flash_config, &nand_flash_device_handle);
    log_i("spi_nand_flash_init_device: %s", esp_err_to_name(ret));

    esp_vfs_fat_mount_config_t config = {0};
    config.max_files = 4;
    config.format_if_mount_failed = true;
    config.allocation_unit_size = 16 * 1024;

    ret = esp_vfs_fat_nand_mount("/nand", nand_flash_device_handle, &config);
    log_i("esp_vfs_fat_nand_mount: %s", esp_err_to_name(ret));
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            log_e("Failed to mount filesystem. "
                  "If you want the flash memory to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        return;
    }
    // Print FAT FS size information
    uint64_t bytes_total, bytes_free;
    // esp_vfs_fat_info("/nand", &bytes_total, &bytes_free);
    // log_i("FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

    bool end = false;
    while (!end)
    {
        if (hal.btnl.isPressing())
        {
            end = true;
        }
        delay(10);
    }

    esp_vfs_fat_nand_unmount("/nand", nand_flash_device_handle);
    spi_nand_flash_deinit_device(nand_flash_device_handle);
    spi_bus_remove_device(spi);
    spi_bus_free(SPI3_HOST);

    gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
    digitalWrite(PIN_SDVDD_CTRL, 1);
    gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);

    appManager.noDeepSleep = false;
    appManager.nextWakeup = 1;
}
