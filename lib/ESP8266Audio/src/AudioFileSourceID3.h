/*
  AudioFileSourceID3.h
  ID3 tag parser based on Audio::read_ID3_Header logic.

  Copyright (C) 2017  Earle F. Philhower, III
  Modified 2026 – complete rewrite of tag parsing, removing AudioFileSourceUnsync.
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef _AUDIOFILESOURCEID3_H
#define _AUDIOFILESOURCEID3_H

#include <Arduino.h>
#include "AudioFileSource.h"
#include <vector>
#include <memory>

class AudioFileSourceID3 : public AudioFileSource
{
  public:
    AudioFileSourceID3(AudioFileSource *src);
    virtual ~AudioFileSourceID3() override;
    
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;

  private:
    // Parse the ID3 tag from a contiguous buffer (already unsync-decoded if needed)
    void parseID3Frames(const uint8_t* buf, size_t len, uint8_t version);
    
    AudioFileSource *src;
    bool checked;
};

#endif