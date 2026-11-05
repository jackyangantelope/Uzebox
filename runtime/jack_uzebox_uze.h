/* BSD-3-Clause parser for external Uzebox UZE content. */
#ifndef JACK_UZEBOX_UZE_H
#define JACK_UZEBOX_UZE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint8_t version;
    uint8_t target;
    uint32_t program_size;
    uint16_t year;
    char name[33];
    char author[33];
    uint32_t header_crc32;
    uint8_t mouse;
    char description[65];
    const uint8_t *program;
} jack_uzebox_uze_info;
bool jack_uzebox_parse_uze(const uint8_t *data,size_t size,jack_uzebox_uze_info *info);
uint32_t jack_uzebox_crc32(uint32_t previous,const void *data,size_t size);
#endif