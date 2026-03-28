/*
  AudioGeneratorOpus
  Audio output generator that plays Opus audio files
    
  Copyright (C) 2020  Earle F. Philhower, III

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

#include <AudioGeneratorOpus.h>

AudioGeneratorOpus::AudioGeneratorOpus()
{
  of = nullptr;
  buff = nullptr;
  buffPtr = 0;
  buffLen = 0;
  running = false;
}

AudioGeneratorOpus::~AudioGeneratorOpus()
{
  if (of) op_free(of);
  of = nullptr;
  free(buff);
  buff = nullptr;
}

#define OPUS_BUFF 1024

bool AudioGeneratorOpus::begin(AudioFileSource *source, AudioOutput *output)
{
  buff = (int16_t*)malloc(OPUS_BUFF * sizeof(int16_t));
  if (!buff) {
    log_e("malloc failed");
    return false;
  }

  if (!source) {
    log_e("source is null");
    return false;
  }
  file = source;
  if (!output) {
    log_e("output is null");
    return false;
  }
  this->output = output;
  if (!file->isOpen()) {
    log_e("file not open");
    return false; // Error
  }

  int err;
  of = op_open_callbacks((void*)this, &_cb, nullptr, 0, &err);
  if (!of) {
    log_e("op_open_callbacks failed, errno: %d", err);
    return false;
  }

  // Process tags immediately after opening the file
  processTags();

  prev_li = -1;
  lastSample[0] = 0;
  lastSample[1] = 0;

  buffPtr = 0;
  buffLen = 0;

  output->begin();

  // These are fixed by Opus
  output->SetRate(48000);
  output->SetBitsPerSample(16);
  output->SetChannels(2);

  running = true;
  return true;
}

bool AudioGeneratorOpus::loop()
{

  if (!running) goto done;

  if (!output->ConsumeSample(lastSample)) goto done; // Try and send last buffered sample

  do {
    if (buffPtr == buffLen) {
      int ret = op_read_stereo(of, (opus_int16 *)buff, OPUS_BUFF);
      if (ret == OP_HOLE) {
        // fprintf(stderr,"\nHole detected! Corrupt file segment?\n");
        continue;
      } else if (ret <= 0) {
        running = false;
        goto done;
      }
     buffPtr = 0;
     buffLen = ret * 2;
    }

    #ifdef CONFIG_DAC_32bit
    lastSample[AudioOutput::LEFTCHANNEL] = buff[buffPtr] << 16; 
    lastSample[AudioOutput::RIGHTCHANNEL] = buff[buffPtr+1] << 16; 
    #else
    lastSample[AudioOutput::LEFTCHANNEL] = buff[buffPtr] & 0xffff; 
    lastSample[AudioOutput::RIGHTCHANNEL] = buff[buffPtr+1] & 0xffff; 
    #endif
    buffPtr += 2;
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();

  return running;
}

bool AudioGeneratorOpus::stop()
{
  if (of) op_free(of);
  of = nullptr;
  free(buff);
  buff = nullptr;
  running = false;
  output->stop();
  return true;
}

bool AudioGeneratorOpus::isRunning()
{
  return running;
}

int AudioGeneratorOpus::read_cb(unsigned char *_ptr, int _nbytes) {
  if (_nbytes == 0) return 0;
  _nbytes = file->read(_ptr, _nbytes);
  if (_nbytes == 0) return -1;
  return _nbytes;
}

int AudioGeneratorOpus::seek_cb(opus_int64 _offset, int _whence) {
  if (!file->seek((int32_t)_offset, _whence)) return -1;
  return 0;
}

opus_int64 AudioGeneratorOpus::tell_cb() {
  return file->getPos();
}

int AudioGeneratorOpus::close_cb() {
  // NO OP, we close in main loop
  return 0;
}

void AudioGeneratorOpus::processTags()
{
  const OpusTags *tags = op_tags(of, -1); // -1 means current link
  if (!tags) {
    log_w("No tags found in Opus file");
    return;
  }

  // Log vendor string
  if (tags->vendor) {
    log_i("Opus Vendor: %s", tags->vendor);
  }

  // Process all user comments
  for (int i = 0; i < tags->comments; i++) {
    if (tags->user_comments[i] && tags->comment_lengths[i] > 0) {
      // Create a null-terminated copy for processing
      const unsigned MAX_STATIC = 255;
      char staticBuf[256];
      char *buf = nullptr;
      
      if (tags->comment_lengths[i] > MAX_STATIC) {
        // Try dynamic allocation
        buf = (char *)malloc(tags->comment_lengths[i] + 1);
        if (buf) {
          memcpy(buf, tags->user_comments[i], tags->comment_lengths[i]);
          buf[tags->comment_lengths[i]] = '\0';
        } else {
          // Allocation failed, fallback to static buffer and truncate
          memcpy(staticBuf, tags->user_comments[i], MAX_STATIC);
          staticBuf[MAX_STATIC] = '\0';
          buf = staticBuf;
        }
      } else {
        // Use static buffer
        memcpy(staticBuf, tags->user_comments[i], tags->comment_lengths[i]);
        staticBuf[tags->comment_lengths[i]] = '\0';
        buf = staticBuf;
      }
      
      // Find the first '=' separator
      char *eq = strchr(buf, '=');
      if (eq) {
        *eq = '\0'; // split into key and value
        const char *key = buf;
        const char *value = eq + 1;
        // log_i("Opus Tag: %s = %s", key, value);
        cb.md(key, false, value);
      } else {
        // No '=', treat whole string as key with empty value
        // log_i("Opus Tag: %s", buf);
        cb.md(buf, false, "");
      }
      
      // Free dynamic allocation if used
      if (buf != staticBuf && buf != nullptr) {
        free(buf);
      }
    }
  }
  
  // Calculate and send total time if possible
  ogg_int64_t total_samples = op_pcm_total(of, -1);
  if (total_samples > 0) {
    // Opus always decodes at 48000 Hz
    uint64_t total_time = (uint64_t)total_samples * 1000 / 48000;
    log_i("Total time: %llu ms", total_time);
    cb.md("tlen", false, ((String)total_time).c_str());
  }
}
