/* Event-free SRAM/ROM execution regions, GPL-3.0-or-later as cu_avr_n.h.
 * Reuses the existing decoder's ISA, not a second arithmetic implementation.
 * Only whitelist instructions can run. I/O, interrupt-enable changes, SPM,
 * WDR and existing native loop heads remain outside the region. PORTC is
 * invariant, so exact pixels and cycles can be committed once at the end.
 * Original instruction/stack/SRAM access ordering and marks remain visible.
 */
#if defined(CU_AVR_TIME_PROFILE) && !defined(CU_AVR_REGION_COMPARE)
#error "Region candidate must be measured without the old D timing decoder"
#endif

static cu_avr_region_stats_t cu_avr_region_data;
static boole cu_avr_region_valid;
void cu_avr_region_stats(cu_avr_region_stats_t *out){*out=cu_avr_region_data;}

/* Return exact instruction cycles, or zero for a barrier/invalid operand.
 * Classification checks the same operands without resolving indirect SRAM.
 * No state changes are made here, including on a rejected I/O address. */
static auint cu_avr_region_cost(auint pc,boole dynamic)
{
 uint32 op=cpu_code[pc&32767U];
 auint const id=op&127U;
 auint a=(op>>8)&255U,b=(op>>16)&65535U,ptr,addr;
 if(id==0x1AU || id==0x20U){a&=127U;}
 if(id==0x11U || id==0x18U || id==0x19U || id==0x2BU){
  if(op&CU_AVR_LOOP_HEAD_MARKER){return 0U;}
 }
 if(id==0x2DU && (op&CU_AVR_LOOP_CALL_MARKER)){return 0U;}
 if(id<=0x16U){
  if(id==0U){return 1U;}
  if(id==1U){return a<31U && b<31U?1U:0U;}
  if(a>=32U){return 0U;}
  if(id<=0x11U){
   if(b>=32U){return 0U;}
   if(id>=2U && id<=6U){return 2U;}
   if(id==0x0AU && dynamic && cpu_state.iors[a]==cpu_state.iors[b]){
    return 2U+((cpu_code[(pc+1U)&32767U]>>7)&1U);
   }
   return 1U;
  }
  return b<=255U?1U:0U;
 }
 switch(id){
  case 0x18U:case 0x19U:return a<32U?3U:0U;
  case 0x1AU:case 0x1BU:return a<32U?2U:0U;
  case 0x1CU:case 0x20U:return a<32U && b>=256U?2U:0U;
  case 0x1DU:case 0x21U:
   ptr=b&255U;if(a>=32U || ptr>=31U){return 0U;}
   if(!dynamic){return 2U;}
   addr=(cpu_state.iors[ptr]+((auint)cpu_state.iors[ptr+1U]<<8)+(b>>8))&65535U;
   return addr>=256U?2U:0U;
  case 0x1EU:case 0x1FU:case 0x22U:case 0x23U:
   if(a>=32U || b>=31U){return 0U;}
   if(!dynamic){return 2U;}
   addr=cpu_state.iors[b]+((auint)cpu_state.iors[b+1U]<<8);
   if(id==0x1EU || id==0x22U){addr=WRAP32(addr-1U);}
   return addr>=256U?2U:0U;
  case 0x24U:case 0x25U:case 0x26U:case 0x27U:case 0x28U:case 0x29U:case 0x2AU:case 0x2BU:
   return a<32U?1U:0U;
  case 0x2CU:return 3U;
  case 0x2DU:return 4U;
  case 0x2EU:case 0x2FU:return a<=255U && !(a&SREG_IM)?1U:0U;
  case 0x30U:return 2U;
  case 0x31U:return 4U;
  case 0x32U:return 3U;
  case 0x37U:return a<32U && b<32U?2U:0U;
  case 0x3AU:case 0x3BU:return a<31U && b<=63U?2U:0U;
  case 0x40U:return 2U;
  case 0x41U:return 3U;
  case 0x42U:case 0x43U:
   if(a>255U){return 0U;}
   if(!dynamic){return 1U;}
   return ((id==0x42U && (cpu_state.iors[CU_IO_SREG]&a)) ||
           (id==0x43U && !(cpu_state.iors[CU_IO_SREG]&a)))?2U:1U;
  case 0x44U:case 0x45U:return a<32U && b<8U?1U:0U;
  case 0x46U:case 0x47U:
   if(a>=32U || b>255U){return 0U;}
   if(dynamic && ((id==0x46U && !(cpu_state.iors[a]&b)) ||
                  (id==0x47U && (cpu_state.iors[a]&b)))){
    return 2U+((cpu_code[(pc+1U)&32767U]>>7)&1U);
   }
   return 1U;
  case 0x48U:return a<32U && b<=255U?1U:0U;
  default:return 0U;
 }
}

static void cu_avr_region_classify(auint base,auint len)
{
 auint i,j,pc,p,id;
 cu_avr_region_valid=TRUE;
 for(i=0U;i<32768U;i++){
  uint32 const op=cpu_code[i];
  auint const k=op&127U,a=((op>>8)&255U)&((k==0x1AU||k==0x20U)?127U:255U);
  auint const b=(op>>16)&65535U;
  /* Original compiler bounds all register operands. Reject malformed
   * synthetic tables globally before the unchecked decoder can execute. */
  if((k==0x1AU||k==0x1BU||k==0x1CU||k==0x20U||k==0x18U||k==0x19U) && a>=32U){cu_avr_region_valid=FALSE;}
  if(k==0x11U && (a>=32U || (b&32767U)>=32U)){cu_avr_region_valid=FALSE;}
  if(k<=0x16U && k!=0x11U && !cu_avr_region_cost(i,FALSE)){cu_avr_region_valid=FALSE;}
  if((k>=0x1DU && k<=0x23U && k!=0x20U) && (a>=32U || ((k==0x1DU||k==0x21U)?(b&255U):b)>=31U)){cu_avr_region_valid=FALSE;}
  if((k>=0x24U && k<=0x2BU) && a>=32U){cu_avr_region_valid=FALSE;}
  if(k==0x37U && (a>=32U || b>=32U)){cu_avr_region_valid=FALSE;}
  if((k==0x3AU||k==0x3BU) && (a>=31U||b>63U)){cu_avr_region_valid=FALSE;}
  if((k==0x44U||k==0x45U) && (a>=32U||b>=8U)){cu_avr_region_valid=FALSE;}
  if((k==0x46U||k==0x47U||k==0x48U) && (a>=32U||b>255U)){cu_avr_region_valid=FALSE;}
 }
 for(i=0U;i<len;i++){
  pc=(base+i)&32767U;id=cpu_code[pc]&127U;
  if(id==0x1AU || id==0x20U){cpu_code[pc]&=~CU_AVR_REGION_MARKER;}
 }
 for(i=0U;i<len;i++){
  pc=(base+i)&32767U;id=cpu_code[pc]&127U;
  if(id!=0x1AU && id!=0x20U){continue;}
  p=pc;
  for(j=0U;j<12U;j++){
   id=cpu_code[p]&127U;
   if(id==0x2CU||id==0x2DU||id==0x30U||id==0x31U||id==0x32U||id==0x40U||id==0x41U){break;}
   if(!cu_avr_region_cost(p,FALSE)){break;}
   p=(p+1U+((cpu_code[p]>>7)&1U))&32767U;
  }
  if(j==12U){cpu_code[pc]|=CU_AVR_REGION_MARKER;}
 }
#ifdef CU_AVR_FUSED
 cu_avr_fused_classify();
#endif
}

/* Every use of cycle/video work in this decoder is guarded out beforehand.
 * No supported instruction observes cycle or writes PORTC/IRQ enable.
 * Keep the original decoder source and frozen scalar oracle independent. */
#pragma push_macro("UPDATE_HARDWARE")
#pragma push_macro("UPDATE_HARDWARE_IT")
#undef UPDATE_HARDWARE
#undef UPDATE_HARDWARE_IT
#define UPDATE_HARDWARE do{region_cycles++;}while(0)
#define UPDATE_HARDWARE_IT UPDATE_HARDWARE
#define CU_AVR_LOOP_SCALAR_ORACLE 1
#define CU_AVR_REGION_COPY 1
#define event_it_enter FALSE
#define cu_avr_exec cu_avr_region_exec
#include "cu_avr_n.h"
#undef cu_avr_exec
#undef event_it_enter
#undef CU_AVR_REGION_COPY
#undef CU_AVR_LOOP_SCALAR_ORACLE
#pragma pop_macro("UPDATE_HARDWARE_IT")
#pragma pop_macro("UPDATE_HARDWARE")

#ifdef CU_AVR_LOOP_NATIVE
#define CU_REGION_RAM __attribute__((noinline,section(".time_critical.cu_avr_region_run")))
#else
#define CU_REGION_RAM
#endif
static boole CU_REGION_RAM cu_avr_region_run(void)
{
 auint const eb=WRAP32(cycle_next_event-cpu_state.cycle);
 auint const row=video_cycle,port=cpu_state.iors[CU_IO_PORTC];
 auint budget=VIDEO_CY_MAX-row,used=0U,steps=0U,cy;
 if(cu_avr_loop_disabled || !cu_avr_region_valid || event_it || event_it_enter || !eb || row>=VIDEO_CY_MAX){return FALSE;}
 if(budget>=eb){budget=eb-1U;}
 if(budget<12U){return FALSE;}
 while(steps<128U && budget-used>=4U){
  /* Guards are fused into relevant cases of the same decoder. A rejected
   * instruction restores its fetch PC before any architectural effect. */
  cy=cu_avr_region_exec();if(!cy){break;}used+=cy;steps++;
 }
 if(!steps){return FALSE;}
 memset(&video_row.pixels[row],(uint8)port,used);
 cpu_state.cycle=WRAP32(cpu_state.cycle+used);video_cycle=row+used;
 cu_avr_region_data.entries++;cu_avr_region_data.instructions+=steps;cu_avr_region_data.cycles+=used;
 return TRUE;
}
#undef CU_REGION_RAM

#ifdef CU_AVR_LOOP_NATIVE
static boole __attribute__((noinline)) cu_avr_region_selftest(void)
{
 uint8 saved_iors[256U],saved_pixels[8U],ram[2U],marks[2U];
 uint32 saved_code[6U];
 auint const pc=cpu_state.pc,cycle=cpu_state.cycle,row=video_cycle,event=cycle_next_event;
 boole const it=event_it,enter=event_it_enter;
 cu_avr_region_stats_t const stats=cu_avr_region_data;
 boole const valid=cu_avr_region_valid;
 boole ok;
 memcpy(saved_iors,cpu_state.iors,sizeof(saved_iors));memcpy(saved_pixels,video_row.pixels,8U);
 memcpy(saved_code,cpu_code,sizeof(saved_code));memcpy(ram,&cpu_state.sram[256U],2U);
 memcpy(marks,&access_mem[256U],2U);
 cpu_code[0]=0x010002A0U;cpu_code[1]=0x4AU;cpu_code[2]=0x00030209U;
 cpu_code[3]=0x0101029CU;cpu_code[4]=0x4AU;cpu_code[5]=0x00000238U;
 cpu_state.pc=0U;cpu_state.cycle=0U;video_cycle=0U;cycle_next_event=100U;
 event_it=FALSE;event_it_enter=FALSE;cpu_state.iors[3U]=10U;cpu_state.iors[CU_IO_PORTC]=0x5AU;
 cpu_state.sram[256U]=250U;
 cu_avr_region_valid=TRUE;
 ok=cu_avr_region_run() && cpu_state.pc==5U && cpu_state.cycle==5U && video_cycle==5U &&
    cpu_state.iors[2U]==4U && cpu_state.sram[257U]==4U &&
    (access_mem[256U]&CU_MEM_R) && (access_mem[257U]&CU_MEM_W);
 ok=ok && video_row.pixels[0U]==0x5AU && video_row.pixels[4U]==0x5AU;
 memcpy(cpu_state.iors,saved_iors,sizeof(saved_iors));memcpy(video_row.pixels,saved_pixels,8U);
 memcpy(cpu_code,saved_code,sizeof(saved_code));memcpy(&cpu_state.sram[256U],ram,2U);
 memcpy(&access_mem[256U],marks,2U);
 cpu_state.pc=pc;cpu_state.cycle=cycle;video_cycle=row;cycle_next_event=event;
 event_it=it;event_it_enter=enter;cu_avr_region_data=stats;
 cu_avr_region_valid=valid;
 return ok;
}
#endif
