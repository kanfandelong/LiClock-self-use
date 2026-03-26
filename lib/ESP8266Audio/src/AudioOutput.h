/*
  AudioOutput
  Base class of an audio output player
  
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

#ifndef _AUDIOOUTPUT_H
#define _AUDIOOUTPUT_H

#include <Arduino.h>
#include "AudioStatus.h"

class AudioOutput
{
  public:
    AudioOutput() { };
    virtual ~AudioOutput() {};
    virtual bool SetRate(int hz) { hertz = hz; return true; }
    virtual bool SetBitsPerSample(int bits) { bps = bits; return true; }
    virtual int GetRate() { return hertz; }
    virtual int GetBitsPerSample() { return bps; }
    virtual bool SetChannels(int chan) { channels = chan; return true; }
    virtual bool SetGain(float f) { if (f>4.0) f = 4.0; if (f<0.0) f=0.0; gainF2P6 = (uint8_t)(f*(1<<6)); return true; }
    virtual bool begin() { return false; };
    typedef enum { LEFTCHANNEL=0, RIGHTCHANNEL=1 } SampleIndex;
    #ifdef CONFIG_DAC_32bit
    virtual bool ConsumeSample(int32_t sample[2]) { (void)sample; return false; }
    virtual uint16_t ConsumeSamples(int32_t *samples, uint16_t count)
    {
      for (uint16_t i=0; i<count; i++) {
        if (!ConsumeSample(samples)) return i;
        samples += 2;
      }
      return count;
    }
    #else
    virtual bool ConsumeSample(int16_t sample[2]) { (void)sample; return false; }
    virtual uint16_t ConsumeSamples(int16_t *samples, uint16_t count)
    {
      for (uint16_t i=0; i<count; i++) {
        if (!ConsumeSample(samples)) return i;
        samples += 2;
      }
      return count;
    }
    #endif
    virtual bool stop() { return false; }
    virtual void flush() { return; }
    virtual bool loop() { return true; }

  public:
    virtual bool RegisterMetadataCB(AudioStatus::metadataCBFn fn, void *data) { return cb.RegisterMetadataCB(fn, data); }
    virtual bool RegisterStatusCB(AudioStatus::statusCBFn fn, void *data) { return cb.RegisterStatusCB(fn, data); }

  protected:
    #ifdef CONFIG_DAC_32bit
    void MakeSampleStereo16(int32_t sample[2]) {
      // Mono to "stereo" conversion
      if (channels == 1)
        sample[RIGHTCHANNEL] = sample[LEFTCHANNEL];
      if (bps == 8) {
        // Upsample from unsigned 8 bits to signed 16 bits
        sample[LEFTCHANNEL] = (((int16_t)(sample[LEFTCHANNEL]&0xff)) - 128) << 8;
        sample[RIGHTCHANNEL] = (((int16_t)(sample[RIGHTCHANNEL]&0xff)) - 128) << 8;
      }
    };

    inline int32_t Amplify(int32_t s) {
        // 32位音频增益调整，gainF2P6为2.6定点格式
        int64_t v = (int64_t(s) * gainF2P6) >> 6;
        if (v < INT32_MIN) return INT32_MIN;
        else if (v > INT32_MAX) return INT32_MAX;
        else return int32_t(v);
    }
    #else
    void MakeSampleStereo16(int16_t sample[2]) {
      // Mono to "stereo" conversion
      if (channels == 1)
        sample[RIGHTCHANNEL] = sample[LEFTCHANNEL];
      if (bps == 8) {
        // Upsample from unsigned 8 bits to signed 16 bits
        sample[LEFTCHANNEL] = (((int16_t)(sample[LEFTCHANNEL]&0xff)) - 128) << 8;
        sample[RIGHTCHANNEL] = (((int16_t)(sample[RIGHTCHANNEL]&0xff)) - 128) << 8;
      }
    };

    inline int16_t Amplify(int16_t s) {
      int32_t v = (s * gainF2P6)>>6;
      if (v < -32767) return -32767;
      else if (v > 32767) return 32767;
      else return (int16_t)(v&0xffff);
    }
    #endif

  protected:
    // Use 32-bit to support high sample rates (e.g., 88200 Hz) without overflow
    uint32_t hertz;
    uint8_t bps;
    uint8_t channels;
    uint8_t gainF2P6; // Fixed point 2.6

  protected:
    AudioStatus cb;
};

#endif

