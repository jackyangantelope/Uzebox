/* Diagnostic-only crash capture. No stdio locks, allocation, or recovery of
 * possibly corrupted guest state. SPDX-License-Identifier: BSD-3-Clause */
#include "uzebox_sampler.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/sync.h"
#include "hardware/structs/scb.h"
uint32_t uzebox_sampler_fault_stack[128] __attribute__((aligned(8),used));
static void __no_inline_not_in_flash_func(uzebox_sampler_fault_char)(char ch){
    unsigned budget=250000u;
    while((uart_get_hw(uart_default)->fr&UART_UARTFR_TXFF_BITS)&&--budget){}
    if(budget)uart_get_hw(uart_default)->dr=(uint32_t)(unsigned char)ch;
}
static void __no_inline_not_in_flash_func(uzebox_sampler_fault_text)(const char *s){
    while(*s)uzebox_sampler_fault_char(*s++);
}
static void __no_inline_not_in_flash_func(uzebox_sampler_fault_hex)(uint32_t value){
    for(int shift=28;shift>=0;shift-=4){unsigned c=(value>>(unsigned)shift)&15u;
        uzebox_sampler_fault_char((char)(c<10u?'0'+c:'a'+c-10u));}
}
#define FAULT_FIELD(name,value) do{uzebox_sampler_fault_text(" " name "=");uzebox_sampler_fault_hex(value);}while(0)
void __attribute__((noreturn)) __no_inline_not_in_flash_func(uzebox_sampler_fault_body)(const uint32_t *frame,uint32_t exc,uint32_t ipsr){
    uint32_t pc=0u,psr=0u;
    if(uz_sample_frame_ok((uint32_t)(uintptr_t)frame,exc)){pc=frame[6];psr=frame[7];}
    uzebox_sampler_fault_text("\r\nUZEBOX_FAULT v=J069");
    FAULT_FIELD("exception",ipsr);FAULT_FIELD("exc_return",exc);
    FAULT_FIELD("sp",(uint32_t)(uintptr_t)frame);FAULT_FIELD("pc",pc);FAULT_FIELD("psr",psr);
    FAULT_FIELD("cfsr",scb_hw->cfsr);FAULT_FIELD("hfsr",scb_hw->hfsr);
    FAULT_FIELD("bfar",scb_hw->bfar);FAULT_FIELD("mmfar",scb_hw->mmfar);
    FAULT_FIELD("stage",uzebox_sample_stage);FAULT_FIELD("requested",uzebox_sample_health.requested);
    FAULT_FIELD("irq_total",uzebox_sample_health.irq_total);FAULT_FIELD("irq_exits",uzebox_sample_health.irq_exits);
    FAULT_FIELD("last_pc",uzebox_sample_health.last_pc);FAULT_FIELD("guard",uzebox_sample_health.guard);
    uzebox_sampler_fault_text("\r\n");
    for(;;)__wfi();
}
/* Change to a private bounded stack even if the original MSP overflowed. */
void __attribute__((naked)) __no_inline_not_in_flash_func(isr_hardfault)(void){
    __asm volatile("cpsid i\n tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n mov r1, lr\n mrs r2, ipsr\n ldr r3, =uzebox_sampler_fault_stack + 512\n mov sp, r3\n b uzebox_sampler_fault_body\n");
}
