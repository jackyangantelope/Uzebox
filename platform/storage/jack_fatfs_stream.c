/* SPDX-License-Identifier: BSD-3-Clause */
#include "jack_fatfs_stream.h"
#include "jack_storage_paths.h"
#include <limits.h>
#include <string.h>

static size_t fs_read(void *context, void *buffer, size_t size) {
    jack_fatfs_file *file = (jack_fatfs_file *)context;
    UINT done = 0u;
    if(size > UINT_MAX) size = UINT_MAX;
    return f_read(&file->file, buffer, (UINT)size, &done) == FR_OK ? done : 0u;
}

static size_t fs_write(void *context, const void *buffer, size_t size) {
    jack_fatfs_file *file = (jack_fatfs_file *)context;
    UINT done = 0u;
    if(size > UINT_MAX) size = UINT_MAX;
    return f_write(&file->file, buffer, (UINT)size, &done) == FR_OK ? done : 0u;
}

static bool fs_seek(void *context, uint64_t offset) {
    const FSIZE_t target = (FSIZE_t)offset;
    return (uint64_t)target == offset &&
        f_lseek(&((jack_fatfs_file *)context)->file, target) == FR_OK;
}

static uint64_t fs_tell(void *context) {
    return f_tell(&((jack_fatfs_file *)context)->file);
}

static uint64_t fs_size(void *context) {
    return f_size(&((jack_fatfs_file *)context)->file);
}

static bool fs_flush(void *context) {
    return f_sync(&((jack_fatfs_file *)context)->file) == FR_OK;
}

static void fs_close(void *context) {
    jack_fatfs_file *file = (jack_fatfs_file *)context;
    if(file->open) {
        (void)f_close(&file->file);
        file->open = false;
    }
}

static const jack_stream_ops ops = {
    fs_read, fs_write, fs_seek, fs_tell, fs_size, fs_flush, fs_close
};

FRESULT jack_fatfs_mount(FATFS *filesystem) {
    return filesystem != NULL ? f_mount(filesystem, "", 1) : FR_INVALID_OBJECT;
}

static bool open_mode(jack_stream *stream, jack_fatfs_file *file,
                      const char *path, BYTE mode) {
    if(stream == NULL || file == NULL || path == NULL) return false;
    memset(file, 0, sizeof(*file));
    if(f_open(&file->file, path, mode) != FR_OK) return false;
    file->open = true;
    *stream = (jack_stream){&ops, file};
    return true;
}

bool jack_fatfs_open_read(jack_stream *stream, jack_fatfs_file *file,
                          const char *path) {
    return open_mode(stream, file, path, FA_READ | FA_OPEN_EXISTING);
}

bool jack_fatfs_open_write(jack_stream *stream, jack_fatfs_file *file,
                           const char *path) {
    return open_mode(stream, file, path, FA_WRITE | FA_CREATE_ALWAYS);
}

static FRESULT mkdir_ok(const char *path) {
    const FRESULT result = f_mkdir(path);
    return result == FR_EXIST ? FR_OK : result;
}

FRESULT jack_fatfs_ensure_save_dir(const char *system_id, uint32_t crc32) {
    char full[96];
    char system[64];
    if(!jack_storage_save_dir(full, sizeof(full), system_id, crc32)) {
        return FR_INVALID_NAME;
    }
    FRESULT result = mkdir_ok("/SAVES");
    if(result != FR_OK) return result;
    const char *last = strrchr(full, '/');
    if(last == NULL || last == full) return FR_INVALID_NAME;
    const size_t length = (size_t)(last - full);
    if(length >= sizeof(system)) return FR_INVALID_NAME;
    memcpy(system, full, length);
    system[length] = '\0';
    result = mkdir_ok(system);
    return result == FR_OK ? mkdir_ok(full) : result;
}