#pragma once
#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include "AudioFileSource.h"

class AsyncFileBuffer : public AudioFileSource
{
public:
    // 构造函数：bufSize 为环形缓冲区总大小，默认 64KB
    explicit AsyncFileBuffer(size_t bufSize = 64 * 1024);
    virtual ~AsyncFileBuffer();

    // 绑定一个已打开的 AudioFileSource，启动后台填充任务
    bool begin(AudioFileSource *source);
    void end();

    // ---------- AudioFileSource 接口实现 ----------
    // 非阻塞读取（内部使用 Ringbuffer 的非阻塞取回）
    uint32_t read(void *data, uint32_t len) override;

    // 非阻塞读取别名，直接调用 read
    uint32_t readNonBlock(void *data, uint32_t len) override {
        return read(data, len);
    }

    // 定位：清空环形缓冲区后直接操作底层源
    bool seek(int32_t pos, int dir) override;
    bool close() override;
    bool isOpen() override;
    uint32_t getSize() override;
    uint32_t getPos() override;
    bool loop() override;   // 可空，填充任务独立运行

private:
    static void fillTask(void *param);
    void fillBufferTask();

    AudioFileSource *_source = nullptr;
    RingbufHandle_t ringBuf = nullptr;
    StaticRingbuffer_t ringBufStruct;
    uint8_t *ringStorage = nullptr;
    size_t bufferSize;
    TaskHandle_t taskHandle = nullptr;
    bool running = false;
};