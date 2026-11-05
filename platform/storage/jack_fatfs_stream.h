/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef JACK_YELLOW_FATFS_STREAM_H
#define JACK_YELLOW_FATFS_STREAM_H
#include <stdbool.h>
#include "ff.h"
#include "jack_stream.h"
typedef struct { FIL file; bool open; } jack_fatfs_file;
FRESULT jack_fatfs_mount(FATFS *filesystem);
bool jack_fatfs_open_read(jack_stream *stream,jack_fatfs_file *file,const char *path);
bool jack_fatfs_open_write(jack_stream *stream,jack_fatfs_file *file,const char *path);
FRESULT jack_fatfs_ensure_save_dir(const char *system_id,uint32_t crc32);
#endif
