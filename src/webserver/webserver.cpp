#include "A_Config.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include "SPIFFSEditor.h"

// 下面是引用网页文件
#include "index.h"
#include "csss.h"
#include "jss.h"
#include "jss2.h"
#include "favicon.h"
#include "blockly.h"
#include "jss3.h"
////////////////////////////下面是lua部分//////////////////////////
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
TaskHandle_t lua_server_handle = NULL;
SPIFFSEditor *spiffs_upload_handler = NULL;
bool serverRunning = false;
bool wsRunning = false;
bool file_for_TF = false;
bool LuaRunning = false; // 全局变量，表示Lua服务器是否运行，用于防止调试时误退出
size_t littlefs_total = 0;
uint64_t sd_total = 0;
extern "C" void lua_printf(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    char str[1024];
    vsprintf(str, format, argptr);
    if (wsRunning)
    {
        ws.textAll(str);
    }
    else
    {
        log_i("%s", str);
    }
    va_end(argptr);
}
static void task_lua_server(void *)
{
    LuaRunning = true;
    lua_execute("/littlefs/webtmp/main.lua");
    lua_printf("[Lua程序结束]");
    lua_server_handle = NULL;
    LuaRunning = false;
    vTaskDelete(NULL);
}

// lua默认上传到LittleFS根目录下的temp.lua
void luaExecuteHandler(AsyncWebServerRequest *request)
{
    closeLua();
    openLua();
    setPath("/littlefs/webtmp");
    xTaskCreate(task_lua_server, "lua_server", 40960, NULL, 0, &lua_server_handle);
    request->send(200, "text/plain", "OK");
}

void luaTerminateHandler(AsyncWebServerRequest *request)
{
    if (lua_server_handle != NULL)
    {
        vTaskDelete(lua_server_handle);
        lua_server_handle = NULL;
    }
    closeLua();
    ws.textAll("\n[Lua语言服务任务已被删除]\n");
    request->send(200, "text/plain", "OK");
    LuaRunning = false;
}

void rmrfHandler(AsyncWebServerRequest *request)
{
    if (request->hasArg("path"))
    {
        String path = request->arg("path");
        if (path == "")
        {
            request->send(500, "text/plain", "EER");
        }
        if (file_for_TF)
            hal.rm_rf((String("/sd/") + path).c_str());
        else
            hal.rm_rf((String("/littlefs/") + path).c_str());
        request->send(200, "text/plain", "OK");
        return;
    }
    request->send(500, "text/plain", "EER");
}

void renameHandler(AsyncWebServerRequest *request)
{
    if (request->hasArg("path") && request->hasArg("new"))
    {
        String path = request->arg("path");
        String newpath = request->arg("new");
        if (file_for_TF)
        {
            if (SD.rename(path, newpath))
            {
                request->send(200, "text/plain", "OK");
                return;
            }
        }
        else
        {
            if (LittleFS.rename(path, newpath))
            {
                request->send(200, "text/plain", "OK");
                return;
            }
        }
    }
    request->send(500, "text/plain", "EER");
}
void mkdirHandler(AsyncWebServerRequest *request)
{
    if (request->hasArg("path"))
    {
        String path = request->arg("path");
        if (file_for_TF)
        {
            if (SD.mkdir(path))
            {
                request->send(200, "text/plain", "OK");
                return;
            }
        }
        else
        {
            if (LittleFS.mkdir(path))
            {
                request->send(200, "text/plain", "OK");
                return;
            }
        }
    }
    request->send(500, "text/plain", "EER");
}

void fs_status(AsyncWebServerRequest *request)
{
    String status;
    if (file_for_TF)
    {
        if (sd_total == 0)
        {
            sd_total = SD.totalBytes();
        }
        status += "{\n";
        status += "    \"type\":\"TF\",\n";
        status += "    \"isOk\":\"true\",\n";
        status += "    \"usedBytes\":" + String(SD.usedBytes()) + ",\n";
        status += "    \"totalBytes\":" + String(sd_total) + ",\n";
        status += "    \"unsupportedFiles\":\"\"\n";
        status += "}";
        request->send(200, "application/json", status);
    }
    else
    {
        if (littlefs_total == 0)
        {
            littlefs_total = LittleFS.totalBytes();
        }
        status += "{\n";
        status += "    \"type\":\"LittleFS\",\n";
        status += "    \"isOk\":\"true\",\n";
        status += "    \"usedBytes\":" + String(LittleFS.usedBytes()) + ",\n";
        status += "    \"totalBytes\":" + String(littlefs_total) + ",\n";
        status += "    \"unsupportedFiles\":\"\"\n";
        status += "}";
        request->send(200, "application/json", status);
    }
}

bool myxcopy(const String path, const String newpath)
{
    std::list<String> filenames;
    File root, file;
    filenames.push_back(path);
    String tmp;
    while (filenames.empty() == false)
    {
        root = LittleFS.open(filenames.back());
        tmp = filenames.back();
        tmp.replace(path, newpath);
        filenames.pop_back();
        if (!root)
        {
            Serial.println("[文件] 无法打开目录");
            continue;
        }
        LittleFS.mkdir(tmp);
        file = root.openNextFile();
        while (file)
        {
            String name = file.name();
            if (file.isDirectory())
            {
                tmp = file.path();
                tmp.replace(path, newpath);
                LittleFS.mkdir(tmp);
                filenames.push_back(file.path());
            }
            else
            {
                // 复制文件
                tmp = file.path();
                tmp.replace(path, newpath);
                File newFile = LittleFS.open(tmp, "w");
                if (!newFile)
                {
                    // 打开失败
                    Serial.println("无法写入文件");
                    file.close();
                    root.close();
                    return false;
                }
                hal.copy(newFile, file);
                newFile.close();
                file.close();
            }
            file.close();
            file = root.openNextFile();
        }
    }
    root.close();
    return true;
}
void createAppHandler(AsyncWebServerRequest *request)
{
    if (request->hasArg("name"))
    {
        String name = request->arg("name");
        if (name == "")
        {
            request->send(500, "text/plain", "EER");
        }
        if (name.endsWith(".app") == false)
            name += ".app";
        hal.rm_rf((String("/littlefs/") + name).c_str());
        String currentPath = "/" + name;
        if (myxcopy("/webtmp", currentPath))
        {
            request->send(200, "text/plain", "OK");
            return;
        }
    }
    request->send(500, "text/plain", "EER");
}
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        String msg = "";
        if (info->final && info->index == 0 && info->len == len)
        {
            // the whole message is in a single frame and we got all of it's data
            // Serial.printf("ws[%u]", client->id());
            if (info->opcode == WS_TEXT)
            {
                /*
                    for (size_t i = 0; i < info->len; i++)
                    {
                            msg += (char)data[i];
                    }
                    */
            }
        }
    }
}
static void sendreq(AsyncWebServerRequest *request, const char *mime, const uint8_t *name, unsigned int len)
{
    const char *buildTime = __DATE__ " " __TIME__ " GMT";
    if (request->header("If-Modified-Since").equals(buildTime))
    {
        request->send(304);
    }
    else
    {
        AsyncWebServerResponse *response = request->beginResponse_P(200, mime, name, len);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Last-Modified", buildTime);
        request->send(response);
    }
}
void beginFileServer(bool for_TF)
{
    bool mdns;
    if (hal.pref.getBool("en_mdns"))
    {
        mdns = MDNS.begin("weatherclock");
    }
    // server = new AsyncWebServer(80);
    // 设置未找到路由的默认响应
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        if(WiFi.softAPgetStationNum() != 0)
        {
            request->redirect("http://192.168.4.1");
        }
        else
        {
            request->send(404);
        } });
    // 添加SPIFFS文件编辑器
    file_for_TF = for_TF;
    if (file_for_TF)
    {
        if (!peripherals.isSDLoaded())
            peripherals.load(PERIPHERALS_SD_BIT);
        spiffs_upload_handler = new SPIFFSEditor(SD);
    }
    else
    {
        spiffs_upload_handler = new SPIFFSEditor(LittleFS);
    }
    spiffs_upload_handler->setlittlefs(LittleFS);
    server.addHandler(spiffs_upload_handler);
    server.on("/system/ace.js", HTTP_GET, [](AsyncWebServerRequest *request)
              {     
                  const char *buildTime = __DATE__ " " __TIME__ " GMT";
                  if (request->header("If-Modified-Since").equals(buildTime))
                  {
                      request->send(304);
                  }
                  else
                  {
                    if (LittleFS.exists("/System/ace.js.gz")) {
                        File file = LittleFS.open("/System/ace.js.gz", "r");
                        time_t lastWrite = file.getLastWrite(); // 获取UTC时间戳
                        file.close();
                  
                        struct tm tm;
                        gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构
                        
                        char timeStr[64];
                        strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                        buildTime = timeStr;
                        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/System/ace.js.gz", "application/javascript", false);
                        response->addHeader("Content-Encoding", "gzip");
                        response->addHeader("Last-Modified", buildTime);
                        request->send(response);
                    }
                    else
                        request->send(404, "text/plain", "资源文件不存在");
                  } });
    server.on("/iconfont.ttf", HTTP_GET, [](AsyncWebServerRequest *request)
              {     
                  const char *buildTime = __DATE__ " " __TIME__ " GMT";
                  if (request->header("If-Modified-Since").equals(buildTime))
                  {
                      request->send(304);
                  }
                  else
                  {
                    if (LittleFS.exists("/System/iconfont.ttf")) {
                        File file = LittleFS.open("/System/iconfont.ttf", "r");
                        time_t lastWrite = file.getLastWrite(); // 获取UTC时间戳
                        file.close();
                  
                        struct tm tm;
                        gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构
                        
                        char timeStr[64];
                        strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                        buildTime = timeStr;
                        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/System/iconfont.ttf", "font/ttf", false);
                        // response->addHeader("Content-Encoding", "gzip");
                        response->addHeader("Last-Modified", buildTime);
                        request->send(response);
                    }
                    else
                        request->send(404, "text/plain", "资源文件不存在");
                  } });
    server.on("/switch_file_system", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                file_for_TF =! file_for_TF;
                if (file_for_TF) {
                    if (!peripherals.isSDLoaded()) {
                        peripherals.load(PERIPHERALS_SD_BIT);
                        if (peripherals.isSDLoaded()) {
                            spiffs_upload_handler->setFileSystem(SD);
                            request->send(200, "text/plain", "SD");
                        }
                        else {
                            file_for_TF =! file_for_TF;
                            peripherals.tf_unload();
                            if (digitalRead(PIN_SD_CARDDETECT) == HIGH)
                                request->send(409, "text/plain", "SD卡不存在");
                            else
                                request->send(409, "text/plain", "SD卡挂载失败");
                        }
                    }
                    else {   
                        spiffs_upload_handler->setFileSystem(SD);
                        request->send(200, "text/plain", "SD");
                    }
                } else {
                    spiffs_upload_handler->setFileSystem(LittleFS);
                    request->send(200, "text/plain", "LittleFS");
                } });
    server.on("/fs_get", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                                if (file_for_TF) {
                                    request->send(200, "text/plain", "SD");
                                } else {
                                    request->send(200, "text/plain", "LittleFS");
                                } });
    server.on("/status", HTTP_GET, fs_status);
    server.on("/rmrf", HTTP_POST, rmrfHandler);
    server.on("/mkdir", HTTP_POST, mkdirHandler);
    server.on("/rename", HTTP_POST, renameHandler);
    // 启动服务器
    server.begin();
    if (mdns)
        MDNS.addService("http", "tcp", 80);
    else
        Serial.println("Error setting up MDNS responder!");

    Serial.println("File Server started");
    serverRunning = true;
    hal.can_sleep = false;
}
void beginWebServer()
{
    bool mdns;
    if (hal.pref.getBool("en_mdns"))
    {
        mdns = MDNS.begin("weatherclock");
    }
    // server = new AsyncWebServer(80);
    if (LittleFS.exists("/webtmp") == false)
    {
        LittleFS.mkdir("/webtmp");
    }
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        if(WiFi.softAPgetStationNum() != 0)
        {
            request->redirect("http://192.168.4.1");
        }
        else
        {
            request->send(404);
        } });
    wsRunning = true;
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendreq(request, "text/html", __web_index_html_gz, __web_index_html_gz_len); });
    server.on("/blockly", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendreq(request, "text/html", __web_Blockly_html_gz, __web_Blockly_html_gz_len); });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendreq(request, "image/x-icon", __favicon_ico_gz, __favicon_ico_gz_len); });
    server.on("/css/csss.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendreq(request, "text/css", __web_css_csss_css_gz, __web_css_csss_css_gz_len); });
    server.on("/js/jss.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendreq(request, "application/javascript", __web_js_jss_js_gz, __web_js_jss_js_gz_len); });
    server.on("/js/jss2.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendreq(request, "application/javascript", __web_js_jss2_js_gz, __web_js_jss2_js_gz_len); });
    server.on("/js/jss3.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendreq(request, "application/javascript", __web_js_jss3_js_gz, __web_js_jss3_js_gz_len); });
    server.on("/system/ace.js", HTTP_GET, [](AsyncWebServerRequest *request)
              {     
                  const char *buildTime = __DATE__ " " __TIME__ " GMT";
                  if (request->header("If-Modified-Since").equals(buildTime))
                  {
                      request->send(304);
                  }
                  else
                  {
                    if (LittleFS.exists("/System/ace.js.gz")) {
                        File file = LittleFS.open("/System/ace.js.gz", "r");
                        time_t lastWrite = file.getLastWrite(); // 获取UTC时间戳
                        file.close();
                  
                        struct tm tm;
                        gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构
                        
                        char timeStr[64];
                        strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                        buildTime = timeStr;
                        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/System/ace.js.gz", "application/javascript", false);
                        response->addHeader("Content-Encoding", "gzip");
                        response->addHeader("Last-Modified", buildTime);
                        request->send(response);
                    }
                    else
                        request->send(404, "text/plain", "资源文件不存在");
                  } });
    server.on("/iconfont.ttf", HTTP_GET, [](AsyncWebServerRequest *request)
              {     
                  const char *buildTime = __DATE__ " " __TIME__ " GMT";
                  if (request->header("If-Modified-Since").equals(buildTime))
                  {
                      request->send(304);
                  }
                  else
                  {
                    if (LittleFS.exists("/System/iconfont.ttf")) {
                        File file = LittleFS.open("/System/iconfont.ttf", "r");
                        time_t lastWrite = file.getLastWrite(); // 获取UTC时间戳
                        file.close();
                  
                        struct tm tm;
                        gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构
                        
                        char timeStr[64];
                        strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                        buildTime = timeStr;
                        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/System/iconfont.ttf", "font/ttf", false);
                        // response->addHeader("Content-Encoding", "gzip");
                        response->addHeader("Last-Modified", buildTime);
                        request->send(response);
                    }
                    else
                        request->send(404, "text/plain", "资源文件不存在");
                  } });
    server.on("/viewth", HTTP_GET, [](AsyncWebServerRequest *request)
              {     
                  const char *buildTime = __DATE__ " " __TIME__ " GMT";
                  if (request->header("If-Modified-Since").equals(buildTime))
                  {
                      request->send(304);
                  }
                  else
                  {
                    if (LittleFS.exists("/System/viewth.html.gz")) {
                        File file = LittleFS.open("/System/viewth.html.gz", "r");
                        time_t lastWrite = file.getLastWrite(); // 获取UTC时间戳
                        file.close();
                  
                        struct tm tm;
                        gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构
                        
                        char timeStr[64];
                        strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                        buildTime = timeStr;
                        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/System/viewth.html.gz", "text/html", false);
                        response->addHeader("Content-Encoding", "gzip");
                        response->addHeader("Last-Modified", buildTime);
                        request->send(response);
                    }
                    else
                        request->send(404, "text/plain", "资源文件不存在");
                  } });
    server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                                String message;
                                message += "IP地址: ";
                                message += hal.getip().toString();
                                message += "<br/>MAC地址: ";
                                message += WiFi.macAddress();
                                message += "<br/>系统版本: " + String(code_version) + " 系统时间: ";
                                char strftime_buf[64];
                                strftime(strftime_buf, sizeof(strftime_buf), "%c", &hal.timeinfo);
                                message += strftime_buf;
                                message += "<br/>ADC电压：";
                                message += hal.VCC;
                                message += "<br/>USB";
                                message += hal.USBPluggedIn?"已插入":"未插入";
                                message += hal.isCharging?"<br/>正在充电":"<br/>未充电";
                                request->send(200, "text/plain", message); });

    server.on("/conf", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                Serial.println(request->getParam("json", true, false)->value());
                                deserializeJson(config, request->getParam(0)->value());
                                request->send(200, "text/plain", "OK");
                                hal.saveConfig(); });

    server.on("/poweroff", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                                request->send(200, "text/plain", "OK");
                                delay(50);
                                hal.powerOff(); });

    server.on("/switch_file_system", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                                file_for_TF =! file_for_TF;
                                if (file_for_TF) {
                                    if (!peripherals.isSDLoaded()) {
                                        peripherals.load(PERIPHERALS_SD_BIT);
                                        if (peripherals.isSDLoaded()) {
                                            spiffs_upload_handler->setFileSystem(SD);
                                            request->send(200, "text/plain", "SD");
                                        }
                                        else {
                                            file_for_TF =! file_for_TF;
                                            peripherals.tf_unload();
                                            if (digitalRead(PIN_SD_CARDDETECT) == HIGH)
                                                request->send(409, "text/plain", "SD卡不存在");
                                            else
                                                request->send(409, "text/plain", "SD卡挂载失败");
                                        }
                                    }
                                    else {   
                                        spiffs_upload_handler->setFileSystem(SD);
                                        request->send(200, "text/plain", "SD");
                                    }
                                } else {
                                    spiffs_upload_handler->setFileSystem(LittleFS);
                                    request->send(200, "text/plain", "LittleFS");
                                } });
    server.on("/fs_get", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                                if (file_for_TF) {
                                    request->send(200, "text/plain", "SD");
                                } else {
                                    request->send(200, "text/plain", "LittleFS");
                                } });

    server.on("/status", HTTP_GET, fs_status);

    server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", config.as<String>()); });

    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                                request->send(200, "text/plain", "OK");
                                delay(100);
                                ESP.restart(); });

    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", String(ESP.getFreeHeap())); });

    if (file_for_TF)
    {
        if (!peripherals.isSDLoaded())
            peripherals.load(PERIPHERALS_SD_BIT);
        spiffs_upload_handler = new SPIFFSEditor(SD);
    }
    else
    {
        spiffs_upload_handler = new SPIFFSEditor(LittleFS);
    }
    spiffs_upload_handler->setlittlefs(LittleFS);
    server.addHandler(spiffs_upload_handler);
    server.on("/rundebug", HTTP_GET, luaExecuteHandler);
    server.on("/terminate", HTTP_GET, luaTerminateHandler);
    server.on("/rmrf", HTTP_POST, rmrfHandler);
    server.on("/mkdir", HTTP_POST, mkdirHandler);
    server.on("/rename", HTTP_POST, renameHandler);
    server.on("/createapp", HTTP_POST, createAppHandler);
    server.begin();
    if (mdns)
        MDNS.addService("http", "tcp", 80);
    else
        Serial.println("Error setting up MDNS responder!");
    Serial.println("HTTP server started");
    serverRunning = true;
    hal.can_sleep = false;
}
void updateWebServer()
{
    ws.cleanupClients();
    delay(100);
}
