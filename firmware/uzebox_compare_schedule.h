/* Automatic same-image diagnostic phases. Independent of platform and clocks.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef UZEBOX_COMPARE_SCHEDULE_H
#define UZEBOX_COMPARE_SCHEDULE_H
#include <stdbool.h>
#include <stdint.h>
#define UZEBOX_COMPARE_REPORTS 8u
#ifdef CU_AVR_FUSED
/* Nonzero first word keeps all mode words in one initialized data object. */
extern volatile uint32_t uzebox_g_control[3];
#define UZEBOX_COMPARE_REGION_MASK uzebox_g_control[0]
#define UZEBOX_COMPARE_TIMING_MASK uzebox_g_control[1]
#else
#define UZEBOX_COMPARE_REGION_MASK 0x96u
#define UZEBOX_COMPARE_TIMING_MASK 0xCCu
#endif
typedef struct {
    uint32_t phase,window,epoch;
    bool warmed;
} uzebox_compare_schedule;
/* Chronological untimed observations are OFF,ON,ON,OFF; timed observations
 * are ON,OFF,OFF,ON. The game itself is never reset or given synthetic input. */
static inline bool uzebox_compare_region(const uzebox_compare_schedule *s){
    return s->warmed&&((UZEBOX_COMPARE_REGION_MASK>>s->phase)&1u)!=0u;
}
static inline bool uzebox_compare_timing(const uzebox_compare_schedule *s){
    return s->warmed&&((UZEBOX_COMPARE_TIMING_MASK>>s->phase)&1u)!=0u;
}
#ifdef CU_AVR_FUSED
static inline bool uzebox_compare_fused(const uzebox_compare_schedule *s){
    return s->warmed&&((uzebox_g_control[2]>>s->phase)&1u)!=0u;
}
#endif
static inline bool uzebox_compare_clean(const uzebox_compare_schedule *s){
    return s->warmed&&s->window!=0u;
}
/* Called AFTER publishing the just-completed report, before another frame.
 * Warmup and all switches therefore occur at complete report boundaries. */
static inline bool uzebox_compare_next(uzebox_compare_schedule *s,uint32_t completed){
    if(!s->warmed){
        if(completed<60u)return false;
        s->warmed=true;s->phase=0u;s->window=0u;s->epoch=1u;
        return true;
    }
    if(++s->window<UZEBOX_COMPARE_REPORTS)return false;
    s->window=0u;s->phase=(s->phase+1u)&7u;++s->epoch;
    return true;
}
#endif
