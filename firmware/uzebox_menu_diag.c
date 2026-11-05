/* BSD-3-Clause. Dedicated fault stack; bounded raw UART, no stdio locks. */
#include "uzebox_menu_diag.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/structs/scb.h"
static volatile uint32_t menu_phase;
uint32_t uzebox_menu_fault_stack[64] __attribute__((aligned(8),used));
void uzebox_menu_phase(uint32_t phase){menu_phase=phase;}
static void raw_char(char ch){
    unsigned budget=250000u;
    while((uart_get_hw(uart_default)->fr&UART_UARTFR_TXFF_BITS)&&--budget){}
    if(budget)uart_get_hw(uart_default)->dr=(uint32_t)(unsigned char)ch;
}
static void raw_text(const char *s){while(*s)raw_char(*s++);}
static void raw_hex(uint32_t value){
    for(int shift=28;shift>=0;shift-=4){unsigned c=(value>>(unsigned)shift)&15u;raw_char((char)(c<10u?'0'+c:'a'+c-10u));}
}
#define FIELD(label,value) do{raw_text(" " label "=");raw_hex(value);}while(0)
void __attribute__((noreturn,noinline)) uzebox_menu_fault(const uint32_t *frame,uint32_t exc,uint32_t ipsr){
    uintptr_t address=(uintptr_t)frame;
    if(!(exc&16u))address+=18u*sizeof(uint32_t);
    uint32_t pc=0u,psr=0u;
    if(address>=0x20000000u&&address<=0x20081fe0u&&(address&3u)==0u){const uint32_t *basic=(const uint32_t *)address;pc=basic[6];psr=basic[7];}
    raw_text("\r\nUZEBOX_MENU_FAULT v=QD1");
    FIELD("core",get_core_num());FIELD("phase",menu_phase);FIELD("exception",ipsr);FIELD("exc_return",exc);
    FIELD("sp",(uint32_t)(uintptr_t)frame);FIELD("pc",pc);FIELD("psr",psr);
    FIELD("cfsr",scb_hw->cfsr);FIELD("hfsr",scb_hw->hfsr);FIELD("bfar",scb_hw->bfar);FIELD("mmfar",scb_hw->mmfar);
    raw_text("\r\n");for(;;)__wfi();
}
void __attribute__((naked)) isr_hardfault(void){
    __asm volatile("cpsid i\n tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n mov r1, lr\n mrs r2, ipsr\n ldr r3, =uzebox_menu_fault_stack + 256\n mov sp, r3\n b uzebox_menu_fault\n");
}
