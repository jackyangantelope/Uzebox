/* GPL-3.0-or-later. Boot-only independent original bitmap reference.
 * Restore SRAM/marks/full row; no guest instruction or peripheral advances. */
static uint32 uz_phase4_hash(void const *data,auint count){
 uint8 const *p=data;uint32 h=2166136261U;
 while(count--)h=(h^*p++)*16777619U;
 return h;
}
boole __attribute__((noinline,section(".text.uzebox_phase4_selftest"))) uzebox_phase4_selftest(void){
 static uint8 const counts[]={0U,1U,2U,8U,11U},ddrs[]={0U,0x5AU,0xFFU};
 if(uz_phase4_status)return uz_phase4_status==1U;
 uz_phase4_status=2U;
 if(cu_avr_loop_disabled)return FALSE;
 cu_row_t const saved_row=video_row;
 cu_avr_bitmap_ctx_t c,e;
 uint8 saved_ram[12U],saved_marks[12U],native_marks[12U];
 auint const saved_calls=uz_phase4_calls;auint i,n,tail,d;boole ok=TRUE;
 for(i=0U;i<12U;i++){saved_ram[i]=cpu_state.sram[512U+i];saved_marks[i]=access_mem[512U+i];cpu_state.sram[512U+i]=(uint8)(0xA5U+17U*i);}
 uz_phase4_calls=0U;uz_phase4_testing=1U;
 for(n=0U;n<5U;n++)for(tail=0U;tail<2U;tail++)for(d=0U;d<3U;d++){
  memset(&c,0,sizeof(c));c.pixels=video_row.pixels;c.sram=cpu_state.sram;c.marks=access_mem;
  c.x=512U;c.n=counts[n];c.port=0xA3U;c.pixel=0x37U;c.bits=0xA5U;c.pos=7U;c.cursor=11U;
  c.dead=0x12U;c.living=0xE7U;c.cursor_color=0xF0U;c.grid=0x81U;c.ddrc=ddrs[d];c.tail_taken=tail;c.phase=4U;
  e=c;video_row=saved_row;
  for(i=0U;i<12U;i++)access_mem[512U+i]=saved_marks[i];
  cu_avr_bitmap_execute(&c);
  uint32 const hash=uz_phase4_hash(&video_row,sizeof(video_row));
  for(i=0U;i<12U;i++)native_marks[i]=access_mem[512U+i];
  video_row=saved_row;
  for(i=0U;i<12U;i++)access_mem[512U+i]=saved_marks[i];
  cu_avr_bitmap_reference(&e);
  ok=ok && memcmp(&c,&e,sizeof(c))==0 && hash==uz_phase4_hash(&video_row,sizeof(video_row));
  for(i=0U;i<12U;i++)ok=ok && native_marks[i]==access_mem[512U+i];
 }
 ok=ok && uz_phase4_calls==30U;
 for(i=0U;i<12U;i++){cpu_state.sram[512U+i]=saved_ram[i];access_mem[512U+i]=saved_marks[i];}
 video_row=saved_row;uz_phase4_calls=saved_calls;uz_phase4_testing=0U;
 if(ok)uz_phase4_status=1U;
 return ok;
}
void uzebox_phase4_stats(uzebox_phase4_stats_t *out){
 out->mode=uzebox_phase4_control[1];out->selftest=uz_phase4_status;out->calls=uz_phase4_calls;
}
