#ifndef AUDIOFILESOURCEVORBIS_H
#define AUDIOFILESOURCEVORBIS_H

#include "AudioFileSource.h"
#include <vector>
#include <utility> // for std::pair

class AudioFileSourceVorbis : public AudioFileSource
{
public:
    typedef void (*MetadataCallback)(void* cbData, const char* type, bool isUnicode, const char* string);
    
    AudioFileSourceVorbis(AudioFileSource *source);
    virtual ~AudioFileSourceVorbis();
    
    bool open(const char *filename) override;
    uint32_t read(void *data, uint32_t len) override;
    bool seek(int32_t pos, int dir) override;
    bool close() override;
    bool isOpen() override;
    uint32_t getSize() override;
    uint32_t getPos() override;
    
    bool parseMetadata(); // 解析元数据
    
private:
    AudioFileSource *file;
    bool metadataParsed;
    
    // Vorbis 注释解析方法
    bool parseFLACMetadata();
    bool parseOpusMetadata();
    bool parseVorbisComments(const uint8_t* data, uint32_t length);
    
    // 工具方法
    uint32_t readUint32BE(const uint8_t* data);
    uint32_t readUint32LE(const uint8_t* data);
};

#endif