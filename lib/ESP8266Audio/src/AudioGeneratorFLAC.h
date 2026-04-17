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

#ifndef _AUDIOGENERATORFLAC_H
#define _AUDIOGENERATORFLAC_H

#include <AudioGenerator.h>
extern "C"
{
#include "libflac/FLAC/stream_decoder.h"
#include <freertos/ringbuf.h>
};

#define RINGBUF_SIZE (1024 * 1024) // 1 MB
#define TEMPBUF_SIZE (16 * 1024)

// Macro to control decode timing logging. Define ENABLE_FLAC_DECODE_TIMING to 0 to disable.
// Users can override this definition via compiler flags or before including this header.

class AudioGeneratorFLAC : public AudioGenerator
{
public:
  AudioGeneratorFLAC();
  virtual ~AudioGeneratorFLAC() override;
  virtual bool begin(AudioFileSource *source, AudioOutput *output) override;
  virtual bool loop() override;
  virtual bool stop() override;
  virtual bool isRunning() override;
  void _en_ringbuff(bool en) { en_ringbuff = en; };

protected:
  // FLAC info
  uint16_t channels;
  uint16_t bitsPerSample;
  uint32_t sampleRate;

  // We need to buffer some data in-RAM to avoid doing 1000s of small reads
  // 支持最多8个声道（FLAC__MAX_CHANNELS）
  const int *buff[8];
  uint16_t buffPtr;
  uint16_t buffLen;
  FLAC__StreamDecoder *flac;
  // Timing statistics for decoder processing (enabled when ENABLE_FLAC_DECODE_TIMING != 0)
#if ENABLE_FLAC_DECODE_TIMING
  uint64_t decodeTimeSumUs = 0; // Sum of decode times in microseconds
  uint32_t decodeCount = 0;     // Number of decode calls measured
  uint32_t lastLogMs = 0;       // Last time average was logged (milliseconds)
#endif

  static void fillTask(void *param);
  void start_fillTask();
  TaskHandle_t *filltaskhandle = NULL;
  RingbufHandle_t ringBuf = NULL;
  StaticRingbuffer_t StaticRingbuffer;
  uint8_t *ringBufferStorage = NULL;
  uint8_t *tempBuffer = NULL;
  bool en_ringbuff = false;

  // FLAC callbacks, need static functions to bounce into c++ from c
  static FLAC__StreamDecoderReadStatus _read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
  {
    return static_cast<AudioGeneratorFLAC *>(client_data)->read_cb(decoder, buffer, bytes);
  };
  static FLAC__StreamDecoderSeekStatus _seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
  {
    return static_cast<AudioGeneratorFLAC *>(client_data)->seek_cb(decoder, absolute_byte_offset);
  };
  static FLAC__StreamDecoderTellStatus _tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
  {
    return static_cast<AudioGeneratorFLAC *>(client_data)->tell_cb(decoder, absolute_byte_offset);
  };
  static FLAC__StreamDecoderLengthStatus _length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
  {
    return static_cast<AudioGeneratorFLAC *>(client_data)->length_cb(decoder, stream_length);
  };
  static FLAC__bool _eof_cb(const FLAC__StreamDecoder *decoder, void *client_data)
  {
    return static_cast<AudioGeneratorFLAC *>(client_data)->eof_cb(decoder);
  };
  static FLAC__StreamDecoderWriteStatus _write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
  {
    return static_cast<AudioGeneratorFLAC *>(client_data)->write_cb(decoder, frame, buffer);
  };
  static void _metadata_cb(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
  {
    static_cast<AudioGeneratorFLAC *>(client_data)->metadata_cb(decoder, metadata);
  };
  static void _error_cb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
  {
    static_cast<AudioGeneratorFLAC *>(client_data)->error_cb(decoder, status);
  };
  // Actual FLAC callbacks
  FLAC__StreamDecoderReadStatus read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes);
  FLAC__StreamDecoderSeekStatus seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset);
  FLAC__StreamDecoderTellStatus tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset);
  FLAC__StreamDecoderLengthStatus length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length);
  FLAC__bool eof_cb(const FLAC__StreamDecoder *decoder);
  FLAC__StreamDecoderWriteStatus write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[]);
  void metadata_cb(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata);
  static char error_cb_str[64];
  void error_cb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status);
};

#endif
