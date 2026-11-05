/* Startup check for the port-private 172-word mixer-prefix extension.
 * GPL-3.0-or-later, following the original fused startup oracle.
 * Requires cu_avr_fused_prefix, CU_Y_ENABLED, cu_y_testing,
 * cu_y_prefix_calls and public uzebox_y_ready/control from the owner.
 * The unmarked original ISA stops BEFORE the cartridge-specific tail.
 */
static void cu_y_selftest_input(auint vector){
 auint j,addr;
 for(j=0U;j<39U;j++){
  addr=j<34U?0x200U+j:0x2FEU+j-34U;
  cpu_state.sram[addr]=(uint8)(vector*17U+j*29U);
  access_mem[addr]=(uint8)(0x40U+(j&15U));
 }
 /* These bytes are untouched by a correct prefix, and form a safe target
  * if a broken executor accidentally runs the legacy POP/RET epilogue. */
 cpu_state.sram[0x301U]=0x30U;cpu_state.sram[0x302U]=0U;
 memset(cpu_state.iors,0,256U);
 cpu_state.iors[16U]=(uint8)(vector*3U+7U);
 cpu_state.iors[17U]=(uint8)(vector*5U+11U);
 cpu_state.iors[18U]=(uint8)(vector*13U+23U);
 cpu_state.iors[30U]=2U;
 cpu_state.iors[CU_IO_SPH]=3U;
 cpu_state.iors[CU_IO_SREG]=(uint8)(vector*17U);
 cpu_state.iors[CU_IO_PORTC]=(uint8)(0x5AU^vector);
 /* No emulated sync edge may be produced while checking the prefix. */
 cpu_state.iors[CU_IO_DDRB]=0U;
 cpu_state.iors[CU_IO_TCCR2B]=(vector%3U==0U)?0U:((vector%3U==1U)?1U:255U);
 access_io[CU_IO_PORTB]=0x50U;access_io[CU_IO_OCR2A]=0xA0U;
 for(j=0U;j<4U;j++)audio_samples[j]=(vector*19U+j*31U)&255U;
 audio_rp=vector&3U;audio_wp=(vector>>2U)&3U;
 memset(video_row.pixels,0xD3,256U);
 cpu_state.pc=0x400U;
 cpu_state.cycle=(vector&1U)?0xFFFFFF80U:0U;
 video_cycle=0U;cycle_next_event=WRAP32(cpu_state.cycle+1000U);
 event_it=FALSE;event_it_enter=FALSE;
}

bool uzebox_y_selftest(void){
 uint32 code[174U];
 uint8 old_io[256U],expected_io[256U],pixels[256U];
 uint8 old_ram[39U],expected_ram[39U],old_marks[39U],expected_marks[39U];
 uint8 old_io_marks[2U],expected_io_marks[2U];
 auint old_audio[4U],expected_audio[4U];
 auint const old_rp=audio_rp,old_wp=audio_wp;
 auint const old_pc=cpu_state.pc,old_cycle=cpu_state.cycle;
 auint const old_row=video_cycle,old_event=cycle_next_event;
 auint const old_base=cu_avr_fused_pc,old_mem=cu_avr_fused_memory;
 auint const old_prefix=cu_avr_fused_prefix,old_calls=cu_y_prefix_calls;
 boole const old_it=event_it,old_enter=event_it_enter;
 boole const old_enable=cu_avr_fused_enabled();
#ifdef CU_AVR_REGION_COMPARE
 boole const old_region=cu_avr_region_enabled();
#endif
 cu_avr_fused_stats_t const old_stats=cu_avr_fused_data;
 auint vector,j,addr,start,ref_pc,ref_cycle,ref_row,ref_cycles,ref_rp,ref_wp,steps;
 bool ok=true;
 if(cu_avr_loop_disabled){uzebox_y_ready=2U;return false;}
 if(uzebox_y_ready==2U)return false;
 if(uzebox_y_ready==1U)return true;
 memcpy(code,&cpu_code[0x400U],sizeof(code));
 memcpy(old_io,cpu_state.iors,sizeof(old_io));
 memcpy(pixels,video_row.pixels,sizeof(pixels));
 memcpy(old_audio,audio_samples,sizeof(old_audio));
 old_io_marks[0]=access_io[CU_IO_PORTB];old_io_marks[1]=access_io[CU_IO_OCR2A];
 for(j=0U;j<39U;j++){
  addr=j<34U?0x200U+j:0x2FEU+j-34U;
  old_ram[j]=cpu_state.sram[addr];old_marks[j]=access_mem[addr];
 }
 for(j=0U;j<174U;j++){
  uint32 op=cu_avr_fused_pattern[j];
  if(((op&127U)==0x1CU||(op&127U)==0x20U)&&(op>>16)<34U)op+=0x200U<<16;
  cpu_code[0x400U+j]=op;
 }
 /* Deliberately distinct from the old POP/RET tail. The prefix must not
  * execute these cartridge-specific instructions, even with budget left. */
 cpu_code[0x400U+172U]=0x00C018A0U;
 cpu_code[0x400U+173U]=0x0000004AU;
 cu_avr_fused_pc=0x400U;cu_avr_fused_memory=0x200U;
 cu_avr_fused_prefix=1U;cu_y_testing=1U;
#ifdef CU_AVR_REGION_COMPARE
 cu_avr_region_enable(TRUE);
#endif
 for(vector=0U;vector<36U&&ok;vector++){
  cu_y_selftest_input(vector);start=cpu_state.cycle;
  cu_avr_fused_enable(FALSE);
  steps=0U;
  while(cpu_state.pc>=0x400U&&cpu_state.pc<0x400U+172U&&steps++<300U)cu_avr_exec();
  ref_pc=cpu_state.pc;ref_cycle=cpu_state.cycle;ref_row=video_cycle;
  ref_cycles=WRAP32(ref_cycle-start);ref_rp=audio_rp;ref_wp=audio_wp;
  if(ref_pc!=0x400U+172U||!ref_cycles||ref_cycles>=256U||ref_row!=ref_cycles){ok=false;break;}
  memcpy(expected_io,cpu_state.iors,sizeof(expected_io));
  memcpy(expected_audio,audio_samples,sizeof(expected_audio));
  expected_io_marks[0]=access_io[CU_IO_PORTB];expected_io_marks[1]=access_io[CU_IO_OCR2A];
  for(j=0U;j<39U;j++){
   addr=j<34U?0x200U+j:0x2FEU+j-34U;
   expected_ram[j]=cpu_state.sram[addr];expected_marks[j]=access_mem[addr];
  }
  /* Establish that the independently executed oracle wrote exactly its
   * constant-PORTC prefix and left all guard bytes intact. */
  for(j=0U;j<256U;j++){
   if(video_row.pixels[j]!=(j<ref_cycles?(uint8)(0x5AU^vector):0xD3U)){ok=false;break;}
  }
  if(!ok)break;
  cu_y_selftest_input(vector);
  cu_avr_fused_enable(TRUE);
  ok=cu_avr_fused_run()&&cpu_state.pc==ref_pc&&cpu_state.cycle==ref_cycle&&video_cycle==ref_row;
  if(memcmp(expected_io,cpu_state.iors,sizeof(expected_io))||memcmp(expected_audio,audio_samples,sizeof(expected_audio))||audio_rp!=ref_rp||audio_wp!=ref_wp)ok=false;
  for(j=0U;j<39U&&ok;j++){
   addr=j<34U?0x200U+j:0x2FEU+j-34U;
   if(cpu_state.sram[addr]!=expected_ram[j]||access_mem[addr]!=expected_marks[j])ok=false;
  }
  if(access_io[CU_IO_PORTB]!=expected_io_marks[0]||access_io[CU_IO_OCR2A]!=expected_io_marks[1])ok=false;
  for(j=0U;j<256U&&ok;j++){
   if(video_row.pixels[j]!=(j<ref_cycles?(uint8)(0x5AU^vector):0xD3U))ok=false;
  }
 }
 memcpy(&cpu_code[0x400U],code,sizeof(code));
 memcpy(cpu_state.iors,old_io,sizeof(old_io));
 memcpy(video_row.pixels,pixels,sizeof(pixels));
 memcpy(audio_samples,old_audio,sizeof(old_audio));audio_rp=old_rp;audio_wp=old_wp;
 for(j=0U;j<39U;j++){
  addr=j<34U?0x200U+j:0x2FEU+j-34U;
  cpu_state.sram[addr]=old_ram[j];access_mem[addr]=old_marks[j];
 }
 access_io[CU_IO_PORTB]=old_io_marks[0];access_io[CU_IO_OCR2A]=old_io_marks[1];
 cpu_state.pc=old_pc;cpu_state.cycle=old_cycle;video_cycle=old_row;cycle_next_event=old_event;
 event_it=old_it;event_it_enter=old_enter;
 cu_avr_fused_pc=old_base;cu_avr_fused_memory=old_mem;cu_avr_fused_prefix=old_prefix;
 cu_avr_fused_data=old_stats;cu_avr_fused_enable(old_enable);
#ifdef CU_AVR_REGION_COMPARE
 cu_avr_region_enable(old_region);
#endif
 cu_y_prefix_calls=old_calls;cu_y_testing=0U;uzebox_y_ready=ok?1U:2U;
 return ok;
}
void uzebox_y_stats(uzebox_y_stats_t *out){
 out->mode=uzebox_y_control[1];out->selftest=uzebox_y_ready;
 out->prefix=cu_avr_fused_prefix;out->base=cu_avr_fused_pc;out->calls=cu_y_prefix_calls;
}
