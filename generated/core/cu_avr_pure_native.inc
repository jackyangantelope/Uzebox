/* Guarded register-only execution regions, selected at qualified MOV heads.
 * A temporary register image and local flags/PC/cycle budget avoid a call
 * through the complete hardware decoder for every arithmetic instruction.
 * Unsupported paths stop, and short paths are discarded without effects.
 * All I/O, SRAM, ROM loads, IRQ enable changes and events use scalar code.
 */
static uint8 const cu_avr_pure_types[128U]={
 [0x00U]=6U,[0x01U]=5U,[0x09U]=2U,[0x0AU]=2U,[0x0BU]=2U,
 [0x0CU]=2U,[0x0DU]=2U,[0x0EU]=2U,[0x0FU]=2U,[0x10U]=2U,
 [0x11U]=2U,[0x12U]=3U,[0x14U]=3U,[0x15U]=3U,[0x16U]=3U,
 [0x24U]=1U,[0x27U]=1U,[0x29U]=1U,[0x2AU]=1U,[0x2BU]=1U,
 [0x40U]=6U,[0x42U]=4U,[0x43U]=4U,[0x46U]=3U,[0x47U]=3U,[0x48U]=3U
};

typedef struct {
 uint8 regs[32U];
 uint32 const *code;
 uint8 const *pflags,*types;
 auint pc,flags,budget,used,steps;
} cu_avr_pure_ctx_t;
static boole cu_avr_pure_ok;

static boole cu_avr_pure_supported(uint32 op)
{
 auint const id=op&127U,type=cu_avr_pure_types[id];
 auint const a=(op>>8)&255U,b=(op>>16)&(id==0x11U?32767U:65535U);
 if(type==6U){return TRUE;}
 if(type==4U){return a<=255U;}
 if(!type || a>=32U){return FALSE;}
 if(type==2U){return b<32U;}
 if(type==3U){return b<=255U;}
 if(type==5U){return a<31U && b<31U;}
 return TRUE;
}

static boole cu_avr_pure_qualify(auint pc)
{
 auint i;
 for(i=0U;i<12U;i++){
  if(!cu_avr_pure_supported(cpu_code[(pc+i)&32767U])){return FALSE;}
 }
 return TRUE;
}

#ifdef CU_AVR_LOOP_NATIVE
#define CU_AVR_PURE_RAM __attribute__((noinline,section(".time_critical.cu_avr_pure_execute")))
#else
#define CU_AVR_PURE_RAM
#endif
static void CU_AVR_PURE_RAM cu_avr_pure_execute(cu_avr_pure_ctx_t *c)
{
 uint8 *r=c->regs;
 uint8 const *pf=c->pflags;
 uint32 const *code=c->code;
 auint pc=c->pc,f=c->flags,budget=c->budget,used=0U,steps=0U;
 while(budget>=3U && steps<128U){
  uint32 const op=code[pc&32767U];
  auint const id=op&127U;
  auint const a=(op>>8)&255U;
  auint b=(op>>16)&65535U,d,s,v,cy=1U;
  if(id==0x11U){b&=32767U;}
  /* Operand bounds are validated over the entire compiled ROM at every
   * classification. Only supported cases below execute; unsupported paths
   * roll the fetch back. No native instruction can modify compiled code. */
  pc=WRAP32(pc+1U);
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
   default:pc=WRAP32(pc-1U);goto done;
  }
  used+=cy;budget-=cy;steps++;
 }
done:
 c->pc=pc;c->flags=f;c->used=used;c->steps=steps;
}
#undef CU_AVR_PURE_RAM

#ifdef CU_AVR_LOOP_NATIVE
static boole __attribute__((noinline)) cu_avr_pure_selftest(void)
{
 uint32 const program[9U]={0x00FA0248U,0x000A0348U,0x00030209U,
  0x00020411U,0x00000429U,0x0003030EU,0x00000327U,0x000B0312U,0x20U};
 cu_avr_pure_ctx_t c;
 memset(&c,0,sizeof(c));c.code=program;c.pflags=cpu_pflags;c.types=cu_avr_pure_types;
 c.budget=32U;c.flags=0xA5U;
 cu_avr_pure_execute(&c);
 return c.pc==8U && c.steps==8U && c.used==8U && c.regs[2U]==4U &&
        c.regs[3U]==11U && c.regs[4U]==2U && c.flags==((0xA5U&(SREG_IM|SREG_TM))|SREG_ZM);
}
#endif


typedef struct { auint entries,instructions,cycles,max_steps,budget_exits,barrier_exits,cap_exits; } uzebox_span_stats_t;
static uzebox_span_stats_t uzebox_span_data;
void uzebox_span_stats(uzebox_span_stats_t *out){*out=uzebox_span_data;}
const volatile uint32 uzebox_span_control[2]
 __attribute__((section(".rodata.uzebox_span"),used))={0x4d313256U,1U};

/* Rare SRAM operations stay out of the hot register decoder. Returning
 * zero is side-effect-free; other results give the consumed packed words. */
static auint __attribute__((noinline,noclone,section(".text.uzebox_span_memory")))
cu_avr_span_memory(uint8 *r,uint32 op)
{
 auint const id=op&127U,b=(op>>16)&65535U;
 auint a=(op>>8)&255U,d,v;
 switch(id){
   case 0x1CU:case 0x20U: /* STS / LDS, including LDS metadata */
    if(id==0x20U){a&=127U;}
    if(a>=32U || b<256U){return 0U;}
    v=b;goto span_memory;
   case 0x1DU:case 0x21U: /* ST / LD with displacement */
    d=b&255U;
    if(a>=32U || d>=31U){return 0U;}
    v=(r[d]+((auint)r[d+1U]<<8)+(b>>8))&65535U;
    if(v<256U){return 0U;}
    goto span_memory;
   case 0x1EU:case 0x1FU:case 0x22U:case 0x23U: /* pre/post ST / LD */
    if(a>=32U || b>=31U){return 0U;}
    v=r[b]+((auint)r[b+1U]<<8);
    if(id==0x1EU || id==0x22U){v=WRAP32(v-1U);}
    if(v<256U){return 0U;}
    /* Original pre-decrement at zero uses unsigned 0xffffffff, then
     * mirrored SRAM. Pointer updates precede aliased source/dest use. */
    d=(id==0x1FU || id==0x23U)?WRAP32(v+1U):v;
    r[b]=(uint8)d;r[b+1U]=(uint8)(d>>8);
span_memory:
    if(id<=0x1FU){cpu_state.sram[v&4095U]=r[a];access_mem[v&4095U]|=CU_MEM_W;}
    else{r[a]=cpu_state.sram[v&4095U];access_mem[v&4095U]|=CU_MEM_R;}
    return (id==0x1CU || id==0x20U)?2U:1U;

 default:return 0U;
 }
}
static void __attribute__((noinline,section(".time_critical.cu_avr_span_execute"))) cu_avr_span_execute(cu_avr_pure_ctx_t *restrict c)
{
 uint8 *r=c->regs;
 uint8 const *pf=c->pflags;
 uint32 const *code=c->code;
 auint pc=c->pc,f=c->flags,budget=c->budget,used=0U,steps=0U;
 while(budget>=3U && steps<1024U){
  uint32 const op=code[pc&32767U];
  auint const id=op&127U;
  auint const a=(op>>8)&255U;
  auint b=(op>>16)&65535U,d,s,v,cy=1U;
  if(id==0x11U){b&=32767U;}
  /* Operand bounds are validated over the entire compiled ROM at every
   * classification. Only supported cases below execute; unsupported paths
   * roll the fetch back. No native instruction can modify compiled code. */
  pc=WRAP32(pc+1U);
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:
span_barrier:pc=WRAP32(pc-1U);goto done;
  }
  used+=cy;budget-=cy;steps++;
 }
done:
 c->pc=pc;c->flags=f;c->used=used;c->steps=steps;
 if(steps){
  uzebox_span_data.entries++;uzebox_span_data.instructions+=steps;uzebox_span_data.cycles+=used;
  if(steps>uzebox_span_data.max_steps){uzebox_span_data.max_steps=steps;}
  if(budget<3U){uzebox_span_data.budget_exits++;}
  else if(steps==1024U){uzebox_span_data.cap_exits++;}
  else{uzebox_span_data.barrier_exits++;}
 }
}

#include "uzebox_hot_region.h"
static auint uz_aot_base,uz_aot_valid,uz_aot_test_status;
static auint uz_aot_entries,uz_aot_instructions,uz_aot_cycles;
const volatile uint32_t uzebox_hot_region_control[2]
 __attribute__((section(".rodata.uzebox_hot_region"),used))={0x50313456U,1U};

/* GPL-3.0-or-later. Frozen132-node safe CFG, from the independently tested
 * 2026-10-28 offline generator. All operand/branch semantics from CUzeBox.
 * Must pass startup selftest and whole compiled-pattern validation. */
static uint32 const uz_aot_pattern[132U]={0x00141001U,0x00f51414U,0x00ff1513U,0x00010b11U,0x00801648U,0x00001748U,0x00130911U,0x0016090eU,0x00160a11U,0x00000a29U,0x00010b0aU,0x00110040U,0x000c1811U,0x00011816U,0x00010846U,0x00ff1814U,0x00010546U,0x00ff1814U,0x00801946U,0x00ff1814U,0x00801246U,0x00ff1814U,0x00401946U,0x00ff1814U,0x00401346U,0x00ff1814U,0x00401247U,0x004f0040U,0x00430040U,0x00071848U,0x00180b0aU,0x001f0040U,0x00011945U,0x0018180fU,0x00001844U,0x00021346U,0x00ff1814U,0x00021246U,0x00ff1814U,0x00011946U,0x00ff1814U,0x00011246U,0x00ff1814U,0x001a0c01U,0x00ff1e48U,0x001e0c0cU,0x001e0d08U,0x001a0721U,0x001c1a01U,0x00011a3aU,0x001c0621U,0x00021c01U,0x00011c3aU,0x00021e01U,0x001e0821U,0x00800746U,0x00ff1814U,0x00800646U,0x00ff1814U,0x00800847U,0x00250040U,0x00ff1814U,0x00230040U,0x00160d11U,0x000d0d09U,0x00190711U,0x000d070eU,0x00011848U,0x00010243U,0x00001848U,0x00130711U,0x000d070eU,0x00010242U,0x00ff1814U,0x00120d0eU,0x00010242U,0x00ff1814U,0x00160d11U,0x00190d0eU,0x00010242U,0x00ff1814U,0x00160d11U,0x00120d0eU,0x00010242U,0x00ff1814U,0x00190d11U,0x000a0d0eU,0x00010242U,0x00ff1814U,0x00130d11U,0x000a0d0eU,0x00010242U,0x00ff1814U,0x00120d11U,0x000a0d0eU,0x000b0242U,0x00ff1814U,0x00090040U,0x001c0201U,0x001a1c01U,0x000c1a01U,0x00120511U,0x00081211U,0x00130811U,0x00061311U,0x00190c11U,0x00071911U,0x0001090aU,0x00030040U,0x00031812U,0x00050243U,0x00030040U,0x00021814U,0x00021812U,0x00010143U,0x00161710U,0x000a1611U,0x00000b27U,0x00081f48U,0x001f0b0aU,0xff8d0040U,0x00101e01U,0x001e171fU,0x001e1001U,0x001e140bU,0x001f1507U,0x00010242U,0xff830040U,0x00140e0bU,0x00150f07U,0x00010242U,0xff7c0040U};
static void __attribute__((noinline,section(".time_critical.uz_aot_execute"))) uz_aot_execute(cu_avr_pure_ctx_t *restrict c){
uint8 *r=c->regs;uint8 const *pf=c->pflags;uint32 const *code=c->code;
auint pc=c->pc,f=c->flags,budget=c->budget,used=0,steps=0,d,s,v;
switch((pc&32767U)-uz_aot_base){
case 0U:goto L2eed;
case 3U:goto L2ef0;
case 6U:goto L2ef3;
case 8U:goto L2ef5;
case 11U:goto L2ef8;
case 12U:goto L2ef9;
case 15U:goto L2efc;
case 16U:goto L2efd;
case 17U:goto L2efe;
case 18U:goto L2eff;
case 19U:goto L2f00;
case 20U:goto L2f01;
case 21U:goto L2f02;
case 22U:goto L2f03;
case 23U:goto L2f04;
case 24U:goto L2f05;
case 25U:goto L2f06;
case 26U:goto L2f07;
case 27U:goto L2f08;
case 28U:goto L2f09;
case 29U:goto L2f0a;
case 31U:goto L2f0c;
case 32U:goto L2f0d;
case 36U:goto L2f11;
case 37U:goto L2f12;
case 38U:goto L2f13;
case 39U:goto L2f14;
case 40U:goto L2f15;
case 41U:goto L2f16;
case 42U:goto L2f17;
case 43U:goto L2f18;
case 56U:goto L2f25;
case 57U:goto L2f26;
case 58U:goto L2f27;
case 59U:goto L2f28;
case 60U:goto L2f29;
case 61U:goto L2f2a;
case 63U:goto L2f2c;
case 69U:goto L2f32;
case 70U:goto L2f33;
case 73U:goto L2f36;
case 74U:goto L2f37;
case 76U:goto L2f39;
case 77U:goto L2f3a;
case 80U:goto L2f3d;
case 81U:goto L2f3e;
case 84U:goto L2f41;
case 85U:goto L2f42;
case 88U:goto L2f45;
case 89U:goto L2f46;
case 92U:goto L2f49;
case 93U:goto L2f4a;
case 96U:goto L2f4d;
case 98U:goto L2f4f;
case 107U:goto L2f58;
case 108U:goto L2f59;
case 109U:goto L2f5a;
case 111U:goto L2f5c;
case 112U:goto L2f5d;
case 115U:goto L2f60;
case 116U:goto L2f61;
case 120U:goto L2f65;
case 121U:goto L2f66;
case 127U:goto L2f6c;
case 128U:goto L2f6d;
case 131U:goto L2f70;
default:cu_avr_span_execute(c);return;
}

L2eed:if(budget<5U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00141001U;auint const id=1U,a=16U;auint b=20U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2eee;}

L2eee:
pc=WRAP32(pc+1U);
{uint32 const op=0x00f51414U;auint const id=20U,a=20U;auint b=245U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2eef;}

L2eef:
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1513U;auint const id=19U,a=21U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef0;}

L2ef0:if(budget<5U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00010b11U;auint const id=17U,a=11U;auint b=1U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef1;}

L2ef1:
pc=WRAP32(pc+1U);
{uint32 const op=0x00801648U;auint const id=72U,a=22U;auint b=128U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef2;}

L2ef2:
pc=WRAP32(pc+1U);
{uint32 const op=0x00001748U;auint const id=72U,a=23U;auint b=0U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef3;}

L2ef3:if(budget<4U || steps>1022U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00130911U;auint const id=17U,a=9U;auint b=19U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef4;}

L2ef4:
pc=WRAP32(pc+1U);
{uint32 const op=0x0016090eU;auint const id=14U,a=9U;auint b=22U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef5;}

L2ef5:if(budget<7U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00160a11U;auint const id=17U,a=10U;auint b=22U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef6;}

L2ef6:
pc=WRAP32(pc+1U);
{uint32 const op=0x00000a29U;auint const id=41U,a=10U;auint b=0U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2ef7;}

L2ef7:
pc=WRAP32(pc+1U);
if(r[11]==r[1]){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2ef9;}
used+=1U;budget-=1U;steps++;goto L2ef8;
L2ef8:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+17U);used+=2U;budget-=2U;steps++;goto L2f0a;
L2ef9:if(budget<7U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x000c1811U;auint const id=17U,a=24U;auint b=12U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2efa;}

L2efa:
pc=WRAP32(pc+1U);
{uint32 const op=0x00011816U;auint const id=22U,a=24U;auint b=1U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2efb;}

L2efb:
pc=WRAP32(pc+1U);
if((r[8]&1U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2efd;}
used+=1U;budget-=1U;steps++;goto L2efc;
L2efc:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2efd;}

L2efd:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[5]&1U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2eff;}
used+=1U;budget-=1U;steps++;goto L2efe;
L2efe:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2eff;}

L2eff:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[25]&128U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f01;}
used+=1U;budget-=1U;steps++;goto L2f00;
L2f00:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f01;}

L2f01:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[18]&128U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f03;}
used+=1U;budget-=1U;steps++;goto L2f02;
L2f02:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f03;}

L2f03:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[25]&64U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f05;}
used+=1U;budget-=1U;steps++;goto L2f04;
L2f04:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f05;}

L2f05:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[19]&64U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f07;}
used+=1U;budget-=1U;steps++;goto L2f06;
L2f06:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f07;}

L2f07:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[18]&64U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f09;}
used+=1U;budget-=1U;steps++;goto L2f08;
L2f08:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+79U);used+=2U;budget-=2U;steps++;goto L2f58;
L2f09:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+67U);used+=2U;budget-=2U;steps++;goto L2f4d;
L2f0a:if(budget<6U || steps>1022U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00071848U;auint const id=72U,a=24U;auint b=7U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f0b;}

L2f0b:
pc=WRAP32(pc+1U);
if(r[11]==r[24]){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f0d;}
used+=1U;budget-=1U;steps++;goto L2f0c;
L2f0c:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+31U);used+=2U;budget-=2U;steps++;goto L2f2c;
L2f0d:if(budget<8U || steps>1020U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00011945U;auint const id=69U,a=25U;auint b=1U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f0e;}

L2f0e:
pc=WRAP32(pc+1U);
{uint32 const op=0x0018180fU;auint const id=15U,a=24U;auint b=24U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f0f;}

L2f0f:
pc=WRAP32(pc+1U);
{uint32 const op=0x00001844U;auint const id=68U,a=24U;auint b=0U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f10;}

L2f10:
pc=WRAP32(pc+1U);
if((r[19]&2U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f12;}
used+=1U;budget-=1U;steps++;goto L2f11;
L2f11:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f12;}

L2f12:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[18]&2U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f14;}
used+=1U;budget-=1U;steps++;goto L2f13;
L2f13:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f14;}

L2f14:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[25]&1U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f16;}
used+=1U;budget-=1U;steps++;goto L2f15;
L2f15:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f16;}

L2f16:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[18]&1U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f18;}
used+=1U;budget-=1U;steps++;goto L2f17;
L2f17:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f18;}

L2f18:if(budget<22U || steps>1011U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x001a0c01U;auint const id=1U,a=12U;auint b=26U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f19;}

L2f19:
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1e48U;auint const id=72U,a=30U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f1a;}

L2f1a:
pc=WRAP32(pc+1U);
{uint32 const op=0x001e0c0cU;auint const id=12U,a=12U;auint b=30U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f1b;}

L2f1b:
pc=WRAP32(pc+1U);
{uint32 const op=0x001e0d08U;auint const id=8U,a=13U;auint b=30U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f1c;}

L2f1c:
pc=WRAP32(pc+1U);
{uint32 const op=0x001a0721U;auint const id=33U,a=7U;auint b=26U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f1d;}

L2f1d:
pc=WRAP32(pc+1U);
{uint32 const op=0x001c1a01U;auint const id=1U,a=26U;auint b=28U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f1e;}

L2f1e:
pc=WRAP32(pc+1U);
{uint32 const op=0x00011a3aU;auint const id=58U,a=26U;auint b=1U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f1f;}

L2f1f:
pc=WRAP32(pc+1U);
{uint32 const op=0x001c0621U;auint const id=33U,a=6U;auint b=28U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f20;}

L2f20:
pc=WRAP32(pc+1U);
{uint32 const op=0x00021c01U;auint const id=1U,a=28U;auint b=2U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f21;}

L2f21:
pc=WRAP32(pc+1U);
{uint32 const op=0x00011c3aU;auint const id=58U,a=28U;auint b=1U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f22;}

L2f22:
pc=WRAP32(pc+1U);
{uint32 const op=0x00021e01U;auint const id=1U,a=30U;auint b=2U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f23;}

L2f23:
pc=WRAP32(pc+1U);
{uint32 const op=0x001e0821U;auint const id=33U,a=8U;auint b=30U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f24;}

L2f24:
pc=WRAP32(pc+1U);
if((r[7]&128U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f26;}
used+=1U;budget-=1U;steps++;goto L2f25;
L2f25:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f26;}

L2f26:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[6]&128U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f28;}
used+=1U;budget-=1U;steps++;goto L2f27;
L2f27:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f28;}

L2f28:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if((r[8]&128U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f2a;}
used+=1U;budget-=1U;steps++;goto L2f29;
L2f29:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+37U);used+=2U;budget-=2U;steps++;goto L2f4f;
L2f2a:if(budget<5U || steps>1022U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f2b;}

L2f2b:
pc=WRAP32(pc+1U);
pc=WRAP32(pc+35U);used+=2U;budget-=2U;steps++;goto L2f4f;
L2f2c:if(budget<9U || steps>1018U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00160d11U;auint const id=17U,a=13U;auint b=22U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f2d;}

L2f2d:
pc=WRAP32(pc+1U);
{uint32 const op=0x000d0d09U;auint const id=9U,a=13U;auint b=13U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f2e;}

L2f2e:
pc=WRAP32(pc+1U);
{uint32 const op=0x00190711U;auint const id=17U,a=7U;auint b=25U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f2f;}

L2f2f:
pc=WRAP32(pc+1U);
{uint32 const op=0x000d070eU;auint const id=14U,a=7U;auint b=13U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f30;}

L2f30:
pc=WRAP32(pc+1U);
{uint32 const op=0x00011848U;auint const id=72U,a=24U;auint b=1U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f31;}

L2f31:
pc=WRAP32(pc+1U);
if((f&2U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f33;}
used+=1U;budget-=1U;steps++;goto L2f32;
L2f32:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00001848U;auint const id=72U,a=24U;auint b=0U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f33;}

L2f33:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00130711U;auint const id=17U,a=7U;auint b=19U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f34;}

L2f34:
pc=WRAP32(pc+1U);
{uint32 const op=0x000d070eU;auint const id=14U,a=7U;auint b=13U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f35;}

L2f35:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f37;}
used+=1U;budget-=1U;steps++;goto L2f36;
L2f36:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f37;}

L2f37:if(budget<5U || steps>1022U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00120d0eU;auint const id=14U,a=13U;auint b=18U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f38;}

L2f38:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f3a;}
used+=1U;budget-=1U;steps++;goto L2f39;
L2f39:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f3a;}

L2f3a:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00160d11U;auint const id=17U,a=13U;auint b=22U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f3b;}

L2f3b:
pc=WRAP32(pc+1U);
{uint32 const op=0x00190d0eU;auint const id=14U,a=13U;auint b=25U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f3c;}

L2f3c:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f3e;}
used+=1U;budget-=1U;steps++;goto L2f3d;
L2f3d:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f3e;}

L2f3e:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00160d11U;auint const id=17U,a=13U;auint b=22U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f3f;}

L2f3f:
pc=WRAP32(pc+1U);
{uint32 const op=0x00120d0eU;auint const id=14U,a=13U;auint b=18U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f40;}

L2f40:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f42;}
used+=1U;budget-=1U;steps++;goto L2f41;
L2f41:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f42;}

L2f42:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00190d11U;auint const id=17U,a=13U;auint b=25U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f43;}

L2f43:
pc=WRAP32(pc+1U);
{uint32 const op=0x000a0d0eU;auint const id=14U,a=13U;auint b=10U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f44;}

L2f44:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f46;}
used+=1U;budget-=1U;steps++;goto L2f45;
L2f45:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f46;}

L2f46:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00130d11U;auint const id=17U,a=13U;auint b=19U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f47;}

L2f47:
pc=WRAP32(pc+1U);
{uint32 const op=0x000a0d0eU;auint const id=14U,a=13U;auint b=10U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f48;}

L2f48:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f4a;}
used+=1U;budget-=1U;steps++;goto L2f49;
L2f49:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f4a;}

L2f4a:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00120d11U;auint const id=17U,a=13U;auint b=18U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f4b;}

L2f4b:
pc=WRAP32(pc+1U);
{uint32 const op=0x000a0d0eU;auint const id=14U,a=13U;auint b=10U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f4c;}

L2f4c:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+11U);used+=2U;budget-=2U;steps++;goto L2f58;}
used+=1U;budget-=1U;steps++;goto L2f4d;
L2f4d:if(budget<5U || steps>1022U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1814U;auint const id=20U,a=24U;auint b=255U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f4e;}

L2f4e:
pc=WRAP32(pc+1U);
pc=WRAP32(pc+9U);used+=2U;budget-=2U;steps++;goto L2f58;
L2f4f:if(budget<11U || steps>1015U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x001c0201U;auint const id=1U,a=2U;auint b=28U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f50;}

L2f50:
pc=WRAP32(pc+1U);
{uint32 const op=0x001a1c01U;auint const id=1U,a=28U;auint b=26U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f51;}

L2f51:
pc=WRAP32(pc+1U);
{uint32 const op=0x000c1a01U;auint const id=1U,a=26U;auint b=12U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f52;}

L2f52:
pc=WRAP32(pc+1U);
{uint32 const op=0x00120511U;auint const id=17U,a=5U;auint b=18U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f53;}

L2f53:
pc=WRAP32(pc+1U);
{uint32 const op=0x00081211U;auint const id=17U,a=18U;auint b=8U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f54;}

L2f54:
pc=WRAP32(pc+1U);
{uint32 const op=0x00130811U;auint const id=17U,a=8U;auint b=19U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f55;}

L2f55:
pc=WRAP32(pc+1U);
{uint32 const op=0x00061311U;auint const id=17U,a=19U;auint b=6U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f56;}

L2f56:
pc=WRAP32(pc+1U);
{uint32 const op=0x00190c11U;auint const id=17U,a=12U;auint b=25U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f57;}

L2f57:
pc=WRAP32(pc+1U);
{uint32 const op=0x00071911U;auint const id=17U,a=25U;auint b=7U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f58;}

L2f58:if(budget<5U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
if(r[9]==r[1]){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f5a;}
used+=1U;budget-=1U;steps++;goto L2f59;
L2f59:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+3U);used+=2U;budget-=2U;steps++;goto L2f5d;
L2f5a:if(budget<5U || steps>1022U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00031812U;auint const id=18U,a=24U;auint b=3U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f5b;}

L2f5b:
pc=WRAP32(pc+1U);
if((f&2U)==0U){pc=WRAP32(pc+5U);used+=2U;budget-=2U;steps++;goto L2f61;}
used+=1U;budget-=1U;steps++;goto L2f5c;
L2f5c:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+3U);used+=2U;budget-=2U;steps++;goto L2f60;
L2f5d:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00021814U;auint const id=20U,a=24U;auint b=2U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f5e;}

L2f5e:
pc=WRAP32(pc+1U);
{uint32 const op=0x00021812U;auint const id=18U,a=24U;auint b=2U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f5f;}

L2f5f:
pc=WRAP32(pc+1U);
if((f&1U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f61;}
used+=1U;budget-=1U;steps++;goto L2f60;
L2f60:if(budget<3U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00161710U;auint const id=16U,a=23U;auint b=22U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f61;}

L2f61:if(budget<8U || steps>1020U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x000a1611U;auint const id=17U,a=22U;auint b=10U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f62;}

L2f62:
pc=WRAP32(pc+1U);
{uint32 const op=0x00000b27U;auint const id=39U,a=11U;auint b=0U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f63;}

L2f63:
pc=WRAP32(pc+1U);
{uint32 const op=0x00081f48U;auint const id=72U,a=31U;auint b=8U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f64;}

L2f64:
pc=WRAP32(pc+1U);
if(r[11]==r[31]){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f66;}
used+=1U;budget-=1U;steps++;goto L2f65;
L2f65:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+65421U);used+=2U;budget-=2U;steps++;goto L2ef3;
L2f66:if(budget<10U || steps>1018U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00101e01U;auint const id=1U,a=30U;auint b=16U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f67;}

L2f67:
pc=WRAP32(pc+1U);
{uint32 const op=0x001e171fU;auint const id=31U,a=23U;auint b=30U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f68;}

L2f68:
pc=WRAP32(pc+1U);
{uint32 const op=0x001e1001U;auint const id=1U,a=16U;auint b=30U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f69;}

L2f69:
pc=WRAP32(pc+1U);
{uint32 const op=0x001e140bU;auint const id=11U,a=20U;auint b=30U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f6a;}

L2f6a:
pc=WRAP32(pc+1U);
{uint32 const op=0x001f1507U;auint const id=7U,a=21U;auint b=31U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f6b;}

L2f6b:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto L2f6d;}
used+=1U;budget-=1U;steps++;goto L2f6c;
L2f6c:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+65411U);used+=2U;budget-=2U;steps++;goto L2ef0;
L2f6d:if(budget<6U || steps>1021U)goto done;
pc=WRAP32(pc+1U);
{uint32 const op=0x00140e0bU;auint const id=11U,a=14U;auint b=20U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f6e;}

L2f6e:
pc=WRAP32(pc+1U);
{uint32 const op=0x00150f07U;auint const id=7U,a=15U;auint b=21U,cy=1U;
  switch(id){
   case 0x00U:break;
   case 0x01U:r[a]=r[b];r[a+1U]=r[b+1U];break;
   case 0x09U:case 0x0DU:
    d=r[a];s=r[b];v=d+s+(id==0x0DU?(f&SREG_CM):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_ADD+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0BU:case 0x0CU:case 0x12U:case 0x14U:
    d=r[a];s=(id==0x12U||id==0x14U)?b:r[b];v=d-s;
    if(id==0x0CU||id==0x14U){r[a]=(uint8)v;}
    f=(f&(SREG_IM|SREG_TM))|pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)];break;
   case 0x0EU:case 0x0FU:case 0x10U:case 0x15U:case 0x16U:case 0x24U:
    d=r[a];s=(id==0x15U||id==0x16U)?b:r[b&31U];
    if(id==0x0EU||id==0x16U){v=d&s;}
    else if(id==0x0FU){v=d^s;}
    else if(id==0x24U){v=d^255U;}
    else{v=d|s;}
    r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
    if(id==0x24U){f|=SREG_CM;}break;
   case 0x11U:r[a]=r[b];break;
   case 0x27U:case 0x2BU:
    v=(r[a]+(id==0x27U?1U:255U))&255U;r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
      pf[(id==0x27U?CU_AVRFG_INC:CU_AVRFG_DEC)+v];break;
   case 0x29U:case 0x2AU:
    s=r[a];v=(s>>1)|(id==0x2AU?((f&SREG_CM)<<7):0U);r[a]=(uint8)v;
    f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((s&1U)<<8)+v];break;
   case 0x0AU:case 0x46U:case 0x47U:
    if((id==0x0AU && r[a]==r[b]) || (id==0x46U && (r[a]&b)==0U) ||
       (id==0x47U && (r[a]&b)!=0U)){
     cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);
    }break;
   case 0x40U:pc=WRAP32(pc+b);cy=2U;break;
   case 0x42U:case 0x43U:
    if((id==0x42U && (f&a)!=0U)||(id==0x43U && (f&a)==0U)){pc=WRAP32(pc+b);cy=2U;}break;
   case 0x48U:r[a]=(uint8)b;break;
/* Extra event-free cases for generated M register spans.
 * Semantics/order from CUzeBox cu_avr_n.h, GPL-3.0-or-later.
 * This fragment is inserted into a copy of cu_avr_pure_execute. It does
 * not replace the frozen scalar oracle. Unsupported I/O has no effect.
 */
   case 0x07U:case 0x08U:case 0x13U: /* CPC / SBC / SBCI */
    if(a>=32U || b>(id==0x13U?255U:31U)){goto span_barrier;}
    d=r[a];s=id==0x13U?b:r[b];v=d-(s+(f&SREG_CM));
    if(id!=0x07U){r[a]=(uint8)v;}
    /* Preserve cumulative Z exactly as the original sbc_tail_flg. */
    f=(f|(SREG_HM|SREG_SM|SREG_VM|SREG_NM|SREG_CM))&
      (pf[CU_AVRFG_SUB+(v&511U)+((s&0x90U)<<5)+((d&0x90U)<<6)]|(SREG_IM|SREG_TM));break;
   case 0x3AU:case 0x3BU: /* ADIW / SBIW; original word flag formulas */
    if(a>=31U || b>63U){goto span_barrier;}
    d=r[a]+((auint)r[a+1U]<<8);v=id==0x3AU?d+b:d-b;
    SREG_CLR(f,SREG_CM|SREG_ZM|SREG_NM|SREG_VM|SREG_SM);
    if(id==0x3AU){SREG_SET(f,SREG_VM&(((~d)&v)>>(15U-SREG_V)));}
    else{SREG_SET(f,SREG_VM&((d&(~v))>>(15U-SREG_V)));}
    r[a]=(uint8)v;r[a+1U]=(uint8)(v>>8);
    SREG_SET(f,SREG_NM&(v>>(15U-SREG_N)));SREG_SET_C_BIT16(f,v);
    SREG_SET_Z(f,v&65535U);SREG_COM_NV(f);cy=2U;break;
   case 0x44U: /* BLD */
    if(a>=32U || b>=8U){goto span_barrier;}
    r[a]=(r[a]&(~(1U<<b)))|(((f>>SREG_T)&1U)<<b);break;
   case 0x45U: /* BST */
    if(a>=32U || b>=8U){goto span_barrier;}
    f=(f&(~(auint)SREG_TM))|(((r[a]>>b)&1U)<<SREG_T);break;
   case 0x1CU:case 0x1DU:case 0x1EU:case 0x1FU:
   case 0x20U:case 0x21U:case 0x22U:case 0x23U:
    s=cu_avr_span_memory(r,op);if(!s){goto span_barrier;}
    pc=WRAP32(pc+s-1U);cy=2U;break;
   default:goto span_barrier;
  }
used+=cy;budget-=cy;steps++;goto L2f6f;}

L2f6f:
pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto done;}
used+=1U;budget-=1U;steps++;goto L2f70;
L2f70:if(budget<4U || steps>1023U)goto done;
pc=WRAP32(pc+1U);
pc=WRAP32(pc+65404U);used+=2U;budget-=2U;steps++;goto L2eed;
span_barrier:pc=WRAP32(pc-1U);
done:
 c->pc=pc;c->flags=f;c->used=used;c->steps=steps;
 if(steps){
  uz_aot_entries++;uz_aot_instructions+=steps;uz_aot_cycles+=used;
  uzebox_span_data.entries++;uzebox_span_data.instructions+=steps;uzebox_span_data.cycles+=used;
  if(steps>uzebox_span_data.max_steps){uzebox_span_data.max_steps=steps;}
  if(budget<3U){uzebox_span_data.budget_exits++;}
  else if(steps==1024U){uzebox_span_data.cap_exits++;}
  else{uzebox_span_data.barrier_exits++;}
 }
}
static void uz_aot_classify(void){
auint pc,i;uz_aot_valid=0U;
for(pc=0;pc<32768U;pc++){

for(i=0;i<132U;i++){uint32 op=cpu_code[(pc+i)&32767U];if((op&127U)==17U)op&=0x7fffffffU;if(op!=uz_aot_pattern[i])break;}
if(i==132U){uz_aot_base=pc;uz_aot_valid=1U;break;}
}
}

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

/* Boot checks for generated M span code. GPL-3.0-or-later. */
static boole __attribute__((noinline)) cu_avr_span_selftest(void)
{
 uint32 const program[]={0x00FA0248U,0x000A0348U,0x00030209U,0x00020245U,
  0x00030444U,0x001A021FU,0x001E0521U,0x00000238U};
 uint32 const closed[]={0x00030211U,0xFFFE0040U};
 uint32 const carry[]={0x00030208U,0x00030207U,0x00050213U,0x00000238U};
 uint32 const word[]={0x003F183AU,0x003F183BU,0x00000238U};
 uint8 const saved_ram=cpu_state.sram[256U],saved_mark=access_mem[256U];
 uzebox_span_stats_t const saved_stats=uzebox_span_data;
 cu_avr_pure_ctx_t c;boole ok;
 memset(&c,0,sizeof(c));c.code=program;c.pflags=cpu_pflags;c.types=cu_avr_pure_types;
 c.budget=32U;c.regs[27]=1U;c.regs[31]=1U;
 cu_avr_span_execute(&c);
 ok=c.steps==7U && c.used==9U && c.pc==7U && c.regs[2]==4U && c.regs[4]==8U &&
    c.regs[5]==4U && c.regs[26]==1U && c.regs[27]==1U &&
    cpu_state.sram[256U]==4U && (access_mem[256U]&(CU_MEM_R|CU_MEM_W))==(CU_MEM_R|CU_MEM_W);
 memset(&c,0,sizeof(c));c.code=closed;c.pflags=cpu_pflags;c.types=cu_avr_pure_types;
 c.budget=300U;c.regs[3]=0xA5U;cu_avr_span_execute(&c);
 ok=ok && c.steps==199U && c.used==298U && c.pc==99U*65536U+1U && c.regs[2]==0xA5U;
 memset(&c,0,sizeof(c));c.code=carry;c.pflags=cpu_pflags;c.types=cu_avr_pure_types;
 c.budget=16U;c.regs[3]=1U;c.flags=SREG_ZM|SREG_CM;cu_avr_span_execute(&c);
 ok=ok && c.steps==3U && c.used==3U && c.pc==3U && c.regs[2]==249U && c.flags==(SREG_NM|SREG_SM);
 memset(&c,0,sizeof(c));c.code=word;c.pflags=cpu_pflags;c.types=cu_avr_pure_types;
 c.budget=16U;c.regs[24]=255U;c.regs[25]=127U;c.flags=SREG_TM|SREG_HM|SREG_CM;cu_avr_span_execute(&c);
 ok=ok && c.steps==2U && c.used==4U && c.pc==2U && c.regs[24]==255U && c.regs[25]==127U &&
    c.flags==(SREG_TM|SREG_HM|SREG_VM|SREG_SM);
 cpu_state.sram[256U]=saved_ram;access_mem[256U]=saved_mark;uzebox_span_data=saved_stats;
 return ok;
}

static __attribute__((always_inline)) inline boole cu_avr_pure_run(void)
{
 cu_avr_pure_ctx_t c;
 cu_avr_loop_ctx_t pixels;
 auint const span=uzebox_span_control[1]==1U;
 auint eb=WRAP32(cycle_next_event-cpu_state.cycle),budget,i;
 if(!cu_avr_pure_ok || event_it || event_it_enter || !eb || video_cycle>=VIDEO_CY_MAX){return FALSE;}
 budget=VIDEO_CY_MAX-video_cycle;if(budget>=eb){budget=eb-1U;}
 if(budget<10U){return FALSE;}
 memcpy(c.regs,cpu_state.iors,32U);c.code=cpu_code;c.pflags=cpu_pflags;c.types=cu_avr_pure_types;
 c.pc=cpu_state.pc;c.flags=cpu_state.iors[CU_IO_SREG];c.budget=budget;
 if(span){if(uzebox_hot_region_control[1]==1U && uz_aot_test_status==1U && uz_aot_valid && ((c.pc&32767U)-uz_aot_base)<132U)uz_aot_execute(&c);else cu_avr_span_execute(&c);}else{cu_avr_pure_execute(&c);}
 /* Extended spans may already have committed SRAM writes. Commit
  * every nonempty prefix; a zero-step rejection has no effects. */
 if(c.steps<(span?1U:8U)){return FALSE;}
 if(UZ_FILL_MODE){uz_fill_constant(&video_row.pixels[video_cycle],cpu_state.iors[CU_IO_PORTC],c.used);}
 else{
 memset(&pixels,0,sizeof(pixels));pixels.pixels=&video_row.pixels[video_cycle];
 pixels.pixel=cpu_state.iors[CU_IO_PORTC];pixels.iterations=c.used/3U;pixels.tail_taken=1U;
 if(pixels.iterations)cu_avr_loop_execute(&pixels,2U);
 for(i=pixels.iterations*3U;i<c.used;i++){pixels.pixels[i]=(uint8)pixels.pixel;}
 }
 memcpy(cpu_state.iors,c.regs,32U);cpu_state.pc=c.pc;cpu_state.iors[CU_IO_SREG]=(uint8)c.flags;
 cpu_state.cycle=WRAP32(cpu_state.cycle+c.used);video_cycle+=c.used;
 cu_avr_loop_counters.entries++;
 cu_avr_loop_counters.cycles+=c.used;cu_avr_loop_counters.alu_entries++;
 cu_avr_loop_counters.alu_instructions+=c.steps;cu_avr_loop_counters.alu_cycles+=c.used;
 return TRUE;
}

void uzebox_hot_region_stats(uzebox_hot_region_stats_t *out){
 out->mode=uzebox_hot_region_control[1];out->selftest=uz_aot_test_status;
 out->matched=uz_aot_valid;out->base=uz_aot_base;
 out->entries=uz_aot_entries;out->instructions=uz_aot_instructions;out->cycles=uz_aot_cycles;
}
