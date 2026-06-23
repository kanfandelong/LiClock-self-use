/*
    AudioGeneratorWAV
    Audio output generator that reads 8 and 16-bit WAV files

    Copyright (C) 2017  Earle F. Philhower, III

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

#include "AudioGeneratorWAV.h"

AudioGeneratorWAV::AudioGeneratorWAV()
{
    running = false;
    file = NULL;
    output = NULL;
    buffSize = 4096;
    buff = NULL;
    buffPtr = 0;
    buffLen = 0;
}

AudioGeneratorWAV::~AudioGeneratorWAV()
{
    free(buff);
    buff = NULL;
}

bool AudioGeneratorWAV::stop()
{
    if (!running)
    {
        return true;
    }
    running = false;
    free(buff);
    buff = NULL;
    output->stop();
    return file->close();
}

bool AudioGeneratorWAV::isRunning()
{
    return running;
}

// Handle buffered reading, reload each time we run out of data
bool AudioGeneratorWAV::GetBufferedData(int bytes, void *dest)
{
    if (!running)
    {
        return false; // Nothing to do here!
    }
    uint8_t *p = reinterpret_cast<uint8_t *>(dest);
    while (bytes--)
    {
        // Potentially load next batch of data...
        if (buffPtr >= buffLen)
        {
            buffPtr = 0;
            uint32_t toRead = availBytes > buffSize ? buffSize : availBytes;
            buffLen = file->read(buff, toRead);
            availBytes -= buffLen;
        }
        if (buffPtr >= buffLen)
        {
            return false; // No data left!
        }
        *(p++) = buff[buffPtr++];
    }
    return true;
}

bool AudioGeneratorWAV::loop()
{
    if (!running)
    {
        goto done; // Nothing to do here!
    }

    // First, try and push in the stored sample.  If we can't, then punt and try later
    if (!output->ConsumeSample(lastSample))
    {
        goto done; // Can't send, but no error detected
    }

    // Try and stuff the buffer one sample at a time
    do
    {
        if (bitsPerSample == 8)
        {
            int8_t u8s; // For read u8 sample
            if (!GetBufferedData(1, &u8s))
            {
                stop();
            }
            int32_t sampleL = u8s << 24;
            lastSample[AudioOutput::LEFTCHANNEL] = sampleL;
            if (channels == 2)
            {
                if (!GetBufferedData(1, &u8s))
                {
                    stop();
                }
                int32_t sampleR = u8s << 24;
                lastSample[AudioOutput::RIGHTCHANNEL] = sampleR;
            }
            else
            {
                lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
            }
        }
        else if (bitsPerSample == 16)
        {
            int16_t sample[2];
            if (!GetBufferedData(2, &sample[AudioOutput::LEFTCHANNEL]))
            {
                stop();
            }
            if (channels == 2)
            {
                if (!GetBufferedData(2, &sample[AudioOutput::RIGHTCHANNEL]))
                {
                    stop();
                }
            }
            else
            {
                sample[AudioOutput::RIGHTCHANNEL] = sample[AudioOutput::LEFTCHANNEL];
            }
            lastSample[AudioOutput::LEFTCHANNEL] = sample[AudioOutput::LEFTCHANNEL] << 16;
            lastSample[AudioOutput::RIGHTCHANNEL] = sample[AudioOutput::RIGHTCHANNEL] << 16;
        }
        else if (bitsPerSample == 24)
        {
            uint8_t b[3]; // 临时存储 3 字节小端数据

            // ---- 读取左声道 ----
            if (!GetBufferedData(3, b))
            {
                stop();
            }
            int32_t sampleL = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16));
            // 24位有符号数符号扩展：如果最高位（bit23）为1，则扩展高8位为全1
            if (sampleL & 0x800000)
                sampleL |= 0xFF000000;
            // 左对齐：左移8位，使24位数据占据高24位
            lastSample[AudioOutput::LEFTCHANNEL] = sampleL << 8;

            // ---- 读取右声道 ----
            if (channels == 2)
            {
                if (!GetBufferedData(3, b))
                {
                    stop();
                }
                int32_t sampleR = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16));
                if (sampleR & 0x800000)
                    sampleR |= 0xFF000000;
                lastSample[AudioOutput::RIGHTCHANNEL] = sampleR << 8;
            }
            else
            {
                // 单声道：左右相同
                lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
            }
        }
        else if (bitsPerSample == 32)
        {
            if (!GetBufferedData(4, &lastSample[AudioOutput::LEFTCHANNEL]))
            {
                stop();
            }
            if (channels == 2)
            {
                if (!GetBufferedData(4, &lastSample[AudioOutput::RIGHTCHANNEL]))
                {
                    stop();
                }
            }
            else
            {
                lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
            }
        }
    } while (running && output->ConsumeSample(lastSample));

done:
    file->loop();
    output->loop();

    return running;
}

static const char *MapWavTagToFlac(uint32_t id)
{
    switch (id)
    {
    case 0x54524149:
        return "ARTIST"; // "IART"
    case 0x4d414e49:
        return "TITLE"; // "INAM"
    case 0x44525049:
        return "ALBUM"; // "IPRD"
    case 0x544d4349:
        return "COMMENT"; // "ICMT"
    case 0x504f4349:
        return "COPYRIGHT"; // "ICOP"
    case 0x44524349:
        return "DATE"; // "ICRD"
    case 0x524e4749:
        return "GENRE"; // "IGNR"
    case 0x4b525449:
        return "TRACKNUMBER"; // "ITRK"
    default:
        return nullptr; // 不识别则忽略
    }
}

bool AudioGeneratorWAV::ReadWAVInfo()
{
    uint32_t u32;
    uint16_t u16;
    int toSkip;
    uint16_t audioFormat;

    // ---- RIFF 头部校验 (省略，与原代码相同) ----
    if (!ReadU32(&u32))
    {
        log_printf("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n");
        return false;
    };
    if (u32 != 0x46464952)
    {
        log_printf("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, invalid RIFF header\n");
        return false;
    }

    // Skip ChunkSize
    if (!ReadU32(&u32))
    {
        log_printf("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n");
        return false;
    };

    // Format == "WAVE"
    if (!ReadU32(&u32))
    {
        log_printf("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n");
        return false;
    };
    if (u32 != 0x45564157)
    {
        log_printf("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, invalid WAVE header\n");
        return false;
    }

    // ---- 查找 "fmt " ----
    while (1)
    {
        if (!ReadU32(&u32))
            return false;
        if (u32 == 0x20746d66)
            break;
    };

    // ---- 读取 fmt chunk ----
    uint32_t fmtChunkSize;
    if (!ReadU32(&fmtChunkSize))
        return false;

    // 读取标准字段（16 字节）
    if (!ReadU16(&audioFormat))
        return false;
    if (!ReadU16(&channels))
        return false;
    if ((channels < 1) || (channels > 2))
    {
        log_printf("Only mono/stereo supported\n");
        return false;
    }
    if (!ReadU32(&sampleRate))
        return false;
    if (!ReadU32(&u32))
        return false; // byteRate
    if (!ReadU16(&u16))
        return false; // blockAlign
    if (!ReadU16(&bitsPerSample))
        return false;

    // ---- 处理扩展格式 ----
    if (audioFormat == 1)
    {
        toSkip = fmtChunkSize - 16;
        while (toSkip--)
        {
            uint8_t ign;
            ReadU8(&ign);
        }
    }
    else if (audioFormat == 0xFFFE)
    {
        uint16_t cbSize;
        if (!ReadU16(&cbSize))
            return false;
        if (cbSize < 22)
            return false;
        uint16_t validBits;
        if (!ReadU16(&validBits))
            return false;
        uint32_t channelMask;
        if (!ReadU32(&channelMask))
            return false;
        uint8_t guid[16];
        for (int i = 0; i < 16; i++)
            ReadU8(&guid[i]);
        uint32_t subFormat;
        memcpy(&subFormat, guid, 4);
        if (subFormat != 0x00000001)
        {
            log_printf("Not PCM subformat\n");
            return false;
        }
        int remaining = cbSize - 22;
        while (remaining--)
        {
            uint8_t ign;
            ReadU8(&ign);
        }
    }
    else
    {
        log_printf("Unsupported audio format 0x%04X\n", audioFormat);
        return false;
    }

    // ---- 位深检查（允许 8/16/24/32） ----
    if ((bitsPerSample != 8) && (bitsPerSample != 16) && (bitsPerSample != 24) && (bitsPerSample != 32))
    {
        log_printf("Unsupported bits per sample\n");
        return false;
    }

    // ===== 查找 "data" 并解析 LIST =====
    do
    {
        if (!ReadU32(&u32))
            return false;
        if (u32 == 0x61746164)
            break; // "data"

        uint32_t chunkSize;
        if (!ReadU32(&chunkSize))
            return false;

        if (u32 == 0x5453494c)
        { // "LIST"
            uint32_t listType;
            if (!ReadU32(&listType))
                return false;
            uint32_t bytesLeft = chunkSize - 4;

            if (listType == 0x4f464e49)
            { // "INFO"
                while (bytesLeft > 8)
                {
                    uint32_t subId, subSize;
                    if (!ReadU32(&subId))
                        break;
                    if (!ReadU32(&subSize))
                        break;
                    bytesLeft -= 8;
                    if (bytesLeft < subSize)
                    {
                        if (bytesLeft > 0)
                            file->seek(bytesLeft, SEEK_CUR);
                        bytesLeft = 0;
                        break;
                    }
                    char *dataBuf = (char *)malloc(subSize + 1);
                    if (dataBuf)
                    {
                        if (file->read((uint8_t *)dataBuf, subSize) == subSize)
                        {
                            dataBuf[subSize] = '\0';
                            const char *flacKey = MapWavTagToFlac(subId);
                            if (flacKey)
                            {
                                cb.md(flacKey, false, dataBuf);
                            }
                        }
                        free(dataBuf);
                    }
                    else
                    {
                        file->seek(subSize, SEEK_CUR);
                    }
                    bytesLeft -= subSize;
                    // 处理奇数字节填充
                    if (subSize % 2 == 1 && bytesLeft > 0)
                    {
                        uint8_t pad;
                        file->read(&pad, 1);
                        bytesLeft--;
                    }
                }
                if (bytesLeft > 0)
                    file->seek(bytesLeft, SEEK_CUR);
            }
            else
            {
                file->seek(bytesLeft, SEEK_CUR);
            }
        }
        else
        {
            // 其他块直接跳过
            file->seek(chunkSize, SEEK_CUR);
        }
    } while (1);

    // ---- 读取 data 块大小 ----
    if (!ReadU32(&u32))
        return false;
    availBytes = u32;

    // ===== 计算并回调总时长 =====
    uint32_t blockAlign = channels * (bitsPerSample / 8);
    if (blockAlign > 0 && sampleRate > 0 && availBytes >= blockAlign)
    {
        output->SetRate(sampleRate);
        output->SetBitsPerSample(bitsPerSample);
        output->SetChannels(channels);
        uint64_t totalSamples = availBytes / blockAlign;
        uint64_t totalMs = totalSamples * 1000 / sampleRate;
        char msStr[32];
        sprintf(msStr, "%llu", totalMs);
        cb.md("tlen", false, msStr);
    }

    // ---- 分配缓冲区 ----
    buff = (uint8_t *)malloc(buffSize);
    if (!buff)
        return false;
    buffPtr = 0;
    buffLen = 0;

    return true;
}

bool AudioGeneratorWAV::begin(AudioFileSource *source, AudioOutput *output)
{
    if (!source)
    {
        log_printf("AudioGeneratorWAV::begin: failed: invalid source\n");
        return false;
    }
    file = source;
    if (!output)
    {
        log_printf("AudioGeneratorWAV::begin: invalid output\n");
        return false;
    }
    this->output = output;
    if (!file->isOpen())
    {
        log_printf("AudioGeneratorWAV::begin: file not open\n");
        return false;
    } // Error

    if (!ReadWAVInfo())
    {
        log_printf("AudioGeneratorWAV::begin: failed during ReadWAVInfo\n");
        return false;
    }

    if (!output->SetRate(sampleRate))
    {
        log_printf("AudioGeneratorWAV::begin: failed to SetRate in output\n");
        return false;
    }
    if (!output->SetChannels(channels))
    {
        log_printf("AudioGeneratorWAV::begin: failed to SetChannels in output\n");
        return false;
    }
    if (!output->begin())
    {
        log_printf("AudioGeneratorWAV::begin: output's begin did not return true\n");
        return false;
    }

    running = true;

    return true;
}
