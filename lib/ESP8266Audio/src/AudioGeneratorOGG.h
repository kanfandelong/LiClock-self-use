/*
  AudioGeneratorOGG
  Audio output generator that plays Ogg Vorbis files using the Tremor decoder
  
  Copyright (C) 2025  (Your Name)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _AUDIOGENERATOROGG_H
#define _AUDIOGENERATOROGG_H

#include "AudioGenerator.h"
#include "libvorbis/ivorbisfile.h"

class AudioGeneratorOGG : public AudioGenerator
{
  public:
    AudioGeneratorOGG();
    virtual ~AudioGeneratorOGG() override;
    virtual bool begin(AudioFileSource *source, AudioOutput *output) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override;

  protected:
    // Static bounce functions for Tremor's ov_callbacks
    static size_t OGG_read(void *ptr, size_t size, size_t nmemb, void *datasource);
    static int    OGG_seek(void *datasource, ogg_int64_t offset, int whence);
    static int    OGG_close(void *datasource);
    static long   OGG_tell(void *datasource);

    // Instance-level callback implementations
    size_t read_cb(void *ptr, size_t size, size_t nmemb);
    int    seek_cb(ogg_int64_t offset, int whence);
    int    close_cb();
    long   tell_cb();

    // Metadata tag processing
    void processTags();

  private:
    ov_callbacks _cb;               // Tremor callback struct
    OggVorbis_File *vf;            // Tremor file handle
    int current_section;           // Current logical bitstream

    int channels;                  // Number of channels from Vorbis info
    int16_t *buff;                 // Stereo interleaved PCM buffer
    uint32_t buffPtr;              // Current read position in buff (in int16_t units)
    uint32_t buffLen;              // Total valid data in buff (in int16_t units)
};

#endif