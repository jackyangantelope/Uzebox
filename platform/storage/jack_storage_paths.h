/* BSD-3-Clause content/save path contract for yellow ports. */
#ifndef JACK_YELLOW_STORAGE_PATHS_H
#define JACK_YELLOW_STORAGE_PATHS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool jack_storage_rom_dir(char *output, size_t capacity, const char *system_id);
bool jack_storage_save_dir(char *output, size_t capacity, const char *system_id,
                           uint32_t content_crc32);
bool jack_storage_content_path(char *output, size_t capacity, const char *system_id,
                               const char *filename);
bool jack_storage_save_path(char *output, size_t capacity, const char *system_id,
                            uint32_t content_crc32, const char *suffix);
uint32_t jack_storage_crc32(uint32_t previous, const void *data, size_t size);

#endif
