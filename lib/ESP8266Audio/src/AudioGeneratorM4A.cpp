/*
  AudioGeneratorM4A
  Audio output generator for M4A (MPEG-4 AAC) files using the Helix AAC decoder

  Parses the MP4/ISOBMFF container to extract AAC access units, prepends
  ADTS headers, and feeds them to the Helix AAC decoder for PCM output.

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

#pragma GCC optimize ("O3")

#include "AudioGeneratorM4A.h"

// ============================================================
//  Sample rate index lookup table (MPEG-4 Audio, Table 1.23)
// ============================================================
static const int srLookup[13] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025,  8000, 7350
};

// Forward declaration of AudioSpecificConfig decoder
// (defined later in this file, called from begin())
static bool DecodeAudioSpecificConfig(const uint8_t *asc, int ascLen,
                                       int *audioObjectType,
                                       int *sampleRate,
                                       int *channelConfig);

// ============================================================
//  Constructor / Destructor
// ============================================================

AudioGeneratorM4A::AudioGeneratorM4A()
{
    preallocateSpace = NULL;
    preallocateSize = 0;

    running = false;
    file = NULL;
    output = NULL;

    buff = (uint8_t*)malloc(INPUT_BUF_SIZE);
    outSample = (int16_t*)malloc(MAX_OUTPUT_SAMPLES * sizeof(int16_t));
    sampleTable = NULL;
    numSamples = 0;
    currentSample = 0;

    hAACDecoder = AACInitDecoder();
    if (!hAACDecoder) {
        log_e("Out of memory error! hAACDecoder==NULL");
    }

    buffValid = 0;
    validSamples = 0;
    curSample = 0;
    lastRate = 0;
    lastChannels = 0;
    ascLen = 0;
    ascAudioObjectType = 0;
    ascSampleRate = 0;
    ascChannelConfig = 0;
    fileSize = 0;
    bitrateSum = 0;
    bitrateCount = 0;
    totalSent = false;
}

AudioGeneratorM4A::AudioGeneratorM4A(void *preallocateData, int preallocateSz)
{
    preallocateSpace = preallocateData;
    preallocateSize = preallocateSz;

    running = false;
    file = NULL;
    output = NULL;

    uint8_t *p = (uint8_t*)preallocateSpace;
    buff = (uint8_t*)p;
    p += (INPUT_BUF_SIZE + 7) & ~7;
    outSample = (int16_t*)p;
    p += (MAX_OUTPUT_SAMPLES * sizeof(int16_t) + 7) & ~7;
    sampleTable = NULL;
    int used = p - (uint8_t*)preallocateSpace;
    int availSpace = preallocateSize - used;
    if (availSpace < 0) {
        log_e("Out of memory in M4A");
    }

    hAACDecoder = AACInitDecoderPre(p, availSpace);
    if (!hAACDecoder) {
        log_e("Out of memory error! hAACDecoder==NULL");
    }

    buffValid = 0;
    validSamples = 0;
    curSample = 0;
    lastRate = 0;
    lastChannels = 0;
    ascLen = 0;
    ascAudioObjectType = 0;
    ascSampleRate = 0;
    ascChannelConfig = 0;
    fileSize = 0;
    bitrateSum = 0;
    bitrateCount = 0;
    totalSent = false;
}

AudioGeneratorM4A::~AudioGeneratorM4A()
{
    if (!preallocateSpace) {
        AACFreeDecoder(hAACDecoder);
        free(buff);
        free(outSample);
        free(sampleTable);
    }
}

// ============================================================
//  begin / stop / isRunning
// ============================================================

bool AudioGeneratorM4A::begin(AudioFileSource *source, AudioOutput *output)
{
    if (!source) return false;
    file = source;
    if (!output) return false;
    this->output = output;
    if (!file->isOpen()) return false;

    fileSize = file->getSize();
    if (fileSize == 0) return false;

    // Parse MP4 container and build sample table
    if (!ParseMP4()) {
        log_e("Failed to parse MP4 container");
        return false;
    }
    if (numSamples == 0) {
        log_e("No audio samples found in MP4");
        return false;
    }

    // Decode AudioSpecificConfig from the esds box
    if (!DecodeAudioSpecificConfig(asc, ascLen,
                                    &ascAudioObjectType,
                                    &ascSampleRate,
                                    &ascChannelConfig)) {
        log_e("Failed to decode AudioSpecificConfig (bad or unsupported M4A)");
        return false;
    }

    output->begin();
    output->SetBitsPerSample(16);

    if (ascSampleRate > 0) {
        output->SetRate(ascSampleRate);
        output->SetChannels(ascChannelConfig);
        lastRate = ascSampleRate;
        lastChannels = ascChannelConfig;
    }

    memset(buff, 0, INPUT_BUF_SIZE);
    memset(outSample, 0, MAX_OUTPUT_SAMPLES * sizeof(int16_t));

    currentSample = 0;
    running = true;

    return true;
}

bool AudioGeneratorM4A::stop()
{
    running = false;
    output->stop();
    return file->close();
}

bool AudioGeneratorM4A::isRunning()
{
    return running;
}

// ============================================================
//  Main decode loop
// ============================================================

bool AudioGeneratorM4A::loop()
{
    if (!running) goto done;

    // Drain the output buffer first
    while (validSamples) {
#ifdef CONFIG_DAC_32bit
        if (lastChannels == 1) {
            lastSample[0] = outSample[curSample] << 16;
            lastSample[1] = outSample[curSample] << 16;
        } else if (lastChannels == 2) {
            lastSample[0] = outSample[curSample * 2] << 16;
            lastSample[1] = outSample[curSample * 2 + 1] << 16;
        } else {
            int32_t left = 0, right = 0;
            for (int i = 0; i < lastChannels; i++) {
                if (i % 2 == 0)
                    left += outSample[curSample * lastChannels + i];
                else
                    right += outSample[curSample * lastChannels + i];
            }
            lastSample[0] = (left / (lastChannels / 2)) << 16;
            lastSample[1] = (right / (lastChannels / 2)) << 16;
        }
#else
        if (lastChannels == 1) {
            lastSample[0] = outSample[curSample];
            lastSample[1] = outSample[curSample];
        } else if (lastChannels == 2) {
            lastSample[0] = outSample[curSample * 2];
            lastSample[1] = outSample[curSample * 2 + 1];
        } else {
            int32_t left = 0, right = 0;
            for (int i = 0; i < lastChannels; i++) {
                if (i % 2 == 0)
                    left += outSample[curSample * lastChannels + i];
                else
                    right += outSample[curSample * lastChannels + i];
            }
            lastSample[0] = left / (lastChannels / 2);
            lastSample[1] = right / (lastChannels / 2);
        }
#endif
        if (!output->ConsumeSample(lastSample)) goto done;
        validSamples--;
        curSample++;
    }

    // Need to decode the next AAC frame
    if (currentSample < numSamples) {
        if (FillBufferForSample(currentSample)) {
            // buff[0..ADTS_HEADER_SIZE-1] = ADTS header
            // buff[ADTS_HEADER_SIZE..] = raw AAC frame data
            unsigned char *inBuf = reinterpret_cast<unsigned char *>(buff);
            int bytesLeft = buffValid;
            int ret = AACDecode(hAACDecoder, &inBuf, &bytesLeft, outSample);
            if (ret == 0) {
                AACFrameInfo fi;
                AACGetLastFrameInfo(hAACDecoder, &fi);

                // Track bitrate for total duration estimation
                if (!totalSent && fi.bitRate > 0) {
                    bitrateSum += (uint64_t)fi.bitRate;
                    bitrateCount++;
                }
                if (bitrateCount >= 50 && !totalSent && bitrateCount % 50 == 0) {
                    ReportTotalDuration();
                }

                // Update output rate/channels if changed
                if ((int)fi.sampRateOut != (int)lastRate) {
                    output->SetRate(fi.sampRateOut);
                    lastRate = fi.sampRateOut;
                }
                if (fi.nChans != lastChannels) {
                    output->SetChannels(fi.nChans);
                    lastChannels = fi.nChans;
                }

                curSample = 0;
                validSamples = fi.outputSamps / (lastChannels > 0 ? lastChannels : 2);
            } else {
                // Decode error, skip this frame
                char msg[48];
                snprintf_P(msg, sizeof(msg), PSTR("M4A decode error %d"), ret);
                cb.st(ret, msg);
                currentSample++;
            }
            currentSample++;
        } else {
            running = false;
        }
    } else {
        running = false;
    }

done:
    file->loop();
    output->loop();
    return running;
}

// ============================================================
//  ADTS header generation
// ============================================================

int AudioGeneratorM4A::GetSampleRateIndex(int hz)
{
    for (int i = 0; i < 13; i++) {
        if (srLookup[i] == hz) return i;
    }
    return 4; // fallback to 44100
}

int AudioGeneratorM4A::BuildADTSHeader(uint8_t *adts, int frameLength,
                                        int profile, int sri, int chans)
{
    // profile in ADTS = audioObjectType - 1
    int adtsProfile = profile - 1;
    if (adtsProfile < 0) adtsProfile = 0;
    if (adtsProfile > 3) adtsProfile = 3;

    int framesize = frameLength + 7; // data + 7-byte header

    adts[0] = 0xFF;
    // sync bits 8-11 = 0xF, ID=0(MPEG4), layer=00, protection_absent=1
    adts[1] = 0xF1;
    adts[2] = (adtsProfile << 6) | (sri << 2) | ((chans >> 2) & 0x01);
    adts[3] = ((chans & 0x03) << 6) | ((framesize >> 11) & 0x3F);
    adts[4] = (framesize >> 3) & 0xFF;
    adts[5] = ((framesize << 5) & 0xE0) | 0x1F; // buffer_fullness upper
    adts[6] = 0xFC; // buffer_fullness lower + 0 raw blocks

    return 7; // ADTS header size
}

// ============================================================
//  Fill the input buffer: ADTS header + raw AAC data for one sample
// ============================================================

bool AudioGeneratorM4A::FillBufferForSample(uint32_t sampleIdx)
{
    if (sampleIdx >= numSamples) return false;

    const SampleEntry &se = sampleTable[sampleIdx];
    uint32_t rawSize = se.size;
    if (rawSize == 0) return false;
    if (rawSize + 7 > (uint32_t)INPUT_BUF_SIZE) {
        log_e("AAC frame too large: %u bytes", (unsigned)rawSize);
        return false;
    }

    // Seek to the raw AAC data in the file
    if (!SeekTo(se.offset)) return false;

    // Read the raw AAC frame
    uint32_t bytesRead = file->read(buff + 7, rawSize);
    if (bytesRead != rawSize) return false;

    // Prepend ADTS header to make it a valid ADTS frame for Helix
    int sri = GetSampleRateIndex(ascSampleRate);
    int hdrSize = BuildADTSHeader(buff, rawSize,
                                   ascAudioObjectType, sri,
                                   ascChannelConfig);
    (void)hdrSize; // always 7

    buffValid = rawSize + 7;
    return true;
}

// ============================================================
//  Total duration reporting (via metadata callback)
// ============================================================

void AudioGeneratorM4A::ReportTotalDuration()
{
    uint64_t currentAvgBitrate = bitrateSum / bitrateCount;
    if (currentAvgBitrate == 0) currentAvgBitrate = 128000;
    uint64_t totalMs = ((uint64_t)fileSize * 8ULL * 1000ULL) / currentAvgBitrate;
    cb.md("tlen", false, ((String)(unsigned long)totalMs).c_str());
    if (bitrateCount >= 400) {
        totalSent = true;
    }
}

// ============================================================
//  Big-endian read helpers
// ============================================================

bool AudioGeneratorM4A::ReadFromFile(void *ptr, uint32_t len)
{
    return file->read(ptr, len) == len;
}

bool AudioGeneratorM4A::SeekTo(uint32_t pos)
{
    return file->seek((int32_t)pos, 0); // SEEK_SET
}

uint8_t AudioGeneratorM4A::ReadU8()
{
    uint8_t v = 0;
    file->read(&v, 1);
    return v;
}

uint16_t AudioGeneratorM4A::ReadU16BE()
{
    uint8_t b[2];
    if (!ReadFromFile(b, 2)) return 0;
    return ((uint16_t)b[0] << 8) | (uint16_t)b[1];
}

uint32_t AudioGeneratorM4A::ReadU24BE()
{
    uint8_t b[3];
    if (!ReadFromFile(b, 3)) return 0;
    return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
}

uint32_t AudioGeneratorM4A::ReadU32BE()
{
    uint8_t b[4];
    if (!ReadFromFile(b, 4)) return 0;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

uint64_t AudioGeneratorM4A::ReadU64BE()
{
    uint32_t hi = ReadU32BE();
    uint32_t lo = ReadU32BE();
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

bool AudioGeneratorM4A::SkipBytes(uint32_t n)
{
    // Efficient skip: read into a small stack buffer iteratively
    uint8_t tmp[64];
    while (n > 0) {
        uint32_t chunk = (n < sizeof(tmp)) ? n : (uint32_t)sizeof(tmp);
        if (file->read(tmp, chunk) != chunk) return false;
        n -= chunk;
    }
    return true;
}

// ============================================================
//  MP4 box reading helper
// ============================================================
// Returns the size of the box, or 0 on error.
// The type string is written to 'type' (4 chars + null).
// After calling, the file cursor is at the first byte of box content.
// ============================================================

static bool ReadBoxHeader(AudioFileSource *file,
                          uint32_t *boxSize, char *type,
                          uint32_t *headerBytes)
{
    uint8_t hdr[8];
    if (file->read(hdr, 8) != 8) return false;

    *boxSize = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
               ((uint32_t)hdr[2] << 8)  | (uint32_t)hdr[3];
    type[0] = (char)hdr[4];
    type[1] = (char)hdr[5];
    type[2] = (char)hdr[6];
    type[3] = (char)hdr[7];
    type[4] = '\0';
    *headerBytes = 8;

    // Extended size (large boxes)
    if (*boxSize == 1) {
        uint8_t ext[8];
        if (file->read(ext, 8) != 8) return false;
        uint64_t largeSize = 0;
        for (int i = 0; i < 8; i++)
            largeSize = (largeSize << 8) | ext[i];
        if (largeSize > 0xFFFFFFFFULL) {
            // Larger than 32 bits — we can't handle this properly,
            // but try to use the lower 32 bits
        }
        *boxSize = (uint32_t)(largeSize & 0xFFFFFFFFULL);
        *headerBytes = 16;
    }

    return true;
}

// ============================================================
//  MP4 Parse helpers — forward declarations
// ============================================================

static bool ParseBoxContent(AudioFileSource *file, uint32_t boxSize,
                            uint32_t headerBytes, const char *targetType,
                            uint32_t *targetBoxSize, uint32_t *targetPos);

// ============================================================
//  ParseAudioSpecificConfig — extract from esds box
// ============================================================

static bool ParseEsds(AudioFileSource *file, uint32_t boxSize,
                      uint32_t headerBytes, uint8_t *ascOut,
                      int *ascLenOut)
{
    // esds (version(8)  flags(24))  ->  ES_Descriptor
    uint32_t contentStart = file->getPos();

    // Skip version(1) + flags(3)
    uint8_t tmp[4];
    if (file->read(tmp, 4) != 4) return false;

    // ES_Descriptor tag (0x03)
    uint8_t tag = 0;
    if (file->read(&tag, 1) != 1) return false;
    if (tag != 0x03) return false;

    // Read descriptor length (variable-length coded)
    uint32_t descLen = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t b;
        if (file->read(&b, 1) != 1) return false;
        descLen = (descLen << 7) | (b & 0x7F);
        if (!(b & 0x80)) break;
    }

    // Skip ES_ID (2 bytes) + streamDependenceFlag/URLFlag/OCRFlag (1 byte)
    if (file->read(tmp, 3) != 3) return false;

    // DecoderConfigDescriptor tag (0x04)
    if (file->read(&tag, 1) != 1) return false;
    if (tag != 0x04) return false;

    // Read descriptor length
    descLen = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t b;
        if (file->read(&b, 1) != 1) return false;
        descLen = (descLen << 7) | (b & 0x7F);
        if (!(b & 0x80)) break;
    }

    // Skip objectTypeIndication(1) + streamType(1) + bufferSizeDB(3) + maxBitrate(4) + avgBitrate(4)
    if (file->read(tmp, 13) != 13) return false;

    // DecoderSpecificInfo tag (0x05)
    if (file->read(&tag, 1) != 1) return false;
    if (tag != 0x05) return false;

    // Read descriptor length
    descLen = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t b;
        if (file->read(&b, 1) != 1) return false;
        descLen = (descLen << 7) | (b & 0x7F);
        if (!(b & 0x80)) break;
    }

    // Read AudioSpecificConfig (2 bytes is enough for AAC-LC)
    int readLen = descLen;
    if (readLen > 4) readLen = 4;
    if (file->read(ascOut, readLen) != (uint32_t)readLen) return false;
    *ascLenOut = readLen;

    return true;
}

// ============================================================
//  Parse a box, optionally locating a sub-box of targetType
// ============================================================

static bool ParseBoxContent(AudioFileSource *file, uint32_t boxSize,
                            uint32_t headerBytes, const char *targetType,
                            uint32_t *targetBoxSize, uint32_t *targetPos)
{
    uint32_t remaining = boxSize - headerBytes;
    uint32_t startPos = file->getPos();
    log_i("ParseBoxContent enter: searching for '%s', startPos=%lu, remaining=%lu",
          targetType, (unsigned long)startPos, (unsigned long)remaining);

    while (remaining >= 8) {
        uint8_t hdr[8];
        if (file->read(hdr, 8) != 8) return false;

        uint32_t subSize = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
                           ((uint32_t)hdr[2] << 8)  | (uint32_t)hdr[3];
        char subType[5];
        subType[0] = (char)hdr[4];
        subType[1] = (char)hdr[5];
        subType[2] = (char)hdr[6];
        subType[3] = (char)hdr[7];
        subType[4] = '\0';

        uint32_t subHdrBytes = 8;
        if (subSize == 1) {
            uint8_t ext[8];
            if (file->read(ext, 8) != 8) return false;
            uint64_t largeSize = 0;
            for (int i = 0; i < 8; i++)
                largeSize = (largeSize << 8) | ext[i];
            subSize = (uint32_t)(largeSize & 0xFFFFFFFFULL);
            subHdrBytes = 16;
        }

        // Check if this is the box we are looking for
        bool found = (strcmp(subType, targetType) == 0);

        // Before recursing or finishing, check if we need to store position
        if (found && targetPos) {
            *targetPos = file->getPos() - subHdrBytes; // point to start of this box
        }
        if (found && targetBoxSize) {
            *targetBoxSize = subSize;
        }

        // If this is the target, return immediately
        if (found) return true;

        // Recurse into known container boxes to find deeper targets
        // This avoids scanning every atom blindly
        const char *containerTypes[] = {
            // stsd is NOT a container — its body is sample descriptions, not boxes.
            // Removing it prevents ParseBoxContent from misreading stsd body data
            // (version/flags/entries) as box headers when searching for stsz/stco.
            "moov", "trak", "mdia", "minf", "stbl",
            "moof", "traf", "mfra", "udta", "meta", "dinf",
            "edts", "meco", "skip", NULL
        };
        bool isContainer = false;
        for (int ci = 0; containerTypes[ci]; ci++) {
            if (strcmp(subType, containerTypes[ci]) == 0) {
                isContainer = true;
                break;
            }
        }

        if (isContainer) {
            // Recurse into the container
            if (ParseBoxContent(file, subSize, subHdrBytes,
                                targetType, targetBoxSize, targetPos))
                return true;
        } else {
            // Skip this box body — use seek for speed and to avoid partial-read failures
            if (subSize > subHdrBytes) {
                uint32_t skip = subSize - subHdrBytes;
                // Seek relative from current position (SEEK_CUR = 1)
                bool skipped = file->seek((int32_t)skip, 1);
                if (!skipped) {
                    // Seek failed, fall back to reading in chunks
                    log_w("Seek-skip of %lu bytes failed; falling back to read", (unsigned long)skip);
                    uint8_t tmp[64];
                    while (skip > 0) {
                        uint32_t chunk = (skip < 64) ? skip : 64;
                        uint32_t rd = file->read(tmp, chunk);
                        if (rd == 0) break; // EOF or error, bail
                        skip -= rd;
                    }
                }
            }
        }

        uint32_t curPos = file->getPos();
        uint32_t consumed = curPos - startPos;
        // Stop if we've exhausted the parent box content
        // (was: consumed >= remaining — a units mismatch bug)
        if (consumed >= (boxSize - headerBytes)) break;
        remaining = boxSize - headerBytes - consumed;
    }

    return false;
}

// ============================================================
//  ParseStbl — extract sample table, sample sizes, chunk offsets
// ============================================================

static bool ParseStblTables(AudioFileSource *file, uint32_t stblSize,
                            uint32_t stblHeaderBytes,
                            AudioGeneratorM4A::SampleEntry **samplesOut,
                            uint32_t *numSamplesOut,
                            uint32_t sampleCount,
                            uint8_t *ascOut, int *ascLenOut)
{
    uint32_t stblStart = file->getPos() - stblHeaderBytes;

    // Parse stsd to get AudioSpecificConfig
    {
        uint32_t stsdSize = 0, stsdPos = 0;
        if (ParseBoxContent(file, stblSize, stblHeaderBytes,
                            "stsd", &stsdSize, &stsdPos)) {
            file->seek((int32_t)stsdPos, 0);

            uint32_t stsdHdr;
            {
                uint8_t hdr[8];
                file->read(hdr, 8);
                stsdHdr = 8;
                uint32_t sz = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
                              ((uint32_t)hdr[2] << 8)  | (uint32_t)hdr[3];
                if (sz == 1) {
                    uint8_t ext[8];
                    file->read(ext, 8);
                    stsdHdr = 16;
                }
            }

            // stsd: version(1) flags(3) entry_count(4)
            uint8_t tmp[8];
            file->read(tmp, 8);

            // Read the first AudioSampleEntry
            // Its format is typically "mp4a"
            uint8_t entryHdr[8];
            if (file->read(entryHdr, 8) != 8) return false;
            uint32_t entrySize = ((uint32_t)entryHdr[0] << 24) |
                                 ((uint32_t)entryHdr[1] << 16) |
                                 ((uint32_t)entryHdr[2] << 8)  |
                                 (uint32_t)entryHdr[3];
            // entryType = entryHdr[4..7]  (should be "mp4a" or "enca")

            // Skip AudioSampleEntry fixed fields:
            // 6 bytes reserved, 2 bytes data_reference_index
            file->read(tmp, 8);

            // Skip channelcount(2) samplesize(2) predefined(2) reserved(2)
            // Then sample_rate (32-bit fixed-point 16.16) = 4 bytes
            // Total: 12 bytes
            file->read(tmp, 12);

            bool esdsParsed = false;
            uint32_t remainingEntry = entrySize - 28; // 8(hdr) + 8 + 8 + 12 = 28
            // Inside the entry we look for "esds" box
            while (remainingEntry >= 8 && !esdsParsed) {
                uint8_t sub[8];
                uint32_t amt = file->read(sub, 8);
                if (amt != 8) break;
                uint32_t subBoxSize = ((uint32_t)sub[0] << 24) |
                                      ((uint32_t)sub[1] << 16) |
                                      ((uint32_t)sub[2] << 8)  |
                                      (uint32_t)sub[3];
                char sbType[5];
                sbType[0] = sub[4]; sbType[1] = sub[5];
                sbType[2] = sub[6]; sbType[3] = sub[7]; sbType[4] = 0;

                if (subBoxSize == 1) {
                    uint8_t ext[8];
                    file->read(ext, 8);
                    uint64_t ls = 0;
                    for (int i = 0; i < 8; i++) ls = (ls << 8) | ext[i];
                    subBoxSize = (uint32_t)(ls & 0xFFFFFFFFULL);
                    remainingEntry -= 8; // we read 8 more bytes for ext size
                }

                uint32_t subHdrBytes = (subBoxSize == 1) ? 16 : 8;

                if (strcmp(sbType, "esds") == 0) {
                    bool ok = ParseEsds(file, subBoxSize, subHdrBytes,
                                        ascOut, ascLenOut);
                    if (!ok) return false;
                    esdsParsed = true;
                    // Don't return — fall through to stsz/stco/stsc parsing
                    remainingEntry -= subBoxSize;
                    continue;
                }

                // Skip the sub-box content
                if (subBoxSize > subHdrBytes) {
                    uint32_t skip = subBoxSize - subHdrBytes;
                    uint8_t t[64];
                    while (skip > 0) {
                        uint32_t ch = (skip < 64) ? skip : 64;
                        file->read(t, ch);
                        skip -= ch;
                    }
                    remainingEntry -= subBoxSize;
                } else {
                    remainingEntry -= subHdrBytes;
                }
            }
        }
    }

    // Re-position to the start of stbl body before searching for sub-boxes
    // (the stsd/esds parsing above left the cursor inside AudioSampleEntry data)
    if (!file->seek((int32_t)(stblStart + stblHeaderBytes), 0)) {
        log_e("Failed to seek to stbl body start for stsz search");
        return false;
    }

    // --- stsz (sample size table) ---
    uint32_t stszSize = 0, stszPos = 0;
    if (!ParseBoxContent(file, stblSize, stblHeaderBytes,
                         "stsz", &stszSize, &stszPos)) {
        return false;
    }
    file->seek((int32_t)stszPos, 0);

    // Read stsz header + data
    {
        uint8_t hdr[8];
        file->read(hdr, 8); // box header
        uint32_t skipSz = 8;
        if (stszSize == 1) {
            file->read(hdr, 8); // extended size
            skipSz = 16;
        }
    }
    // stsz body: version(1) + flags(3) + constant_sample_size(4) + sample_count(4)
    uint8_t stszSkip[4];
    file->read(stszSkip, 4); // skip version + flags
    uint32_t constantSize = 0;
    {
        uint8_t cs[4];
        if (file->read(cs, 4) == 4)
            constantSize = ((uint32_t)cs[0] << 24) | ((uint32_t)cs[1] << 16) |
                           ((uint32_t)cs[2] << 8)  | (uint32_t)cs[3];
    }
    {
        uint8_t sc[4];
        file->read(sc, 4);
        sampleCount = ((uint32_t)sc[0] << 24) | ((uint32_t)sc[1] << 16) |
                      ((uint32_t)sc[2] << 8)  | (uint32_t)sc[3];
    }

    // Re-position before searching for stco
    if (!file->seek((int32_t)(stblStart + stblHeaderBytes), 0)) {
        log_e("Failed to re-seek for stco");
        return false;
    }

    // --- stco (chunk offset table, 32-bit) / co64 (64-bit) ---
    uint32_t chunkOffsetSize = 0, chunkOffsetPos = 0;
    bool useCo64 = false;
    if (!ParseBoxContent(file, stblSize, stblHeaderBytes,
                         "stco", &chunkOffsetSize, &chunkOffsetPos)) {
        // Try co64 (64-bit offsets)
        if (!ParseBoxContent(file, stblSize, stblHeaderBytes,
                             "co64", &chunkOffsetSize, &chunkOffsetPos)) {
            log_e("Neither stco nor co64 found in stbl");
            return false;
        }
        useCo64 = true;
        log_i("Using co64 (64-bit chunk offsets)");
    }
    file->seek((int32_t)chunkOffsetPos, 0);

    // Read chunk offset box header + body
    {
        uint8_t hdr[8];
        file->read(hdr, 8);
        if (chunkOffsetSize == 1) {
            file->read(hdr, 8);
        }
    }
    // Body: version(1) + flags(3) + entry_count(4)
    uint8_t coSkip[4];
    file->read(coSkip, 4); // skip version + flags
    uint8_t coBody[4];
    file->read(coBody, 4);
    uint32_t numChunks = ((uint32_t)coBody[0] << 24) |
                         ((uint32_t)coBody[1] << 16) |
                         ((uint32_t)coBody[2] << 8)  |
                         (uint32_t)coBody[3];
    // Allocate chunk offset array
    uint32_t *chunkOffsets = (uint32_t*)malloc(numChunks * sizeof(uint32_t));
    if (!chunkOffsets) return false;
    for (uint32_t i = 0; i < numChunks; i++) {
        if (useCo64) {
            uint8_t off[8];
            file->read(off, 8);
            uint64_t val = ((uint64_t)off[0] << 56) | ((uint64_t)off[1] << 48) |
                            ((uint64_t)off[2] << 40) | ((uint64_t)off[3] << 32) |
                            ((uint64_t)off[4] << 24) | ((uint64_t)off[5] << 16) |
                            ((uint64_t)off[6] << 8)  | (uint64_t)off[7];
            if (val > 0xFFFFFFFFULL) {
                log_w("co64 offset >4GB, truncating: %llu", (unsigned long long)val);
            }
            chunkOffsets[i] = (uint32_t)(val & 0xFFFFFFFFULL);
        } else {
            uint8_t off[4];
            file->read(off, 4);
            chunkOffsets[i] = ((uint32_t)off[0] << 24) |
                              ((uint32_t)off[1] << 16) |
                              ((uint32_t)off[2] << 8)  |
                              (uint32_t)off[3];
        }
    }

    // Re-position before searching for stsc
    if (!file->seek((int32_t)(stblStart + stblHeaderBytes), 0)) {
        log_e("Failed to re-seek for stsc");
        return false;
    }

    // --- stsc (sample-to-chunk table) ---
    uint32_t stscSize = 0, stscPos = 0;
    if (!ParseBoxContent(file, stblSize, stblHeaderBytes,
                         "stsc", &stscSize, &stscPos)) {
        free(chunkOffsets);
        return false;
    }
    file->seek((int32_t)stscPos, 0);

    // Read stsc header + body
    {
        uint8_t hdr[8];
        file->read(hdr, 8);
        uint32_t skipHdr = 8;
        if (stscSize == 1) {
            file->read(hdr, 8);
            skipHdr = 16;
        }
    }
    // stsc body: version(1) + flags(3) + entry_count(4)
    uint8_t stscSkip[4];
    file->read(stscSkip, 4); // skip version + flags
    uint8_t stscCount[4];
    file->read(stscCount, 4);
    uint32_t numStscEntries = ((uint32_t)stscCount[0] << 24) |
                              ((uint32_t)stscCount[1] << 16) |
                              ((uint32_t)stscCount[2] << 8)  |
                              (uint32_t)stscCount[3];
    // Allocate stsc entries
    struct StscEntry {
        uint32_t firstChunk;       // 1-based
        uint32_t samplesPerChunk;
        uint32_t sampleDescIndex;  // 1-based
    };
    StscEntry *stscEntries = (StscEntry*)malloc(numStscEntries * sizeof(StscEntry));
    if (!stscEntries) { free(chunkOffsets); return false; }
    for (uint32_t i = 0; i < numStscEntries; i++) {
        uint8_t e[12];
        file->read(e, 12);
        stscEntries[i].firstChunk = ((uint32_t)e[0] << 24) | ((uint32_t)e[1] << 16) |
                                     ((uint32_t)e[2] << 8)  | (uint32_t)e[3];
        stscEntries[i].samplesPerChunk = ((uint32_t)e[4] << 24) | ((uint32_t)e[5] << 16) |
                                          ((uint32_t)e[6] << 8)  | (uint32_t)e[7];
        stscEntries[i].sampleDescIndex = ((uint32_t)e[8] << 24) | ((uint32_t)e[9] << 16) |
                                          ((uint32_t)e[10] << 8) | (uint32_t)e[11];
    }

    // --- Read all sample sizes into an array ---
    uint32_t *sampleSizes = (uint32_t*)malloc(sampleCount * sizeof(uint32_t));
    if (!sampleSizes) {
        free(chunkOffsets);
        free(stscEntries);
        return false;
    }
    if (constantSize > 0) {
        // Constant size: all samples have the same size
        for (uint32_t i = 0; i < sampleCount; i++) {
            sampleSizes[i] = constantSize;
        }
    } else {
        // Variable size: re-seek to stsz and read the individual per-sample sizes
        file->seek((int32_t)stszPos, 0);
        {
            uint8_t hdr[8];
            file->read(hdr, 8);
            if (stszSize == 1) file->read(hdr, 8);
        }
        // Skip version(1) flags(3) constant_size(4) sample_count(4)
        uint8_t stszHdr[12];
        file->read(stszHdr, 12);
        for (uint32_t i = 0; i < sampleCount; i++) {
            uint8_t sz[4];
            file->read(sz, 4);
            sampleSizes[i] = ((uint32_t)sz[0] << 24) | ((uint32_t)sz[1] << 16) |
                              ((uint32_t)sz[2] << 8) | (uint32_t)sz[3];
        }
    }

    // --- Build the sample table using chunk structure ---
    AudioGeneratorM4A::SampleEntry *samples =
        (AudioGeneratorM4A::SampleEntry*)malloc(sampleCount * sizeof(AudioGeneratorM4A::SampleEntry));
    if (!samples) {
        free(sampleSizes);
        free(chunkOffsets);
        free(stscEntries);
        return false;
    }

    uint32_t sampleIdx = 0;
    for (uint32_t sc = 0; sc < numStscEntries && sampleIdx < sampleCount; sc++) {
        uint32_t firstChunk = stscEntries[sc].firstChunk; // 1-based
        uint32_t lastChunk;
        if (sc + 1 < numStscEntries)
            lastChunk = stscEntries[sc + 1].firstChunk - 1;
        else
            lastChunk = numChunks;

        uint32_t samplesPerChunk = stscEntries[sc].samplesPerChunk;

        for (uint32_t ch = firstChunk; ch <= lastChunk && ch <= numChunks; ch++) {
            uint32_t chunkOffset = chunkOffsets[ch - 1]; // stco is 0-based index

            for (uint32_t s = 0; s < samplesPerChunk && sampleIdx < sampleCount; s++) {
                samples[sampleIdx].offset = chunkOffset;
                samples[sampleIdx].size = sampleSizes[sampleIdx];
                chunkOffset += sampleSizes[sampleIdx];
                sampleIdx++;
            }
        }
    }

    free(sampleSizes);

    *samplesOut = samples;
    *numSamplesOut = sampleCount;

    free(chunkOffsets);
    free(stscEntries);

    return true;
}

// ============================================================
//  ParseMP4 — top-level entry point
// ============================================================

bool AudioGeneratorM4A::ParseMP4()
{
    if (!file) return false;

    // Reset to beginning of file
    if (!SeekTo(0)) return false;

    // Step 1: find the "moov" box by scanning
    // We need to find moov and also optionally handle mdat ordering.
    // Seek to the moov box and hand off to the box parser.

    // Scan from the beginning for moov
    uint32_t moovSize = 0;
    uint32_t moovPos = 0;

    // Read box-by-box at top level until we find moov
    if (!SeekTo(0)) return false;

    // First, check ftyp box
    {
        uint8_t ftyp[8];
        if (file->read(ftyp, 8) != 8) return false;
        uint32_t ftypSize = ((uint32_t)ftyp[0] << 24) | ((uint32_t)ftyp[1] << 16) |
                             ((uint32_t)ftyp[2] << 8)  | (uint32_t)ftyp[3];
        // Skip past ftyp
        if (!SeekTo(ftypSize)) return false;
    }

    // Scan remaining top-level boxes for moov
    char type[5];
    while (true) {
        uint8_t hdr[8];
        if (file->read(hdr, 8) != 8) break;

        uint32_t boxSize = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
                            ((uint32_t)hdr[2] << 8)  | (uint32_t)hdr[3];
        type[0] = (char)hdr[4];
        type[1] = (char)hdr[5];
        type[2] = (char)hdr[6];
        type[3] = (char)hdr[7];
        type[4] = '\0';
        uint32_t hdrBytes = 8;

        if (boxSize == 1) {
            uint8_t ext[8];
            if (file->read(ext, 8) != 8) break;
            uint64_t large = (uint64_t)ext[0] << 56 | (uint64_t)ext[1] << 48 |
                             (uint64_t)ext[2] << 40 | (uint64_t)ext[3] << 32 |
                             (uint64_t)ext[4] << 24 | (uint64_t)ext[5] << 16 |
                             (uint64_t)ext[6] << 8  | (uint64_t)ext[7];
            boxSize = (uint32_t)(large & 0xFFFFFFFFULL);
            hdrBytes = 16;
        }

        if (strcmp(type, "moov") == 0) {
            moovSize = boxSize;
            moovPos = file->getPos() - hdrBytes;
            break;
        }

        // Skip this box
        if (boxSize > hdrBytes) {
            if (!SkipBytes(boxSize - hdrBytes)) break;
        }
    }

    if (moovSize == 0) return false;

    // Step 2: parse the moov box tree to find stbl and extract sample info
    if (!SeekTo(moovPos)) return false;

    // Find stsd inside moov and parse its contents
    // We'll use the recursive box parser
    // First find stbl
    uint32_t stblSize = 0, stblPos = 0;
    // We need to read the moov box header first
    {
        uint8_t moovHdr[8];
        file->read(moovHdr, 8);
        // already at moov position, box header was read at scan time
        // re-read the header
    }

    // Parse the moov box recursing to find stbl
    // Re-seek to moov start
    SeekTo(moovPos);

    // Find stbl by recursing into moov -> trak -> mdia -> minf -> stbl
    // Use our recursive parser to find stbl
    // We need to pass the correct box boundaries
    // First read the moov header
    {
        uint8_t mhdr[8];
        file->read(mhdr, 8);
    }
    uint32_t moovHdrBytes = 8;
    if (moovSize == 1) {
        uint8_t ext[8];
        file->read(ext, 8);
        moovHdrBytes = 16;
    }

    if (!ParseBoxContent(file, moovSize, moovHdrBytes,
                         "stbl", &stblSize, &stblPos)) {
        return false;
    }

    // Step 3: parse the stbl box
    if (stblSize == 0) return false;
    if (!SeekTo(stblPos)) return false;

    // Read stbl header
    uint8_t stblHdr[8];
    file->read(stblHdr, 8);
    uint32_t stblHdrBytes = 8;

    // Now parse stbl contents for stsd, stsz, stco, stsc
    // Note: ParseStblTables also handles AudioSpecificConfig extraction from stsd/esds.
    // After this call, asc[], ascLen, sampleTable, and numSamples are populated.
    // The ASC is decoded in begin() via DecodeAudioSpecificConfig().
    return ParseStblTables(file, stblSize, stblHdrBytes,
                           &sampleTable, &numSamples,
                           0, asc, &ascLen);
}

// ============================================================
//  ParseMP4 — continued: decode AudioSpecificConfig
// ============================================================

// Note: AudioSpecificConfig decoding is done after ParseMP4 returns in begin().
// The 2-byte (or more) ASC in asc[] is decoded to populate:
//   ascAudioObjectType, ascSampleRate, ascChannelConfig
// This is called from begin() after ParseMP4() succeeds.

// We place it here for clarity.
static bool DecodeAudioSpecificConfig(const uint8_t *asc, int ascLen,
                                       int *audioObjectType,
                                       int *sampleRate,
                                       int *channelConfig)
{
    if (ascLen < 2) return false;

    // Byte 0: [AOT bits 4-0 (5 bits)] [SRI bits 3-1 (3 bits)]
    // Byte 1: [SRI bit 0 (1 bit)] [CC bits 2-0 (3 bits)] ...
    *audioObjectType = (asc[0] >> 3) & 0x1F;
    int sri = ((asc[0] & 0x07) << 1) | ((asc[1] >> 7) & 0x01);
    *channelConfig = (asc[1] >> 3) & 0x07;

    // Handle SBR signaling (AOT == 5 means SBR, real AOT follows)
    // For basic AAC-LC this isn't needed, but we handle the common case
    if (*audioObjectType == 5 && ascLen >= 4) {
        // SBR with implicit signaling: sri from second part
        // The real AOT is in the extension part
        // For simplicity, keep AAC-LC decoding if detected
        if (ascLen >= 4) {
            int extAOT = (asc[2] >> 3) & 0x1F;
            if (extAOT == 2) { // AAC-LC with SBR
                *audioObjectType = 2;
                // The sample rate from ASC is the output rate (SBR doubles it)
                // But we keep sri as-is for ADTS
            }
        }
    }

    // Map sample rate index to Hz
    static const int srTable[13] = {
        96000, 88200, 64000, 48000, 44100,
        32000, 24000, 22050, 16000, 12000,
        11025, 8000, 7350
    };
    if (sri >= 0 && sri <= 12) {
        *sampleRate = srTable[sri];
    } else if (sri == 13 || sri == 14) {
        // Reserved — fall back to 44100
        *sampleRate = 44100;
    } else if (sri == 15) {
        // Explicit frequency follows in the ASC (3 bytes after the index)
        // We don't parse this for now — fall back to 44100
        *sampleRate = 44100;
    } else {
        *sampleRate = 44100;
    }

    return true;
}

// ============================================================
//  EOF
// ============================================================
