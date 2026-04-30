#include "AsyncFileBuffer.h"
#include <esp_heap_caps.h>

AsyncFileBuffer::AsyncFileBuffer(size_t bufSize)
    : bufferSize(bufSize)
{
}

AsyncFileBuffer::~AsyncFileBuffer()
{
    close();
}

bool AsyncFileBuffer::begin(AudioFileSource *source)
{
    if (!source || !source->isOpen())
        return false;

    _source = source;

    // 从 PSRAM 分配环形缓冲区存储
    ringStorage = (uint8_t *)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    if (!ringStorage) {
        _source = nullptr;
        return false;
    }

    ringBuf = xRingbufferCreateStatic(bufferSize, RINGBUF_TYPE_BYTEBUF,
                                      ringStorage, &ringBufStruct);
    if (!ringBuf) {
        heap_caps_free(ringStorage);
        ringStorage = nullptr;
        _source = nullptr;
        return false;
    }

    running = true;
    // 将填充任务固定在核心 1，优先级 1，栈 4096
    xTaskCreatePinnedToCore(fillTask, "AsyncFill", 4096,
                            this, 1, &taskHandle, 1);
    return true;
}

void AsyncFileBuffer::end()
{
    running = false;
    delay(150); // 等待填充任务退出
    if (taskHandle) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
    if (ringBuf) {
        vRingbufferDelete(ringBuf);
        ringBuf = nullptr;
    }
    if (ringStorage) {
        heap_caps_free(ringStorage);
        ringStorage = nullptr;
    }
    if (_source) {
        _source->close();
        _source = nullptr;
    }
}

uint32_t AsyncFileBuffer::read(void *data, uint32_t len)
{
    if (!ringBuf || !running)
        return 0;

    size_t received = 0;
    unsigned char *buf = (unsigned char *)xRingbufferReceiveUpTo(ringBuf,
                                                                 &received,
                                                                 0,    // 非阻塞
                                                                 len);
    if (buf) {
        memcpy(data, buf, received);
        vRingbufferReturnItem(ringBuf, (void *)buf);
        return (uint32_t)received;
    }

    // 无数据时检查底层源是否已读完，且缓冲区空，则返回 0 表示 EOF
    if (_source && _source->getPos() >= _source->getSize())
        return 0;   // EOF

    return 0;       // 暂时无数据，稍后重试
}

bool AsyncFileBuffer::seek(int32_t pos, int dir)
{
    if (!_source)
        return false;

    // 清空环形缓冲区：非阻塞取回所有数据并归还
    if (ringBuf) {
        size_t received;
        while (true) {
            unsigned char *buf = (unsigned char *)xRingbufferReceiveUpTo(
                ringBuf, &received, 0, SIZE_MAX);
            if (buf) {
                vRingbufferReturnItem(ringBuf, (void *)buf);
            } else {
                break;
            }
        }
    }

    // 底层源定位
    return _source->seek(pos, dir);
}

bool AsyncFileBuffer::close()
{
    end();
    return true;
}

bool AsyncFileBuffer::isOpen()
{
    return running && _source && _source->isOpen();
}

uint32_t AsyncFileBuffer::getSize()
{
    if (!_source)
        return 0;
    return _source->getSize();
}

uint32_t AsyncFileBuffer::getPos()
{
    if (!_source)
        return 0;

    uint32_t sourcePos = _source->getPos();
    size_t bufferedBytes = 0;
    if (ringBuf) {
        // 获取环形缓冲区中未读字节数
        vRingbufferGetInfo(ringBuf, NULL, NULL, NULL, NULL, &bufferedBytes);
    }

    // 逻辑位置 = 物理文件位置 - 缓冲区中尚未消费的字节数
    return (sourcePos >= bufferedBytes) ? (sourcePos - bufferedBytes) : 0;
}

bool AsyncFileBuffer::loop()
{
    // 填充任务独立运行，无需在主循环中处理
    return true;
}

// 静态转接函数
void AsyncFileBuffer::fillTask(void *param)
{
    AsyncFileBuffer *self = static_cast<AsyncFileBuffer *>(param);
    self->fillBufferTask();
}

// 后台填充任务
void AsyncFileBuffer::fillBufferTask()
{
    const size_t chunkSize = 16 * 1024; // 每次读取 16KB
    uint8_t *tempBuf = (uint8_t *)heap_caps_malloc(chunkSize, MALLOC_CAP_SPIRAM);
    if (!tempBuf) {
        running = false;
        vTaskDelete(NULL);
        return;
    }

    while (running && _source && _source->isOpen()) {
        uint32_t bytesRead = _source->read(tempBuf, chunkSize);
        if (bytesRead == 0) {
            // 检查是否真的 EOF
            if (_source->getPos() >= _source->getSize())
                break;
            // 可能只是暂时无数据（如网络流），等待
            vTaskDelay(1);
            continue;
        }

        // 发送到环形缓冲区，超时 100ms
        BaseType_t ret = xRingbufferSend(ringBuf, tempBuf, bytesRead, pdMS_TO_TICKS(10));
        if (ret != pdTRUE) {
            // 缓冲区满，稍作延迟再尝试
            vTaskDelay(1);
        }
    }

    heap_caps_free(tempBuf);
    vTaskDelete(NULL);
}