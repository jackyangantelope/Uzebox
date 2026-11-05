/* BSD-3-Clause: host output clock adapter; never changes guest samples/events. */
#ifndef UZEBOX_AUDIO_CLOCK_H
#define UZEBOX_AUDIO_CLOCK_H
#include <stdint.h>
#define UZ_AUDIO_OUTPUT_HZ 44100u
#define UZ_AUDIO_VIDEO_HZ 60u
#define UZ_AUDIO_PREFILL 441u
typedef struct {
    uint32_t rate,phase,epoch_rows,epoch_frames,changes,invalid,started;
} uz_audio_clock_t;
#define UZ_AUDIO_CLOCK_INIT {262u*UZ_AUDIO_VIDEO_HZ,0,0,0,0,0,0}
static inline uint32_t uz_audio_clock_emit(uz_audio_clock_t *s){
    uint32_t n=0;s->phase+=UZ_AUDIO_OUTPUT_HZ;
    while(s->phase>=s->rate){s->phase-=s->rate;++n;}
    return n;
}
/* Average complete 60-frame epochs, including alternating 262/263-row modes. */
static inline void uz_audio_clock_frame(uz_audio_clock_t *s,uint32_t rows,int complete){
    if(!complete||!rows||rows>268u){++s->invalid;return;}
    s->epoch_rows+=rows;
    if(++s->epoch_frames==UZ_AUDIO_VIDEO_HZ){
        const uint32_t rate=s->epoch_rows;
        if(rate!=s->rate){
            s->phase=(uint32_t)((uint64_t)s->phase*rate/s->rate);
            s->rate=rate;++s->changes;
        }
        s->epoch_rows=0;s->epoch_frames=0;
    }
}
/* Same inclusive deadline, cap and discontinuity reset as the old loop. */
static inline uint32_t uz_audio_clock_due(uint64_t now,uint64_t *deadline){
    if(now>*deadline+1000000ull*2048u)*deadline=now;
    if(now<*deadline)return 0;
    /* The reset above bounds lateness to 2,048,000,000 (< UINT32_MAX).
     * Constant 32-bit division avoids a 64-bit helper in frequent callbacks. */
    uint32_t due=(uint32_t)(now-*deadline)/1000000u+1u;
    if(due>512u)due=512u;
    *deadline+=due*1000000u;
    return due;
}
static inline uint32_t uz_audio_clock_service(uz_audio_clock_t *s,uint32_t queued,
                                             uint64_t now,uint64_t *deadline){
    if(!s->started){
        *deadline=now;
        if(queued<UZ_AUDIO_PREFILL)return 0;
        s->started=1;
    }
    return uz_audio_clock_due(now,deadline);
}
#endif
