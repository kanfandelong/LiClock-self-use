#ifndef __TMPFILE_H__
#define __TMPFILE_H__

#include "A_Config.h"
#include <cstring>

class tmpFile;

class tmpFile
{
public:
    tmpFile()
    { 
    };
    bool open(File file);
    bool open(const char* path);
    bool open(const String& path);
    bool close();
    bool seek(uint32_t pos);
    size_t position();
    int read()
    {
        uint8_t buf;
        if (read(&buf, 1) != 1)
            return -1;

        return buf;
    }
    size_t read(uint8_t *buf, size_t size);
protected:
    uint8_t *temp_ptr = nullptr;
    uint32_t offset = 0;
    size_t temp_size = 0; // size of allocated temporary buffer
    File src;
};
#endif