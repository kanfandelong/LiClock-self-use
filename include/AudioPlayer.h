#pragma once
#include "A_Config.h"
#include <AudioFileSourceSD.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

enum PlayerCommand {
    CMD_PLAY,
    CMD_PAUSE,
    CMD_STOP,
    CMD_VOLUME,
    CMD_SWITCH_FILE
};

struct PlayerEvent {
    PlayerCommand cmd;
    const char* newPath;
    float volume;
};

class AudioPlayer {
private:
public:
    AudioGenerator* decoder = nullptr;
    AudioFileSource* file = nullptr;
    AudioOutputI2S* out = nullptr;
    TaskHandle_t playTaskHandle = nullptr;
    QueueHandle_t eventQueue = nullptr;
    bool isPlaying = false;
    bool loopPlay = false;
    float currentVolume = 1.0;

    void createAudioSource(const char* path);
    void createDecoder(const char* path);
    static void playTask(void* params);
    void stopPlayback(bool keepSource = false);

    AudioPlayer();
    ~AudioPlayer();
    void play(const char* path);
    void pause();
    void stop();
    void setVolume(float volume);
    void setLoop(bool loop);
};


