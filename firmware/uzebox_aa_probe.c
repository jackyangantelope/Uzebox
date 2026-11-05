/* Diagnostic only; original guest PCM and event execution are not touched. */
#include "uzebox_aa_probe.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/structs/xip.h"
#define AA_CODE __attribute__((section(".aa_probe_text"),noinline,noclone))
struct uzebox_aa_state_s uzebox_aa_state __attribute__((section(".aa_probe_state"),aligned(8),used));
extern char __aa_mode1_start,__aa_loop_start;
AA_CODE void uzebox_aa_init(void){
    memset(&uzebox_aa_state,0,sizeof(uzebox_aa_state));
    printf("UZEBOX_AA_CONFIG version=0.6.26-AA-xip-layout baseline=Y0623 full_path=1 mode1=%08lx loop=%08lx cache=global_both_cores bounds=bracketed reset=report_no_flush timing=AA_FRAME+Y_FRAME_ALL pcm=original events=strict\n",
        (unsigned long)(uintptr_t)&__aa_mode1_start,(unsigned long)(uintptr_t)&__aa_loop_start);
    stdio_flush();
    xip_ctrl_hw->ctr_hit=0; xip_ctrl_hw->ctr_acc=0;
}
AA_CODE void uzebox_aa_frame_begin(uint32_t start){
    uzebox_aa_state.frame_start=start;
    uzebox_aa_state.begin_a0=xip_ctrl_hw->ctr_acc;
    uzebox_aa_state.begin_h=xip_ctrl_hw->ctr_hit;
    uzebox_aa_state.begin_a1=xip_ctrl_hw->ctr_acc;
}
AA_CODE void uzebox_aa_frame_end(void){
    const uint32_t a0=xip_ctrl_hw->ctr_acc;
    const uint32_t h=xip_ctrl_hw->ctr_hit;
    const uint32_t a1=xip_ctrl_hw->ctr_acc;
    struct uzebox_aa_state_s *s=&uzebox_aa_state;
    s->frames++;s->total_frames++;
    if(a1==UINT32_MAX || h==UINT32_MAX || s->begin_a1==UINT32_MAX || s->begin_h==UINT32_MAX ||
       a0<s->begin_a1 || a1<a0 || h<s->begin_h || s->begin_a1<s->begin_a0){s->bad++;}
    else{
        const uint32_t lo=a0-s->begin_a1,hi=a1-s->begin_a0,hit=h-s->begin_h;
        if(hit>hi){s->bad++;}
        else{s->acc_lo+=lo;s->acc_hi+=hi;s->hit+=hit;}
    }
    const uint32_t elapsed=time_us_32()-s->frame_start;
    s->frame_us+=elapsed;
    if(s->frames==1u || elapsed<s->min_us)s->min_us=elapsed;
    if(elapsed>s->max_us)s->max_us=elapsed;
}
AA_CODE void uzebox_aa_report(void){
    struct uzebox_aa_state_s *s=&uzebox_aa_state;
    if(s->cursor<s->length){s->dropped++;}
    else{
        const int count=snprintf(s->line,sizeof(s->line),
            "UZEBOX_XIP end=%lu n=%lu us=%llu min=%lu max=%lu acc_lo=%llu acc_hi=%llu hit=%llu bad=%lu drop=%lu ctrl=%08lx\n",
            (unsigned long)s->total_frames,(unsigned long)s->frames,
            (unsigned long long)s->frame_us,(unsigned long)s->min_us,(unsigned long)s->max_us,
            (unsigned long long)s->acc_lo,(unsigned long long)s->acc_hi,(unsigned long long)s->hit,
            (unsigned long)s->bad,
            (unsigned long)s->dropped,(unsigned long)xip_ctrl_hw->ctrl);
        s->cursor=0;
        if(count<0 || (size_t)count>=sizeof(s->line)){s->length=0;s->dropped++;}
        else{s->length=(uint16_t)count;}
    }
    s->frames=0;s->bad=0;s->acc_lo=0;s->acc_hi=0;s->hit=0;s->frame_us=0;s->min_us=0;s->max_us=0;
    /* These are write-to-clear performance counters, not cache invalidation. */
    xip_ctrl_hw->ctr_hit=0;xip_ctrl_hw->ctr_acc=0;
}
AA_CODE void uzebox_aa_pump(unsigned budget){
    struct uzebox_aa_state_s *s=&uzebox_aa_state;
    while(budget && s->cursor<s->length && uart_is_writable(uart_default)){
        --budget;uart_putc_raw(uart_default,s->line[s->cursor++]);
    }
}
