#include "AudioFileSourceVorbis.h"
#include <Arduino.h>

AudioFileSourceVorbis::AudioFileSourceVorbis(AudioFileSource *source) 
{
  this->file = source;
  this->metadataParsed = false;
}

AudioFileSourceVorbis::~AudioFileSourceVorbis()
{
    if (file) delete file;
}

bool AudioFileSourceVorbis::open(const char *filename)
{
    if (!file->open(filename)) return false;
    
    // 尝试解析元数据
    parseMetadata();
    return true;
}

bool AudioFileSourceVorbis::parseMetadata()
{
    if (metadataParsed) return true;
    
    // 读取文件头来判断格式
    uint8_t header[16];
    if (file->read(header, 16) != 16) return false;
    file->seek(0, SEEK_SET); // 重置位置
    
    // 检查文件格式
    if (memcmp(header, "fLaC", 4) == 0) {
        return parseFLACMetadata();
    } else if (memcmp(header, "OggS", 4) == 0) {
        return parseOpusMetadata();
    }
    
    return false;
}

bool AudioFileSourceVorbis::parseFLACMetadata()
{
    uint8_t buffer[4096];
    file->seek(0, SEEK_SET);
    
    // 读取FLAC文件头
    if (file->read(buffer, 4) != 4 || memcmp(buffer, "fLaC", 4) != 0) {
        return false;
    }
    
    // 解析元数据块
    while (true) {
        if (file->read(buffer, 4) != 4) break;
        
        uint8_t blockType = buffer[0] & 0x7F;
        uint32_t blockLength = (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
        bool isLastBlock = (buffer[0] & 0x80) != 0;
        
        if (blockLength > sizeof(buffer)) {
            file->seek(blockLength, SEEK_CUR);
            continue;
        }
        
        if (file->read(buffer, blockLength) != blockLength) break;
        
        if (blockType == 4) { // VORBIS_COMMENT
            return parseVorbisComments(buffer, blockLength);
        }
        
        if (isLastBlock) break;
    }
    
    return false;
}

bool AudioFileSourceVorbis::parseOpusMetadata()
{
    uint8_t buffer[8192];
    file->seek(0, SEEK_SET);
    
    // 解析Ogg页面
    while (true) {
        if (file->read(buffer, 27) != 27) break; // Ogg页头
        
        if (memcmp(buffer, "OggS", 4) != 0) break;
        
        uint8_t segmentCount = buffer[26];
        if (file->read(buffer, segmentCount) != segmentCount) break;
        
        // 计算页面数据大小
        uint32_t pageSize = 0;
        for (int i = 0; i < segmentCount; i++) {
            pageSize += buffer[i];
        }
        
        if (pageSize > sizeof(buffer) - 100) {
            file->seek(pageSize, SEEK_CUR);
            continue;
        }
        
        // 读取页面数据
        if (file->read(buffer, pageSize) != pageSize) break;
        
        // 检查Opus标识
        if (memcmp(buffer, "OpusHead", 8) == 0) {
            // 跳过音频头，继续查找注释
            continue;
        }
        
        if (memcmp(buffer, "OpusTags", 8) == 0) {
            // 找到Vorbis注释
            return parseVorbisComments(buffer + 8, pageSize - 8);
        }
    }
    
    return false;
}

bool AudioFileSourceVorbis::parseVorbisComments(const uint8_t* data, uint32_t length)
{
    if (length < 8) return false;
    
    // 读取厂商字符串长度（小端序）
    uint32_t vendorLength = readUint32LE(data);
    if (vendorLength + 8 > length) return false;
    
    const uint8_t* commentData = data + 4 + vendorLength;
    uint32_t commentsLength = length - 4 - vendorLength;
    
    if (commentsLength < 4) return false;
    
    // 读取注释数量
    uint32_t commentCount = readUint32LE(commentData);
    commentData += 4;
    commentsLength -= 4;
    
    for (uint32_t i = 0; i < commentCount && commentsLength >= 4; i++) {
        // 读取注释长度
        uint32_t commentLength = readUint32LE(commentData);
        commentData += 4;
        commentsLength -= 4;
        
        if (commentLength > commentsLength) break;
        
        // 提取注释字符串
        String commentStr;
        for (uint32_t j = 0; j < commentLength; j++) {
            commentStr += (char)commentData[j];
        }
        
        commentData += commentLength;
        commentsLength -= commentLength;
        
        // 分离键值对
        int equalsPos = commentStr.indexOf('=');
        if (equalsPos > 0) {
        
            String key = commentStr.substring(0, equalsPos);
            key.toUpperCase();
            String value = commentStr.substring(equalsPos + 1);
            
            // 映射到标准标签类型
            const char* tagType = nullptr;
            if (key == "TITLE") tagType = "title";
            else if (key == "ARTIST") tagType = "performer";
            else if (key == "ALBUM") tagType = "album";
            else if (key == "DATE") tagType = "date";
            else if (key == "GENRE") tagType = "genre";
            else if (key == "TRACKNUMBER") tagType = "track";
            else if (key == "COMMENT") tagType = "comment";
            
            if (tagType) {
                cb.md(tagType, false, value.c_str());
            }
        }
    }
    
    return true;
}

uint32_t AudioFileSourceVorbis::readUint32BE(const uint8_t* data)
{
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

uint32_t AudioFileSourceVorbis::readUint32LE(const uint8_t* data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// 其他方法的实现（委托给底层文件）
uint32_t AudioFileSourceVorbis::read(void *data, uint32_t len)
{
    return file->read(data, len);
}

bool AudioFileSourceVorbis::seek(int32_t pos, int dir)
{
    return file->seek(pos, dir);
}

bool AudioFileSourceVorbis::close()
{
    return file->close();
}

bool AudioFileSourceVorbis::isOpen()
{
    return file->isOpen();
}

uint32_t AudioFileSourceVorbis::getSize()
{
    return file->getSize();
}

uint32_t AudioFileSourceVorbis::getPos()
{
    return file->getPos();
}