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
  buffSize = 128;
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
  if (!running) return true;
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
  if (!running) return false; // Nothing to do here!
  uint8_t *p = reinterpret_cast<uint8_t*>(dest);
  while (bytes--) {
    // Potentially load next batch of data...
    if (buffPtr >= buffLen) {
      buffPtr = 0;
      uint32_t toRead = availBytes > buffSize ? buffSize : availBytes;
      buffLen = file->read( buff, toRead );
      availBytes -= buffLen;
    }
    if (buffPtr >= buffLen)
      return false; // No data left!
    *(p++) = buff[buffPtr++];
  }
  return true;
}

bool AudioGeneratorWAV::loop() {
    if (!running) goto done;

    // 先尝试发送上一次处理好的样本
    if (!output->ConsumeSample(lastSample)) goto done;

    do {
        if (bitsPerSample == 8) {
            // 8位无符号扩展为32位有符号
            uint8_t l, r;
            if (!GetBufferedData(1, &l)) stop();
            int32_t left  = ((int32_t)l - 128) << 24;   // 转为有符号，再左移24位
            lastSample[0] = left;

            if (channels == 2) {
                if (!GetBufferedData(1, &r)) stop();
                int32_t right = ((int32_t)r - 128) << 24;
                lastSample[1] = right;
            } else {
                lastSample[1] = 0;
            }

        } else if (bitsPerSample == 16) {
            // 16位有符号左移16位
            uint16_t l, r; // 先读取原始字节，再转换为 int16_t
            if (!GetBufferedData(2, &l)) stop();
            int32_t left = (int16_t)l << 16;   // 有符号扩展后左移16位
            lastSample[0] = left;

            if (channels == 2) {
                if (!GetBufferedData(2, &r)) stop();
                int32_t right = (int16_t)r << 16;
                lastSample[1] = right;
            } else {
                lastSample[1] = 0;
            }

        } else if (bitsPerSample == 24) {
            // 24位有符号，读取3字节，符号扩展到32位，再左移8位
            uint8_t raw[3];
            int32_t sample;
            if (!GetBufferedData(3, raw)) stop();
            sample = raw[0] | (raw[1] << 8) | (raw[2] << 16); // 小端
            if (sample & 0x800000) sample |= 0xFF000000;      // 符号扩展
            lastSample[0] = sample << 8;                     // 左移8位

            if (channels == 2) {
                if (!GetBufferedData(3, raw)) stop();
                sample = raw[0] | (raw[1] << 8) | (raw[2] << 16);
                if (sample & 0x800000) sample |= 0xFF000000;
                lastSample[1] = sample << 8;
            } else {
                lastSample[1] = 0;
            }

        } else if (bitsPerSample == 32) {
            // 32位有符号直接读取
            if (!GetBufferedData(4, &lastSample[0])) stop();
            if (channels == 2) {
                if (!GetBufferedData(4, &lastSample[1])) stop();
            } else {
                lastSample[1] = 0;
            }
        }
    } while (running && output->ConsumeSample(lastSample));

done:
    file->loop();
    output->loop();
    return running;
}


bool AudioGeneratorWAV::ReadWAVInfo()
{
  uint32_t u32;
  uint16_t u16;
  int toSkip;

  // WAV specification document:
  // https://www.aelius.com/njh/wavemetatools/doc/riffmci.pdf

  // Header == "RIFF"
  if (!ReadU32(&u32)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if (u32 != 0x46464952) {
    log_e("cannot read WAV, invalid RIFF header\n");
    return false;
  }

  // Skip ChunkSize
  if (!ReadU32(&u32)) {
    log_e("failed to read WAV data\n");
    return false;
  };

  // Format == "WAVE"
  if (!ReadU32(&u32)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if (u32 != 0x45564157) {
    log_e("cannot read WAV, invalid WAVE header\n");
    return false;
  }

  // there might be JUNK or PAD - ignore it by continuing reading until we get to "fmt "
  while (1) {
    if (!ReadU32(&u32)) {
      log_e("failed to read WAV data\n");
      return false;
    };
    if (u32 == 0x20746d66) break; // 'fmt '
  };

  // subchunk size
  if (!ReadU32(&u32)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if (u32 == 16) { toSkip = 0; }
  else if (u32 == 18) { toSkip = 18 - 16; }
  else if (u32 == 40) { toSkip = 40 - 16; }
  else {
    log_e("cannot read WAV, appears not to be standard PCM \n");
    return false;
  } // we only do standard PCM

  // AudioFormat
  if (!ReadU16(&u16)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if (u16 != 1) {
    log_e("cannot read WAV, AudioFormat appears not to be standard PCM \n");
    return false;
  } // we only do standard PCM

  // NumChannels
  if (!ReadU16(&channels)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if ((channels<1) || (channels>2)) {
    log_e("cannot read WAV, only mono and stereo are supported \n");
    return false;
  } // Mono or stereo support only

  // SampleRate
  if (!ReadU32(&sampleRate)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if (sampleRate < 1) {
    log_e("cannot read WAV, unknown sample rate \n");
    return false;
  }  // Weird rate, punt.  Will need to check w/DAC to see if supported

  // Ignore byterate and blockalign
  if (!ReadU32(&u32)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if (!ReadU16(&u16)) {
    log_e("failed to read WAV data\n");
    return false;
  };

  // Bits per sample
  if (!ReadU16(&bitsPerSample)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  if ((bitsPerSample != 8) && (bitsPerSample != 16) &&
    (bitsPerSample != 24) && (bitsPerSample != 32)) {
    log_e("only 8/16/24/32 bits supported\n");
    return false;
  }

  // Skip any extra header
  while (toSkip) {
    uint8_t ign;
    if (!ReadU8(&ign)) {
      log_e("failed to read WAV data\n");
      return false;
    };
    toSkip--;
  }

  // look for data subchunk
  do {
    // id == "data"
    if (!ReadU32(&u32)) {
      log_e("failed to read WAV data\n");
      return false;
    };
    if (u32 == 0x61746164) break; // "data"
    // Skip size, read until end of chunk
    if (!ReadU32(&u32)) {
      log_e("failed to read WAV data\n");
      return false;
    };
    if(!file->seek(u32, SEEK_CUR)) {
      log_e("failed to read WAV data, seek failed\n");
      return false;
    }
  } while (1);
  if (!file->isOpen()) {
    log_e("cannot read WAV, file is not open\n");
    return false;
  };

  // Skip size, read until end of file...
  if (!ReadU32(&u32)) {
    log_e("failed to read WAV data\n");
    return false;
  };
  availBytes = u32;

  // Now set up the buffer or fail
  buff = reinterpret_cast<uint8_t *>(malloc(buffSize));
  if (!buff) {
    log_e("cannot read WAV, failed to set up buffer \n");
    return false;
  };
  buffPtr = 0;
  buffLen = 0;

  return true;
}

bool AudioGeneratorWAV::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source) {
    log_e("failed: invalid source\n");
    return false;
  }
  file = source;
  if (!output) {
    log_e("invalid output\n");
    return false;
  }
  this->output = output;
  if (!file->isOpen()) {
    log_e("file not open\n");
    return false;
  } // Error

  if (!ReadWAVInfo()) {
    log_e("failed during ReadWAVInfo\n");
    return false;
  }

  if (!output->SetRate( sampleRate )) {
    log_e("failed to SetRate in output\n");
    return false;
  }
  if (!output->SetBitsPerSample( bitsPerSample )) {
    log_e("failed to SetBitsPerSample in output\n");
    return false;
  }
  if (!output->SetChannels( channels )) {
    log_e("failed to SetChannels in output\n");
    return false;
  }
  if (!output->begin()) {
    log_e("output's begin did not return true\n");
    return false;
  }

  running = true;

  return true;
}
