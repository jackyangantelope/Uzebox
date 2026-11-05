/* Port-local, original-PCM audio-output experiment. */
#ifndef UZEBOX_AUDIO_BATCH_H
#define UZEBOX_AUDIO_BATCH_H
#include "jack_audio.h"
#include <stdint.h>
typedef struct {
    uint32_t calls,samples,real,zero,batches,peak;
    uint32_t packets,queued,highdrop,pushfail,pending,phase;
} uzebox_audio_batch_stats_t;
extern const volatile uint32_t uzebox_audio_batch_control[2];
static inline uint32_t uzebox_audio_batch_mode(void){return uzebox_audio_batch_control[1];}
size_t uzebox_audio_drain(jack_audio_ring *ring,size_t count);
void uzebox_audio_batch_stats(uzebox_audio_batch_stats_t *out);
#endif
