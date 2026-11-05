/* GPL-3.0-or-later. Boot-only original-ISA comparison. No heap/PSRAM.
 * Events are held beyond both bounded vectors, so guest PCM and peripheral
 * state cannot advance. Restore guest state and invalidate test bindings. */
#define CU_AVR_LOOP_SCALAR_ORACLE 1
#define cu_avr_exec uz_scan_reference_step
#include "uzebox_scan_oracle.inc"
#undef cu_avr_exec
#undef CU_AVR_LOOP_SCALAR_ORACLE

static uint32 uz_scan_hash(void const *data,auint count)
{
 uint8 const *p=data;uint32 h=2166136261U;
 while(count--){h=(h^*p++)*16777619U;}
 return h;
}

static boole __attribute__((noinline,section(".text.uz_scan_bitmap_test"))) uz_scan_bitmap_test(void)
{
 cu_row_t saved_row=video_row;
 cu_avr_bitmap_ctx_t c,e;
 uint8 const m0=access_mem[512U],m1=access_mem[513U];
 auint tail;boole ok=TRUE;
 for(tail=0U;tail<2U;tail++){
  memset(&c,0,sizeof(c));c.pixels=video_row.pixels;c.sram=cpu_state.sram;c.marks=access_mem;
  c.x=512U;c.n=2U;c.port=0xA3U;c.pixel=0x37U;c.bits=0xA5U;c.pos=7U;c.cursor=11U;
  c.dead=0x12U;c.living=0xE7U;c.cursor_color=0xF0U;c.grid=0x81U;c.ddrc=0x5AU;c.tail_taken=tail;c.phase=4U;
  e=c;video_row=saved_row;access_mem[512U]=m0;access_mem[513U]=m1;
  uz_bitmap_local(&c);
  uint32 const hash=uz_scan_hash(&video_row,sizeof(video_row));
  uint8 const n0=access_mem[512U],n1=access_mem[513U];
  video_row=saved_row;access_mem[512U]=m0;access_mem[513U]=m1;
  cu_avr_bitmap_reference(&e);
  ok=ok && memcmp(&c,&e,sizeof(c))==0 && hash==uz_scan_hash(&video_row,sizeof(video_row)) &&
      n0==access_mem[512U] && n1==access_mem[513U];
 }
 video_row=saved_row;access_mem[512U]=m0;access_mem[513U]=m1;
 return ok;
}

static boole __attribute__((noinline,section(".text.uz_scan_function_test"))) uz_scan_function_test(void)
{
 static uint16 const locations[]={259U,261U,512U,513U,514U,515U,516U,517U,518U,519U,520U,521U,522U,523U,2302U,2303U};
 static uint8 const io[]={CU_IO_DDRC,CU_IO_PORTC,CU_IO_SPL,CU_IO_SPH,CU_IO_SREG};
 cu_row_t const saved_row=video_row;
 uint32 saved_code[136U];uint8 saved_regs[32U],saved_io[5U],saved_ram[16U],saved_marks[16U];
 uint8 native_regs[32U],native_io[5U],native_marks[16U];
 auint const old_pc=cpu_state.pc,old_cycle=cpu_state.cycle,old_video=video_cycle,old_event=cycle_next_event;
 auint const old_base=uz_video_base,old_valid=uz_video_valid;
 boole const old_it=event_it,old_enter=event_it_enter,old_pure=cu_avr_pure_ok;
 auint i,j;boole ok=TRUE;
 memcpy(saved_code,cpu_code,sizeof(saved_code));memcpy(saved_regs,cpu_state.iors,sizeof(saved_regs));
 for(i=0U;i<5U;i++)saved_io[i]=cpu_state.iors[io[i]];
 for(i=0U;i<16U;i++){saved_ram[i]=cpu_state.sram[locations[i]];saved_marks[i]=access_mem[locations[i]];}
 uz_video_base=0U;uz_video_valid=1U;cu_avr_pure_ok=TRUE;
 for(j=0U;j<2U;j++){
  memcpy(cpu_code,uz_scan_pattern,sizeof(saved_code));memset(cu_avr_loop_cache,0,sizeof(cu_avr_loop_cache));
  for(i=0U;i<32U;i++)cpu_state.iors[i]=(uint8)(i*13U+j*17U);
  cpu_state.iors[20]=3U;cpu_state.iors[14]=7U;cpu_state.iors[25]=(uint8)(4U+j*31U);
  cpu_state.iors[28]=0U;cpu_state.iors[29]=2U;
  cpu_state.iors[CU_IO_DDRC]=j?255U:0x5AU;cpu_state.iors[CU_IO_PORTC]=0x81U;
  cpu_state.iors[CU_IO_SPL]=0xFDU;cpu_state.iors[CU_IO_SPH]=8U;cpu_state.iors[CU_IO_SREG]=j?255U:0U;
  for(i=0U;i<16U;i++){cpu_state.sram[locations[i]]=(uint8)(0xA5U+i*17U);access_mem[locations[i]]=saved_marks[i];}
  cpu_state.sram[259U]=0x3FU;cpu_state.sram[261U]=7U;cpu_state.sram[2302U]=1U;cpu_state.sram[2303U]=1U;
  video_row=saved_row;cpu_state.pc=0U;cpu_state.cycle=0U;video_cycle=0U;cycle_next_event=4096U;event_it=FALSE;event_it_enter=FALSE;
  uz_video_run();
  auint const native_pc=cpu_state.pc,native_cycle=cpu_state.cycle,native_video=video_cycle;
  uint32 const hash=uz_scan_hash(&video_row,sizeof(video_row));
  memcpy(native_regs,cpu_state.iors,sizeof(native_regs));
  for(i=0U;i<5U;i++)native_io[i]=cpu_state.iors[io[i]];
  for(i=0U;i<16U;i++)native_marks[i]=access_mem[locations[i]];
  /* Metadata-free image for the frozen independent scalar decoder. */
  for(i=0U;i<136U;i++){
   uint32 w=uz_scan_pattern[i];auint id=w&127U;
   if(id==0x11U || id==0x18U || id==0x19U || id==0x2BU)w&=0x7FFFFFFFU;
   if(id==0x20U || id==0x1AU)w&=~0x8000U;
   cpu_code[i]=w;
  }
  for(i=0U;i<32U;i++)cpu_state.iors[i]=(uint8)(i*13U+j*17U);
  cpu_state.iors[20]=3U;cpu_state.iors[14]=7U;cpu_state.iors[25]=(uint8)(4U+j*31U);
  cpu_state.iors[28]=0U;cpu_state.iors[29]=2U;
  cpu_state.iors[CU_IO_DDRC]=j?255U:0x5AU;cpu_state.iors[CU_IO_PORTC]=0x81U;
  cpu_state.iors[CU_IO_SPL]=0xFDU;cpu_state.iors[CU_IO_SPH]=8U;cpu_state.iors[CU_IO_SREG]=j?255U:0U;
  for(i=0U;i<16U;i++){cpu_state.sram[locations[i]]=(uint8)(0xA5U+i*17U);access_mem[locations[i]]=saved_marks[i];}
  cpu_state.sram[259U]=0x3FU;cpu_state.sram[261U]=7U;cpu_state.sram[2302U]=1U;cpu_state.sram[2303U]=1U;
  video_row=saved_row;cpu_state.pc=0U;cpu_state.cycle=0U;video_cycle=0U;cycle_next_event=4096U;event_it=FALSE;event_it_enter=FALSE;
  auint steps=0U;
  while((cpu_state.pc&32767U)<136U && video_cycle<VIDEO_CY_MAX && steps++<4096U)uz_scan_reference_step();
  ok=ok && native_pc==257U && native_cycle>512U && native_cycle<1920U &&
      cpu_state.pc==native_pc && cpu_state.cycle==native_cycle && video_cycle==native_video &&
      memcmp(native_regs,cpu_state.iors,sizeof(native_regs))==0 && hash==uz_scan_hash(&video_row,sizeof(video_row));
  for(i=0U;i<5U;i++)ok=ok && native_io[i]==cpu_state.iors[io[i]];
  for(i=0U;i<16U;i++)ok=ok && native_marks[i]==access_mem[locations[i]];
 }
 memcpy(cpu_code,saved_code,sizeof(saved_code));memcpy(cpu_state.iors,saved_regs,sizeof(saved_regs));
 for(i=0U;i<5U;i++)cpu_state.iors[io[i]]=saved_io[i];
 for(i=0U;i<16U;i++){cpu_state.sram[locations[i]]=saved_ram[i];access_mem[locations[i]]=saved_marks[i];}
 video_row=saved_row;cpu_state.pc=old_pc;cpu_state.cycle=old_cycle;video_cycle=old_video;cycle_next_event=old_event;
 event_it=old_it;event_it_enter=old_enter;cu_avr_pure_ok=old_pure;uz_video_base=old_base;uz_video_valid=old_valid;
 memset(cu_avr_loop_cache,0,sizeof(cu_avr_loop_cache));
 return ok;
}

boole __attribute__((noinline,section(".text.uzebox_scan_selftest"))) uzebox_scan_selftest(void)
{
 if(uz_scan_test_status)return uz_scan_test_status==1U;
 uz_scan_test_status=2U;
 if(cu_avr_loop_disabled)return FALSE;
 cu_avr_loop_stats_t const loop=cu_avr_loop_counters;
 uzebox_span_stats_t const span=uzebox_span_data;
 cu_avr_region_stats_t const region=cu_avr_region_data;
 auint const functions=uz_scan_function_entries,prefixes=uz_scan_prefix_entries,bitmaps=uz_scan_bitmap_entries;
 uz_scan_function_entries=0U;uz_scan_prefix_entries=0U;uz_scan_bitmap_entries=0U;uz_scan_testing=1U;
 boole const ok=uz_scan_bitmap_test() && uz_scan_function_test() &&
  uz_scan_function_entries>=2U && uz_scan_prefix_entries>=2U && uz_scan_bitmap_entries>=2U;
 uz_scan_testing=0U;cu_avr_loop_counters=loop;uzebox_span_data=span;cu_avr_region_data=region;
 uz_scan_function_entries=functions;uz_scan_prefix_entries=prefixes;uz_scan_bitmap_entries=bitmaps;
 if(ok)uz_scan_test_status=1U;
 return ok;
}
