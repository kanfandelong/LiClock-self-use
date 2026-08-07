#ifndef NANDFS_H
#define NANDFS_H

#include "FS.h"
#include "driver/spi_master.h"
#include "spi_nand_flash.h"

class NANDFS : public fs::FS
{
public:
    NANDFS();
    
    // 设置 SPI 引脚（必须在 begin 之前调用）
    bool setPins(int clk, int mosi, int miso, int cs, int wp = -1, int hold = -1, int power = -1);
    
    // 挂载文件系统
    bool begin(const char* mountpoint = "/nand", bool format_if_mount_failed = true, uint8_t max_files = 5);
    
    // 卸载
    void end();
    
    // 容量信息
    uint64_t totalBytes();
    uint64_t usedBytes();
    
    // 获取底层设备句柄（可选）
    spi_nand_flash_device_t* getDevice() { return _nand_device; }

private:
    int8_t _pin_clk;
    int8_t _pin_mosi;
    int8_t _pin_miso;
    int8_t _pin_cs;
    int8_t _pin_wp;
    int8_t _pin_hold;
    int8_t _pin_power;
    
    spi_device_handle_t _spi;
    spi_nand_flash_device_t* _nand_device;
    bool _mounted;
    String _mountpoint;
    
    bool _initHardware();
    void _deinitHardware();
};

extern NANDFS NAND;

#endif