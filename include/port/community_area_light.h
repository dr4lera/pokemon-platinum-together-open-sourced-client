// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_AREA_LIGHT_H
#define COMMUNITY_AREA_LIGHT_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Original bounded desktop reader for the existing public lighting format.
 * Each record is end-time, four lights and four material colors. Empty lines
 * and CR/LF runs are separators, never zero-valued fields. */
typedef struct CommunityAreaLightRecord {
    uint32_t endTime;
    uint8_t validLightsMask;
    uint16_t lightColors[4];
    int16_t lightVectors[4][3];
    uint16_t materialColors[4];
} CommunityAreaLightRecord;
typedef struct CommunityAreaLightReader {
    const unsigned char *bytes;
    size_t length, offset;
} CommunityAreaLightReader;
static inline int CommunityAreaLight_Line(CommunityAreaLightReader *reader, const unsigned char **start, size_t *length)
{
    while (reader->offset < reader->length) {
        size_t begin, end;
        unsigned char c = reader->bytes[reader->offset];
        if (c == '\r' || c == '\n') { ++reader->offset; continue; }
        if (c == 0) {
            while (reader->offset < reader->length) if (reader->bytes[reader->offset++] != 0) return -1;
            return 0;
        }
        begin = reader->offset;
        while (reader->offset < reader->length && reader->bytes[reader->offset] != '\r' && reader->bytes[reader->offset] != '\n') {
            if (!reader->bytes[reader->offset] || reader->offset - begin >= 255) return -1;
            ++reader->offset;
        }
        end = reader->offset;
        while (begin < end && (reader->bytes[begin] == ' ' || reader->bytes[begin] == '\t')) ++begin;
        while (end > begin && (reader->bytes[end - 1] == ' ' || reader->bytes[end - 1] == '\t')) --end;
        if (begin == end) continue;
        *start = reader->bytes + begin;
        *length = end - begin;
        return 1;
    }
    return 0;
}
static inline int CommunityAreaLight_Values(const unsigned char *line, size_t length, int32_t *values, unsigned count)
{
    size_t p = 0;
    for (unsigned field = 0; field < count; ++field) {
        uint32_t number = 0, limit;
        int negative = 0;
        unsigned digits = 0;
        while (p < length && (line[p] == ' ' || line[p] == '\t')) ++p;
        if (p < length && line[p] == '-') { negative = 1; ++p; }
        limit = negative ? 2147483648u : 2147483647u;
        while (p < length && line[p] >= '0' && line[p] <= '9') {
            unsigned digit = line[p++] - '0';
            if (++digits > 10 || number > (limit - digit) / 10) return 0;
            number = number * 10 + digit;
        }
        if (!digits) return 0;
        values[field] = negative ? (int32_t)(-(int64_t)number) : (int32_t)number;
        while (p < length && (line[p] == ' ' || line[p] == '\t')) ++p;
        if (field + 1 < count) {
            if (p >= length || line[p++] != ',') return 0;
        } else if (p < length && line[p] == ',') ++p;
    }
    return p == length;
}
static inline uint16_t CommunityAreaLight_RGB(int32_t r, int32_t g, int32_t b)
{
    return (uint16_t)(r | (g << 5) | (b << 10));
}
static inline int CommunityAreaLight_ColorRange(const int32_t *values)
{
    return values[0] >= 0 && values[0] <= 31 && values[1] >= 0 && values[1] <= 31 && values[2] >= 0 && values[2] <= 31;
}
static inline int CommunityAreaLight_Pass(const void *bytes, size_t length, CommunityAreaLightRecord *records, size_t capacity, size_t *count)
{
    CommunityAreaLightReader reader = {(const unsigned char *)bytes, length, 0};
    const unsigned char *line;
    size_t lineLength, total = 0;
    uint32_t previousTime = 0;
    int status;
    while ((status = CommunityAreaLight_Line(&reader, &line, &lineLength)) == 1) {
        int32_t values[7];
        CommunityAreaLightRecord record;
        if (lineLength == 3 && !memcmp(line, "EOF", 3)) {
            if (!total || CommunityAreaLight_Line(&reader, &line, &lineLength) != 0) return 0;
            *count = total;
            return 1;
        }
        memset(&record, 0, sizeof(record));
        if (total >= 256 || !CommunityAreaLight_Values(line, lineLength, values, 1)
            || values[0] < 0 || values[0] > 43200 || (total && (uint32_t)values[0] <= previousTime)) return 0;
        record.endTime = previousTime = (uint32_t)values[0];
        for (unsigned light = 0; light < 4; ++light) {
            if (CommunityAreaLight_Line(&reader, &line, &lineLength) != 1 || !CommunityAreaLight_Values(line, lineLength, values, 7)
                || (values[0] != 0 && values[0] != 1) || !CommunityAreaLight_ColorRange(values + 1)) return 0;
            if (values[0]) {
                record.validLightsMask |= (uint8_t)(1u << light);
                record.lightColors[light] = CommunityAreaLight_RGB(values[1], values[2], values[3]);
                for (unsigned axis = 0; axis < 3; ++axis) {
                    int32_t vector = values[4 + axis];
                    if (vector > 4096) vector = 4096;
                    if (vector < -4096) vector = -4096;
                    record.lightVectors[light][axis] = (int16_t)vector;
                }
            }
        }
        for (unsigned material = 0; material < 4; ++material) {
            if (CommunityAreaLight_Line(&reader, &line, &lineLength) != 1 || !CommunityAreaLight_Values(line, lineLength, values, 3)
                || !CommunityAreaLight_ColorRange(values)) return 0;
            record.materialColors[material] = CommunityAreaLight_RGB(values[0], values[1], values[2]);
        }
        if (records) {
            if (total >= capacity) return 0;
            records[total] = record;
        }
        ++total;
    }
    (void)status;
    return 0; /* Missing EOF, truncated or malformed input is never a record. */
}
/* Validate the whole immutable input before touching caller records or count.
 * NULL records requests only a validated count for a bounded allocation. */
static inline int CommunityAreaLight_Parse(const void *bytes, size_t length, CommunityAreaLightRecord *records, size_t capacity, size_t *count)
{
    size_t total = 0;
    if (!bytes || !count || !length || length > 131072 || !CommunityAreaLight_Pass(bytes, length, NULL, 0, &total)
        || (records && capacity < total)) return 0;
    if (records && !CommunityAreaLight_Pass(bytes, length, records, capacity, &total)) return 0;
    *count = total;
    return 1;
}
#endif
