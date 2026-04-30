/*
  AudioGeneratorMP3
  Wrap libmad MP3 library to play audio

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


#include "AudioGeneratorMP3.h"

AudioGeneratorMP3::AudioGeneratorMP3()
{
  running = false;
  file = NULL;
  output = NULL;
  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;
  nsCountMax = 1152/32;
  madInitted = false;
  totalSent = false;
}

AudioGeneratorMP3::AudioGeneratorMP3(void *space, int size): preallocateSpace(space), preallocateSize(size)
{
  running = false;
  file = NULL;
  output = NULL;
  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;
  nsCountMax = 1152/32;
  madInitted = false;
  totalSent = false;
}

AudioGeneratorMP3::AudioGeneratorMP3(void *buff, int buffSize, void *stream, int streamSize, void *frame, int frameSize, void *synth, int synthSize):
    preallocateSpace(buff), preallocateSize(buffSize),
    preallocateStreamSpace(stream), preallocateStreamSize(streamSize),
    preallocateFrameSpace(frame), preallocateFrameSize(frameSize),
    preallocateSynthSpace(synth), preallocateSynthSize(synthSize)
{
  running = false;
  file = NULL;
  output = NULL;
  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;
  nsCountMax = 1152/32;
  madInitted = false;
  totalSent = false;
}

AudioGeneratorMP3::~AudioGeneratorMP3()
{
  if (!preallocateSpace) {
    free(buff);
    free(synth);
    free(frame);
    free(stream);
  }
}


bool AudioGeneratorMP3::stop()
{
  if (madInitted) {
    mad_synth_finish(synth);
    mad_frame_finish(frame);
    mad_stream_finish(stream);
    madInitted = false;
  }

  if (!preallocateSpace) {
    free(buff);
    free(synth);
    free(frame);
    free(stream);
  }

  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;

  running = false;
  output->stop();
  return file->close();
}

bool AudioGeneratorMP3::isRunning()
{
  return running;
}

enum mad_flow AudioGeneratorMP3::ErrorToFlow()
{
  char err[64];
  char errLine[128];

  // Special case - eat "lost sync @ byte 0" as it always occurs and is not really correct....it never had sync!
  if ((lastReadPos==0) && (stream->error==MAD_ERROR_LOSTSYNC)) return MAD_FLOW_CONTINUE;

  strcpy_P(err, mad_stream_errorstr(stream));
  snprintf_P(errLine, sizeof(errLine), PSTR("Decoding error '%s' at byte offset %d"),
           err, (stream->this_frame - buff) + lastReadPos);
  yield(); // Something bad happened anyway, ensure WiFi gets some time, too
  log_e("error_code:%d %s", stream->error, errLine);
  return MAD_FLOW_CONTINUE;
}

enum mad_flow AudioGeneratorMP3::Input()
{
  int unused = 0;

  if (stream->next_frame) {
    unused = lastBuffLen - (stream->next_frame - buff);
    if (unused < 0) {
      desync();
      unused = 0;
    } else {
      memmove(buff, stream->next_frame, unused);
    }
    stream->next_frame = NULL;
  }

  if (unused == lastBuffLen) {
    // Something wicked this way came, throw it all out and try again
    unused = 0;
  }

  lastReadPos = file->getPos() - unused;
  int len = buffLen - unused;
  len = file->read(buff + unused, len);
  if ((len == 0)  && (unused == 0)) {
    // Can't read any from the file, and we don't have anything left.  It's done....
    return MAD_FLOW_STOP;
  }
  if (len < 0) {
    desync();
    unused = 0;
  }

  lastBuffLen = len + unused;
  mad_stream_buffer(stream, buff, lastBuffLen);

  return MAD_FLOW_CONTINUE;
}

void AudioGeneratorMP3::desync ()
{
    log_printf("desync\n");
    if (stream) {
        stream->next_frame = nullptr;
        stream->this_frame = nullptr;
        stream->sync = 0;
    }
    lastBuffLen = 0;
}

bool AudioGeneratorMP3::DecodeNextFrame()
{
  if (mad_frame_decode(frame, stream) == -1) {
    // ErrorToFlow(); // Always returns CONTINUE
    return false;
  }
  
  nsCountMax = MAD_NSBSAMPLES(&frame->header);
  
  // 只统计有效比特率
  if (!totalSent && frame->header.bitrate > 0) {
    bitrateSum += (uint64_t)frame->header.bitrate;
    bitrateCount++;
  }
  
  return true;
}
#ifdef CONFIG_DAC_32bit
bool AudioGeneratorMP3::GetOneSample(int32_t sample[2])
#else
bool AudioGeneratorMP3::GetOneSample(int16_t sample[2])
#endif
{
  if (synth->pcm.samplerate != lastRate) {
    output->SetRate(synth->pcm.samplerate);
    lastRate = synth->pcm.samplerate;
  }
  if (synth->pcm.channels != lastChannels) {
    output->SetChannels(synth->pcm.channels);
    lastChannels = synth->pcm.channels;
  }

  // If we're here, we have one decoded frame and sent 0 or more samples out
  if (samplePtr < synth->pcm.length) {
    #ifdef CONFIG_DAC_32bit
    sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr] << 16;
    sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr] << 16;
    #else
    sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr];
    sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr];
    #endif
    samplePtr++;
  } else {
    samplePtr = 0;

    switch ( mad_synth_frame_onens(synth, frame, nsCount++) ) {
        case MAD_FLOW_STOP:
        case MAD_FLOW_BREAK: log_printf("msf1ns failed\n");
          return false; // Either way we're done
        default:
          break; // Do nothing
    }
    // for IGNORE and CONTINUE, just play what we have now
    #ifdef CONFIG_DAC_32bit
    sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr] << 16;
    sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr] << 16;
    #else
    sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr];
    sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr];
    #endif
    samplePtr++;
  }
  return true;
}

const char* mad_error_to_string(enum mad_error error) {
    switch (error) {
        case MAD_ERROR_NONE:            return "no error";
        case MAD_ERROR_BUFLEN:          return "input buffer too small (or EOF)";
        case MAD_ERROR_BUFPTR:          return "invalid (null) buffer pointer";
        case MAD_ERROR_NOMEM:           return "not enough memory";
        case MAD_ERROR_LOSTSYNC:        return "lost synchronization";
        case MAD_ERROR_BADLAYER:        return "reserved header layer value";
        case MAD_ERROR_BADBITRATE:      return "forbidden bitrate value";
        case MAD_ERROR_BADSAMPLERATE:   return "reserved sample frequency value";
        case MAD_ERROR_BADEMPHASIS:     return "reserved emphasis value";
        case MAD_ERROR_BADCRC:          return "CRC check failed";
        case MAD_ERROR_BADBITALLOC:     return "forbidden bit allocation value";
        case MAD_ERROR_BADSCALEFACTOR:  return "bad scalefactor index";
        case MAD_ERROR_BADMODE:         return "bad bitrate/mode combination";
        case MAD_ERROR_BADFRAMELEN:     return "bad frame length";
        case MAD_ERROR_BADBIGVALUES:    return "bad big_values count";
        case MAD_ERROR_BADBLOCKTYPE:    return "reserved block_type";
        case MAD_ERROR_BADSCFSI:        return "bad scalefactor selection info";
        case MAD_ERROR_BADDATAPTR:      return "bad main_data_begin pointer";
        case MAD_ERROR_BADPART3LEN:     return "bad audio data length";
        case MAD_ERROR_BADHUFFTABLE:    return "bad Huffman table select";
        case MAD_ERROR_BADHUFFDATA:     return "Huffman data overrun";
        case MAD_ERROR_BADSTEREO:       return "incompatible block_type for JS";
        default:                        return "unknown error";
    }
}

void print_mad_error(enum mad_error error) {
  if (error != MAD_ERROR_NONE && error != MAD_ERROR_BUFLEN && error != MAD_ERROR_LOSTSYNC) {
    log_e("MP3 Decoding error: %s", mad_error_to_string(error));
  }
}

bool AudioGeneratorMP3::loop()
{
  if (!running) goto done; // Nothing to do here!
  
  // 只有有足够帧数来计算平均比特率时才进行处理
  if (bitrateCount >= 50 && !totalSent) {
    uint64_t currentAvgBitrate = bitrateSum / bitrateCount;
    
    // 如果是第一次计算，或者帧数刚好是50的倍数时重新计算
    // 这样可以避免频繁计算，只在关键点更新
    if (bitrateCount == 50 || bitrateCount % 50 == 0) {
      // 避免除零错误
      if (currentAvgBitrate == 0) currentAvgBitrate = frame->header.bitrate;
      if (currentAvgBitrate == 0) currentAvgBitrate = 128000;
      
      // 计算总时长（毫秒）
      uint64_t total_seconds = ((uint64_t)(file->getSize() - first_frame_pos) * 8ULL) / currentAvgBitrate;
      uint64_t total_ms = total_seconds * 1000ULL;
      
      // 发送总时长回调
      cb.md("tlen", false, ((String)total_ms).c_str());
      
      // log_i("更新总时长: %llu ms, 平均比特率: %llu bps (基于 %u 帧)", total_ms, currentAvgBitrate, bitrateCount);
      
      // 记录最后一次计算时的比特率
      lastAvgBitrate = currentAvgBitrate;
    }
    
    // 当有足够多帧数时检查稳定性
    if (bitrateCount >= 400) { // 200帧后认为比特率已稳定
      totalSent = true;
      // log_i("比特率已稳定，基于 %u 帧，平均比特率: %llu bps", bitrateCount, lastAvgBitrate);
    } 
  }
  // First, try and push in the stored sample.  If we can't, then punt and try later
  if (!output->ConsumeSample(lastSample)) {
    delay(2);
    goto done;
  } // Can't send, but no error detected

  // Try and stuff the buffer one sample at a time
  do
  {
    // Decode next frame if we're beyond the existing generated data
    if ( (samplePtr >= synth->pcm.length) && (nsCount >= nsCountMax) ) {
retry:
      if (Input() == MAD_FLOW_STOP) {
        return false;
      }

      if (!DecodeNextFrame()) {
        // 仅在需要时开启
        // print_mad_error(stream->error);
        if (stream->error == MAD_ERROR_BUFLEN) {
          // randomly seeking can lead to endless
          // and unrecoverable "MAD_ERROR_BUFLEN" loop
          log_printf("ERROR_BUFLEN %d\n", unrecoverable);
          if (++unrecoverable >= 3) {
            unrecoverable = 0;
            stop();
            return running;
          }
        } else {
          unrecoverable = 0;
        }
        goto retry;
      }
      samplePtr = 9999;
      nsCount = 0;
    }

    if (!GetOneSample(lastSample)) {
      log_printf("G1S failed\n");
      running = false;
      goto done;
    }
    if (lastChannels == 1)
    {
      lastSample[1] = lastSample[0];
    }
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();

  return running;
}



bool AudioGeneratorMP3::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source)  return false;
  file = source;
  if (!output) return false;
  this->output = output;
  if (!file->isOpen()) {
    log_printf("MP3 source file not open\n");
    return false; // Error
  }

  first_frame_pos = file->getPos();
  // Reset error count from previous file
  unrecoverable = 0;

  output->SetBitsPerSample(16); // Constant for MP3 decoder
  output->SetChannels(2);

  if (!output->begin()) return false;

  // Where we are in generating one frame's data, set to invalid so we will run loop on first getsample()
  samplePtr = 9999;
  nsCount = 9999;
  lastRate = 0;
  lastChannels = 0;
  lastReadPos = 0;
  lastBuffLen = 0;

  // Allocate all large memory chunks
  if (preallocateStreamSize + preallocateFrameSize + preallocateSynthSize) {
    if (preallocateSize >= preAllocBuffSize() &&
        preallocateStreamSize >= preAllocStreamSize() &&
        preallocateFrameSize >= preAllocFrameSize() &&
        preallocateSynthSize >= preAllocSynthSize()) {
      buff = reinterpret_cast<unsigned char *>(preallocateSpace);
      stream = reinterpret_cast<struct mad_stream *>(preallocateStreamSpace);
      frame = reinterpret_cast<struct mad_frame *>(preallocateFrameSpace);
      synth = reinterpret_cast<struct mad_synth *>(preallocateSynthSpace);
    }
    else {
      output->stop();
      audioLogger->printf_P("OOM error in MP3:  Want %d/%d/%d/%d bytes, have %d/%d/%d/%d bytes preallocated.\n",
          preAllocBuffSize(), preAllocStreamSize(), preAllocFrameSize(), preAllocSynthSize(),
          preallocateSize, preallocateStreamSize, preallocateFrameSize, preallocateSynthSize);
      return false;
    }
  } else if (preallocateSpace) {
    uint8_t *p = reinterpret_cast<uint8_t *>(preallocateSpace);
    buff = reinterpret_cast<unsigned char *>(p);
    p += preAllocBuffSize();
    stream = reinterpret_cast<struct mad_stream *>(p);
    p += preAllocStreamSize();
    frame = reinterpret_cast<struct mad_frame *>(p);
    p += preAllocFrameSize();
    synth = reinterpret_cast<struct mad_synth *>(p);
    p += preAllocSynthSize();
    int neededBytes = p - reinterpret_cast<uint8_t *>(preallocateSpace);
    if (neededBytes > preallocateSize) {
      output->stop();
      audioLogger->printf_P("OOM error in MP3:  Want %d bytes, have %d bytes preallocated.\n", neededBytes, preallocateSize);
      return false;
    }
  } else {
    buff = reinterpret_cast<unsigned char *>(ps_malloc(buffLen));
    stream = reinterpret_cast<struct mad_stream *>(ps_malloc(sizeof(struct mad_stream)));
    frame = reinterpret_cast<struct mad_frame *>(ps_malloc(sizeof(struct mad_frame)));
    synth = reinterpret_cast<struct mad_synth *>(ps_malloc(sizeof(struct mad_synth)));
    if (!buff || !stream || !frame || !synth) {
      free(buff);
      free(stream);
      free(frame);
      free(synth);
      buff = NULL;
      stream = NULL;
      frame = NULL;
      synth = NULL;

      output->stop();
      audioLogger->printf_P("OOM error in MP3\n");
      return false;
    }
  }

  mad_stream_init(stream);
  mad_frame_init(frame);
  mad_synth_init(synth);
  synth->pcm.length = 0;
  mad_stream_options(stream, 0); // TODO - add options support
  madInitted = true;

  running = true;
  return true;
}

// The following are helper routines for use in libmad to check stack/heap free
// and to determine if there's enough stack space to allocate some blocks there
// instead of precious heap.

#undef stack
extern "C" {
#ifdef ESP32
  //TODO - add ESP32 checks
  void stack(const char *s, const char *t, int i)
  {
  }
  int stackfree()
  {
    return 8192;
  }
#elif defined(ESP8266) && !defined(CORE_MOCK)
  #include <cont.h>
  extern cont_t g_cont;

  void stack(const char *s, const char *t, int i)
  {
    (void) t;
    (void) i;
    register uint32_t *sp asm("a1");
    int freestack = 4 * (sp - g_cont.stack);
    int freeheap = ESP.getFreeHeap();
    if ((freestack < 512) || (freeheap < 5120)) {
      static int laststack, lastheap;
      if (laststack!=freestack|| lastheap !=freeheap) {
        log_printf("%s: FREESTACK=%d, FREEHEAP=%d\n", s, /*t, i,*/ freestack, /*cont_get_free_stack(&g_cont),*/ freeheap);
      }
      if (freestack < 256) {
        log_printf("out of stack!\n");
      }
      if (freeheap < 1024) {
        log_printf("out of heap!\n");
      }
      Serial.flush();
      laststack = freestack;
      lastheap = freeheap;
    }
  }

  int stackfree()
  {
    register uint32_t *sp asm("a1");
    int freestack = 4 * (sp - g_cont.stack);
    return freestack;
  }
#else
  void stack(const char *s, const char *t, int i)
  {
    (void) s;
    (void) t;
    (void) i;
  }
  int stackfree()
  {
    return 8192;
  }
#endif
}
