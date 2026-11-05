/* Hardware PC sampler; never calls or replaces an AVR decoder.
 * SPDX-License-Identifier: BSD-3-Clause */
#include "uzebox_sampler.h"
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/structs/busctrl.h"
#include "hardware/structs/xip.h"
/* Bound AFTER linking using final symbol sizes. The binder modifies ONLY
 * this reserved Flash table in ELF + UF2, never executable instructions. */
const volatile uz_sample_map uzebox_sampler_map __attribute__((section(".rodata.uzebox_sampler_map"),used))={0x554e4244u,0u,0u,0u,{{0u,0u,0u}}};
static uz_sample_map active_map; /* IRQ performs all lookups in SRAM. */
volatile uint32_t uzebox_sample_stage;
static uz_sample_report counters;
static const volatile unsigned int *avr_pc;
static uint32_t rng=0x61c88647u,target,mask,bus_start;
static int alarm_index=-1;
static unsigned irq_number,bus_group;
static bool bus_active;
volatile uz_sample_health uzebox_sample_health;
static uint32_t foreground_stamp,last_irq_seen;
static const uint8_t events[4][4]={{60,61,56,57},{52,48,44,40},{36,32,28,24},{8,9,64,65}};

void __no_inline_not_in_flash_func(uzebox_sampler_irq_body)(const uint32_t *frame,uint32_t exc){
    const uint32_t begin=timer_hw->timerawl;
    timer_hw->intr=mask;
    hw_clear_bits(&timer_hw->intf,mask);
    if(!uz_sample_irq_mode(counters.mode)){__dsb();return;}
    ++uzebox_sample_health.irq_total;
    uzebox_sample_health.last_exc=exc;
    uzebox_sample_health.last_sp=(uint32_t)(uintptr_t)frame;
    /* An IRQ storm must stop itself even if foreground never runs again. */
    if(++uzebox_sample_health.burst>UZ_SAMPLE_IRQ_BURST_MAX){
        uzebox_sample_health.guard|=1u;
        hw_clear_bits(&timer_hw->inte,mask);timer_hw->armed=mask;
        __dsb();return;
    }
    uz_sample_counts *p=&counters.pc;
    ++p->total;
    uint32_t late=begin-target;
    if(late>8u)++p->late;
    if(late>p->max_late_us)p->max_late_us=late;
    if(!uz_sample_frame_ok((uint32_t)(uintptr_t)frame,exc)||!uz_sample_pc_ok(frame[6],frame[7]))++p->invalid;
    else{
        const uint32_t pc=frame[6]&~1u;
        uzebox_sample_health.last_pc=pc;uzebox_sample_health.last_psr=frame[7];
        const unsigned kind=uz_sample_lookup(&active_map,pc);
        const unsigned stage=uzebox_sample_stage;
        ++p->kinds[kind];
        ++p->stages[stage<UZ_SAMPLE_STAGES?stage:UZ_IDLE];
        if(frame[7]&0x1ffu)++p->isr;
        /* AVR PC is a coarse execution-region hint: native regions update
         * it at exits, so this is NOT an instruction-frequency histogram. */
        if(stage==UZ_CORE && !(frame[7]&0x1ffu)){
            uint32_t guest=*avr_pc;
            if(guest<32768u){++p->guest[guest>>10];++p->guest_samples;}
            else ++p->guest_invalid;
        }
    }
    const uint32_t end=timer_hw->timerawl;
    const uint32_t body=end-begin;
    p->body_us+=body;if(body>p->max_body_us)p->max_body_us=body;
    target=end+uz_sample_gap(&rng,counters.mode);
    timer_hw->alarm[alarm_index]=target;
    /* The lowest-priority sampler may itself be delayed by USB. A past
     * compare value would otherwise suppress IRQs for a 32-bit timer wrap. */
    if((int32_t)(target-timer_hw->timerawl)<=0){
        uzebox_sample_health.guard|=2u;
        hw_clear_bits(&timer_hw->inte,mask);timer_hw->armed=mask;
    }
    ++uzebox_sample_health.irq_exits;
    __dsb();
}
/* Tail branch preserves EXC_RETURN for the C epilogue. No C prologue may
 * precede selection of the interrupted MSP/PSP. M33 FP context follows the
 * eight core registers; it does not precede them as on Cortex-M4. */
void __attribute__((naked)) __no_inline_not_in_flash_func(uzebox_sampler_irq)(void){
    __asm volatile("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n mov r1, lr\n b uzebox_sampler_irq_body\n");
}
void uzebox_sampler_init(const volatile unsigned int *guest_pc){
    avr_pc=guest_pc;
    if(uzebox_sampler_map.magic!=0x555a5043u || !uzebox_sampler_map.count || uzebox_sampler_map.count>UZ_SAMPLE_RANGES)return;
    uint32_t sum=2166136261u,previous=0u;
    for(unsigned i=0;i<uzebox_sampler_map.count;i++){
        const uz_sample_range r=uzebox_sampler_map.ranges[i];
        if(r.start<previous||r.end<=r.start||r.kind>=UZ_SAMPLE_KINDS)return;
        sum=(sum^r.start)*16777619u;sum=(sum^r.end)*16777619u;sum=(sum^r.kind)*16777619u;
        previous=r.end;active_map.ranges[i]=r;
    }
    if(sum!=uzebox_sampler_map.checksum)return;
    active_map.count=uzebox_sampler_map.count;
    counters.map_id=sum;
    alarm_index=hardware_alarm_claim_unused(false);
    if(alarm_index<0)return;
    mask=1u<<(unsigned)alarm_index;irq_number=hardware_alarm_get_irq_num((unsigned)alarm_index);
    irq_set_exclusive_handler(irq_number,uzebox_sampler_irq);
    /* Do not preempt time-critical PIO USB or create nested timer frames.
     * Samples now describe interruptible foreground, not ISR occupancy. */
    irq_set_priority(irq_number,PICO_LOWEST_IRQ_PRIORITY);
    counters.available=1u;
    /* This firmware has no other BUSCTRL user. If already active, leave it
     * alone. Counters cover ALL masters, including HSTX/core1/USB DMA. */
    counters.bus_available=busctrl_hw->perfctr_en==0u;
}
void uzebox_sampler_set_mode(unsigned mode){
    uzebox_sample_health.requested=mode;++uzebox_sample_health.transitions;
    if(!counters.available)return;
    irq_set_enabled(irq_number,false);
    hw_clear_bits(&timer_hw->inte,mask);timer_hw->armed=mask;timer_hw->intr=mask;
    hw_clear_bits(&timer_hw->intf,mask);
    irq_clear(irq_number);
    counters.mode=mode<=5u?mode:0u;
    if(uz_sample_irq_mode(counters.mode)&&uzebox_sample_health.guard)counters.mode=0u;
    uzebox_sample_health.burst=0u;
    foreground_stamp=timer_hw->timerawl;last_irq_seen=uzebox_sample_health.irq_total;
    if(uz_sample_irq_mode(counters.mode)){
        target=timer_hw->timerawl+uz_sample_gap(&rng,counters.mode);
        timer_hw->alarm[alarm_index]=target;hw_set_bits(&timer_hw->inte,mask);
        __dsb();irq_set_enabled(irq_number,true);
    }
}
void uzebox_sampler_take(uz_sample_report *out){
    const uint32_t saved=save_and_disable_interrupts();
    *out=counters;
    memset(&counters.pc,0,sizeof(counters.pc));
    memset(counters.bus,0,sizeof(counters.bus));
    counters.xip_hit=counters.xip_access=0u;counters.xip_frames=counters.xip_saturated=0u;
    restore_interrupts(saved);
}
void uzebox_sampler_frame_begin(void){
    uzebox_sample_health.burst=0u;
    if(uz_sample_irq_mode(counters.mode)){
        if(last_irq_seen!=uzebox_sample_health.irq_total){
            last_irq_seen=uzebox_sample_health.irq_total;foreground_stamp=timer_hw->timerawl;
        }else if((uint32_t)(timer_hw->timerawl-foreground_stamp)>200000u){
            uzebox_sample_health.guard|=4u;uzebox_sampler_set_mode(0u);
        }
    }
    if(!uz_sample_bus_mode(counters.mode)||!counters.bus_available)return;
    busctrl_hw->perfctr_en=0u;
    for(unsigned i=0;i<4u;i++){busctrl_hw->counter[i].sel=events[bus_group][i];busctrl_hw->counter[i].value=0u;}
    xip_ctrl_hw->ctr_hit=0u;xip_ctrl_hw->ctr_acc=0u;
    bus_start=timer_hw->timerawl;bus_active=true;busctrl_hw->perfctr_en=1u;
}
void uzebox_sampler_frame_end(void){
    if(!bus_active)return;
    busctrl_hw->perfctr_en=0u;
    const uint32_t elapsed=timer_hw->timerawl-bus_start;
    uint32_t values[4];bool saturated=false;
    for(unsigned i=0;i<4u;i++){values[i]=busctrl_hw->counter[i].value;saturated|=values[i]==0xffffffu;}
    uz_bus_counts *b=&counters.bus[bus_group];
    if(saturated)++b->saturated;
    else{++b->frames;b->us+=elapsed;for(unsigned i=0;i<4u;i++)b->counters[i]+=values[i];}
    const uint32_t hit=xip_ctrl_hw->ctr_hit,acc=xip_ctrl_hw->ctr_acc;
    if(hit==UINT32_MAX||acc==UINT32_MAX||hit>acc)++counters.xip_saturated;
    else{++counters.xip_frames;counters.xip_hit+=hit;counters.xip_access+=acc;}
    bus_group=(bus_group+1u)&3u;bus_active=false;
}
bool uzebox_sampler_probe(void){
    if(!counters.available)return false;
    const uint32_t before=uzebox_sample_health.irq_total;
    uzebox_sampler_set_mode(1u);
    const uint32_t begin=timer_hw->timerawl;
    while((uint32_t)(timer_hw->timerawl-begin)<10000u)tight_loop_contents();
    uzebox_sampler_set_mode(0u);
    const bool ok=uzebox_sample_health.irq_total-before>=2u &&
        uzebox_sample_health.irq_total==uzebox_sample_health.irq_exits &&
        !uzebox_sample_health.guard&&!counters.pc.invalid;
    uzebox_sample_health.probe_ok=ok?1u:0u;
    if(!ok)uzebox_sample_health.guard|=8u;
    memset(&counters.pc,0,sizeof(counters.pc));
    return ok;
}
void uzebox_sampler_health_snapshot(uz_sample_health *out,uint32_t *alarm,uint32_t *irq,uint32_t *priority,uint32_t *armed,uint32_t *inte,uint32_t *intf){
    const uint32_t saved=save_and_disable_interrupts();
    *out=uzebox_sample_health;
    *alarm=(uint32_t)alarm_index;*irq=irq_number;
    *priority=counters.available?irq_get_priority(irq_number):UINT32_MAX;
    *armed=timer_hw->armed;*inte=timer_hw->inte;*intf=timer_hw->intf;
    restore_interrupts(saved);
}
