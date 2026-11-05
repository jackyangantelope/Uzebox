/* GPL-3.0-or-later. Independent byte-loop oracle, including guard bytes. */
boole __attribute__((noinline,section(".text.uzebox_fill_selftest"))) uzebox_fill_selftest(void){
 static uint16 const lengths[]={0,1,2,3,4,5,7,8,11,12,31,32,63,64,127,128,255,256,381,384,511};
 static uint16 const colors[]={0,1,0x55,0xAA,0xFF};
 if(uz_fill_status)return uz_fill_status==1U;
 uz_fill_status=2U;if(cu_avr_loop_disabled)return FALSE;
 uint8 actual[544U],expected[544U];
 auint const old_calls=uz_fill_calls,old_bytes=uz_fill_bytes;auint n,a,c,i;boole ok=TRUE;
 for(n=0U;n<21U;n++)for(a=0U;a<4U;a++)for(c=0U;c<5U;c++){
  memset(actual,0xD3U,sizeof(actual));memset(expected,0xD3U,sizeof(expected));
  for(i=0U;i<lengths[n];i++)expected[16U+a+i]=(uint8)colors[c];
  uz_fill_constant(actual+16U+a,colors[c],lengths[n]);
  ok=ok && memcmp(actual,expected,sizeof(actual))==0;
 }
 uz_fill_calls=old_calls;uz_fill_bytes=old_bytes;
 if(ok)uz_fill_status=1U;
 return ok;
}
void uzebox_fill_stats(uzebox_fill_stats_t *out){
 out->mode=uzebox_fill_control[1];out->selftest=uz_fill_status;out->calls=uz_fill_calls;out->bytes=uz_fill_bytes;
}
