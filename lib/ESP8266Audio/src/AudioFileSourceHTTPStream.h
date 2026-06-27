/*
  AudioFileSourceHTTPStream
  Connect to a HTTP based streaming service
  
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
#pragma once

#include <Arduino.h>
#ifdef ESP32
  #include <HTTPClient.h>
#else
  #include <ESP8266HTTPClient.h>
#endif
#include "AudioFileSource.h"

// 自定义请求头容器
struct HTTPHeaderPair {
  const char *name;
  const char *value;
};

class AudioFileSourceHTTPStream : public AudioFileSource
{
  friend class AudioFileSourceICYStream;

  public:
    AudioFileSourceHTTPStream();
    AudioFileSourceHTTPStream(const char *url);
    virtual ~AudioFileSourceHTTPStream() override;
    
    virtual bool open(const char *url) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual uint32_t readNonBlock(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;
    bool SetReconnect(int tries, int delayms) { reconnectTries = tries; reconnectDelayMs = delayms; return true; }
    void useHTTP10 () { http.useHTTP10(true); }

    // 添加自定义 HTTP 请求头 (需在 open() 前调用)
    void addCustomHeader(const char *name, const char *value);

    enum { STATUS_HTTPFAIL=2, STATUS_DISCONNECTED, STATUS_RECONNECTING, STATUS_RECONNECTED, STATUS_NODATA };

  private:
    virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock);
    void applyCustomHeaders();

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    NetworkClient client;
    NetworkClientSecure _client;
#else
    WiFiClient client;
    WiFiClientSecure _client;
#endif
    HTTPClient http;
    int pos;
    int size;
    int reconnectTries;
    int reconnectDelayMs;
    char saveURL[256];
    
    // 自定义请求头存储 (最多 4 个)
    HTTPHeaderPair customHeaders[4];
    int customHeaderCount;
};


#endif

