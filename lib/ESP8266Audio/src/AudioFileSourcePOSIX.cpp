#include "AudioFileSourcePOSIX.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <esp_heap_caps.h>
#include <errno.h>
#include <string.h>

// 构造函数
AudioFileSourcePOSIX::AudioFileSourcePOSIX()
    : fd(-1), buffer(nullptr), readIndex(0), validBytes(0),
      eofReached(false), filePos(0), fileSize(0) {
}

AudioFileSourcePOSIX::AudioFileSourcePOSIX(const char *filename)
    : AudioFileSourcePOSIX() {
    open(filename);
}

AudioFileSourcePOSIX::~AudioFileSourcePOSIX() {
    close();
}

// 打开文件，分配 DMA 缓冲区，获取文件大小
bool AudioFileSourcePOSIX::open(const char *filename) {
    if (fd >= 0) close(); // 如果已打开则关闭

    fd = ::open(filename, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    // 获取文件大小
    struct stat st;
    if (fstat(fd, &st) != 0) {
        ::close(fd);
        fd = -1;
        return false;
    }
    fileSize = st.st_size;

    // 分配 PSRAM 中的 DMA 对齐缓冲区
    buffer = (uint8_t*)heap_caps_aligned_alloc(32, BUFFER_SIZE,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (!buffer) {
        // 如果 PSRAM 分配失败，尝试 DRAM（但用户说内存紧张，这里作为备选）
        buffer = (uint8_t*)heap_caps_aligned_alloc(32, BUFFER_SIZE,
                    MALLOC_CAP_DMA);
        if (!buffer) {
            ::close(fd);
            fd = -1;
            return false;
        }
    }

    // 重置状态
    resetBuffer();
    filePos = 0;
    eofReached = false;

    // 预读第一块数据
    refill();

    return true;
}

// 关闭文件，释放缓冲区
bool AudioFileSourcePOSIX::close() {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
    if (buffer) {
        heap_caps_free(buffer);
        buffer = nullptr;
    }
    readIndex = 0;
    validBytes = 0;
    eofReached = true;
    filePos = 0;
    fileSize = 0;
    return true;
}

bool AudioFileSourcePOSIX::isOpen() {
    return fd >= 0 && buffer != nullptr;
}

uint32_t AudioFileSourcePOSIX::getSize() {
    return fileSize;
}

uint32_t AudioFileSourcePOSIX::getPos() {
    return filePos;
}

// 填充缓冲区：从当前文件偏移读取数据到缓冲区
void AudioFileSourcePOSIX::refill() {
    if (!isOpen() || eofReached) return;

    // 如果缓冲区还有未消费数据，先将其移到开头（避免溢出）
    if (readIndex > 0 && validBytes > readIndex) {
        memmove(buffer, buffer + readIndex, validBytes - readIndex);
        validBytes -= readIndex;
        readIndex = 0;
    } else if (readIndex == validBytes) {
        // 缓冲区已空
        validBytes = 0;
        readIndex = 0;
    }

    // 计算剩余空间
    size_t space = BUFFER_SIZE - validBytes;
    if (space == 0) return; // 缓冲区满

    // 从文件读取数据填充剩余空间
    ssize_t ret = ::read(fd, buffer + validBytes, space);
    if (ret <= 0) {
        eofReached = true;
        return;
    }
    validBytes += ret;
    // 如果读取的字节数小于请求的，说明到文件尾了
    if ((size_t)ret < space) {
        eofReached = true;
    }
}

// 读取 len 字节到 data，返回实际读取的字节数
uint32_t AudioFileSourcePOSIX::read(void *data, uint32_t len) {
    if (!isOpen() || len == 0) return 0;

    uint8_t *out = (uint8_t*)data;
    uint32_t totalRead = 0;

    while (len > 0) {
        // 如果缓冲区数据不足，尝试填充
        if (validBytes - readIndex == 0) {
            if (eofReached) break;
            refill();
            if (validBytes - readIndex == 0) break; // 确实无数据
        }

        size_t avail = validBytes - readIndex;
        size_t toCopy = (len < avail) ? len : avail;
        memcpy(out, buffer + readIndex, toCopy);
        readIndex += toCopy;
        out += toCopy;
        len -= toCopy;
        totalRead += toCopy;
        filePos += toCopy; // 更新逻辑位置
    }

    return totalRead;
}

// seek 函数
bool AudioFileSourcePOSIX::seek(int32_t pos, int dir) {
    if (!isOpen()) return false;

    off_t target;
    switch (dir) {
        case SEEK_SET:
            target = pos;
            break;
        case SEEK_CUR:
            target = filePos + pos;
            break;
        case SEEK_END:
            target = fileSize + pos;
            break;
        default:
            return false;
    }

    // 边界检查
    if (target < 0) target = 0;
    if (target > (off_t)fileSize) target = fileSize;

    // 定位到目标位置
    if (lseek(fd, target, SEEK_SET) == (off_t)-1) {
        return false;
    }

    // 更新逻辑位置和缓冲区状态
    filePos = target;
    resetBuffer();
    // 预读第一块数据
    refill();

    return true;
}

// 重置缓冲区（清空数据），不修改文件指针
void AudioFileSourcePOSIX::resetBuffer() {
    readIndex = 0;
    validBytes = 0;
    eofReached = false;
}