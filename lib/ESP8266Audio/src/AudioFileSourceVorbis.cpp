/*
  AudioFileSourceVorbis
  Vorbis comment filter that extracts any fields and sends to callback function
  
  Copyright (C) 2025  (Your Name)

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

#include "AudioFileSourceVorbis.h"
#include "Arduino.h"

// Helper to read little‑endian 32‑bit value
uint32_t AudioFileSourceVorbis::readLE32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

AudioFileSourceVorbis::AudioFileSourceVorbis(AudioFileSource *src)
    : src(src), checked(false), buffer(nullptr), bufferSize(0), bufferLen(0), bufferPos(0) {
}

AudioFileSourceVorbis::~AudioFileSourceVorbis() {
    if (buffer) {
        free(buffer);
        buffer = nullptr;
    }
}

bool AudioFileSourceVorbis::parseOggVorbisComment() {
    // Minimum size for an Ogg page header: 27 bytes
    if (bufferLen < 27) return false;

    // Check OggS sync pattern
    if (buffer[0] != 'O' || buffer[1] != 'g' || buffer[2] != 'g' || buffer[3] != 'S') {
        return false; // not an Ogg stream
    }

    uint8_t version = buffer[4];
    if (version != 0) return false; // only version 0 supported

    uint8_t headerType = buffer[5];
    (void)headerType; // unused for our purposes

    uint8_t segments = buffer[26];
    if (segments == 0) return false; // no segments

    // Segment table starts at offset 27
    const uint8_t *segTable = buffer + 27;
    if (27 + segments > bufferLen) return false; // segment table not fully in buffer

    // Parse packets from segment table
    uint32_t packetStart = 27 + segments; // start of first packet data
    uint32_t packetEnd = packetStart;

    // Find first packet boundaries
    int packetIdx = 0;
    uint32_t packetLengths[10] = {0}; // enough for first few packets
    uint32_t packetOffsets[10] = {0};

    for (int seg = 0; seg < segments; ) {
        uint32_t pktLen = 0;
        uint32_t pktStart = packetEnd; // start of this packet (absolute offset)
        // Accumulate segments until a segment < 255
        while (seg < segments) {
            uint8_t segLen = segTable[seg];
            pktLen += segLen;
            seg++;
            if (segLen < 255) break;
        }
        if (packetIdx < 10) {
            packetLengths[packetIdx] = pktLen;
            packetOffsets[packetIdx] = pktStart;
        }
        packetIdx++;
        packetEnd += pktLen;
    }

    if (packetIdx < 2) return false; // need at least identification and comment header

    // --- First packet: identification header ---
    uint32_t idPktOff = packetOffsets[0];
    uint32_t idPktLen = packetLengths[0];
    if (idPktOff + idPktLen > bufferLen) return false; // not enough data

    const uint8_t *idPkt = buffer + idPktOff;
    if (idPkt[0] != 1) return false; // packet type 1
    if (memcmp(idPkt + 1, "vorbis", 6) != 0) return false; // magic

    // --- Second packet: comment header ---
    uint32_t cmtPktOff = packetOffsets[1];
    uint32_t cmtPktLen = packetLengths[1];
    if (cmtPktOff + cmtPktLen > bufferLen) return false;

    const uint8_t *cmtPkt = buffer + cmtPktOff;
    if (cmtPkt[0] != 3) return false; // packet type 3
    if (memcmp(cmtPkt + 1, "vorbis", 6) != 0) return false;

    // --- Now parse comments ---
    uint32_t pos = 7; // skip 1 (type) + 6 "vorbis"
    if (pos + 4 > cmtPktLen) return false;
    uint32_t vendorLen = readLE32(cmtPkt + pos); pos += 4;
    if (pos + vendorLen > cmtPktLen) return false;
    const char *vendor = (const char*)(cmtPkt + pos); pos += vendorLen;

    // Vendor string (optional)
    if (vendorLen) {
        cb.md("VENDOR", true, vendor);
    }

    if (pos + 4 > cmtPktLen) return false;
    uint32_t userCommentCount = readLE32(cmtPkt + pos); pos += 4;

    for (uint32_t i = 0; i < userCommentCount; i++) {
        if (pos + 4 > cmtPktLen) break;
        uint32_t commentLen = readLE32(cmtPkt + pos); pos += 4;
        if (pos + commentLen > cmtPktLen) break;
        const char *comment = (const char*)(cmtPkt + pos);
        pos += commentLen;

        // Find '=' separator
        const char *eq = strchr(comment, '=');
        if (!eq) continue; // invalid comment, skip

        // Split into key and value
        size_t keyLen = eq - comment;
        size_t valLen = commentLen - keyLen - 1;
        const char *val = eq + 1;

        char *keyBuf = new char[keyLen + 1];
        memcpy(keyBuf, comment, keyLen);
        keyBuf[keyLen] = '\0';
        char *valBuf = new char[valLen + 1];
        memcpy(valBuf, val, valLen);
        valBuf[valLen] = '\0';

        cb.md(keyBuf, true, valBuf);

        delete[] keyBuf;
        delete[] valBuf;
    }

    // Store the audio data start offset (end of second packet)
    uint32_t audioStart = packetOffsets[1] + packetLengths[1];
    // Set bufferPos to that offset so that read() returns only audio data
    bufferPos = audioStart;
    return true;
}

uint32_t AudioFileSourceVorbis::read(void *data, uint32_t len) {
    if (checked) {
        // Already processed header: if we still have buffered data, serve from it
        if (buffer && bufferPos < bufferLen) {
            uint32_t available = bufferLen - bufferPos;
            uint32_t copy = len < available ? len : available;
            memcpy(data, buffer + bufferPos, copy);
            bufferPos += copy;
            if (bufferPos >= bufferLen) {
                // Buffer exhausted, free it
                free(buffer);
                buffer = nullptr;
                bufferLen = 0;
                bufferPos = 0;
            }
            return copy;
        } else {
            // No buffered data left, forward to source
            return src->read(data, len);
        }
    }

    // First call – read initial chunk and attempt to parse Vorbis comment
    checked = true;

    // Allocate 64KB buffer (memory is abundant)
    bufferSize = 64 * 1024;
    buffer = (uint8_t *)malloc(bufferSize);
    bufferLen = src->read(buffer, bufferSize);
    bufferPos = 0;

    // Try to parse Vorbis comment
    bool success = parseOggVorbisComment();
    if (!success) {
        // Not a valid Ogg Vorbis file (or parsing failed). Reset bufferPos to 0,
        // so we serve everything we read as data.
        bufferPos = 0;
        // Also send an "eof" type marker if desired, but not required.
        cb.md("eof", false, "vorbis");
        
    } else {
        // Parsing succeeded, we have already set bufferPos to audio start.
        cb.md("eof", false, "vorbis");
    }

    // Now serve the first chunk of data to the caller
    uint32_t available = bufferLen - bufferPos;
    uint32_t copy = len < available ? len : available;
    memcpy(data, buffer + bufferPos, copy);
    bufferPos += copy;
    if (bufferPos >= bufferLen) {
        free(buffer);
        buffer = nullptr;
        bufferLen = 0;
        bufferPos = 0;
    }
    return copy;
}

bool AudioFileSourceVorbis::seek(int32_t pos, int dir) {
    // If we still have a buffer, discard it (can't seek inside buffered data easily)
    if (buffer) {
        free(buffer);
        buffer = nullptr;
        bufferLen = 0;
        bufferPos = 0;
    }
    return src->seek(pos, dir);
}

bool AudioFileSourceVorbis::close() {
    if (buffer) {
        free(buffer);
        buffer = nullptr;
        bufferLen = 0;
        bufferPos = 0;
    }
    return src->close();
}

bool AudioFileSourceVorbis::isOpen() {
    return src->isOpen();
}

uint32_t AudioFileSourceVorbis::getSize() {
    return src->getSize();
}

uint32_t AudioFileSourceVorbis::getPos() {
    // If we have buffered data, we need to adjust the position
    // This is an estimate; for accurate position we might need more complex tracking.
    // For simplicity, forward to source and ignore buffer.
    // Could also compute: src->getPos() - (bufferLen - bufferPos) if buffer was from source.
    // But for typical usage (streaming), it may be acceptable to return src position.
    return src->getPos();
}