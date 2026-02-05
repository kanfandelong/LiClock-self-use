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
}

AudioGeneratorFLAC::~AudioGeneratorFLAC()
{
  if (flac)
    FLAC__stream_decoder_delete(flac);
  flac = NULL;
}

bool AudioGeneratorFLAC::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source) return false;
  file = source;
  if (!output) return false;
  this->output = output;
  if (!file->isOpen()) return false; // Error

  flac = FLAC__stream_decoder_new();
  if (!flac) return false;
  
  (void)FLAC__stream_decoder_set_md5_checking(flac, false);

  FLAC__StreamDecoderInitStatus ret = FLAC__stream_decoder_init_stream(flac, _read_cb, _seek_cb, _tell_cb, _length_cb, _eof_cb, _write_cb, _metadata_cb, _error_cb, reinterpret_cast<void*>(this) );
  if (ret != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    FLAC__stream_decoder_delete(flac);
    flac = NULL;
    return false;
  }

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

  if (!running) goto done;

  if (channels && !output->ConsumeSample(lastSample)) goto done; // Try and send last buffered sample

  do {
    if (buffPtr == buffLen) {
      ret = FLAC__stream_decoder_process_single(flac);
      if (!ret) {
        running = false;
        goto done;
      } else {
        // We might be done...
        if (FLAC__stream_decoder_get_state(flac)==FLAC__STREAM_DECODER_END_OF_STREAM) {
          running = false;
          goto done;
        }
        unsigned newsr = FLAC__stream_decoder_get_sample_rate(flac);
        unsigned newch = FLAC__stream_decoder_get_channels(flac);
        unsigned newbps = FLAC__stream_decoder_get_bits_per_sample(flac);
        if (newsr != sampleRate) output->SetRate(sampleRate = newsr);
        if (newch != channels) output->SetChannels(channels = newch);
        if (newbps != bitsPerSample) output->SetBitsPerSample( bitsPerSample = newbps);
      }
    }

    // Check for some weird case where above didn't give any data
    if (buffPtr == buffLen) {
      goto done; // At some point the flac better error and we'll return 
    }
    #ifdef CONFIG_DAC_32bit
    int shift = 0;
    if (bitsPerSample <= 8) shift = 24;
    else if (bitsPerSample <= 16) shift = 16;
    else if (bitsPerSample <= 24) shift = 8;
    else shift = 0;

    if (channels == 2) {
        lastSample[AudioOutput::LEFTCHANNEL]  = buff[0][buffPtr] << shift;
        lastSample[AudioOutput::RIGHTCHANNEL] = buff[1][buffPtr] << shift;
    } else {
        lastSample[AudioOutput::LEFTCHANNEL]  = buff[0][buffPtr] << shift;
        lastSample[AudioOutput::RIGHTCHANNEL] = buff[0][buffPtr] << shift;
    }
    #else
    if (bitsPerSample <= 16) {
      lastSample[AudioOutput::LEFTCHANNEL] = buff[0][buffPtr] & 0xffff; 
      if (channels==2) lastSample[AudioOutput::RIGHTCHANNEL] = buff[1][buffPtr] & 0xffff; 
      else lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
    } else if (bitsPerSample <= 24) {
      lastSample[AudioOutput::LEFTCHANNEL] = (buff[0][buffPtr]>>8) & 0xffff; 
      if (channels==2) lastSample[AudioOutput::RIGHTCHANNEL] = (buff[1][buffPtr]>>8) & 0xffff; 
      else lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
    } else {
      lastSample[AudioOutput::LEFTCHANNEL] = (buff[0][buffPtr]>>16) & 0xffff; 
      if (channels==2) lastSample[AudioOutput::RIGHTCHANNEL] = (buff[1][buffPtr]>>16) & 0xffff; 
      else lastSample[AudioOutput::RIGHTCHANNEL] = lastSample[AudioOutput::LEFTCHANNEL];
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
  (void) decoder;
  if (*bytes==0) return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  *bytes = file->read(buffer, sizeof(FLAC__byte) * (*bytes));
  if (*bytes==0) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}
FLAC__StreamDecoderSeekStatus AudioGeneratorFLAC::seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset)
{
  (void) decoder;
  if (!file->seek((int32_t)absolute_byte_offset, 0)) return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}
FLAC__StreamDecoderTellStatus AudioGeneratorFLAC::tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset)
{
  (void) decoder;
  *absolute_byte_offset = file->getPos();
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus AudioGeneratorFLAC::length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length)
{
  (void) decoder;
  *stream_length = file->getSize();
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}
FLAC__bool AudioGeneratorFLAC::eof_cb(const FLAC__StreamDecoder *decoder)
{
  (void) decoder;
  if (file->getPos() >= file->getSize()) return true;
  return false;
}
FLAC__StreamDecoderWriteStatus AudioGeneratorFLAC::write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[])
{
  (void) decoder;
  // Hackish warning here.  FLAC sends the buffer but doesn't free it until the next call to decode_frame, so we stash
  // the pointers here and use it in our loop() instead of memcpy()'ing into yet another buffer.
  buffLen = frame->header.blocksize;
  buff[0] = (const int *)buffer[0];
  if (frame->header.channels>1) buff[1] = (const int *)buffer[1];
  else buff[1] = (const int *)buffer[0];
  buffPtr = 0;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
void AudioGeneratorFLAC::metadata_cb(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata)
{
  (void) decoder;
  (void) metadata;
  audioLogger->printf_P(PSTR("Metadata\n"));
/*   if (!metadataCallback) return; // 如果没有注册回调，直接返回
  
  switch (metadata->type) {
      case FLAC__METADATA_TYPE_STREAMINFO:
          // 处理流信息（采样率、声道数等）
          audioLogger->printf_P(PSTR("FLAC StreamInfo: %lu Hz, %u channels, %lu samples\n"),
                                metadata->data.stream_info.sample_rate,
                                metadata->data.stream_info.channels,
                                (unsigned long)metadata->data.stream_info.total_samples);
          if (metadata->data.stream_info.total_samples > 0 && 
              metadata->data.stream_info.sample_rate > 0) {
              
              uint32_t duration_ms = (uint32_t)((metadata->data.stream_info.total_samples * 1000) / 
                                              metadata->data.stream_info.sample_rate);
              
              // 通过回调传递时长信息
              char durationStr[20];
              snprintf(durationStr, sizeof(durationStr), "%lu", duration_ms);
              metadataCallback(metadataCallbackData, "tlen", false, durationStr);
          }
          break;
          
      case FLAC__METADATA_TYPE_VORBIS_COMMENT:
          // 处理 Vorbis 注释（标签信息）
          audioLogger->printf_P(PSTR("FLAC Vorbis Comments: %u comments\n"),
                                metadata->data.vorbis_comment.num_comments);
          
          // 遍历所有注释
          for (unsigned int i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
              FLAC__StreamMetadata_VorbisComment_Entry entry = metadata->data.vorbis_comment.comments[i];
              
              // 将注释转换为字符串
              String commentStr;
              for (unsigned int j = 0; j < entry.length; j++) {
                  commentStr += (char)entry.entry[j];
              }
              
              // 分离键值对
              int equalsPos = commentStr.indexOf('=');
              if (equalsPos > 0) {
                  String key = commentStr.substring(0, equalsPos);
                  String value = commentStr.substring(equalsPos + 1);
                  
                  // 映射到 ID3 标签类型
                  const char* id3Type = nullptr;
                  if (key.equalsIgnoreCase("TITLE")) id3Type = "title";
                  else if (key.equalsIgnoreCase("ARTIST")) id3Type = "performer";
                  else if (key.equalsIgnoreCase("ALBUM")) id3Type = "album";
                  else if (key.equalsIgnoreCase("DATE")) id3Type = "date";
                  else if (key.equalsIgnoreCase("GENRE")) id3Type = "genre";
                  else if (key.equalsIgnoreCase("TRACKNUMBER")) id3Type = "track";
                  else if (key.equalsIgnoreCase("COMMENT")) id3Type = "comment";
                  
                  if (id3Type) {
                      // 调用现有的 MDCallback 函数
                      metadataCallback(metadataCallbackData, id3Type, false, value.c_str());
                  }
              }
          }
          break;
          
      case FLAC__METADATA_TYPE_PICTURE:
          // 处理专辑封面图片
          audioLogger->printf_P(PSTR("FLAC Picture: %u x %u, %u bytes\n"),
                                metadata->data.picture.width,
                                metadata->data.picture.height,
                                metadata->data.picture.data_length);
          break;   
      default:
          // 其他类型的元数据
          audioLogger->printf_P(PSTR("FLAC Metadata type: %d\n"), metadata->type);
          break;
  } */
}
char AudioGeneratorFLAC::error_cb_str[64];
void AudioGeneratorFLAC::error_cb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status)
{
  (void) decoder;
  strncpy_P(error_cb_str, FLAC__StreamDecoderErrorStatusString[status], sizeof(AudioGeneratorFLAC::error_cb_str) - 1);
  cb.st((int)status, error_cb_str);
}

