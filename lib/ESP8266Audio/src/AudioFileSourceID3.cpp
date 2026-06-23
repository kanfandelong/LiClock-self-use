/*
  AudioFileSourceID3.cpp
  ID3 tag parser – 严格基于 Audio::read_ID3_Header 移植，修正编码支持。

  Copyright (C) 2017  Earle F. Philhower, III
  Modified 2026 – 基于 Audio 库逻辑重写，修复 UTF‑16/BOM 等。
  本文件可单独编译，无需依赖 Audio 库。
*/

#include "AudioFileSourceID3.h"
#include <vector>
#include <cstring>
#include <cstdlib>

// 最大允许的 ID3 标签大小（防止内存耗尽）
static const size_t MAX_ID3_SIZE = 384 * 1024; // 384 KB

// ---------- 静态辅助函数 (来自 Audio 库或等价实现) ----------

// 正常 big‑endian 读取
static inline uint32_t big32(const uint8_t *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
static inline uint32_t big24(const uint8_t *p)
{
    return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2];
}

// Synchsafe 解码 (每字节低 7 位)
static inline uint32_t synch32(const uint8_t *p)
{
    return (uint32_t(p[0] & 0x7F) << 21) | (uint32_t(p[1] & 0x7F) << 14) |
           (uint32_t(p[2] & 0x7F) << 7) | (uint32_t(p[3] & 0x7F));
}

// 从缓冲区中复制字符串 (ISO‑8859‑1) 并转换为 UTF‑8
static char *copyLatin1ToUTF8(const uint8_t *src, int len)
{
    if (len <= 0)
        return nullptr;
    char *out = (char *)malloc(len * 2 + 1); // 最坏情况每个字符占 2 字节
    if (!out)
        return nullptr;
    int j = 0;
    for (int i = 0; i < len; i++)
    {
        uint8_t c = src[i];
        if (c == 0)
            break;
        if (c < 0x80)
        {
            out[j++] = (char)c;
        }
        else
        {
            out[j++] = (char)(0xC0 | (c >> 6));
            out[j++] = (char)(0x80 | (c & 0x3F));
        }
    }
    out[j] = '\0';
    return out;
}

// 从缓冲区中复制 UTF‑8 字符串 (纯拷贝)
static char *copyUTF8(const uint8_t *src, int len)
{
    if (len <= 0)
        return nullptr;
    char *out = (char *)malloc(len + 1);
    if (!out)
        return nullptr;
    memcpy(out, src, len);
    out[len] = '\0';
    return out;
}

// 从缓冲区中复制 UTF‑16 字符串并转换为 UTF‑8。
// encoding: 1 = UTF‑16LE (可能带 BOM), 2 = UTF‑16BE (可能带 BOM)
// 注意：Audio 库的 copy_from_utf16 会先读取 BOM 来决定字节序，若没有 BOM 则保持给定 endian。
// 此处严格复刻这一逻辑。
static char *copyUTF16ToUTF8(const uint8_t *src, int len, bool isBigEndian)
{
    if (len < 2)
        return nullptr;

    // 检测 BOM
    bool bom_found = false;
    bool bigEndian = isBigEndian; // 默认使用传入的字节序
    if (len >= 2)
    {
        uint16_t bom = (uint16_t(src[0]) << 8) | (src[1]);
        if (bom == 0xFEFF)
        {
            bigEndian = true;
            bom_found = true;
        }
        else if (bom == 0xFFFE)
        {
            bigEndian = false;
            bom_found = true;
        }
    }

    int start = bom_found ? 2 : 0;
    int maxChars = (len - start) / 2;
    // 为 UTF‑8 分配足够空间（最多每个 UTF‑16 字符占 3 字节）
    char *out = (char *)malloc(maxChars * 3 + 1);
    if (!out)
        return nullptr;
    int j = 0;
    for (int i = start; i + 1 < len; i += 2)
    {
        uint32_t code;
        if (bigEndian)
            code = ((uint32_t)src[i] << 8) | src[i + 1];
        else
            code = ((uint32_t)src[i + 1] << 8) | src[i];
        if (code == 0)
            break; // 空终止符
        if (code < 0x80)
        {
            out[j++] = (char)code;
        }
        else if (code < 0x800)
        {
            out[j++] = (char)(0xC0 | (code >> 6));
            out[j++] = (char)(0x80 | (code & 0x3F));
        }
        else
        {
            out[j++] = (char)(0xE0 | (code >> 12));
            out[j++] = (char)(0x80 | ((code >> 6) & 0x3F));
            out[j++] = (char)(0x80 | (code & 0x3F));
        }
    }
    out[j] = '\0';
    return out;
}

// 跳过 UTF‑16 的空终止符 (两个零字节)
static int skipUTF16Null(const uint8_t *buf, int max)
{
    for (int i = 0; i + 1 < max; i += 2)
    {
        if (buf[i] == 0 && buf[i + 1] == 0)
            return i + 2;
    }
    return -1;
}

// 跳过 ISO‑8859‑1/UTF‑8 的空终止符
static int skipLatinNull(const uint8_t *buf, int max)
{
    for (int i = 0; i < max; ++i)
    {
        if (buf[i] == 0)
            return i + 1; // 包括 '\0' 本身
    }
    return -1;
}

// 从 ID3 标签数据中提取字符串，严格按照 Audio::read_ID3_Header 的做法。
// enc: 0=ISO‑8859‑1, 1=UTF‑16 (BOM), 2=UTF‑16BE (BOM), 3=UTF‑8
static char *extractString(const uint8_t *data, int len, uint8_t enc, bool &isUnicode)
{
    if (len <= 0)
        return nullptr;
    isUnicode = false;
    char *result = nullptr;

    switch (enc)
    {
    case 0: // ISO‑8859‑1
        result = copyLatin1ToUTF8(data, len);
        break;
    case 3: // UTF‑8
        result = copyUTF8(data, len);
        break;
    case 1:               // UTF‑16LE (可能带 BOM)
    case 2:               // UTF‑16BE (可能带 BOM)
        isUnicode = true; // 标记为 Unicode，对齐原回调习惯
        result = copyUTF16ToUTF8(data, len, (enc == 2));
        break;
    default:
        // 未知编码，当作 ISO‑8859‑1
        result = copyLatin1ToUTF8(data, len);
        break;
    }
    return result;
}

// ---------- AudioFileSourceID3 实现 ----------

AudioFileSourceID3::AudioFileSourceID3(AudioFileSource *src)
{
    this->src = src;
    this->checked = false;
}

AudioFileSourceID3::~AudioFileSourceID3()
{
}

uint32_t AudioFileSourceID3::read(void *data, uint32_t len)
{
    if (checked)
    {
        return src->read(data, len);
    }
    checked = true;

    if (len < 10)
    {
        return src->read(data, len);
    }

    uint8_t header[10];
    uint32_t got = src->read(header, 10);
    if (got < 10)
    {
        memcpy(data, header, got);
        return got;
    }

    // 无 ID3 标签，直接透传
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3' ||
        header[3] < 2 || header[3] > 4 || header[4] != 0)
    {
        uint8_t *out = (uint8_t *)data;
        memcpy(out, header, 10);
        uint32_t rest = len - 10;
        if (rest > 0)
        {
            return 10 + src->read(out + 10, rest);
        }
        return 10;
    }

    uint8_t version = header[3];
    bool unsync = (header[5] & 0x80) != 0;
    bool exthdr = (version >= 3) && (header[5] & 0x40);

    // 所有 ID3v2 版本（2.2 / 2.3 / 2.4）的标签大小都是 4 字节 synchsafe 整数
    uint32_t rawSize = synch32(header + 6);
    uint32_t id3Size = rawSize + 10;

    if (id3Size > MAX_ID3_SIZE)
    {
        // 跳过超大标签
        uint32_t remain = id3Size - 10;
        uint8_t sink[256];
        while (remain > 0)
        {
            uint32_t chunk = std::min<uint32_t>(remain, sizeof(sink));
            uint32_t bytesRead = src->read(sink, chunk);
            if (bytesRead == 0)
                break; // EOF
            remain -= bytesRead;
        }
        return src->read(data, len);
    }

    // 分配缓冲区，一次性读入整个标签
    std::vector<uint8_t> tagBuf(id3Size);
    memcpy(tagBuf.data(), header, 10);
    uint32_t toRead = id3Size - 10;
    uint32_t totalRead = 10;
    while (toRead > 0)
    {
        uint32_t chunk = src->read(tagBuf.data() + totalRead, toRead);
        if (chunk == 0)
            break;
        totalRead += chunk;
        toRead -= chunk;
    }

    // 去同步
    if (unsync && totalRead > 10)
    {
        size_t bodyLen = totalRead - 10;
        size_t newBody = 0;
        for (size_t i = 0; i < bodyLen; ++i)
        {
            uint8_t c = tagBuf[10 + i];
            tagBuf[10 + newBody++] = c;
            if (c == 0xFF && i + 1 < bodyLen && tagBuf[10 + i + 1] == 0x00)
            {
                ++i; // 跳过同步填充字节
            }
        }
        totalRead = 10 + newBody;
    }

    // 跳过扩展头 (如果存在)
    size_t bodyOffset = 10;
    size_t bodyLength = totalRead - 10;
    if (exthdr && bodyLength >= 4)
    {
        uint32_t ehsize = synch32(tagBuf.data() + bodyOffset);
        if (ehsize >= 4 && ehsize <= bodyLength)
        {
            bodyOffset += ehsize;
            bodyLength -= ehsize;
        }
    }

    // 解析所有帧
    parseID3Frames(tagBuf.data() + bodyOffset, bodyLength, version);

    // 标签结束回调
    cb.md("eof", false, "id3");

    log_i("ID3v%d tag parsed, size: %u bytes, unsync: %s, frames processed, now at file pos: %u",
          version, totalRead, unsync ? "yes" : "no", src->getPos());
    // 返回之后的音频数据
    return src->read(data, len);
}

bool AudioFileSourceID3::seek(int32_t pos, int dir) { return src->seek(pos, dir); }
bool AudioFileSourceID3::close() { return src->close(); }
bool AudioFileSourceID3::isOpen() { return src->isOpen(); }
uint32_t AudioFileSourceID3::getSize() { return src->getSize(); }
uint32_t AudioFileSourceID3::getPos() { return src->getPos(); }

// ======== 核心：移植于 Audio::read_ID3_Header 的帧解析 ========
void AudioFileSourceID3::parseID3Frames(const uint8_t *buf, size_t len, uint8_t version)
{
    size_t pos = 0;
    bool v2_2 = (version == 2);

    while (pos < len)
    {
        // 填充 (padding) 检测
        if (len - pos >= 4 && buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 0 && buf[pos + 3] == 0)
        {
            break; // 到达填充区
        }

        // 读取帧 ID
        char frameId[5] = {0};
        uint32_t frameSize = 0;
        int headerLen = 0;
        bool compressed = false;
        uint8_t encFlag = 0;

        if (v2_2)
        {
            if (pos + 6 > len)
                break;
            memcpy(frameId, buf + pos, 3);
            frameId[3] = '\0';
            frameSize = big24(buf + pos + 3);
            headerLen = 6;
            pos += headerLen;
        }
        else
        {
            if (pos + 10 > len)
                break;
            memcpy(frameId, buf + pos, 4);
            frameId[4] = '\0';
            // v2.4 使用 synchsafe，v2.3 使用 normal big‑endian
            if (version == 4)
                frameSize = synch32(buf + pos + 4);
            else
                frameSize = big32(buf + pos + 4);
            uint8_t flag2 = buf[pos + 9];
            compressed = (flag2 & 0x80) != 0;
            encFlag = buf[pos + 8]; // 未使用，保留
            (void)encFlag;
            headerLen = 10;
            pos += headerLen;
        }

        if (frameSize == 0)
            continue;

        // 压缩帧：跳过
        if (compressed)
        {
            // 压缩帧开头有 4 字节解压大小
            pos += 4;
            if (pos + frameSize > len)
                break;
            pos += frameSize - 4;
            continue;
        }

        // 确保不超出缓冲区
        if (pos + frameSize > len)
            break;
        size_t dataLen = frameSize;
        const uint8_t *frameData = buf + pos;

        // 图片帧跳过
        if (strcmp(frameId, "APIC") == 0 || (v2_2 && strcmp(frameId, "PIC") == 0))
        {
            cb.md("APIC", true, "跳过内嵌图片");
            pos += dataLen;
            continue;
        }

        // 歌词帧 (SYLT, USLT, SLT)
        if (strcmp(frameId, "SYLT") == 0 || strcmp(frameId, "USLT") == 0 || (v2_2 && strcmp(frameId, "SLT") == 0))
        {
            if (dataLen < 1)
            {
                pos += dataLen;
                continue;
            }
            uint8_t enc = frameData[0];
            int consumed = 1; // encoding 占用 1 字节

            // 语言 (3 字节)
            if (dataLen < 4)
            {
                pos += dataLen;
                continue;
            }
            consumed += 3; // 跳过语言

            // 内容描述符 (以 null 结尾)
            if (enc == 1 || enc == 2)
            { // UTF‑16
                int skip = skipUTF16Null(frameData + consumed, dataLen - consumed);
                if (skip >= 0)
                    consumed += skip;
                else
                    consumed = dataLen; // 错误，跳过整个帧
            }
            else
            {
                int skip = skipLatinNull(frameData + consumed, dataLen - consumed);
                if (skip >= 0)
                    consumed += skip;
                else
                    consumed = dataLen;
            }

            int lyricLen = dataLen - consumed;
            if (lyricLen < 0)
                lyricLen = 0;
            if (lyricLen > 10240)
                lyricLen = 10240; // 10 KB 上限

            bool isUnicode = false;
            char *lyrics = extractString(frameData + consumed, lyricLen, enc, isUnicode);
            if (lyrics)
            {
                cb.md(frameId, isUnicode, lyrics);
                free(lyrics);
            }
            pos += dataLen;
            continue;
        }

        // COMM / COM 帧特殊处理：跳过语言和内容描述符，仅返回评论文本
        if (strcmp(frameId, "COMM") == 0 || (v2_2 && strcmp(frameId, "COM") == 0))
        {
            if (dataLen < 1)
            {
                pos += dataLen;
                continue;
            }
            uint8_t enc = frameData[0];
            int consumed = 1;

            // 语言
            if (dataLen < 4)
            {
                pos += dataLen;
                continue;
            }
            consumed += 3;

            // 内容描述符
            if (enc == 1 || enc == 2)
            {
                int skip = skipUTF16Null(frameData + consumed, dataLen - consumed);
                if (skip >= 0)
                    consumed += skip;
                else
                    consumed = dataLen;
            }
            else
            {
                int skip = skipLatinNull(frameData + consumed, dataLen - consumed);
                if (skip >= 0)
                    consumed += skip;
                else
                    consumed = dataLen;
            }

            int textLen = dataLen - consumed;
            if (textLen <= 0)
            {
                pos += dataLen;
                continue;
            }
            bool isUnicode = false;
            char *text = extractString(frameData + consumed, textLen, enc, isUnicode);
            if (text)
            {
                cb.md("COMM", isUnicode, text); // 标准回调
                free(text);
            }
            pos += dataLen;
            continue;
        }

        if (strcmp(frameId, "TXXX") == 0 || (v2_2 && strcmp(frameId, "TXX") == 0))
        {
            if (dataLen < 1)
            {
                pos += dataLen;
                continue;
            }
            uint8_t enc = frameData[0];
            int consumed = 1;
            // 跳过描述符
            if (enc == 1 || enc == 2)
            {
                int skip = skipUTF16Null(frameData + consumed, dataLen - consumed);
                consumed = (skip >= 0) ? skip : dataLen;
            }
            else
            {
                int skip = skipLatinNull(frameData + consumed, dataLen - consumed);
                consumed = (skip >= 0) ? skip : dataLen;
            }
            int textLen = dataLen - consumed;
            if (textLen > 10240)
                textLen = 10240; // 限制大小
            bool isUnicode = false;
            char *text = extractString(frameData + consumed, textLen, enc, isUnicode);
            if (text)
            {
                cb.md(frameId, isUnicode, text);
                free(text);
            }
            pos += dataLen;
            continue;
        }

        // 普通文本帧
        if (dataLen < 1)
        {
            pos += dataLen;
            continue;
        }
        uint8_t enc = frameData[0];
        int textLen = dataLen - 1;
        if (textLen <= 0)
        {
            pos += dataLen;
            continue;
        }
        bool isUnicode = false;
        char *text = extractString(frameData + 1, textLen, enc, isUnicode);
        if (text)
        {
            cb.md(frameId, isUnicode, text);
            free(text);
        }
        pos += dataLen;
    }
}