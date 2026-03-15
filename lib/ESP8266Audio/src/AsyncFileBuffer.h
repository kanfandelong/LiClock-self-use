#pragma once
#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "AudioFileSource.h"
class AsyncFileBuffer {
public:
    AsyncFileBuffer(size_t bufSize, size_t chunkSize);
    ~AsyncFileBuffer();
    bool begin(AudioFileSource file);
    void end();
    size_t read(uint8_t* dst, size_t len);
    bool seek(size_t pos);
    size_t getSize() const;
    size_t getPos() const;
    bool isOpen() const;
    void loop();

private:
    static void fileReadTask(void* arg);
    void fillBuffer();
    AudioFileSource _file;
    uint8_t *buffer;
    size_t bufferSize;
    size_t chunkSize;
    size_t readPtr;
    size_t writePtr;
    size_t filePos;
    size_t fileSize;
    TaskHandle_t taskHandle;
    SemaphoreHandle_t mutex;
    bool running;
    bool eof;
};
