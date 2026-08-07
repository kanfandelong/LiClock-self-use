#include "temp_file.h"

bool tmpFile::open(File file)
{
    if (!file)
        return false;
    src = file;
    size_t size;
    size = src.size();
    if (size < 524288)
    {
        if (temp_ptr != nullptr) {
            free(temp_ptr);
            temp_size = 0;
        }
        temp_ptr = (uint8_t *)ps_malloc(size);
        temp_size = size; // store allocated size
        offset = 0; // reset read/write offset
        log_i("Temp buffer allocated, size=%zu", size);
        src.close();
        return true;
    }
    return true;
}

bool tmpFile::open(const char* path)
{
    src = hal.open(path);
    if (!src)
        return false;
    size_t size;
    size = src.size();
    if (size < 524288)
    {
        if (temp_ptr != nullptr) {
            free(temp_ptr);
            temp_size = 0;
        }
        temp_ptr = (uint8_t *)ps_malloc(size);
        temp_size = size; // store allocated size
        offset = 0; // reset offset for new buffer
        src.close();
        log_i("Temp buffer allocated, size=%zu", size);
        return true;
    }
    return true;
}

bool tmpFile::open(const String& path)
{
    return open(path.c_str());
}

bool tmpFile::close()
{
    if (temp_ptr != nullptr){
        free(temp_ptr);
        temp_ptr = nullptr;
        temp_size = 0;
        return true;
    }
    else if (temp_ptr == nullptr && src)
    {
        src.close();
        return true;
    }
    return false;
}

bool tmpFile::seek(uint32_t pos)
{
    if (temp_ptr != nullptr){
        offset = pos;
        return true;
    }
    else if (temp_ptr == nullptr && src)
    {
        return src.seek(pos);
    }
    return false;
}

size_t tmpFile::position()
{
    if (temp_ptr != nullptr){
        return offset;
    }
    else if (temp_ptr == nullptr && src)
    {
        return src.position();
    }
    return false;
}

size_t tmpFile::read(uint8_t *buf, size_t size)
{
    if (temp_ptr != nullptr){
        // Read from temporary buffer
        if (offset >= temp_size) {
            // No more data
            return 0;
        }
        // Determine how many bytes we can read
        size_t remaining = temp_size - offset;
        size_t toRead = (size < remaining) ? size : remaining;
        memcpy(buf, temp_ptr + offset, toRead);
        offset += toRead;
        return toRead;
    }
    else if (temp_ptr == nullptr && src)
    {
        return src.read(buf, size);
    }
    return false;
}