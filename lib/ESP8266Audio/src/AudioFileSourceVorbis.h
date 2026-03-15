#ifndef AUDIOFILESOURCEVORBIS_H
#define AUDIOFILESOURCEVORBIS_H

#include "AudioFileSource.h"
#include <vector>
#include <utility> // for std::pair

class AudioFileSourceVorbis : public AudioFileSource
{
public:

    AudioFileSourceVorbis(AudioFileSource *source);
    virtual ~AudioFileSourceVorbis();
    
    // bool open(const char *filename) override;
    uint32_t read(void *data, uint32_t len) override;
    bool seek(int32_t pos, int dir) override;
    bool close() override;
    bool isOpen() override;
    uint32_t getSize() override;
    uint32_t getPos() override;
    
    bool parseOggVorbisComment(); // 解析元数据
    
private:
    AudioFileSource *src;
    bool checked;

    uint32_t bufferSize;
    uint32_t bufferLen;
    uint32_t bufferPos;
    uint8_t *buffer;
    uint32_t readLE32(const uint8_t* p);
};

#endif