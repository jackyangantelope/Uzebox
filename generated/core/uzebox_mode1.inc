/* Experimental Mode1 6-pixel/20-op tile semantic loop. GPL-3.0-or-later.
 * Admission and partial prefixes retain every event, IRQ and row boundary.
 * Boot self-test gates the shared worker and latches failures until restart. */
#include "uzebox_mode1.h"
const volatile uint32_t uzebox_mode1_control[2]
 __attribute__((section(".rodata.uzebox_mode1"),used))={0x57323156U,1U};
static auint cu_mode1_ready,cu_mode1_testing;
static volatile auint cu_mode1_calls,cu_mode1_iterations;
#define cu_mode1_enabled (cu_mode1_testing || (cu_mode1_ready==1U && uzebox_mode1_control[1]==1U))
static boole cu_avr_mode1_bind(cu_avr_loop_binding_t *b,auint pc){
 static uint8 const ids[20]={0x19,0x49,0x23,0x19,0x49,0x23,0x19,0x49,0x40,0x19,0x49,0x09,0x0d,0x19,0x49,0x19,0x01,0x2b,0x49,0x43};
 static uint8 const loads[6]={0,3,6,9,13,15},outs[6]={1,4,7,10,14,18};
 uint32 op[20],used=0;auint roles[10],i,p,w,base,c,x;
 for(i=0;i<20;i++){op[i]=cpu_code[(pc+i)&32767U];if((op[i]&127U)!=ids[i])return FALSE;}
 p=cu_avr_loop_a(op[0]);w=cu_avr_loop_a(op[2]);x=cu_avr_loop_b(op[2]);base=cu_avr_loop_b(op[11]);c=cu_avr_loop_a(op[17]);
 if(w>=31U||base>=31U||(w&1U)||(base&1U)||(x!=26U&&x!=28U))return FALSE;
 if(cu_avr_loop_a(op[5])!=w+1U||cu_avr_loop_b(op[5])!=x||cu_avr_loop_b(op[8])!=0U||
    cu_avr_loop_a(op[11])!=w||cu_avr_loop_a(op[12])!=w+1U||cu_avr_loop_b(op[12])!=base+1U||
    cu_avr_loop_a(op[16])!=30U||cu_avr_loop_b(op[16])!=w||cu_avr_loop_a(op[19])!=SREG_ZM||
    ((20U+cu_avr_loop_b(op[19]))&32767U)!=0U)return FALSE;
 for(i=0;i<6;i++)if(cu_avr_loop_a(op[loads[i]])!=p||cu_avr_loop_b(op[outs[i]])!=p)return FALSE;
 roles[0]=p;roles[1]=w;roles[2]=w+1;roles[3]=base;roles[4]=base+1;roles[5]=c;roles[6]=x;roles[7]=x+1;roles[8]=30;roles[9]=31;
 for(i=0;i<10;i++){if(roles[i]>=32U||(used&(1U<<roles[i])))return FALSE;used|=1U<<roles[i];}
 b->pixel=p;b->work=w;b->base=base;b->count=c;b->pointer=x;b->branch_delta=cu_avr_loop_b(op[19]);b->kind=6U;
 cu_avr_loop_counters.builds++;return TRUE;
}
/* Volatile byte stores prevent GCC from outlining tiny pixel runs into
 * repeated general memset calls; every destination remains a byte address. */
static inline void cu_m1_fill(uint8 *p,auint value,auint n){
 volatile uint8 *q=p;
 for(auint k=0;k<n;k++)q[k]=(uint8)value;
}
static boole __attribute__((noinline,noclone,section(".aa_flash_mode1"))) cu_avr_mode1_run(cu_avr_loop_binding_t *b){
 auint budget,event,remaining,count,n,cycles,x,z,base,ddr,pixel,raw=0,work=0,src=0,dst=0,res=0,flags,i;
 uint8 *out;
 if(!cu_mode1_enabled)return FALSE;
 count=cpu_state.iors[b->count];remaining=count?count:256U;
 event=WRAP32(cycle_next_event-cpu_state.cycle);if(!event)return FALSE;
 budget=VIDEO_CY_MAX-video_cycle;if(budget>=event)budget=event-1U;
 n=budget/36U;if(n>=remaining)n=remaining;
 else if(n+1U==remaining&&budget>=remaining*36U-1U)n++;
 x=cpu_state.iors[b->pointer]+((auint)cpu_state.iors[b->pointer+1U]<<8);
 if(x<256U)return FALSE;
 if(n>(65536U-x)/2U)n=(65536U-x)/2U;
 if(!n)return FALSE;
 z=cpu_state.iors[30]+((auint)cpu_state.iors[31]<<8);
 base=cpu_state.iors[b->base]+((auint)cpu_state.iors[b->base+1U]<<8);
 ddr=cpu_state.iors[CU_IO_DDRC];pixel=cpu_state.iors[CU_IO_PORTC];out=&video_row.pixels[video_cycle];
 for(i=0;i<n;i++){
  cu_m1_fill(out,pixel,3U);
  raw=cpu_state.crom[z];z=(z+1U)&65535U;pixel=raw&ddr;cu_m1_fill(out+3,pixel,6U);
  work=cpu_state.sram[x&4095U];access_mem[x&4095U]|=CU_MEM_R;x++;
  raw=cpu_state.crom[z];z=(z+1U)&65535U;pixel=raw&ddr;cu_m1_fill(out+9,pixel,6U);
  work|=(auint)cpu_state.sram[x&4095U]<<8;access_mem[x&4095U]|=CU_MEM_R;x++;
  raw=cpu_state.crom[z];z=(z+1U)&65535U;pixel=raw&ddr;cu_m1_fill(out+15,pixel,6U);
  raw=cpu_state.crom[z];z=(z+1U)&65535U;pixel=raw&ddr;cu_m1_fill(out+21,pixel,6U);
  src=base>>8;dst=work>>8;res=src+dst+(((work&255U)+(base&255U))>>8);
  work=(work+base)&65535U;
  raw=cpu_state.crom[z];z=(z+1U)&65535U;pixel=raw&ddr;cu_m1_fill(out+27,pixel,6U);
  raw=cpu_state.crom[z];pixel=raw&ddr;z=work;
  cu_m1_fill(out+33,pixel,(i+1U==n && n==remaining)?2U:3U);out+=36U;
 }
 x&=65535U;cpu_state.iors[b->pointer]=x;cpu_state.iors[b->pointer+1U]=x>>8;
 cpu_state.iors[30]=z;cpu_state.iors[31]=z>>8;
 cpu_state.iors[b->work]=work;cpu_state.iors[b->work+1U]=work>>8;
 cpu_state.iors[b->pixel]=raw;cpu_state.iors[CU_IO_PORTC]=pixel;
 flags=(cpu_state.iors[CU_IO_SREG]&(SREG_IM|SREG_TM))|cpu_pflags[CU_AVRFG_ADD+(res&511U)+((src&0x90U)<<5)+((dst&0x90U)<<6)];
 count=(count-n)&255U;cpu_state.iors[b->count]=count;
 cpu_state.iors[CU_IO_SREG]=(flags&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|cpu_pflags[CU_AVRFG_DEC+count];
 cpu_state.pc=WRAP32(cpu_state.pc+(n-(n==remaining?1U:0U))*(20U+b->branch_delta)+(n==remaining?20U:0U));
 cycles=n*36U-(n==remaining?1U:0U);cpu_state.cycle=WRAP32(cpu_state.cycle+cycles);video_cycle+=cycles;
 cu_mode1_calls++;cu_mode1_iterations+=n;return TRUE;
}
