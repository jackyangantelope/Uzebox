/* Isolated semantic-block prototype. GPL-3.0-or-later, original CUzeBox ISA.
 * Pure register blocks only; no event, I/O, SRAM or audio policy changes. */
#ifndef UZEBOX_SEMANTIC_SLOTS
#define UZEBOX_SEMANTIC_SLOTS 16U
#endif
#define UZ_SEM_WORDS 256U
typedef struct {
 _Alignas(4) uint16 code[UZ_SEM_WORDS];
 uint16 pc;uint8 valid,steps,cost,heat;
} uz_sem_block;
static uz_sem_block uz_sem_cache[UZEBOX_SEMANTIC_SLOTS];
static volatile auint uz_sem_mode;
static struct {auint tries,hits,builds,entries,steps,cycles,rejected;} uz_sem_stats;
static void uz_sem_invalidate(void){memset(uz_sem_cache,0,sizeof(uz_sem_cache));}
static void uz_sem_emit_exit(uint16 *o,auint *n,auint delta,auint cycles,auint steps){
 /* r0=context, r4=SREG, r5=original full32 PC; r1-r3 scratch. */
 o[(*n)++]=0x6304U; /* str r4,[r0,#48] */
 o[(*n)++]=(uint16)(0x2100U|(delta&255U));
 o[(*n)++]=(uint16)(0x2200U|((delta>>8)&255U));
 o[(*n)++]=0x0212U; /* lsls r2,r2,#8 */
 o[(*n)++]=0x430AU; /* orrs r2,r1 */
 o[(*n)++]=0x18ADU; /* adds r5,r5,r2 */
 if(delta>>16){o[(*n)++]=(uint16)(0x2100U|((delta>>16)&255U));o[(*n)++]=0x0409U;o[(*n)++]=0x186DU;}
 o[(*n)++]=0x62C5U; /* str r5,[r0,#44] */
 o[(*n)++]=(uint16)(0x2100U|cycles);o[(*n)++]=0x6381U; /* used56 */
 o[(*n)++]=(uint16)(0x2100U|steps);o[(*n)++]=0x63C1U; /* steps60 */
 o[(*n)++]=0xBCF0U;o[(*n)++]=0x4770U;
}
static void __attribute__((noinline)) uz_sem_build(uz_sem_block *b,cu_avr_pure_ctx_t const *c){
 auint n=0U,i=0U,cy=0U,pc=c->pc,flags=c->flags;uint8 regs[32];uint16 *o=b->code;
 memcpy(regs,c->regs,32U);b->valid=1U;b->pc=(uint16)(pc&32767U);b->steps=0U;b->cost=0U;b->heat=4U;uz_sem_stats.builds++;
 o[n++]=0xB4F0U;o[n++]=0x6B04U;o[n++]=0x6AC5U;o[n++]=0x6A46U;
 while(i<32U && n+32U<UZ_SEM_WORDS){
  uint32 op=cpu_code[pc&32767U];auint id=op&127U,a=(op>>8)&255U,d=(op>>16)&65535U;
  if(id==0x11U)d&=32767U;
  if(id==0U){o[n++]=0xBF00U;pc++;cy++;}
  else if(id==0x11U && a<32U && d<32U){
   o[n++]=(uint16)(0x7801U|(d<<6));o[n++]=(uint16)(0x7001U|(a<<6));regs[a]=regs[d];pc++;cy++;
  }else if(id==0x48U && a<32U && d<256U){
   o[n++]=(uint16)(0x2100U|d);o[n++]=(uint16)(0x7001U|(a<<6));regs[a]=(uint8)d;pc++;cy++;
  }else if(id==0x0EU && a<32U && d<32U){
   o[n++]=(uint16)(0x7801U|(a<<6));o[n++]=(uint16)(0x7802U|(d<<6));o[n++]=0x4011U;
   o[n++]=(uint16)(0x7001U|(a<<6));o[n++]=0x22E1U;o[n++]=0x4014U;
   o[n++]=0x2282U;o[n++]=0x0212U;o[n++]=0x1852U;o[n++]=0x5CB2U;o[n++]=0x4314U;
   regs[a]&=regs[d];flags=(flags&0xE1U)|cpu_pflags[CU_AVRFG_LOG+regs[a]];pc++;cy++;
  }else if(id==0x40U){pc=WRAP32(pc+1U+d);cy+=2U;}
  else if((id==0x0AU&&a<32U&&d<32U) || ((id==0x42U||id==0x43U)&&a<256U)){
   auint expected,branch;
   if(id==0x0AU){
    expected=regs[a]==regs[d];o[n++]=(uint16)(0x7801U|(a<<6));o[n++]=(uint16)(0x7802U|(d<<6));o[n++]=0x4291U;
   }else{expected=(flags&a)!=0U;o[n++]=(uint16)(0x2100U|a);o[n++]=0x4221U;}
   branch=n++;uz_sem_emit_exit(o,&n,WRAP32(pc-c->pc),cy,i);
   /* CPSE equality -> Z=1; TST nonzero -> Z=0. Guard exits BEFORE branch. */
   o[branch]=(uint16)(((id==0x0AU?expected:!expected)?0xD000U:0xD100U)|((n-branch-2U)&255U));
   if(id==0x0AU){auint skip=(cpu_code[(pc+1U)&32767U]>>7)&1U;pc=WRAP32(pc+1U+(expected?1U+skip:0U));cy+=expected?2U+skip:1U;}
   else{auint taken=id==0x42U?expected:!expected;pc=WRAP32(pc+1U+(taken?d:0U));cy+=taken?2U:1U;}
  }else break;
  i++;
 }
 if(i<4U)return;
 b->steps=(uint8)i;b->cost=(uint8)(cy+2U);uz_sem_emit_exit(o,&n,WRAP32(pc-c->pc),cy,i);
 if(n>UZ_SEM_WORDS){b->steps=0U;}
 __asm volatile("dsb sy\n\tisb sy" ::: "memory");
}
typedef struct {uint16 pc;uint8 score,valid;} uz_sem_hot_t;
static uz_sem_hot_t uz_sem_hot[64];static uint8 uz_sem_victim[2];
static boole __attribute__((noinline)) uz_sem_run(cu_avr_pure_ctx_t *c,auint remaining){
 auint pc=c->pc&32767U,set=(pc^(pc>>4))&1U,j;uz_sem_block *b=0;
 uz_sem_hot_t *hot=&uz_sem_hot[(pc^(pc>>6))&63U];
 uz_sem_stats.tries++;
 if((uz_sem_stats.tries&4095U)==0U){for(j=0;j<UZEBOX_SEMANTIC_SLOTS;j++)uz_sem_cache[j].heat>>=1;}
 for(j=0U;j<UZEBOX_SEMANTIC_SLOTS/2U;j++){
  uz_sem_block *x=&uz_sem_cache[set*(UZEBOX_SEMANTIC_SLOTS/2U)+j];if(x->valid&&x->pc==pc){b=x;if(b->heat<255U)b->heat++;uz_sem_stats.hits++;break;}
 }
 if(!b){
  if(!hot->valid||hot->pc!=pc){hot->pc=(uint16)pc;hot->valid=1U;hot->score=1U;return FALSE;}
  if(hot->score<255U)hot->score++;if(hot->score<4U)return FALSE;
  b=&uz_sem_cache[set*(UZEBOX_SEMANTIC_SLOTS/2U)];
  for(j=1U;j<UZEBOX_SEMANTIC_SLOTS/2U;j++){uz_sem_block *x=&uz_sem_cache[set*(UZEBOX_SEMANTIC_SLOTS/2U)+j];if(!x->valid||x->heat<b->heat)b=x;}
  if(b->valid && hot->score<=b->heat)return FALSE;
  uz_sem_build(b,c);b->heat=hot->score;
 }
 if(!b->steps||b->steps>remaining||c->budget<b->cost){uz_sem_stats.rejected++;return FALSE;}
 ((void(*)(cu_avr_pure_ctx_t*))((uintptr_t)b->code|1U))(c);
 if(!c->steps){uz_sem_stats.rejected++;return FALSE;}
 uz_sem_stats.entries++;uz_sem_stats.steps+=c->steps;uz_sem_stats.cycles+=c->used;return TRUE;
}
