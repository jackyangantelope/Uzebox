/* BSD-3-Clause backend-neutral streaming I/O contract. */
#include "jack_stream.h"
#include <string.h>

bool jack_stream_valid(const jack_stream *s) { return s != NULL && s->ops != NULL && s->context != NULL; }
bool jack_stream_read_exact(jack_stream *s, void *b, size_t n) {
    return jack_stream_valid(s) && s->ops->read != NULL && s->ops->read(s->context, b, n) == n;
}
bool jack_stream_write_exact(jack_stream *s, const void *b, size_t n) {
    return jack_stream_valid(s) && s->ops->write != NULL && s->ops->write(s->context, b, n) == n;
}
bool jack_stream_seek(jack_stream *s, uint64_t o) {
    return jack_stream_valid(s) && s->ops->seek != NULL && s->ops->seek(s->context, o);
}
uint64_t jack_stream_tell(jack_stream *s) {
    return jack_stream_valid(s) && s->ops->tell != NULL ? s->ops->tell(s->context) : 0u;
}
uint64_t jack_stream_size(jack_stream *s) {
    return jack_stream_valid(s) && s->ops->size != NULL ? s->ops->size(s->context) : 0u;
}
bool jack_stream_flush(jack_stream *s) {
    return jack_stream_valid(s) && (s->ops->flush == NULL || s->ops->flush(s->context));
}
void jack_stream_close(jack_stream *s) {
    if(jack_stream_valid(s) && s->ops->close != NULL) s->ops->close(s->context);
    if(s != NULL) { s->ops = NULL; s->context = NULL; }
}
static size_t mem_read(void *c, void *b, size_t n) {
    jack_memory_stream *m = (jack_memory_stream *)c;
    const size_t a = m->position < m->length ? m->length - m->position : 0u;
    const size_t k = n < a ? n : a;
    if(k) { memcpy(b, m->data + m->position, k); m->position += k; }
    return k;
}
static size_t mem_write(void *c, const void *b, size_t n) {
    jack_memory_stream *m = (jack_memory_stream *)c;
    if(!m->writable || m->position > m->capacity) return 0u;
    const size_t a = m->capacity - m->position;
    const size_t k = n < a ? n : a;
    if(k) {
        memcpy(m->data + m->position, b, k); m->position += k;
        if(m->position > m->length) m->length = m->position;
    }
    return k;
}
static bool mem_seek(void *c, uint64_t o) {
    jack_memory_stream *m = (jack_memory_stream *)c;
    if(o > m->length || o > SIZE_MAX) return false;
    m->position = (size_t)o; return true;
}
static uint64_t mem_tell(void *c) { return ((jack_memory_stream *)c)->position; }
static uint64_t mem_size(void *c) { return ((jack_memory_stream *)c)->length; }
static bool mem_flush(void *c) { (void)c; return true; }
static const jack_stream_ops mem_ops = {mem_read, mem_write, mem_seek, mem_tell, mem_size, mem_flush, NULL};
bool jack_memory_stream_open(jack_stream *s, jack_memory_stream *m, void *data,
                             size_t length, size_t capacity, bool writable) {
    if(s == NULL || m == NULL || data == NULL || length > capacity) return false;
    *m = (jack_memory_stream){(uint8_t *)data, capacity, length, 0u, writable};
    *s = (jack_stream){&mem_ops, m}; return true;
}
