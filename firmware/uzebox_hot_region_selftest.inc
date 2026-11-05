/* GPL-3.0-or-later. Boot-only differential against the unchanged M executor.
 * Code patch, stats and the single tested SRAM byte/mark are restored before
 * returning. No heap, PSRAM, I/O, guest cycles or PCM producer is touched.
 * Failure stays latched across ROM reclassification and AVR reset. */
boole __attribute__((noinline,section(".text.uzebox_hot_region_selftest")))
uzebox_hot_region_selftest(void)
{
 uint32 saved_code[132U];
 cu_avr_pure_ctx_t c,e;
 uzebox_span_stats_t const saved_stats=uzebox_span_data;
 auint const saved_base=uz_aot_base,saved_valid=uz_aot_valid;
 auint const saved_entries=uz_aot_entries,saved_instructions=uz_aot_instructions,saved_cycles=uz_aot_cycles;
 uint8 const saved_ram=cpu_state.sram[256U],saved_mark=access_mem[256U];
 static uint8 const offsets[]={6U,8U,63U,70U,74U,77U,81U,85U,89U,93U,107U,109U,112U,116U,121U};
 auint i,j,executed=0U;
 boole ok=TRUE;
 if(uz_aot_test_status)return uz_aot_test_status==1U;
 uz_aot_test_status=2U; /* Fail closed until every vector passes. */
 if(!pflags_done){cu_avrfg_fill(cpu_pflags);pflags_done=TRUE;}
 memcpy(saved_code,cpu_code,sizeof(saved_code));
 memcpy(cpu_code,uz_aot_pattern,sizeof(saved_code));uz_aot_base=0U;
 for(i=0U;i<30U;i++){
  memset(&c,0,sizeof(c));c.code=cpu_code;c.pflags=cpu_pflags;c.types=cu_avr_pure_types;
  for(j=0U;j<32U;j++)c.regs[j]=(uint8)(j*17U+i*13U);
  c.regs[16]=0U;c.regs[17]=1U;c.regs[26]=0U;c.regs[27]=1U;
  c.regs[28]=4U;c.regs[29]=1U;c.regs[30]=8U;c.regs[31]=1U;
  c.pc=offsets[i%15U];c.flags=i<15U?0U:255U;c.budget=c.pc==121U?10U:12U;
  e=c;cpu_state.sram[256U]=saved_ram;access_mem[256U]=saved_mark;
  uz_aot_execute(&c);
  uint8 const native_ram=cpu_state.sram[256U],native_mark=access_mem[256U];
  cpu_state.sram[256U]=saved_ram;access_mem[256U]=saved_mark;
  /* The region may stop earlier than its budget. Match precisely the
   * consumed cycles, leaving the reference's mandatory two-cycle margin. */
  e.budget=c.used+2U;cu_avr_span_execute(&e);
  ok=ok && c.pc==e.pc && c.flags==e.flags && c.used==e.used && c.steps==e.steps &&
     memcmp(c.regs,e.regs,sizeof(c.regs))==0 && native_ram==cpu_state.sram[256U] && native_mark==access_mem[256U];
  if(c.steps)executed++;
  if(offsets[i%15U]==121U)ok=ok && c.steps>=6U;
 }
 memcpy(cpu_code,saved_code,sizeof(saved_code));
 cpu_state.sram[256U]=saved_ram;access_mem[256U]=saved_mark;
 uz_aot_base=saved_base;uz_aot_valid=saved_valid;uzebox_span_data=saved_stats;
 uz_aot_entries=saved_entries;uz_aot_instructions=saved_instructions;uz_aot_cycles=saved_cycles;
 ok=ok && executed>=20U;
 if(ok)uz_aot_test_status=1U;
 return ok;
}
