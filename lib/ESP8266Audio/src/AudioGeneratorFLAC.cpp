/*
  AudioGeneratorFLAC
  Audio output generator that plays FLAC audio files

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

#include <AudioGeneratorFLAC.h>
#include <stdlib.h>

AudioGeneratorFLAC::AudioGeneratorFLAC()
{
  flac = NULL;
  channels = 0;
  sampleRate = 0;
  bitsPerSample = 0;
  buff[0] = NULL;
  buff[1] = NULL;
  buffPtr = 0;
  buffLen = 0;
  running = false;
  // Initialize timing statistics (if enabled)
#if ENABLE_FLAC_DECODE_TIMING
  decodeTimeSumUs = 0;
  decodeCount = 0;
  lastLogMs = 0;
#endif
}

AudioGeneratorFLAC::~AudioGeneratorFLAC()
{
  if (flac)
    FLAC__stream_decoder_delete(flac);
  flac = NULL;
}

bool AudioGeneratorFLAC::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source)
    return false;
  file = source;
  if (!output)
    return false;
  this->output = output;
  if (!file->isOpen())
    return false; // Error

  flac = FLAC__stream_decoder_new();
  if (!flac)
    return false;

  (void)FLAC__stream_decoder_set_md5_checking(flac, false);
  // Request only the metadata types we need (stream info and Vorbis comments).
  // Skipping picture metadata avoids large memory allocations on constrained devices.
  // FLAC__stream_decoder_set_metadata_respond(flac, FLAC__METADATA_TYPE_STREAMINFO);
  // FLAC__stream_decoder_set_metadata_respond(flac, FLAC__METADATA_TYPE_VORBIS_COMMENT);
  FLAC__stream_decoder_set_metadata_respond_all(flac);

  FLAC__StreamDecoderInitStatus ret = FLAC__stream_decoder_init_stream(
      flac,
      _read_cb,
      _seek_cb,
      _tell_cb,
      _length_cb,
      _eof_cb,
      _write_cb,
      _metadata_cb,
      _error_cb,
      reinterpret_cast<void *>(this));
  if (ret != FLAC__STREAM_DECODER_INIT_STATUS_OK)
  {
    FLAC__stream_decoder_delete(flac);
    flac = NULL;
    return false;
  }
  // Process metadata immediately so that callbacks (e.g., Vorbis comments) are invoked before audio playback starts
  FLAC__stream_decoder_process_until_end_of_metadata(flac);

  output->begin();
  running = true;
  lastSample[0] = 0;
  lastSample[1] = 0;
  channels = 0;
  return true;
}

bool AudioGeneratorFLAC::loop()
{
  FLAC__bool ret;

  if (!running)
    goto done;

  if (channels && !output->ConsumeSample(lastSample))
    goto done; // Try and send last buffered sample

  do
  {
    if (buffPtr == buffLen)
    {
      // Decode timing (optional)
    #if ENABLE_FLAC_DECODE_TIMING
      uint64_t startUs = micros();
      ret = FLAC__stream_decoder_process_single(flac);
      uint64_t elapsedUs = micros() - startUs;
      decodeTimeSumUs += elapsedUs;
      decodeCount++;
      uint32_t nowMs = millis();
      if (nowMs - lastLogMs >= 5000 && decodeCount > 0) {
        uint64_t avgUs = decodeTimeSumUs / decodeCount;
        log_i("Avg FLAC decode time: %llu us over %u calls", avgUs, decodeCount);
        decodeTimeSumUs = 0;
        decodeCount = 0;
        lastLogMs = nowMs;
      }
    #else
      ret = FLAC__stream_decoder_process_single(flac);
    #endif
      if (!ret)
      {
        FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(flac);
        log_e("FLAC Stream Decoder state: %d", state);
        running = false;
        goto done;
      }
      else
      {
        // We might be done...
        if (FLAC__stream_decoder_get_state(flac) == FLAC__STREAM_DECODER_END_OF_STREAM)
        {
          running = false;
          goto done;
        }
        unsigned newsr = FLAC__stream_decoder_get_sample_rate(flac);
        unsigned newch = FLAC__stream_decoder_get_channels(flac);
        unsigned newbps = FLAC__stream_decoder_get_bits_per_sample(flac);
        if (newsr != sampleRate)
          output->SetRate(sampleRate = newsr);
        if (newch != channels)
          output->SetChannels(channels = newch);
        if (newbps != bitsPerSample)
          output->SetBitsPerSample(bitsPerSample = newbps);
      }
    }

    // Check for some weird case where above didn't give any data
    if (buffPtr == buffLen)
    {
      goto done; // At some point the flac better error and we'll return
    }
#ifdef CONFIG_DAC_32bit
    int shift = 0;
    if (bitsPerSample <= 8)
      shift = 24;
    else if (bitsPerSample <= 16)
      shift = 16;
    else if (bitsPerSample <= 24)
      shift = 8;
    else
      shift = 0;

    if (channels == 2)
    {
      lastSample[AudioOutput::LEFTCHANNEL] = buff[0][buffPtr] << shift;
      lastSample[AudioOutput::RIGHTCHANNEL] = buff[1][buffPtr] << shift;
    }
    else
    {
      lastSample[AudioOutput::LEFTCHANNEL] = buff[0][buffPtr] << shift;
      lastSample[AudioOutput::RIGHTCHANNEL] = buff[0][buffPtr] << shift;
    }
#else
    if (bitsPerSample <= 16)
    {
      lastSample[AudioOutput::LEFTCHANNEL] = buff[0][buffPtr] & 0xffff;
      if (channels == 2)
        lastSample[AudioOutput::RIGHTCHANNEL] = buff[1][buffPtr] & 0xffff;
      else
        lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
    }
    else if (bitsPerSample <= 24)
    {
      lastSample[AudioOutput::LEFTCHANNEL] = (buff[0][buffPtr] >> 8) & 0xffff;
      if (channels == 2)
        lastSample[AudioOutput::RIGHTCHANNEL] = (buff[1][buffPtr] >> 8) & 0xffff;
      else
        lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
    }
    else
    {
      lastSample[AudioOutput::LEFTCHANNEL] = (buff[0][buffPtr] >> 16) & 0xffff;
      if (channels == 2)
        lastSample[AudioOutput::RIGHTCHANNEL] = (buff[1][buffPtr] >> 16) & 0xffff;
      else
        lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
    }
#endif
    buffPtr++;
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();

  return running;
}

bool AudioGeneratorFLAC::stop()
{
  if (flac)
    FLAC__stream_decoder_delete(flac);
  flac = NULL;
  running = false;
  output->stop();
  return true;
}

bool AudioGeneratorFLAC::isRunning()
{
  return running;
}

FLAC__StreamDecoderReadStatus AudioGeneratorFLAC::read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes)
{
  (void)decoder;
  if (*bytes == 0)
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  *bytes = file->read(buffer, sizeof(FLAC__byte) * (*bytes));
  if (*bytes == 0)
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}
FLAC__StreamDecoderSeekStatus AudioGeneratorFLAC::seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset)
{
  (void)decoder;
  if (!file->seek((int32_t)absolute_byte_offset, 0))
    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}
FLAC__StreamDecoderTellStatus AudioGeneratorFLAC::tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset)
{
  (void)decoder;
  *absolute_byte_offset = file->getPos();
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus AudioGeneratorFLAC::length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length)
{
  (void)decoder;
  *stream_length = file->getSize();
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}
FLAC__bool AudioGeneratorFLAC::eof_cb(const FLAC__StreamDecoder *decoder)
{
  (void)decoder;
  if (file->getPos() >= file->getSize())
    return true;
  return false;
}
FLAC__StreamDecoderWriteStatus AudioGeneratorFLAC::write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[])
{
  (void)decoder;
  // Hackish warning here.  FLAC sends the buffer but doesn't free it until the next call to decode_frame, so we stash
  // the pointers here and use it in our loop() instead of memcpy()'ing into yet another buffer.
  buffLen = frame->header.blocksize;
  buff[0] = (const int *)buffer[0];
  if (frame->header.channels > 1)
    buff[1] = (const int *)buffer[1];
  else
    buff[1] = (const int *)buffer[0];
  buffPtr = 0;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
void AudioGeneratorFLAC::metadata_cb(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata)
{
  (void)decoder;
  (void)metadata;

  switch (metadata->type)
  {
  case FLAC__METADATA_TYPE_STREAMINFO:
    // 处理流信息（采样率、声道数等）
    log_i("FLAC StreamInfo: %lu Hz, %u bits, %u channels, %llu samples",
          metadata->data.stream_info.sample_rate,
          metadata->data.stream_info.bits_per_sample,
          metadata->data.stream_info.channels,
          metadata->data.stream_info.total_samples);
    if (metadata->data.stream_info.sample_rate != 0 && metadata->data.stream_info.total_samples != 0)
    {
      uint64_t total_time = metadata->data.stream_info.total_samples * 1000 / metadata->data.stream_info.sample_rate;
      log_i("tatal time: %llu", total_time);
      cb.md("tlen", false, ((String)total_time).c_str());
    }

    break;
  case FLAC__METADATA_TYPE_VORBIS_COMMENT:
    // 处理 Vorbis 注释（标签信息）
    log_i("FLAC Vorbis Comments: %u comments",
          metadata->data.vorbis_comment.num_comments);
    // If a user‑provided metadata callback is registered, forward each comment
    for (unsigned i = 0; i < metadata->data.vorbis_comment.num_comments; ++i)
    {
      const FLAC__StreamMetadata_VorbisComment_Entry *entry = &metadata->data.vorbis_comment.comments[i];
      // The entry is a "KEY=VALUE" UTF‑8 string (not null‑terminated, length given)
      // Allocate memory if the entry length exceeds 255 bytes.
      const unsigned MAX_STATIC = 255; // max length for static buffer (excluding null)
      char staticBuf[256];
      char *buf = nullptr;
      unsigned copyLen = 0;
      if (entry->length > MAX_STATIC)
      {
        // Try dynamic allocation
        buf = (char *)malloc(entry->length + 1);
        if (buf)
        {
          memcpy(buf, entry->entry, entry->length);
          buf[entry->length] = '\0';
          copyLen = entry->length;
        }
        else
        {
          // Allocation failed, fallback to static buffer and truncate
          copyLen = MAX_STATIC;
          memcpy(staticBuf, entry->entry, copyLen);
          staticBuf[copyLen] = '\0';
          buf = staticBuf;
        }
      }
      else
      {
        // Use static buffer
        copyLen = entry->length;
        memcpy(staticBuf, entry->entry, copyLen);
        staticBuf[copyLen] = '\0';
        buf = staticBuf;
      }
      // Find the first '=' separator
      char *eq = strchr(buf, '=');
      if (eq)
      {
        *eq = '\0'; // split into key and value
        const char *key = buf;
        const char *value = eq + 1;
        cb.md(key, false, value);
      }
      else
      {
        // No '=', treat whole string as type with empty value
        cb.md(buf, false, "");
      }
      // Free dynamic allocation if used
      if (buf != staticBuf && buf != nullptr)
      {
        free(buf);
      }
    }
    break;

  case FLAC__METADATA_TYPE_PICTURE:
    // 处理专辑封面图片
    log_i("FLAC Picture: %u x %u, %u bytes",
          metadata->data.picture.width,
          metadata->data.picture.height,
          metadata->data.picture.data_length);
    break;
  default:
    // 其他类型的元数据
    log_i("FLAC Metadata type: %d", metadata->type);
    break;
  }
}
char AudioGeneratorFLAC::error_cb_str[64];
void AudioGeneratorFLAC::error_cb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status)
{
  (void)decoder;
  strncpy_P(error_cb_str, FLAC__StreamDecoderErrorStatusString[status], sizeof(AudioGeneratorFLAC::error_cb_str) - 1);
  log_e("%s", error_cb_str);
}
