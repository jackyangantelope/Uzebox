/* BSD-3-Clause backend-neutral streaming I/O contract. */
#ifndef JACK_YELLOW_STREAM_H
#define JACK_YELLOW_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct jack_stream jack_stream;
typedef struct {
    size_t (*read)(void *context, void *buffer, size_t size);
    size_t (*write)(void *context, const void *buffer, size_t size);
    bool (*seek)(void *context, uint64_t offset);
    uint64_t (*tell)(void *context);
    uint64_t (*size)(void *context);
    bool (*flush)(void *context);
    void (*close)(void *context);
} jack_stream_ops;
struct jack_stream { const jack_stream_ops *ops; void *context; };
typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t length;
    size_t position;
    bool writable;
} jack_memory_stream;

bool jack_stream_valid(const jack_stream *stream);
bool jack_stream_read_exact(jack_stream *stream, void *buffer, size_t size);
bool jack_stream_write_exact(jack_stream *stream, const void *buffer, size_t size);
bool jack_stream_seek(jack_stream *stream, uint64_t offset);
uint64_t jack_stream_tell(jack_stream *stream);
uint64_t jack_stream_size(jack_stream *stream);
bool jack_stream_flush(jack_stream *stream);
void jack_stream_close(jack_stream *stream);
bool jack_memory_stream_open(jack_stream *stream, jack_memory_stream *memory, void *data,
                             size_t length, size_t capacity, bool writable);
#endif
