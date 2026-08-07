#include <cstdint>
#include <algorithm>
#include "AudioGenerator.h"

// #define xing_log log_i
#define xing_log(format, ...) do {} while(0)
#define xing_log_e log_e

// 解析 Xing / Info 头，buf 至少包含一个完整的 MPEG 音频帧数据，len 为帧大小
XingHeaderInfo parseXingHeader(const uint8_t* buf, size_t len) {
    XingHeaderInfo info;
    if (len < 4) {
        xing_log_e("parseXingHeader: buffer too short (<4)");
        return info;
    }

    // 1. 同步字检查
    if ((buf[0] != 0xFF) || ((buf[1] & 0xE0) != 0xE0)) {
        xing_log_e("parseXingHeader: no sync word (0xFF 0xE0...)");
        return info;
    }

    // 2. 解析帧头
    uint8_t b1 = buf[1];
    uint8_t b2 = buf[2];
    uint8_t b3 = buf[3];

    int version = (b1 >> 3) & 0x03;      // 0=MPEG2.5, 2=MPEG2, 3=MPEG1
    int layer = (b1 >> 1) & 0x03;        // 3=Layer1, 2=Layer2, 1=Layer3
    int protection = b1 & 0x01;          // 0=CRC present, 1=no CRC
    int bitrate_index = (b2 >> 4) & 0x0F;
    int samplerate_index = (b2 >> 2) & 0x03;
    int padding = (b2 >> 1) & 0x01;
    int mode = (b3 >> 6) & 0x03;         // 0=stereo, 3=mono

    // 采样率表
    static const int samplerates[4][3] = {
        {11025, 12000, 8000},   // MPEG2.5
        {0, 0, 0},
        {22050, 24000, 16000},  // MPEG2
        {44100, 48000, 32000}   // MPEG1
    };
    int sample_rate = samplerates[version][samplerate_index];
    if (!sample_rate) {
        xing_log_e("parseXingHeader: invalid samplerate index");
        return info;
    }

    // 每帧采样数
    static const int spf_table[4][4] = {
        {0, 576, 1152, 384},  // MPEG2.5
        {0, 0, 0, 0},
        {0, 576, 1152, 384},  // MPEG2
        {0, 1152, 1152, 384}  // MPEG1
    };
    int spf = spf_table[version][layer];
    if (!spf) {
        xing_log_e("parseXingHeader: invalid layer");
        return info;
    }

    // 比特率表 (kbps)
    static const uint16_t bitrates[2][3][16] = {
        {   // MPEG1
            {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
            {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
            {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}
        },
        {   // MPEG2/2.5
            {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
            {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
            {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
        }
    };
    int mpeg1 = (version == 3) ? 0 : 1;
    int layer_idx = layer - 1;
    uint32_t bitrate_kbps = bitrates[mpeg1][layer_idx][bitrate_index];
    if (!bitrate_kbps) {
        xing_log_e("parseXingHeader: invalid bitrate index");
        return info;
    }

    // 帧大小计算
    int frame_size = (spf * bitrate_kbps * 1000) / (sample_rate * 8);
    if (padding) frame_size++;
    xing_log("parseXingHeader: frame_size=%d, spf=%d, bitrate=%ukbps, samplerate=%d, padding=%d",
          frame_size, spf, bitrate_kbps, sample_rate, padding);

    if (frame_size <= 0 || frame_size > (int)len) {
        xing_log_e("parseXingHeader: frame_size out of range (len=%d)", len);
        return info;
    }

    // 3. 计算 Xing/Info 偏移 (修正 protection 位逻辑)
    int side_info_size = 0;
    if (version == 3) {              // MPEG1
        side_info_size = (mode == 3) ? 17 : 32;  // mono 17, stereo 32
    } else {                          // MPEG2/2.5
        side_info_size = (mode == 3) ? 9 : 17;
    }
    // protection == 0 表示有 CRC，需要额外跳过 2 字节
    int xing_offset = 4 + ((protection == 0) ? 2 : 0) + side_info_size;
    xing_log("parseXingHeader: version=%d, mode=%d, protection=%d, side_info=%d, xing_offset=%d",
          version, mode, protection, side_info_size, xing_offset);

    if (xing_offset + 8 > frame_size) {
        xing_log_e("parseXingHeader: xing_offset too large for frame");
        return info;
    }

    // 4. 检查 Xing/Info 标记
    const char* tag = (const char*)(buf + xing_offset);
    bool isXing = (tag[0] == 'X' && tag[1] == 'i' && tag[2] == 'n' && tag[3] == 'g');
    bool isInfo = (tag[0] == 'I' && tag[1] == 'n' && tag[2] == 'f' && tag[3] == 'o');
    if (!isXing && !isInfo) {
        xing_log_e("parseXingHeader: no Xing/Info found at offset %d (got '%c%c%c%c')",
              xing_offset, tag[0], tag[1], tag[2], tag[3]);
        return info;
    }

    // 5. 解析标志
    uint32_t flags = (uint32_t(buf[xing_offset+4]) << 24) | (uint32_t(buf[xing_offset+5]) << 16) |
                     (uint32_t(buf[xing_offset+6]) << 8)  | buf[xing_offset+7];
    xing_log("parseXingHeader: flags=0x%08X", flags);
    int dataPos = xing_offset + 8;

    if (flags & 0x0001) {
        if (dataPos + 4 > frame_size) { log_i("parseXingHeader: not enough data for frames"); return info; }
        info.frames = (uint32_t(buf[dataPos]) << 24) | (uint32_t(buf[dataPos+1]) << 16) |
                      (uint32_t(buf[dataPos+2]) << 8) | buf[dataPos+3];
        dataPos += 4;
    }
    if (flags & 0x0002) {
        if (dataPos + 4 > frame_size) { log_i("parseXingHeader: not enough data for bytes"); return info; }
        info.bytes = (uint32_t(buf[dataPos]) << 24) | (uint32_t(buf[dataPos+1]) << 16) |
                     (uint32_t(buf[dataPos+2]) << 8) | buf[dataPos+3];
        dataPos += 4;
    }
    if (flags & 0x0004) dataPos += 100; // TOC
    if (flags & 0x0008) dataPos += 4;   // quality

    info.valid = true;
    info.sampleRate = sample_rate;
    info.channels = (mode == 3) ? 1 : 2;

    if (info.frames && spf && sample_rate) {
        info.duration = (float)info.frames * spf / sample_rate;
        if (info.bytes) {
            info.bitrate = (uint32_t)((double)info.bytes * 8 / info.duration);
        } else {
            info.bitrate = bitrate_kbps * 1000;
        }
        xing_log("parseXingHeader: success: frames=%u, bytes=%u, duration=%.2fs, bitrate=%u",
              info.frames, info.bytes, info.duration, info.bitrate);
    }

    return info;
}