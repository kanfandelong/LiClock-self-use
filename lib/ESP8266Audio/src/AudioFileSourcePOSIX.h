#ifndef AUDIOFILESOURCEPOSIX_H
#define AUDIOFILESOURCEPOSIX_H

#include "AudioFileSource.h"
#include <stdint.h>
#include <stddef.h>

class AudioFileSourcePOSIX : public AudioFileSource {
public:
    AudioFileSourcePOSIX();
    explicit AudioFileSourcePOSIX(const char *filename);
    virtual ~AudioFileSourcePOSIX() override;

    virtual bool open(const char *filename) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;

protected:
    // 缓冲区大小，可根据 PSRAM 情况调整（例如 16KB）
    static constexpr size_t BUFFER_SIZE = 32 * 1024;

    int fd;                     // 文件描述符
    uint8_t *buffer;            // DMA 对齐的环形缓冲区（PSRAM）
    size_t readIndex;           // 当前已消费位置
    size_t validBytes;          // 缓冲区中有效数据量
    bool eofReached;            // 是否已到文件尾
    uint32_t filePos;           // 逻辑文件位置（已提供给调用者的字节数）
    uint32_t fileSize;          // 文件总大小（缓存，避免频繁 stat）

    void refill();              // 用 read() 填充缓冲区
    void resetBuffer();         // 重置缓冲区状态（用于 seek）
};

#endif // AUDIOFILESOURCEPOSIX_H