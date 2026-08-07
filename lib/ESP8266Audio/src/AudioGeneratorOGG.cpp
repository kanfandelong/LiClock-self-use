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

#define OGG_BUFF_BYTES 4096   // Buffer size in bytes (2048 PCM samples)
#define MAX_CHANNELS    8     // 最大支持的声道数（超过则只处理前 MAX_CHANNELS 个）

// 定点下混系数 (Q15 格式：1.0 = 32768)
static const int16_t COEF_MAIN_Q15     = 32768;  // 1.0
static const int16_t COEF_CENTER_Q15   = 23170;  // 0.707
static const int16_t COEF_SURROUND_Q15 = 23170;  // 0.707
static const int16_t COEF_LFE_Q15      = 16384;  // 0.5

// 定点饱和加法宏
#define SATURATE_ADD(x, y) ( (int32_t)x + (int32_t)y > 32767 ? 32767 : ( (int32_t)x + (int32_t)y < -32768 ? -32768 : (x + y) ) )

// 定点下混函数（无浮点）
static void downmixStereo_fixed(int16_t *outL, int16_t *outR,
                                const int16_t *samples, int channels) {
    if (channels == 1) {
        *outL = *outR = samples[0];
        return;
    }
    if (channels == 2) {
        *outL = samples[0];
        *outR = samples[1];
        return;
    }

    int32_t left = 0, right = 0;
    int ch = (channels > MAX_CHANNELS) ? MAX_CHANNELS : channels;

    for (int i = 0; i < ch; i++) {
        int32_t s = samples[i];
        switch (i) {
            case 0: left  += (s * COEF_MAIN_Q15) >> 15; break;      // FL
            case 1: right += (s * COEF_MAIN_Q15) >> 15; break;      // FR
            case 2: left  += (s * COEF_CENTER_Q15) >> 15;           // FC
                    right += (s * COEF_CENTER_Q15) >> 15; break;
            case 3: left  += (s * COEF_LFE_Q15) >> 15;              // LFE
                    right += (s * COEF_LFE_Q15) >> 15; break;
            case 4: left  += (s * COEF_SURROUND_Q15) >> 15; break;  // BL
            case 5: right += (s * COEF_SURROUND_Q15) >> 15; break;  // BR
            case 6: left  += (s * COEF_SURROUND_Q15) >> 15; break;  // SL
            case 7: right += (s * COEF_SURROUND_Q15) >> 15; break;  // SR
            default:
                left  += s >> 1;
                right += s >> 1;
                break;
        }
    }

    // 饱和截断
    *outL = left > 32767 ? 32767 : (left < -32768 ? -32768 : (int16_t)left);
    *outR = right > 32767 ? 32767 : (right < -32768 ? -32768 : (int16_t)right);
}

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

  // 尝试消费上一次剩余的样本（或静音初始化）
  if (!output->ConsumeSample(lastSample)) goto done;

  do {
    // 判断缓冲区是否足够取出一帧（channels 个样本）
    int samples_needed = channels;
    if (buffPtr + samples_needed > buffLen) {
      // 缓冲区不足一帧，解码更多数据
      long ret = ov_read(vf, (char *)buff, OGG_BUFF_BYTES, &current_section);
      if (ret == OV_HOLE) {
        continue;  // 数据空洞，跳过
      } else if (ret < 0) {
        running = false;
        goto done;
      } else if (ret == 0) {
        running = false;  // EOF
        goto done;
      }
      buffLen = ret / sizeof(int16_t);
      buffPtr = 0;
      if (buffLen < samples_needed) {
        running = false;  // 末尾不足一帧
        goto done;
      }
    }

    // 从缓冲区提取一帧原始样本
    int16_t frame[MAX_CHANNELS];
    int ch = (channels > MAX_CHANNELS) ? MAX_CHANNELS : channels;
    for (int i = 0; i < ch; i++) {
      frame[i] = buff[buffPtr++];
    }
    if (channels > MAX_CHANNELS) {
      buffPtr += (channels - MAX_CHANNELS);
    }

    // 下混为立体声
    int16_t left, right;
    downmixStereo_fixed(&left, &right, frame, ch);

    // 填充 lastSample（兼容 16/32 位 DAC）
    #ifdef CONFIG_DAC_32bit
    lastSample[AudioOutput::LEFTCHANNEL]  = ((int32_t)left) << 16;
    lastSample[AudioOutput::RIGHTCHANNEL] = ((int32_t)right) << 16;
    #else
    lastSample[AudioOutput::LEFTCHANNEL]  = left;
    lastSample[AudioOutput::RIGHTCHANNEL] = right;
    #endif

    // ★关键修复：立即尝试消费此样本，若输出忙则退出循环，下次再试
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