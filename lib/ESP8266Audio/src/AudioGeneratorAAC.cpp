/*
  AudioGeneratorAAC
  Audio output generator using the Helix AAC decoder
  
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

#pragma GCC optimize ("O3")

#include "AudioGeneratorAAC.h"

AudioGeneratorAAC::AudioGeneratorAAC()
{
  preallocateSpace = NULL;
  preallocateSize = 0;

  running = false;
  file = NULL;
  output = NULL;

  buff = (uint8_t*)malloc(buffLen);
  outSample = (int16_t*)malloc(1024 * 2 * sizeof(uint16_t));
  if (!buff || !outSample) {
    log_e("Out of memory in AAC");
  }

  hAACDecoder = AACInitDecoder();
  if (!hAACDecoder) {
    log_e("Out of memory error! hAACDecoder==NULL");
  }

  buffValid = 0;
  lastFrameEnd = 0;
  validSamples = 0;
  curSample = 0;
  lastRate = 0;
  lastChannels = 0;
}

AudioGeneratorAAC::AudioGeneratorAAC(void *preallocateData, int preallocateSz)
{
  preallocateSpace = preallocateData;
  preallocateSize = preallocateSz;

  running = false;
  file = NULL;
  output = NULL;

  uint8_t *p = (uint8_t*)preallocateSpace;
  buff = (uint8_t*) p;
  p += (buffLen + 7) & ~7;
  outSample = (int16_t*) p;
  p += (1024 * 2 * sizeof(int16_t) + 7) & ~7;
  int used = p - (uint8_t*)preallocateSpace;
  int availSpace = preallocateSize - used;
  if (availSpace < 0 ) {
    log_e("Out of memory in AAC");
  }

  hAACDecoder = AACInitDecoderPre(p, availSpace);
  if (!hAACDecoder) {
    log_e("Out of memory error! hAACDecoder==NULL");
  }
  buffValid = 0;
  lastFrameEnd = 0;
  validSamples = 0;
  curSample = 0;
  lastRate = 0;
  lastChannels = 0;
}



AudioGeneratorAAC::~AudioGeneratorAAC()
{
  if (!preallocateSpace) {
    AACFreeDecoder(hAACDecoder);
    free(buff);
    free(outSample);
  }
}

bool AudioGeneratorAAC::stop()
{
  running = false;
  output->stop();
  return file->close();
}

bool AudioGeneratorAAC::isRunning()
{
  return running;
}

bool AudioGeneratorAAC::FillBufferWithValidFrame()
{
  buff[0] = 0; // Destroy any existing sync word @ 0
  int nextSync;
  do {
    nextSync = AACFindSyncWord(buff + lastFrameEnd, buffValid - lastFrameEnd);
    if (nextSync >= 0) nextSync += lastFrameEnd;
    lastFrameEnd = 0;
    if (nextSync == -1) {
      if (buffValid && buff[buffValid-1]==0xff) { // Could be 1st half of syncword, preserve it...
        buff[0] = 0xff;
        buffValid = file->read(buff+1, buffLen-1);
        if (buffValid==0) return false; // No data available, EOF
      } else { // Try a whole new buffer
        buffValid = file->read(buff, buffLen-1);
        if (buffValid==0) return false; // No data available, EOF
      }
    }
  } while (nextSync == -1);

  // Move the frame to start at offset 0 in the buffer
  buffValid -= nextSync; // Throw out prior to nextSync
  memmove(buff, buff+nextSync, buffValid);

  // We have a sync word at 0 now, try and fill remainder of buffer
  buffValid += file->read(buff + buffValid, buffLen - buffValid);

  return true;
}

bool AudioGeneratorAAC::loop()
{
  if (!running) goto done; // Nothing to do here!

  // If we've got data, try and pump it out...
  while (validSamples) {
#ifdef CONFIG_DAC_32bit
    if (lastChannels == 1) {
       lastSample[0] = outSample[curSample] << 16;
       lastSample[1] = outSample[curSample] << 16;
    } else if (lastChannels == 2) {
      lastSample[0] = outSample[curSample*2] << 16;
      lastSample[1] = outSample[curSample*2 + 1] << 16;
    } else {
      // Mix multi-channel audio to stereo
      int32_t left = 0, right = 0;
      for (int i = 0; i < lastChannels; i++) {
        if (i % 2 == 0) {
          left += outSample[curSample * lastChannels + i];
        } else {
          right += outSample[curSample * lastChannels + i];
        }
      }
      lastSample[0] = (left / (lastChannels / 2)) << 16;
      lastSample[1] = (right / (lastChannels / 2)) << 16;
    }
#else
    if (lastChannels == 1) {
       lastSample[0] = outSample[curSample];
       lastSample[1] = outSample[curSample];
    } else if (lastChannels == 2) {
      lastSample[0] = outSample[curSample*2];
      lastSample[1] = outSample[curSample*2 + 1];
    } else {
      // Mix multi-channel audio to stereo
      int32_t left = 0, right = 0;
      for (int i = 0; i < lastChannels; i++) {
        if (i % 2 == 0) {
          left += outSample[curSample * lastChannels + i];
        } else {
          right += outSample[curSample * lastChannels + i];
        }
      }
      lastSample[0] = left / (lastChannels / 2);
      lastSample[1] = right / (lastChannels / 2);
    }
#endif
    if (!output->ConsumeSample(lastSample)) goto done; // Can't send, but no error detected
    validSamples--;
    curSample++;
  }

  // No samples available, need to decode a new frame
  if (FillBufferWithValidFrame()) {
    // buff[0] start of frame, decode it...
    unsigned char *inBuff = reinterpret_cast<unsigned char *>(buff);
    int bytesLeft = buffValid;
    int ret = AACDecode(hAACDecoder, &inBuff, &bytesLeft, outSample);
    if (ret) {
      // Error, skip the frame...
      char buff[48];
      sprintf_P(buff, PSTR("AAC decode error %d"), ret);
      cb.st(ret, buff);
    } else {
      lastFrameEnd = buffValid - bytesLeft;
      AACFrameInfo fi;
      AACGetLastFrameInfo(hAACDecoder, &fi);
      // Track bitrate to estimate total duration (similar to MP3 implementation)
      if (!totalSent && fi.bitRate > 0) {
        bitrateSum += (uint64_t)fi.bitRate;
        bitrateCount++;
      }
      if (bitrateCount >= 50 && !totalSent) {
        uint64_t currentAvgBitrate = bitrateSum / bitrateCount;
        if (bitrateCount == 50 || bitrateCount % 50 == 0) {
          if (currentAvgBitrate == 0) currentAvgBitrate = fi.bitRate;
          if (currentAvgBitrate == 0) currentAvgBitrate = 128000;
          // Estimate total duration based on file size and average bitrate
          uint64_t total_seconds = ((uint64_t)file->getSize() * 8ULL) / currentAvgBitrate;
          uint64_t total_ms = total_seconds * 1000ULL;
          cb.md("tlen", false, ((String)total_ms).c_str());
          lastAvgBitrate = currentAvgBitrate;
        }
        if (bitrateCount >= 400) {
          totalSent = true;
        }
      }
      if ((int)fi.sampRateOut != (int)lastRate) {
        output->SetRate(fi.sampRateOut);
        lastRate = fi.sampRateOut;
      }
      if (fi.nChans != lastChannels) {
        output->SetChannels(fi.nChans);
        lastChannels = fi.nChans;
      }
      curSample = 0;
      validSamples = fi.outputSamps / lastChannels;
    }
  } else {
    running = false; // No more data, we're done here...
  }

done:
  file->loop();
  output->loop();

  return running;
}

bool AudioGeneratorAAC::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source) return false;
  file = source;
  if (!output) return false;
  this->output = output;
  if (!file->isOpen()) return false; // Error

  output->begin();
  
  // AAC always comes out at 16 bits
  output->SetBitsPerSample(16);
 

  memset(buff, 0, buffLen);
  memset(outSample, 0, 1024*2*sizeof(int16_t));

 
  running = true;
  
  return true;
}


