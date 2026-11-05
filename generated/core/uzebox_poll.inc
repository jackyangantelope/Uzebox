/* Structurally matched, event-bounded idle input polling cycle.
 * GPL-3.0-or-later. Never skips an input change, IRQ or hardware event.
 * Only the idle zero-input/zero-vsync path is admitted; all others use ISA.
 * CALL/RET stack residues, access marks, flags and raw PC are preserved. */
#include "uzebox_poll.h"
const volatile uint32_t uzebox_poll_control[2]
 __attribute__((section(".rodata.uzebox_poll"),used))={0x58323256U,1U};
static auint cu_poll_ready,cu_poll_testing;
static volatile auint cu_poll_calls,cu_poll_iterations;
#define cu_poll_enabled (cu_poll_testing || (cu_poll_ready==1U && uzebox_poll_control[1]==1U))
static uint32 cu_poll_op(auint pc){return cpu_code[pc&32767U]&~CU_AVR_LOOP_CALL_MARKER;}
static boole cu_poll_bind(cu_avr_loop_binding_t *b,auint pc){
 auint joy,flag,p,i,addr;
 uint32 op=cu_poll_op(pc);
 if((op&65535U)!=0xadU || pc<1U || pc>32500U)return FALSE;
 joy=op>>16;
 if(joy>32760U || cu_poll_op(joy)!=0x18180eU || cu_poll_op(joy+1U)!=0x50243U ||
    (cu_poll_op(joy+2U)&65535U)!=0x18a0U || (cu_poll_op(joy+4U)&65535U)!=0x19a0U ||
    cu_poll_op(joy+6U)!=0x31U)return FALSE;
 addr=cpu_code[(joy+2U)&32767U]>>16;
 if(addr<256U || addr>=65535U || (cpu_code[(joy+4U)&32767U]>>16)!=addr+1U)return FALSE;
 if(cu_poll_op(pc+2U)!=0x180e01U || (cu_poll_op(pc+3U)&65535U)!=0xadU ||
    cu_poll_op(pc+5U)!=0x18180eU || cu_poll_op(pc+6U)!=0x10243U ||
    (cu_poll_op(pc+7U)&65535U)!=0x40U || cu_poll_op(pc-1U)!=0x1848U)return FALSE;
 flag=cu_poll_op(pc+3U)>>16;
 if(flag>32763U || (cu_poll_op(flag)&65535U)!=0x18a0U ||
    cu_poll_op(flag+2U)!=0x11816U || cu_poll_op(flag+3U)!=0x31U)return FALSE;
 flag=cpu_code[flag]>>16;if(flag<256U)return FALSE;
 p=pc+8U+(cu_poll_op(pc+7U)>>16);
 for(i=0;i<7;i++){
  if(p>32765U || (cu_poll_op(p)&255U)!=0x47U ||
     (cu_avr_loop_a(cu_poll_op(p))!=14U && cu_avr_loop_a(cu_poll_op(p))!=15U) ||
     !cu_avr_loop_b(cu_poll_op(p)) || (cu_avr_loop_b(cu_poll_op(p))&(cu_avr_loop_b(cu_poll_op(p))-1U)) ||
     cu_avr_loop_b(cu_poll_op(p))>128U || (cu_poll_op(p+1U)&65535U)!=0x40U)return FALSE;
  p=p+2U+(cu_poll_op(p+1U)>>16);
  if(i==6U){if((p&32767U)!=pc-1U || p!=pc-1U+65536U)return FALSE;}
 }
 b->work=addr;b->base=addr>>8;b->stride=flag;b->count=flag>>8;b->kind=7U;return TRUE;
}
static boole __attribute__((noinline,noclone)) cu_poll_run(cu_avr_loop_binding_t *b){
 auint joy=b->work+((auint)b->base<<8),flag=b->stride+((auint)b->count<<8);
 auint sp=cpu_state.iors[CU_IO_SPL]+((auint)cpu_state.iors[CU_IO_SPH]<<8),s0=sp&4095U,s1=(sp-1U)&4095U;
 auint event,budget,n,used,ret;
 if(!cu_poll_enabled || event_it || event_it_enter || video_cycle>=VIDEO_CY_MAX ||
    cpu_state.iors[24] || (cpu_state.pc&65535U)!=b->pc)return FALSE;
 if(s0==(joy&4095U)||s1==(joy&4095U)||s0==((joy+1U)&4095U)||s1==((joy+1U)&4095U)||s0==(flag&4095U)||s1==(flag&4095U))return FALSE;
 if(cpu_state.sram[joy&4095U]||cpu_state.sram[(joy+1U)&4095U]||(cpu_state.sram[flag&4095U]&1U))return FALSE;
 event=WRAP32(cycle_next_event-cpu_state.cycle);if(!event)return FALSE;
 budget=VIDEO_CY_MAX-video_cycle;if(budget>=event)budget=event-1U;
 n=budget/52U;if(!n)return FALSE;used=n*52U;
 ret=b->pc+5U;cpu_state.sram[s0]=ret;cpu_state.sram[s1]=ret>>8;
 access_mem[s0]|=CU_MEM_R|CU_MEM_W;access_mem[s1]|=CU_MEM_R|CU_MEM_W;
 access_mem[joy&4095U]|=CU_MEM_R;access_mem[(joy+1U)&4095U]|=CU_MEM_R;access_mem[flag&4095U]|=CU_MEM_R;
 cpu_state.iors[14]=cpu_state.iors[15]=cpu_state.iors[24]=cpu_state.iors[25]=0;
 cpu_state.iors[CU_IO_SREG]=(cpu_state.iors[CU_IO_SREG]&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|SREG_ZM;
 cpu_state.pc=b->pc+65536U;cpu_state.cycle=WRAP32(cpu_state.cycle+used);
 memset(video_row.pixels+video_cycle,cpu_state.iors[CU_IO_PORTC],used);video_cycle+=used;
 cu_poll_calls++;cu_poll_iterations+=n;return TRUE;
}
