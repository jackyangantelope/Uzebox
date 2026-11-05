/* Same-path ARM PC / bus diagnostic. SPDX-License-Identifier: BSD-3-Clause */
#ifndef UZEBOX_SAMPLER_H
#define UZEBOX_SAMPLER_H
#include <stdbool.h>
#include <stdint.h>
#define UZ_SAMPLE_RANGES 96u
#define UZ_SAMPLE_KINDS 12u
#define UZ_SAMPLE_STAGES 9u
#define UZ_SAMPLE_ID "J069"
#define UZ_SAMPLE_IRQ_BURST_MAX 512u
/* Separate mechanisms: 1 IRQ low, 2 IRQ dense, 3 bus only, 4/5 both. */
static inline bool uz_sample_irq_mode(unsigned mode){return mode==1u||mode==2u||mode==4u||mode==5u;}
static inline bool uz_sample_bus_mode(unsigned mode){return mode>=3u&&mode<=5u;}
enum { UZ_IDLE, UZ_CORE, UZ_RENDER, UZ_SOURCE, UZ_INPUT, UZ_DRAIN, UZ_UART, UZ_REPORT, UZ_SAVE };
/* PC kinds: other, ordinary/run, loop dispatch, tile, bitmap, ALU, wait,
 * REGION, fused, AVR peripheral/event helpers, HDMI/audio, sampler itself. */
typedef struct { uint32_t start,end,kind; } uz_sample_range;
typedef struct {
    uint32_t magic,count,checksum,reserved;
    uz_sample_range ranges[UZ_SAMPLE_RANGES];
} uz_sample_map;
typedef struct {
    uint32_t total,invalid,isr,late,max_late_us,body_us,max_body_us;
    uint32_t kinds[UZ_SAMPLE_KINDS],stages[UZ_SAMPLE_STAGES],guest[32];
    uint32_t guest_samples,guest_invalid;
} uz_sample_counts;
typedef struct { uint32_t frames,saturated; uint64_t us,counters[4]; } uz_bus_counts;
typedef struct {
    uz_sample_counts pc;
    uz_bus_counts bus[4];
    uint64_t xip_hit,xip_access;
    uint32_t xip_frames,xip_saturated,mode,available,bus_available,map_id;
} uz_sample_report;
static inline unsigned uz_sample_mode(unsigned phase,bool warmed){
    const unsigned modes[8]={0u,1u,0u,3u,0u,4u,0u,5u};
    return warmed?modes[phase&7u]:0u;
}
static inline uint32_t uz_sample_gap(uint32_t *rng,unsigned mode){
    *rng=*rng*1664525u+1013904223u;
    return mode==2u||mode==5u?128u+(*rng>>24):512u+(*rng>>22);
}
/* Arm Cortex-M33 DUG 100235, Fig 2-6: PC is word 6 for BOTH basic
 * and FP-extended frames. Reject security-transition/additional-context
 * frames. No inference from an unvalidated stack pointer is permitted. */
static inline bool uz_sample_frame_ok(uint32_t sp,uint32_t exc){
    return (exc&0xffffff83u)==0xffffff81u && (exc&0x60u)==0x60u &&
        (sp&3u)==0u && sp>=0x20000000u && sp<=0x20081fe0u;
}
static inline bool uz_sample_pc_ok(uint32_t pc,uint32_t psr){
    return (psr&0x01000000u)!=0u && (pc<0x00010000u ||
        (pc>=0x10000000u&&pc<0x11000000u) ||
        (pc>=0x20000000u&&pc<0x20082000u));
}
static inline unsigned uz_sample_lookup(const volatile uz_sample_map *map,uint32_t pc){
    unsigned lo=0u,hi=map->count;
    while(lo<hi){unsigned mid=lo+(hi-lo)/2u;
        if(pc<map->ranges[mid].start)hi=mid;
        else if(pc>=map->ranges[mid].end)lo=mid+1u;
        else return map->ranges[mid].kind;
    }
    return 0u;
}
#ifdef UZEBOX_PC_SAMPLER
typedef struct {
    uint32_t requested,transitions,irq_total,irq_exits,burst,guard;
    uint32_t last_exc,last_pc,last_sp,last_psr,probe_ok;
} uz_sample_health;
extern volatile uz_sample_health uzebox_sample_health;
extern volatile uint32_t uzebox_sample_stage;
extern const volatile uz_sample_map uzebox_sampler_map;
static inline void uzebox_sampler_stage(unsigned stage){
    __asm volatile("" ::: "memory");uzebox_sample_stage=stage;__asm volatile("" ::: "memory");
}
void uzebox_sampler_init(const volatile unsigned int *guest_pc);
void uzebox_sampler_set_mode(unsigned mode);
void uzebox_sampler_take(uz_sample_report *out);
void uzebox_sampler_frame_begin(void);
void uzebox_sampler_frame_end(void);
bool uzebox_sampler_probe(void);
void uzebox_sampler_health_snapshot(uz_sample_health *out,uint32_t *alarm,uint32_t *irq,uint32_t *priority,uint32_t *armed,uint32_t *inte,uint32_t *intf);
#else
#define uzebox_sampler_stage(stage) ((void)0)
#define uzebox_sampler_frame_begin() ((void)0)
#define uzebox_sampler_frame_end() ((void)0)
#endif
#endif
