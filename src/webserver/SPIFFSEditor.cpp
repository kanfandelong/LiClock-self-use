#include "SPIFFSEditor.h"
#include <FS.h>


typedef enum {
    OP_WRITE,
    OP_CLOSE
} file_op_t;

typedef struct
{
    file_op_t op;      // 操作类型
    File *file;
    uint8_t *data;
    size_t size;
} multi_thread_params_t;

static TaskHandle_t file_task_handle = NULL;
QueueHandle_t multi_thread_queue = NULL;

static void process_multi_thread_queue()
{
    multi_thread_params_t params;
    xQueueReceive(multi_thread_queue, &params, portMAX_DELAY);
    if (params.op == OP_WRITE && params.size != 0) {
        params.file->write(params.data, params.size);
        free(params.data);
    } else if (params.op == OP_CLOSE) {
        params.file->close();
    }
}

static void task_file_write(void *params)
{
    multi_thread_queue = xQueueCreate(100, sizeof(multi_thread_params_t));
    while (1)
    {
        process_multi_thread_queue();
        delay(1);
    }
}

void begin_file_task()
{
    if (file_task_handle == NULL)
        xTaskCreate(task_file_write, "task_file_w", 4096, NULL, 1, &file_task_handle);
}

void file_write(File *file, uint8_t *data, size_t size)
{
    if (file_task_handle == NULL) // 确保任务已启动
        xTaskCreate(task_file_write, "task_file_w", 4096, NULL, 1, &file_task_handle);
    multi_thread_params_t params;
    params.op = OP_WRITE;
    params.file = file;
    params.size = size;
    params.data = (uint8_t *)ps_malloc(size);
    memcpy(params.data, data, size);
    xQueueSend(multi_thread_queue, &params, portMAX_DELAY);
    // log_i("add write %lu", size);
}

void file_close(File *file)
{
    if (file_task_handle == NULL)
        xTaskCreate(task_file_write, "task_file_w", 4096, NULL, 1, &file_task_handle);
    multi_thread_params_t params;
    params.op = OP_CLOSE;
    params.file = file;
    params.data = NULL;
    params.size = 0;
    xQueueSend(multi_thread_queue, &params, portMAX_DELAY);
}

#ifdef ESP32
SPIFFSEditor::SPIFFSEditor(const fs::FS &fs, const String &username, const String &password)
#else
SPIFFSEditor::SPIFFSEditor(const String &username, const String &password, const fs::FS &fs)
#endif
    : _fs(fs), _littlefs(fs), _username(username), _password(password), _authenticated(false), _startTime(0)
{
}

void SPIFFSEditor::setlittlefs(fs::FS &fs)
{
    _littlefs = fs;
}

void SPIFFSEditor::setFileSystem(fs::FS &fs)
{
    _fs = fs;
}

bool SPIFFSEditor::canHandle(AsyncWebServerRequest *request)
{
    log_i("检查客户端请求是否有效");
    if (request->url().equalsIgnoreCase("/edit"))
    {
        if (request->method() == HTTP_GET)
        {
            if (request->hasParam("list"))
                return true;
            if (request->hasParam("edit"))
            {
                String arg = request->arg("edit");
                if (arg.charAt(0) != '/')
                    arg = "/" + arg;
                if (_fs.exists(arg))
                {
                    request->_tempFile = _fs.open(arg, "r");
                    if (!request->_tempFile)
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
#ifdef ESP32
                if (request->_tempFile.isDirectory())
                {
                    request->_tempFile.close();
                    return false;
                }
#endif
            }
            if (request->hasParam("download"))
            {
                String arg = request->arg("download");
                if (arg.charAt(0) != '/')
                    arg = "/" + arg;
                if (_fs.exists(arg))
                {
                    request->_tempFile = _fs.open(arg, "r");
                    if (!request->_tempFile)
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
#ifdef ESP32
                if (request->_tempFile.isDirectory())
                {
                    request->_tempFile.close();
                    return false;
                }
#endif
            }
            // request->addInterestingHeader("If-Modified-Since");
            return true;
        }
        else if (request->method() == HTTP_POST)
            return true;
        else if (request->method() == HTTP_DELETE)
            return true;
        else if (request->method() == HTTP_PUT)
            return true;
    }
    return false;
}

void SPIFFSEditor::handleRequest(AsyncWebServerRequest *request)
{
    log_i("处理客户端请求");
    if (_username.length() && _password.length() && !request->authenticate(_username.c_str(), _password.c_str()))
        return request->requestAuthentication();

    if (request->method() == HTTP_GET)
    {
        if (request->hasParam("list"))
        {
            String path = request->getParam("list")->value();
#ifdef ESP32
            File dir = _fs.open(path);
#else
            Dir dir = _fs.openDir(path);
#endif
            path = String();
            String output = "[";
#ifdef ESP32
            File entry = dir.openNextFile();
            while (entry)
            {
#else
            while (dir.next())
            {
                fs::File entry = dir.openFile("r");
#endif
                if (output != "[")
                    output += ',';
                output += "{\"type\":\"";
                if (entry.isDirectory())
                    output += "folder";
                else
                    output += "file";
                output += "\",\"name\":\"";
                output += String(entry.name());
                output += "\",\"size\":";
                output += String(entry.size());
                output += "}";
#ifdef ESP32
                entry = dir.openNextFile();
#else
                entry.close();
#endif
            }
#ifdef ESP32
            dir.close();
#endif
            output += "]";
            request->send(200, "application/json", output);
            output = String();
        }
        else if (request->hasParam("edit") || request->hasParam("download"))
        {
            if (request->_tempFile)
            {
                AsyncWebServerResponse *response = request->beginResponse(request->_tempFile, request->_tempFile.name(), String(), request->hasParam("download"), nullptr);
                time_t lastWrite = request->_tempFile.getLastWrite(); // 获取UTC时间戳
                struct tm tm;
                gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构
                char timeStr[64];
                strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                const char *LastWriteTime = timeStr;
                response->addHeader("Last-Modified", LastWriteTime);
                request->send(response);
                log_i("%s %s %d", request->_tempFile.name(), timeStr, request->_tempFile.size());
            }
            else
            {
                request->send(404);
                log_i("404");
            }
            // request->send(request->_tempFile, request->_tempFile.name(), String(), request->hasParam("download"));
        }
        else
        {
            const char *buildTime = __DATE__ " " __TIME__ " GMT";
            if (request->header("If-Modified-Since").equals(buildTime))
            {
                request->send(304);
            }
            else
            {
                if (_littlefs.exists("/System/edit.html.gz"))
                {
                    File file = _littlefs.open("/System/edit.html.gz", "r");
                    time_t lastWrite = file.getLastWrite(); // 获取UTC时间戳
                    file.close();

                    struct tm tm;
                    gmtime_r(&lastWrite, &tm); // 转换为GMT时间结构

                    char timeStr[64];
                    strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                    buildTime = timeStr;
                    AsyncWebServerResponse *response = request->beginResponse(_littlefs, "/System/edit.html.gz", "text/html", false, nullptr);
                    // response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                    // response->addHeader("Pragma", "no-cache");
                    // response->addHeader("Expires", "0");
                    response->addHeader("Content-Encoding", "gzip");
                    response->addHeader("Last-Modified", buildTime);
                    request->send(response);
                }
                else
                {
                    request->send(404, "text/plain", "资源文件不存在");
                }
            }
        }
    }
    else if (request->method() == HTTP_DELETE)
    {
        if (request->hasParam("path", true))
        {
            _fs.remove(request->getParam("path", true)->value());
            request->send(200, "", "DELETE: " + request->getParam("path", true)->value());
        }
        else
        {
            request->send(404);
        }
    }
    else if (request->method() == HTTP_POST)
    {
        if (request->hasParam("data", true, true) && _fs.exists(request->getParam("data", true, true)->value()))
            request->send(200, "", "UPLOADED: " + request->getParam("data", true, true)->value());
        else
        {
            request->send(500);
        }
    }
    else if (request->method() == HTTP_PUT)
    {
        if (request->hasParam("path", true))
        {
            String filename = request->getParam("path", true)->value();
            if (_fs.exists(filename))
            {
                request->send(200);
            }
            else
            {
                fs::File f = _fs.open(filename, "w");
                if (f)
                {
                    f.write((uint8_t)0x00);
                    f.close();
                    request->send(200, "", "CREATE: " + filename);
                }
                else
                {
                    request->send(500);
                }
            }
        }
        else
        {
            request->send(400);
        }
    }
}

void SPIFFSEditor::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!index)
    {
        if (!_username.length() || request->authenticate(_username.c_str(), _password.c_str()))
        {
            _authenticated = true;
            request->_tempFile = _fs.open(filename, "w");
            request->_tempFile.setBufferSize(8192);
            _startTime = millis();
        }
    }
    if (_authenticated && request->_tempFile)
    {
        if (len)
        {
            // file_write(&request->_tempFile, data, len);
            request->_tempFile.write(data, len);
        }
        if (final)
        {
            // file_close(&request->_tempFile);
            request->_tempFile.close();
        }
    }
}
