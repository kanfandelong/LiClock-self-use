/*
  AudioGeneratorM4A
  Audio output generator for M4A (MPEG-4 AAC) files using the Helix AAC decoder

  Parses the MP4/ISOBMFF container to extract AAC frames and decodes them
  via the same Helix AAC decoder used by AudioGeneratorAAC.

  Copyright (C) 2025  Earle F. Philhower, III

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

#ifndef _AUDIOGENERATORM4A_H
#define _AUDIOGENERATORM4A_H

#include "AudioGenerator.h"
#include "libhelix-aac/aacdec.h"

class AudioGeneratorM4A : public AudioGenerator
{
  public:
    AudioGeneratorM4A();
    AudioGeneratorM4A(void *preallocateData, int preallocateSize);
    virtual ~AudioGeneratorM4A() override;
    virtual bool begin(AudioFileSource *source, AudioOutput *output) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override;
    struct SampleEntry {
        uint32_t offset;  // absolute file offset of this AAC frame
        uint32_t size;    // size of this AAC frame in bytes
    };

  protected:
    // --- Pre-allocated memory support ---
    void *preallocateSpace;
    int preallocateSize;

    // --- Helix AAC decoder ---
    HAACDecoder hAACDecoder;

    // --- Input buffer (one raw AAC frame + room for ADTS header) ---
    static const int INPUT_BUF_SIZE = 1600;
    uint8_t *buff;
    int buffValid;

    // --- Output buffer (interleaved L/R) ---
    static const int MAX_OUTPUT_SAMPLES = 1024 * 2;
    int16_t *outSample;
    int16_t validSamples;
    int16_t curSample;

    // --- Stream properties from the decoded frames ---
    unsigned int lastRate;
    int lastChannels;

    // --- MP4 sample table ---
    SampleEntry *sampleTable;
    uint32_t numSamples;
    uint32_t currentSample;

    // --- AudioSpecificConfig from the esds box ---
    uint8_t asc[4];           // raw ASC bytes (store up to 4)
    int ascLen;               // actual ASC length
    int ascAudioObjectType;   // audio object type (e.g. 2 = AAC-LC)
    int ascSampleRate;        // sample rate from ASC
    int ascChannelConfig;     // channel config from ASC

    // --- Container metadata ---
    uint64_t mp4DurationMs;  // total duration in ms from mvhd box

    // --- Bitrate tracking for total duration ---
    uint64_t bitrateSum;
    uint32_t bitrateCount;
    bool totalSent;

    // --- Internal helpers ---
    bool ParseMP4();
    int GetSampleRateIndex(int hz);
    int BuildADTSHeader(uint8_t *adts, int frameLength, int profile, int sri, int chans);
    bool FillBufferForSample(uint32_t sampleIdx);
    void ReportTotalDuration();

    // --- Big-endian helpers (track file position internally) ---
    uint32_t fileSize;
    bool ReadFromFile(void *ptr, uint32_t len);
    bool SeekTo(uint32_t pos);
    uint8_t ReadU8();
    uint16_t ReadU16BE();
    uint32_t ReadU24BE();
    uint32_t ReadU32BE();
    uint64_t ReadU64BE();
    bool SkipBytes(uint32_t n);
};

#endif
