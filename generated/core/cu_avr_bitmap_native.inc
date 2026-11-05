/* Semantic translation of a closed, eight-cell bitmap output loop.
 * Recognition uses decoded instructions and register roles only. The entry
 * may be any of its seven non-incrementing LPMs. Ignored arg2 carries the
 * marker. This permits reentry after an event, avoiding a probe on every
 * PIXEL instruction. Each backedge costs 128 cycles; terminal exit costs
 * 8+16*phase fewer on terminal exit. Every event guard remains intact.
 */
typedef struct {
 uint8 *pixels, *sram, *marks;
 auint x, n, port, pixel, bits, pos, cursor;
 auint dead, living, cursor_color, grid, ddrc, tail_taken, compare_pos;
 auint phase;
} cu_avr_bitmap_ctx_t;

static uint32 cu_avr_bitmap_op(auint start, auint offset)
{
 return cpu_code[(start + offset) & 0x7FFFU];
}

static boole cu_avr_bitmap_is(auint start, auint offset,
                            auint id, auint a, auint d)
{
 uint32 const op = cu_avr_bitmap_op(start, offset);
 /* Reject two-word instructions and require exact operands. Only the LPM
  * head's ignored arg2 may contain our own preclassification metadata. */
 if ((op & 0xFFU) != id || cu_avr_loop_a(op) != a){ return FALSE; }
 return (id == 0x18U || id==0x11U ? (cu_avr_loop_b(op) & 0x7FFFU) : cu_avr_loop_b(op)) == d;
}

static void cu_avr_bitmap_bind_phase(cu_avr_loop_binding_t *b, auint pc, auint phase)
{
 auint const start = (pc - 7U - 12U*phase) & 0x7FFFU;
 auint j, i, used = 1U, role[12U];
 if ((cpu_code[pc] & 0x7FFFU) != 0x18U){ return; }
 b->pixel = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 0U));
 b->stride = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 1U));
 b->work = (uint8)cu_avr_loop_a(cu_avr_bitmap_op(start, 2U));
 b->living = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 3U));
 b->base = (uint8)cu_avr_loop_a(cu_avr_bitmap_op(start, 4U));
 b->cursor = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 4U));
 b->cursor_color = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 6U));
 b->grid = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 9U));
 b->pointer = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 85U));
 b->count = (uint8)cu_avr_loop_a(cu_avr_bitmap_op(start, 94U));
 b->stop = (uint8)cu_avr_loop_b(cu_avr_bitmap_op(start, 96U));
 if (b->pointer != 26U && b->pointer != 28U){ return; }
 role[0]=b->pixel; role[1]=b->stride; role[2]=b->work;
 role[3]=b->living; role[4]=b->base; role[5]=b->cursor;
 role[6]=b->cursor_color; role[7]=b->grid; role[8]=b->pointer;
 role[9]=b->pointer+1U; role[10]=b->count; role[11]=b->stop;
 for (i=0U; i<12U; i++){
  j=role[i];
  if (j >= 32U || (used & ((uint32)1U << j)) != 0U){ return; }
  /* LPM writes r0 and reads invariant Z. Only the read-only cursor may
   * share Z; all other roles are conservatively disjoint from it. */
  if (i != 5U && j >= 30U){ return; }
  used |= (uint32)1U << j;
 }
 for (j=0U; j<7U; j++){
  auint const o=j*12U;
  if (!cu_avr_bitmap_is(start,o+0U,0x49U,0U,b->pixel) ||
      !cu_avr_bitmap_is(start,o+1U,0x11U,b->pixel,b->stride) ||
      !cu_avr_bitmap_is(start,o+2U,0x46U,b->work,1U<<(6U-j)) ||
      !cu_avr_bitmap_is(start,o+3U,0x11U,b->pixel,b->living) ||
      !cu_avr_bitmap_is(start,o+4U,0x0BU,b->base,b->cursor) ||
      !cu_avr_bitmap_is(start,o+5U,0x43U,SREG_ZM,1U) ||
      !cu_avr_bitmap_is(start,o+6U,0x11U,b->pixel,b->cursor_color) ||
      !cu_avr_bitmap_is(start,o+7U,0x18U,0U,0U) ||
      !cu_avr_bitmap_is(start,o+8U,0x40U,0U,0U) ||
      !cu_avr_bitmap_is(start,o+9U,0x49U,0U,b->grid) ||
      !cu_avr_bitmap_is(start,o+10U,0x27U,b->base,0U) ||
      !cu_avr_bitmap_is(start,o+11U,0x40U,0U,0U)){ return; }
 }
 if (!cu_avr_bitmap_is(start,84U,0x49U,0U,b->pixel) ||
     !cu_avr_bitmap_is(start,85U,0x23U,b->work,b->pointer) ||
     !cu_avr_bitmap_is(start,86U,0x11U,b->pixel,b->stride) ||
     !cu_avr_bitmap_is(start,87U,0x46U,b->work,128U) ||
     !cu_avr_bitmap_is(start,88U,0x11U,b->pixel,b->living) ||
     !cu_avr_bitmap_is(start,89U,0x0BU,b->base,b->cursor) ||
     !cu_avr_bitmap_is(start,90U,0x43U,SREG_ZM,1U) ||
     !cu_avr_bitmap_is(start,91U,0x11U,b->pixel,b->cursor_color) ||
     !cu_avr_bitmap_is(start,92U,0x00U,0U,0U) ||
     !cu_avr_bitmap_is(start,93U,0x27U,b->base,0U) ||
     !cu_avr_bitmap_is(start,94U,0x2BU,b->count,0U) ||
     !cu_avr_bitmap_is(start,95U,0x49U,0U,b->grid) ||
     !cu_avr_bitmap_is(start,96U,0x0AU,b->count,b->stop) ||
     !cu_avr_bitmap_is(start,97U,0x40U,0U,0xFF9EU)){ return; }
 b->kind=3U;
 b->phase=(uint8)phase;
 b->branch_delta=0xFF9EU;
 cu_avr_loop_counters.builds++;
}

static void cu_avr_bitmap_bind(cu_avr_loop_binding_t *b, auint pc)
{
 auint phase;
 for(phase=0U;phase<7U;phase++){
  cu_avr_bitmap_bind_phase(b,pc,phase);
  if(b->kind==3U){ return; }
 }
}

static uint8 cu_avr_bitmap_color(cu_avr_bitmap_ctx_t const *c, auint mask)
{
 return (uint8)(c->pos == c->cursor ? c->cursor_color :
                ((c->bits & mask) != 0U ? c->living : c->dead));
}

/* Reference is intentionally byte/cycle oriented. Native output uses
 * constant-size stores below; neither path emits substitute game audio. */
static void cu_avr_bitmap_reference(cu_avr_bitmap_ctx_t *c)
{
 auint i,j,k,pos=0U;
 for (i=0U; i<c->n; i++){
  for (k=0U;k<5U;k++){ c->pixels[pos++]=(uint8)c->port; }
  for (k=0U;k<4U;k++){ c->pixels[pos++]=(uint8)(c->grid & c->ddrc); }
  c->pos=(c->pos+1U)&255U;
  for (j=c->phase+1U;j<8U;j++){
   for (k=0U;k<12U;k++){ c->pixels[pos++]=(uint8)(c->pixel & c->ddrc); }
   if (j==7U){
    c->bits=c->sram[c->x&4095U]; c->marks[c->x&4095U]|=CU_MEM_R;
    c->x=(c->x+1U)&65535U;
   }
   c->compare_pos=c->pos;
   c->pixel=cu_avr_bitmap_color(c,j==7U?128U:1U<<(6U-j));
   c->pos=(c->pos+1U)&255U;
   for (k=0U;k<4U-(j==7U && i+1U==c->n && !c->tail_taken);k++){
    c->pixels[pos++]=(uint8)(c->grid & c->ddrc);
   }
  }
  c->port=c->grid & c->ddrc;
  if (i+1U!=c->n || c->tail_taken){
   for(j=0U;j<=c->phase;j++){
    c->port=c->pixel & c->ddrc;
    for(k=0U;k<(j==c->phase?7U:12U);k++){ c->pixels[pos++]=(uint8)c->port; }
    c->compare_pos=c->pos;
    c->pixel=cu_avr_bitmap_color(c,1U<<(6U-j));
    if(j!=c->phase){
     for(k=0U;k<4U;k++){c->pixels[pos++]=(uint8)(c->grid&c->ddrc);}
     c->pos=(c->pos+1U)&255U;
    }
   }
  }
 }
}

#ifdef CU_AVR_LOOP_NATIVE
static inline void cu_avr_bitmap_store4(uint8 *p, auint c)
{
 p[0]=(uint8)c; p[1]=(uint8)c; p[2]=(uint8)c; p[3]=(uint8)c;
}

/* Pico SDK copies .time_critical.* into SRAM. This is one compiled native
 * closed loop with retained locals, not a C call for each AVR instruction.
 * The ELF audit requires this function in SRAM and actual Thumb tests
 * execute the production function against the frozen scalar interpreter. */
/* GPL-3.0-or-later. Screened phase4 whole bitmap loop; other phases original P. */
static void __attribute__((noinline,section(".time_critical.cu_avr_bitmap_execute")))
uz_phase4_original(cu_avr_bitmap_ctx_t *c)
{
 uint8 *p=c->pixels;
 auint i,j,color,grid=c->grid & c->ddrc;
 for (i=0U;i<c->n;i++){
  cu_avr_bitmap_store4(p,c->port); p[4]=(uint8)c->port;
  cu_avr_bitmap_store4(p+5,grid); p+=9;
  c->pos=(c->pos+1U)&255U;
  for (j=c->phase+1U;j<8U;j++){
   color=c->pixel & c->ddrc;
   cu_avr_bitmap_store4(p,color); cu_avr_bitmap_store4(p+4,color);
   cu_avr_bitmap_store4(p+8,color);
   if (j==7U){
    auint const a=c->x&4095U;
    c->bits=c->sram[a]; c->marks[a]|=CU_MEM_R;
    c->x=(c->x+1U)&65535U;
   }
   c->compare_pos=c->pos;
   c->pixel=cu_avr_bitmap_color(c,j==7U?128U:1U<<(6U-j));
   c->pos=(c->pos+1U)&255U;
   p[12]=(uint8)grid; p[13]=(uint8)grid; p[14]=(uint8)grid;
   if (j!=7U || i+1U!=c->n || c->tail_taken){ p[15]=(uint8)grid; p+=16; }
   else{ p+=15; }
  }
  c->port=grid;
  if (i+1U!=c->n || c->tail_taken){
   for(j=0U;j<=c->phase;j++){
    c->port=c->pixel & c->ddrc;
    cu_avr_bitmap_store4(p,c->port);p[4]=(uint8)c->port;
    p[5]=(uint8)c->port;p[6]=(uint8)c->port;
    c->compare_pos=c->pos;
    c->pixel=cu_avr_bitmap_color(c,1U<<(6U-j));
    if(j!=c->phase){
     p[7]=(uint8)c->port;cu_avr_bitmap_store4(p+8,c->port);
     cu_avr_bitmap_store4(p+12,grid);p+=16;
     c->pos=(c->pos+1U)&255U;
    }else{p+=7;}
   }
  }
 }
}
static inline void uz_phase4_store4(uint8 *p,auint color){uint32 word=(uint32)(uint8)color*0x01010101U;memcpy(p,&word,4U);}
static void __attribute__((noinline,section(".time_critical.uz_phase4_kernel"))) uz_phase4_kernel(cu_avr_bitmap_ctx_t *c){
 uz_phase4_calls++;
 cu_avr_bitmap_ctx_t q=*c;q.phase=4U;
uint8 *p=q.pixels;auint i,color,grid=q.grid&q.ddrc;
for(i=0;i<q.n;i++){
uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;uz_phase4_store4(p+5,grid);p+=9;q.pos=(q.pos+1U)&255U;
switch(q.phase){case 0U:goto Cell1;case 1U:goto Cell2;case 2U:goto Cell3;case 3U:goto Cell4;case 4U:goto Cell5;case 5U:goto Cell6;case 6U:goto Cell7;default:break;}
Cell1:
color=q.pixel&q.ddrc;uz_phase4_store4(p,color);uz_phase4_store4(p+4,color);uz_phase4_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,32U);q.pos=(q.pos+1U)&255U;
uz_phase4_store4(p+12,grid);p+=16;
Cell2:
color=q.pixel&q.ddrc;uz_phase4_store4(p,color);uz_phase4_store4(p+4,color);uz_phase4_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,16U);q.pos=(q.pos+1U)&255U;
uz_phase4_store4(p+12,grid);p+=16;
Cell3:
color=q.pixel&q.ddrc;uz_phase4_store4(p,color);uz_phase4_store4(p+4,color);uz_phase4_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,8U);q.pos=(q.pos+1U)&255U;
uz_phase4_store4(p+12,grid);p+=16;
Cell4:
color=q.pixel&q.ddrc;uz_phase4_store4(p,color);uz_phase4_store4(p+4,color);uz_phase4_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,4U);q.pos=(q.pos+1U)&255U;
uz_phase4_store4(p+12,grid);p+=16;
Cell5:
color=q.pixel&q.ddrc;uz_phase4_store4(p,color);uz_phase4_store4(p+4,color);uz_phase4_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,2U);q.pos=(q.pos+1U)&255U;
uz_phase4_store4(p+12,grid);p+=16;
Cell6:
color=q.pixel&q.ddrc;uz_phase4_store4(p,color);uz_phase4_store4(p+4,color);uz_phase4_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,1U);q.pos=(q.pos+1U)&255U;
uz_phase4_store4(p+12,grid);p+=16;
Cell7:
color=q.pixel&q.ddrc;uz_phase4_store4(p,color);uz_phase4_store4(p+4,color);uz_phase4_store4(p+8,color);
q.bits=q.sram[q.x&4095U];q.marks[q.x&4095U]|=CU_MEM_R;q.x=(q.x+1U)&65535U;
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,128U);q.pos=(q.pos+1U)&255U;
p[12]=(uint8)grid;p[13]=(uint8)grid;p[14]=(uint8)grid;if(i+1U!=q.n || q.tail_taken){p[15]=(uint8)grid;p+=16;}else p+=15;
q.port=grid;
if(i+1U!=q.n || q.tail_taken){
q.port=q.pixel&q.ddrc;uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,64U);
if(q.phase==0U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_phase4_store4(p+8,q.port);uz_phase4_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,32U);
if(q.phase==1U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_phase4_store4(p+8,q.port);uz_phase4_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,16U);
if(q.phase==2U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_phase4_store4(p+8,q.port);uz_phase4_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,8U);
if(q.phase==3U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_phase4_store4(p+8,q.port);uz_phase4_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,4U);
if(q.phase==4U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_phase4_store4(p+8,q.port);uz_phase4_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,2U);
if(q.phase==5U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_phase4_store4(p+8,q.port);uz_phase4_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_phase4_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,1U);
if(q.phase==6U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_phase4_store4(p+8,q.port);uz_phase4_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
}
AfterTail:;
}

*c=q;
}

static void __attribute__((noinline,noclone,section(".time_critical.cu_avr_bitmap_execute"))) cu_avr_bitmap_execute(cu_avr_bitmap_ctx_t *c){
 if(uz_phase4_mode && c->phase==4U)uz_phase4_kernel(c);else uz_phase4_original(c);
}


static boole __attribute__((noinline)) cu_avr_bitmap_selftest(void)
{
 uint8 pixels[260],expected[260],ram[258],marks[258],emarks[258];
 cu_avr_bitmap_ctx_t c,e;
 auint tail,phase;
 for(phase=0U;phase<7U;phase++){
 for (tail=0U;tail<2U;tail++){
  memset(pixels,0xDA,sizeof(pixels)); memset(expected,0xDA,sizeof(expected));
  memset(ram,0,sizeof(ram)); ram[256]=0x3CU;ram[257]=0x81U;
  memset(marks,0x82,sizeof(marks));memset(emarks,0x82,sizeof(emarks));
  memset(&c,0,sizeof(c));
  c.pixels=pixels;c.sram=ram;c.marks=marks;c.x=256U;c.n=2U;
  c.port=0xABU;c.pixel=0x33U;c.bits=0xA5U;c.pos=5U;c.cursor=9U;
  c.dead=0x22U;c.living=0xE7U;c.cursor_color=0x47U;c.grid=0x19U;
  c.ddrc=0x5AU;c.tail_taken=tail;c.phase=phase;
  e=c;e.pixels=expected;e.marks=emarks;
  cu_avr_bitmap_reference(&e);cu_avr_bitmap_execute(&c);
  if (memcmp(pixels,expected,sizeof(pixels)) || memcmp(marks,emarks,sizeof(marks)) ||
      c.x!=e.x || c.port!=e.port || c.pixel!=e.pixel || c.bits!=e.bits ||
      c.pos!=e.pos || c.compare_pos!=e.compare_pos){ return FALSE; }
 }
 }
 return TRUE;
}
#else
#define cu_avr_bitmap_execute cu_avr_bitmap_reference
#endif

static __attribute__((always_inline)) inline boole cu_avr_bitmap_run(cu_avr_loop_binding_t const *b)
{
 cu_avr_bitmap_ctx_t c;
 auint count=cpu_state.iors[b->count],remaining,eb,budget,n,cycles,src,dst,flags;
 remaining=(count-cpu_state.iors[b->stop])&255U;
 if (!remaining){ remaining=256U; }
 eb=WRAP32(cycle_next_event-cpu_state.cycle);
 budget=VIDEO_CY_MAX-video_cycle;
 if (!eb){ cu_avr_loop_counters.event_rejects++; return FALSE; }
 if (budget>=eb){ budget=eb-1U; }
 n=budget/128U;
 if (n>=remaining){ n=remaining; }
 else if (n+1U==remaining && budget>=remaining*128U-8U-16U*b->phase){ n++; }
 if (!n){ cu_avr_loop_counters.event_rejects++; return FALSE; }
 memset(&c,0,sizeof(c));
 c.x=cpu_state.iors[b->pointer]+((auint)cpu_state.iors[b->pointer+1U]<<8);
 if (c.x<256U){ cu_avr_loop_counters.guard_rejects++; return FALSE; }
 if (n>65536U-c.x){ n=65536U-c.x; }
 c.pixels=&video_row.pixels[video_cycle];c.sram=cpu_state.sram;c.marks=access_mem;
 c.n=n;c.tail_taken=n!=remaining;c.port=cpu_state.iors[CU_IO_PORTC];c.phase=b->phase;
 c.pixel=cpu_state.iors[b->pixel];c.bits=cpu_state.iors[b->work];
 c.pos=cpu_state.iors[b->base];c.cursor=cpu_state.iors[b->cursor];
 c.dead=cpu_state.iors[b->stride];c.living=cpu_state.iors[b->living];
 c.cursor_color=cpu_state.iors[b->cursor_color];c.grid=cpu_state.iors[b->grid];
 c.ddrc=cpu_state.iors[CU_IO_DDRC];
 cycles=n*128U-(c.tail_taken?0U:8U+16U*b->phase);
 cu_avr_bitmap_execute(&c);
 cpu_state.iors[0U]=cpu_state.crom[cpu_state.iors[30U]+((auint)cpu_state.iors[31U]<<8)];
 cpu_state.iors[b->pointer]=(uint8)c.x;
 cpu_state.iors[b->pointer+1U]=(uint8)(c.x>>8);
 cpu_state.iors[b->pixel]=(uint8)c.pixel;
 cpu_state.iors[b->work]=(uint8)c.bits;
 cpu_state.iors[b->base]=(uint8)c.pos;
 count=(count-n)&255U;cpu_state.iors[b->count]=(uint8)count;
 cpu_state.iors[CU_IO_PORTC]=(uint8)c.port;
 src=c.cursor;dst=c.compare_pos;
 flags=(cpu_state.iors[CU_IO_SREG]&(SREG_IM|SREG_TM)) |
  cpu_pflags[CU_AVRFG_SUB+((dst-src)&511U)+((src&0x90U)<<5)+((dst&0x90U)<<6)];
 if (!c.tail_taken){
  flags=(flags&(SREG_IM|SREG_TM|SREG_HM|SREG_CM)) | cpu_pflags[CU_AVRFG_DEC+count];
 }
 cpu_state.iors[CU_IO_SREG]=(uint8)flags;
 cpu_state.pc=WRAP32(cpu_state.pc+(n-(c.tail_taken?0U:1U))*65536U+(c.tail_taken?0U:91U-12U*b->phase));
 cpu_state.cycle=WRAP32(cpu_state.cycle+cycles);video_cycle+=cycles;
 cu_avr_loop_counters.entries++;cu_avr_loop_counters.iterations+=n;
 cu_avr_loop_counters.cycles+=cycles;cu_avr_loop_counters.bitmap_entries++;
 cu_avr_loop_counters.bitmap_cycles+=cycles;
 if(b->phase!=0U){cu_avr_loop_counters.bitmap_resumes++;}
 return TRUE;
}
