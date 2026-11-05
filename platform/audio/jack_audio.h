/* BSD-3-Clause caller-owned stereo audio ring for HDMI output. */
#ifndef JACK_YELLOW_AUDIO_H
#define JACK_YELLOW_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t left;
    int16_t right;
} jack_audio_frame;

typedef struct {
    jack_audio_frame *frames;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    uint32_t sample_rate;
    uint32_t underruns;
    uint32_t overruns;
    size_t high_water;
} jack_audio_ring;

typedef void (*jack_audio_sink)(void *context, int16_t left, int16_t right);

bool jack_audio_init(jack_audio_ring *ring, jack_audio_frame *frames, size_t capacity,
                     uint32_t sample_rate);
bool jack_audio_push(jack_audio_ring *ring, jack_audio_frame frame);
size_t jack_audio_push_many(jack_audio_ring *ring, const jack_audio_frame *frames, size_t count);
bool jack_audio_pop(jack_audio_ring *ring, jack_audio_frame *frame);
jack_audio_frame jack_audio_pull_or_silence(jack_audio_ring *ring);
size_t jack_audio_drain(jack_audio_ring *ring, size_t count, jack_audio_sink sink, void *context);

#endif
