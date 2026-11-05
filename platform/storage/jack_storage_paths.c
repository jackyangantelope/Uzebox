/* BSD-3-Clause content/save path contract for yellow ports. */
#include "jack_storage_paths.h"

#include <stdio.h>
#include <string.h>

static bool normalize_system(char *output, size_t capacity, const char *system_id) {
    if(output == 0 || capacity < 2 || system_id == 0 || *system_id == '\0') {
        return false;
    }
    size_t index = 0;
    while(system_id[index] != '\0') {
        unsigned char c = (unsigned char)system_id[index];
        if(c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - ('a' - 'A'));
        }
        if(!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') ||
           index + 1 >= capacity) {
            return false;
        }
        output[index++] = (char)c;
    }
    output[index] = '\0';
    return true;
}

static bool safe_filename(const char *filename) {
    if(filename == 0 || *filename == '\0' || strstr(filename, "..") != 0) {
        return false;
    }
    for(const unsigned char *p = (const unsigned char *)filename; *p != '\0'; ++p) {
        if(!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
             (*p >= '0' && *p <= '9') || *p == ' ' || *p == '-' ||
             *p == '_' || *p == '.')) {
            return false;
        }
    }
    return true;
}

static bool format_path(char *output, size_t capacity, const char *format,
                        const char *system, uint32_t crc, const char *tail) {
    const int length = snprintf(output, capacity, format, system, (unsigned long)crc, tail);
    return length > 0 && (size_t)length < capacity;
}

bool jack_storage_rom_dir(char *output, size_t capacity, const char *system_id) {
    char system[24];
    if(!normalize_system(system, sizeof(system), system_id)) {
        return false;
    }
    return format_path(output, capacity, "/roms/%s", system, 0, "");
}

bool jack_storage_save_dir(char *output, size_t capacity, const char *system_id,
                           uint32_t content_crc32) {
    char system[24];
    if(!normalize_system(system, sizeof(system), system_id)) {
        return false;
    }
    return format_path(output, capacity, "/SAVES/%s/%08lX", system, content_crc32, "");
}

bool jack_storage_content_path(char *output, size_t capacity, const char *system_id,
                               const char *filename) {
    char system[24];
    if(!normalize_system(system, sizeof(system), system_id) || !safe_filename(filename)) {
        return false;
    }
    const int length = snprintf(output, capacity, "/roms/%s/%s", system, filename);
    return length > 0 && (size_t)length < capacity;
}

bool jack_storage_save_path(char *output, size_t capacity, const char *system_id,
                            uint32_t content_crc32, const char *suffix) {
    char system[24];
    if(!normalize_system(system, sizeof(system), system_id) ||
       !safe_filename(suffix) || suffix[0] != '.') {
        return false;
    }
    return format_path(output, capacity, "/SAVES/%s/%08lX%s", system, content_crc32, suffix);
}

uint32_t jack_storage_crc32(uint32_t previous, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = ~previous;
    for(size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for(unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}
