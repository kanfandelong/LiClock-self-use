#include "AudioPlayer.h"


void AudioPlayer::createAudioSource(const char* path) {
    if (strncmp(path, "/sd/", 4) == 0) {
        file = new AudioFileSourceSD(path);
    } else if (strncmp(path, "/littlefs/", 10) == 0) {
        file = new AudioFileSourceLittleFS(path);
    }
}

void AudioPlayer::createDecoder(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".mp3") == 0) {
            decoder = new AudioGeneratorMP3();
        } else if (strcmp(ext, ".wav") == 0) {
            decoder = new AudioGeneratorWAV();
        }
    }
}

static void playTask(void* params) {
    AudioPlayer* player = static_cast<AudioPlayer*>(params);
    PlayerEvent event;

    while (1) {
        if (xQueueReceive(player->eventQueue, &event, 0) == pdTRUE) {
            // 处理命令
            switch (event.cmd) {
                case CMD_SWITCH_FILE:
                    player->stopPlayback(true);
                    player->createAudioSource(event.newPath);
                    player->createDecoder(event.newPath);
                    break;
                case CMD_VOLUME:
                    player->out->SetGain(event.volume);
                    break;
                case CMD_PAUSE:
                    player->isPlaying = !player->isPlaying;
                    break;
                case CMD_STOP:
                    player->stopPlayback();
                    break;
            }
        }

        if (player->decoder && player->isPlaying) {
            if (player->decoder->isRunning()) {
                if (!player->decoder->loop()) {
                    player->decoder->stop();
                }
            } else {
                if (player->loopPlay) {
                    player->file->seek(0, SEEK_SET);
                    player->decoder->begin(player->file, player->out);
                } else {
                    player->stopPlayback();
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void AudioPlayer::stopPlayback(bool keepSource = false) {
    if (decoder && decoder->isRunning()) {
        decoder->stop();
    }
    if (!keepSource) {
        delete decoder;
        delete file;
        decoder = nullptr;
        file = nullptr;
    }
    isPlaying = false;
}
AudioPlayer::AudioPlayer() {
    out = new AudioOutputI2S(0, 1);
    out->SetGain(currentVolume);

    eventQueue = xQueueCreate(5, sizeof(PlayerEvent));
    xTaskCreate(playTask, "AudioPlayer", 4096, this, 5, &playTaskHandle);
}

AudioPlayer::~AudioPlayer() {
    vTaskDelete(playTaskHandle);
    stopPlayback();
    delete out;
    vQueueDelete(eventQueue);
}

void AudioPlayer::play(const char* path) {
    PlayerEvent event = {CMD_SWITCH_FILE, path};
    xQueueSend(eventQueue, &event, portMAX_DELAY);
    event.cmd = CMD_PLAY;
    xQueueSend(eventQueue, &event, portMAX_DELAY);
}

void AudioPlayer::pause() {
    PlayerEvent event = {CMD_PAUSE};
    xQueueSend(eventQueue, &event, portMAX_DELAY);
}

void AudioPlayer::stop() {
    PlayerEvent event = {CMD_STOP};
    xQueueSend(eventQueue, &event, portMAX_DELAY);
}

void AudioPlayer::setVolume(float volume) {
    PlayerEvent event = {CMD_VOLUME, nullptr, volume};
    xQueueSend(eventQueue, &event, portMAX_DELAY);
}

void AudioPlayer::setLoop(bool loop) {
    loopPlay = loop;
}

// 全局变量
AudioPlayer player;
//volatile bool g_playbackFinished = false;
