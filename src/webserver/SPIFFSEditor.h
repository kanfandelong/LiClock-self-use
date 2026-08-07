#ifndef SPIFFSEditor_H_
#define SPIFFSEditor_H_
#include <ESPAsyncWebServer.h>

extern "C" {
#include <dirent.h>
}

class SPIFFSEditor: public AsyncWebHandler {
  private:
    fs::FS _fs;
    fs::FS _littlefs;
    String _username;
    String _password; 
    bool _authenticated;
    uint32_t _startTime;
  public:
#ifdef ESP32
    SPIFFSEditor(const fs::FS &fs, const String &username = String(), const String &password = String());
#else
    SPIFFSEditor(const String& username=String(), const String& password=String(), const fs::FS& fs=SPIFFS);
#endif
    bool is_littlefs;
    void setlittlefs(fs::FS &fs);
    void setFileSystem(fs::FS &fs);
    bool canHandle(AsyncWebServerRequest *request);
    virtual bool canHandle(AsyncWebServerRequest *request) const override final
    {
      return const_cast<SPIFFSEditor *>(this)->canHandle(request);
    };
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override final;
    virtual bool isRequestHandlerTrivial() const override final {return false;}
};

#endif