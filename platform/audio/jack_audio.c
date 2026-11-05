/* BSD-3-Clause caller-owned stereo audio ring for HDMI output. */
#include "jack_audio.h"

bool jack_audio_init(jack_audio_ring *ring, jack_audio_frame *frames, size_t capacity,
                     uint32_t sample_rate) {
    if(ring == NULL || frames == NULL || capacity < 2u || sample_rate == 0u) {
        return false;
    }
    *ring = (jack_audio_ring){.frames = frames, .capacity = capacity, .sample_rate = sample_rate};
    return true;
}

bool jack_audio_push(jack_audio_ring *ring, jack_audio_frame frame) {
    if(ring == NULL || ring->frames == NULL || ring->count == ring->capacity) {
        if(ring != NULL) {
            ring->overruns++;
        }
        return false;
    }
    ring->frames[ring->head] = frame;
    ring->head = (ring->head + 1u) % ring->capacity;
    ring->count++;
    if(ring->count > ring->high_water) {
        ring->high_water = ring->count;
    }
    return true;
}

size_t jack_audio_push_many(jack_audio_ring *ring, const jack_audio_frame *frames, size_t count) {
    if(frames == NULL) {
        return 0u;
    }
    size_t pushed = 0u;
    while(pushed < count && jack_audio_push(ring, frames[pushed])) {
        pushed++;
    }
    return pushed;
}

bool jack_audio_pop(jack_audio_ring *ring, jack_audio_frame *frame) {
    if(ring == NULL || frame == NULL || ring->frames == NULL || ring->count == 0u) {
        return false;
    }
    *frame = ring->frames[ring->tail];
    ring->tail = (ring->tail + 1u) % ring->capacity;
    ring->count--;
    return true;
}

jack_audio_frame jack_audio_pull_or_silence(jack_audio_ring *ring) {
    jack_audio_frame frame = {0, 0};
    if(!jack_audio_pop(ring, &frame) && ring != NULL) {
        ring->underruns++;
    }
    return frame;
}

size_t jack_audio_drain(jack_audio_ring *ring, size_t count, jack_audio_sink sink, void *context) {
    if(ring == NULL || sink == NULL) {
        return 0u;
    }
    for(size_t index = 0; index < count; ++index) {
        const jack_audio_frame frame = jack_audio_pull_or_silence(ring);
        sink(context, frame.left, frame.right);
    }
    return count;
}
