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

#include "AudioGeneratorOGG.h"

#define OGG_BUFF_BYTES 4096   // Buffer size in bytes (2048 PCM samples = 1024 stereo frames)

AudioGeneratorOGG::AudioGeneratorOGG()
{
  vf = nullptr;
  buff = nullptr;
  buffPtr = 0;
  buffLen = 0;
  current_section = 0;
  channels = 0;
  running = false;

  // Set up Tremor callback struct with static bounce functions
  _cb.read_func  = OGG_read;
  _cb.seek_func  = OGG_seek;
  _cb.close_func = OGG_close;
  _cb.tell_func  = OGG_tell;
}

AudioGeneratorOGG::~AudioGeneratorOGG()
{
  if (vf) {
    ov_clear(vf);
    free(vf);
    vf = nullptr;
  }
  free(buff);
  buff = nullptr;
}

bool AudioGeneratorOGG::begin(AudioFileSource *source, AudioOutput *output)
{
  buff = (int16_t *)malloc(OGG_BUFF_BYTES);
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
    return false;
  }

  // Allocate Tremor file handle
  vf = (OggVorbis_File *)malloc(sizeof(OggVorbis_File));
  if (!vf) {
    log_e("malloc vf failed");
    return false;
  }
  memset(vf, 0, sizeof(OggVorbis_File));

  // Open with callbacks
  int err = ov_open_callbacks((void *)this, vf, NULL, 0, _cb);
  if (err < 0) {
    log_e("ov_open_callbacks failed, errno: %d", err);
    free(vf);
    vf = nullptr;
    return false;
  }

  // Get stream info
  vorbis_info *vi = ov_info(vf, -1);
  if (!vi) {
    log_e("ov_info failed");
    ov_clear(vf);
    free(vf);
    vf = nullptr;
    return false;
  }
  channels = vi->channels;

  // Process metadata tags
  processTags();

  lastSample[0] = 0;
  lastSample[1] = 0;
  current_section = 0;
  buffPtr = 0;
  buffLen = 0;

  output->begin();
  output->SetRate(vi->rate);
  output->SetBitsPerSample(16);
  output->SetChannels(channels);

  running = true;
  return true;
}

bool AudioGeneratorOGG::loop()
{
  if (!running) goto done;

  // Try to consume the last sample first
  if (!output->ConsumeSample(lastSample)) goto done;

  do {
    if (buffPtr >= buffLen) {
      // Buffer exhausted, decode more data
      long ret = ov_read(vf, (char *)buff, OGG_BUFF_BYTES, &current_section);
      if (ret == OV_HOLE) {
        // Hole in data, skip and continue
        continue;
      } else if (ret < 0) {
        // Error, stop
        running = false;
        goto done;
      } else if (ret == 0) {
        // EOF
        running = false;
        goto done;
      }
      // ret is the number of bytes of PCM data (interleaved 16-bit samples)
      buffLen = ret / sizeof(int16_t);  // Number of int16_t values
      buffPtr = 0;
    }

    // We have buffered samples, feed them to output
    if (buffPtr + 1 < buffLen) {
      #ifdef CONFIG_DAC_32bit
      lastSample[AudioOutput::LEFTCHANNEL]  = ((int32_t)buff[buffPtr]) << 16;
      #else
      lastSample[AudioOutput::LEFTCHANNEL]  = buff[buffPtr] & 0xffff;
      #endif
      buffPtr++;

      if (channels == 2) {
        #ifdef CONFIG_DAC_32bit
        lastSample[AudioOutput::RIGHTCHANNEL] = ((int32_t)buff[buffPtr]) << 16;
        #else
        lastSample[AudioOutput::RIGHTCHANNEL] = buff[buffPtr] & 0xffff;
        #endif
        buffPtr++;
      } else {
        // Mono: duplicate left channel data
        lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
        // If ov_read returned mono interleaved (which shouldn't happen
        // for Vorbis with channels==1, but just in case)
      }
    } else {
      // Not enough data for a full stereo frame, need more
      buffPtr = buffLen; // Force reload on next iteration
    }
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();
  return running;
}

bool AudioGeneratorOGG::stop()
{
  if (!running && !vf) return true;

  if (vf) {
    ov_clear(vf);
    free(vf);
    vf = nullptr;
  }
  free(buff);
  buff = nullptr;
  running = false;
  output->stop();
  return file->close();
}

bool AudioGeneratorOGG::isRunning()
{
  return running;
}

// ---- Callback bounce functions (static -> instance) ----

size_t AudioGeneratorOGG::OGG_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
  return static_cast<AudioGeneratorOGG *>(datasource)->read_cb(ptr, size, nmemb);
}

int AudioGeneratorOGG::OGG_seek(void *datasource, ogg_int64_t offset, int whence)
{
  return static_cast<AudioGeneratorOGG *>(datasource)->seek_cb(offset, whence);
}

int AudioGeneratorOGG::OGG_close(void *datasource)
{
  return static_cast<AudioGeneratorOGG *>(datasource)->close_cb();
}

long AudioGeneratorOGG::OGG_tell(void *datasource)
{
  return static_cast<AudioGeneratorOGG *>(datasource)->tell_cb();
}

// ---- Instance-level callbacks ----

size_t AudioGeneratorOGG::read_cb(void *ptr, size_t size, size_t nmemb)
{
  if (size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  uint32_t read = file->read(ptr, total);
  if (read == 0) {
    // Tremor treats 0 bytes as EOF/error
    return 0;
  }
  return read / size;
}

int AudioGeneratorOGG::seek_cb(ogg_int64_t offset, int whence)
{
  if (!file->seek((int32_t)offset, whence)) return -1;
  return 0;
}

int AudioGeneratorOGG::close_cb()
{
  // No-op: we manage close in stop()
  return 0;
}

long AudioGeneratorOGG::tell_cb()
{
  return (long)file->getPos();
}

// ---- Metadata tag processing ----

void AudioGeneratorOGG::processTags()
{
  vorbis_comment *vc = ov_comment(vf, -1);
  if (!vc) {
    log_w("No Vorbis comments found");
    return;
  }

  // Vendor string
  if (vc->vendor) {
    log_i("Vorbis Vendor: %s", vc->vendor);
    cb.md("VENDOR", true, vc->vendor);
  }

  // Process all user comments (TITLE, ARTIST, ALBUM, etc.)
  for (int i = 0; i < vc->comments; i++) {
    if (vc->user_comments[i] && vc->comment_lengths[i] > 0) {
      // Create a null-terminated copy
      const unsigned MAX_STATIC = 255;
      char staticBuf[256];
      char *buf = nullptr;

      if (vc->comment_lengths[i] > (int)MAX_STATIC) {
        buf = (char *)malloc(vc->comment_lengths[i] + 1);
        if (buf) {
          memcpy(buf, vc->user_comments[i], vc->comment_lengths[i]);
          buf[vc->comment_lengths[i]] = '\0';
        } else {
          memcpy(staticBuf, vc->user_comments[i], MAX_STATIC);
          staticBuf[MAX_STATIC] = '\0';
          buf = staticBuf;
        }
      } else {
        memcpy(staticBuf, vc->user_comments[i], vc->comment_lengths[i]);
        staticBuf[vc->comment_lengths[i]] = '\0';
        buf = staticBuf;
      }

      // Split on '='
      char *eq = strchr(buf, '=');
      if (eq) {
        *eq = '\0';
        cb.md(buf, false, eq + 1);
      } else {
        cb.md(buf, false, "");
      }

      if (buf != staticBuf) {
        free(buf);
      }
    }
  }

  // Report total time if seekable
  ogg_int64_t total_samples = ov_pcm_total(vf, -1);
  if (total_samples > 0) {
    vorbis_info *vi = ov_info(vf, -1);
    if (vi && vi->rate > 0) {
      uint64_t total_time = (uint64_t)total_samples * 1000 / vi->rate;
      log_i("Total time: %llu ms", total_time);
      cb.md("tlen", false, ((String)total_time).c_str());
    }
  }
}