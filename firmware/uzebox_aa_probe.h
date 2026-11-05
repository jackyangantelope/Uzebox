#ifndef UZEBOX_AA_PROBE_H
#define UZEBOX_AA_PROBE_H
#include <stdbool.h>
#include <stdint.h>
void uzebox_aa_init(void);
void uzebox_aa_frame_begin(uint32_t start);
void uzebox_aa_frame_end(void);
void uzebox_aa_report(void);
void uzebox_aa_pump(unsigned budget);
extern struct uzebox_aa_state_s {
    uint32_t begin_a0,begin_h,begin_a1,frame_start;
    uint64_t acc_lo,acc_hi,hit,frame_us;
    uint32_t frames,bad,total_frames,min_us,max_us;
    uint16_t length,cursor;
    uint32_t dropped;
    char line[256];
} uzebox_aa_state;
static inline bool uzebox_aa_pending(void){return uzebox_aa_state.cursor<uzebox_aa_state.length;}
#endif
