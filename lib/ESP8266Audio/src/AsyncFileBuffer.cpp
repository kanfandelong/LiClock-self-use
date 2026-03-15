#include "AsyncFileBuffer.h"
#include <esp_heap_caps.h>

AsyncFileBuffer::AsyncFileBuffer(size_t bufSize, size_t chunkSize)
    : bufferSize(bufSize), chunkSize(chunkSize), buffer(nullptr), readPtr(0), writePtr(0), filePos(0), fileSize(0), taskHandle(nullptr), running(false), eof(false) {
    mutex = xSemaphoreCreateMutex();
}

AsyncFileBuffer::~AsyncFileBuffer() {
    end();
    if (buffer) {
        heap_caps_free(buffer);
        buffer = nullptr;
    }
    if (mutex) {
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }
}

bool AsyncFileBuffer::begin(AudioFileSource file) {
    _file = file;
    fileSize = file.getSize();
    filePos = 0;
    readPtr = 0;
    writePtr = 0;
    eof = false;
    running = true;
    buffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    if (!buffer) return false;
    xTaskCreatePinnedToCore(fileReadTask, "FileReadTask", 4096, this, 1, &taskHandle, 1);
    return true;
}

void AsyncFileBuffer::end() {
    running = false;
    if (taskHandle) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
}

size_t AsyncFileBuffer::read(uint8_t* dst, size_t len) {
    size_t bytesRead = 0;
    xSemaphoreTake(mutex, portMAX_DELAY);
    while (bytesRead < len && readPtr != writePtr) {
        dst[bytesRead++] = buffer[readPtr];
        readPtr = (readPtr + 1) % bufferSize;
    }
    xSemaphoreGive(mutex);
    return bytesRead;
}

bool AsyncFileBuffer::seek(size_t pos) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    _file.seek(pos, SEEK_SET);
    filePos = pos;
    readPtr = 0;
    writePtr = 0;
    eof = false;
    xSemaphoreGive(mutex);
    return true;
}

size_t AsyncFileBuffer::getSize() const {
    return fileSize;
}

size_t AsyncFileBuffer::getPos() const {
    return filePos;
}

bool AsyncFileBuffer::isOpen() const {
    return true;
}

void AsyncFileBuffer::loop() {
    // Optionally trigger buffer fill or status update
}

void AsyncFileBuffer::fileReadTask(void* arg) {
    AsyncFileBuffer* self = static_cast<AsyncFileBuffer*>(arg);
    while (self->running && !self->eof) {
        self->fillBuffer();
        vTaskDelay(1);
    }
    vTaskDelete(nullptr);
}

void AsyncFileBuffer::fillBuffer() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    while (((writePtr + 1) % bufferSize) != readPtr && !eof) {
        uint8_t buf;
        if (_file.read(&buf, 1) != 1){
            eof = true;
        }
        size_t space = (readPtr > writePtr) ? (readPtr - writePtr - 1) : (bufferSize - writePtr + readPtr - 1);
        size_t toRead = (space > chunkSize) ? chunkSize : space;
        if (toRead == 0) break;
        size_t n = _file.read(buffer + writePtr, toRead);
        if (n == 0) {
            eof = true;
            break;
        }
        writePtr = (writePtr + n) % bufferSize;
        filePos += n;
    }
    xSemaphoreGive(mutex);
}
