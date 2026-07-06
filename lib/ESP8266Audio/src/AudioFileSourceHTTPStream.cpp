/*
  AudioFileSourceHTTPStream
  Streaming HTTP source

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

#if defined(ESP32) || defined(ESP8266)

#include "AudioFileSourceHTTPStream.h"

AudioFileSourceHTTPStream::AudioFileSourceHTTPStream()
{
  pos = 0;
  reconnectTries = 0;
  saveURL[0] = 0;
  customHeaderCount = 0;
}

AudioFileSourceHTTPStream::AudioFileSourceHTTPStream(const char *url)
{
  saveURL[0] = 0;
  reconnectTries = 0;
  customHeaderCount = 0;
  open(url);
}

bool AudioFileSourceHTTPStream::open(const char *url)
{
  pos = 0;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(60000);
  http.setReuse(false);
  String urlStr = String(url);
  bool isHttps = urlStr.startsWith("https://");

  if (isHttps)
  {
    _client.setInsecure();
    http.begin(_client, url);
  }
  else
  {
    http.begin(client, url);
  }

  http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
  http.addHeader("Accept", "*/*");
  http.addHeader("Connection", "keep-alive");

  // 应用自定义请求头
  applyCustomHeaders();
  
  int code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    http.end();
    log_e("Can't open HTTP request");
    return false;
  }
  size = http.getSize();
  strncpy(saveURL, url, sizeof(saveURL));
  saveURL[sizeof(saveURL) - 1] = 0;
  return true;
}

AudioFileSourceHTTPStream::~AudioFileSourceHTTPStream()
{
  http.end();
}

uint32_t AudioFileSourceHTTPStream::read(void *data, uint32_t len)
{
  if (data == NULL)
  {
    log_printf("ERROR! read passed NULL data\n");
    return 0;
  }
  return readInternal(data, len, false);
}

uint32_t AudioFileSourceHTTPStream::readNonBlock(void *data, uint32_t len)
{
  if (data == NULL)
  {
    log_printf("ERROR! readNonBlock passed NULL data\n");
    return 0;
  }
  return readInternal(data, len, true);
}

uint32_t AudioFileSourceHTTPStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
  if (!http.connected())
  {
    cb.st(STATUS_DISCONNECTED, PSTR("Stream disconnected"));
    http.end();
    for (int i = 0; i < reconnectTries; i++)
    {
      char buff[64];
      sprintf_P(buff, PSTR("Attempting to reconnect, try %d"), i);
      cb.st(STATUS_RECONNECTING, buff);
      delay(reconnectDelayMs);
      if (open(saveURL))
      {
        cb.st(STATUS_RECONNECTED, PSTR("Stream reconnected"));
        break;
      }
    }
    if (!http.connected())
    {
      cb.st(STATUS_DISCONNECTED, PSTR("Unable to reconnect"));
      return 0;
    }
  }
  if ((size > 0) && (pos >= size))
    return 0;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  NetworkClient *stream = http.getStreamPtr();
#else
  WiFiClient *stream = http.getStreamPtr();
#endif

  // Can't read past EOF...
  if ((size > 0) && (len > (uint32_t)(pos - size)))
    len = pos - size;

  if (!nonBlock)
  {
    int start = millis();
    while ((stream->available() < (int)len) && (millis() - start < 500))
      yield();
  }

  size_t avail = stream->available();
  if (!nonBlock && !avail)
  {
    cb.st(STATUS_NODATA, PSTR("No stream data available"));
    http.end();
    goto retry;
  }
  if (avail == 0)
    return 0;
  if (avail < len)
    len = avail;

  int read = stream->read(reinterpret_cast<uint8_t *>(data), len);
  if (read < 0) read = 0;
  pos += read;
  return read;
}

bool AudioFileSourceHTTPStream::seek(int32_t pos, int dir)
{
  log_printf("ERROR! seek not implemented!\n");
  (void)pos;
  (void)dir;
  return false;
}

bool AudioFileSourceHTTPStream::close()
{
  http.end();
  http.~HTTPClient();
  return true;
}

bool AudioFileSourceHTTPStream::isOpen()
{
  return http.connected();
}

uint32_t AudioFileSourceHTTPStream::getSize()
{
  return size;
}

uint32_t AudioFileSourceHTTPStream::getPos()
{
  return pos;
}

void AudioFileSourceHTTPStream::addCustomHeader(const char *name, const char *value)
{
  if (customHeaderCount < 4) {
    customHeaders[customHeaderCount].name = name;
    customHeaders[customHeaderCount].value = value;
    customHeaderCount++;
  }
}

void AudioFileSourceHTTPStream::applyCustomHeaders()
{
  for (int i = 0; i < customHeaderCount; i++) {
    http.addHeader(customHeaders[i].name, customHeaders[i].value);
  }
}

#endif
