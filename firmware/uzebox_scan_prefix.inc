/* GPL-3.0-or-later. Screened event-free scan initialization. */
static void __attribute__((noinline,section(".text.uz_video_prefix"))) uz_video_prefix(cu_avr_pure_ctx_t *restrict c){
uint8 *r=c->regs;uint8 const *pf=c->pflags;uint32 const *code=c->code;auint pc=c->pc,f=c->flags,budget=c->budget,used=0U,steps=0U,d,s,v;
switch((pc&32767U)-uz_video_base){
case 0U:goto P1c20;
case 1U:goto P1c21;
case 2U:goto P1c22;
case 3U:goto P1c23;
case 4U:goto P1c24;
case 6U:goto P1c26;
case 7U:goto P1c27;
case 8U:goto P1c28;
case 9U:goto P1c29;
case 10U:goto P1c2a;
case 11U:goto P1c2b;
case 12U:goto P1c2c;
case 13U:goto P1c2d;
case 14U:goto P1c2e;
case 15U:goto P1c2f;
case 16U:goto P1c30;
case 17U:goto P1c31;
case 19U:goto P1c33;
case 20U:goto P1c34;
case 21U:goto P1c35;
case 22U:goto P1c36;
case 23U:goto P1c37;
case 24U:goto P1c38;
case 25U:goto P1c39;
case 26U:goto P1c3a;
case 27U:goto P1c3b;
case 28U:goto P1c3c;
case 29U:goto P1c3d;
case 30U:goto P1c3e;
case 31U:goto P1c3f;
default:goto done;
}
P1c20:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
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
used+=cy;budget-=cy;steps++;goto P1c21;}
P1c21:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00141011U;auint const id=17U,a=16U;auint b=20U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c22;}
P1c22:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00031248U;auint const id=72U,a=18U;auint b=3U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c23;}
P1c23:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x0012100eU;auint const id=14U,a=16U;auint b=18U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c24;}
P1c24:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x010395a0U;auint const id=32U,a=149U;auint b=259U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c26;}
P1c26:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00151111U;auint const id=17U,a=17U;auint b=21U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c27;}
P1c27:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
if(r[16]==r[18]){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto P1c29;}
used++;budget--;steps++;goto P1c28;
P1c28:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00001148U;auint const id=72U,a=17U;auint b=0U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c29;}
P1c29:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00151311U;auint const id=17U,a=19U;auint b=21U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c2a;}
P1c2a:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
if(r[16]==r[18]){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto P1c2c;}
used++;budget--;steps++;goto P1c2b;
P1c2b:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00ff1348U;auint const id=72U,a=19U;auint b=255U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c2c;}
P1c2c:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00151711U;auint const id=17U,a=23U;auint b=21U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c2d;}
P1c2d:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
if(r[16]==r[18]){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto P1c2f;}
used++;budget--;steps++;goto P1c2e;
P1c2e:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00071748U;auint const id=72U,a=23U;auint b=7U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c2f;}
P1c2f:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
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
used+=cy;budget-=cy;steps++;goto P1c30;}
P1c30:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00191e11U;auint const id=17U,a=30U;auint b=25U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c31;}
P1c31:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x01058fa0U;auint const id=32U,a=143U;auint b=261U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c33;}
P1c33:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x000e0f0bU;auint const id=11U,a=15U;auint b=14U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c34;}
P1c34:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
if((f&2U)!=0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto P1c36;}
used++;budget--;steps++;goto P1c35;
P1c35:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
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
used+=cy;budget-=cy;steps++;goto P1c36;}
P1c36:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00001648U;auint const id=72U,a=22U;auint b=0U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c37;}
P1c37:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x0002020fU;auint const id=15U,a=2U;auint b=2U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c38;}
P1c38:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x000b1248U;auint const id=72U,a=18U;auint b=11U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c39;}
P1c39:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x001a1023U;auint const id=35U,a=16U;auint b=26U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c3a;}
P1c3a:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00110111U;auint const id=17U,a=1U;auint b=17U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c3b;}
P1c3b:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
if((r[16]&128U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto P1c3d;}
used++;budget--;steps++;goto P1c3c;
P1c3c:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00130111U;auint const id=17U,a=1U;auint b=19U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c3d;}
P1c3d:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x001e180bU;auint const id=11U,a=24U;auint b=30U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto P1c3e;}
P1c3e:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
if((f&2U)==0U){pc=WRAP32(pc+1U);used+=2U;budget-=2U;steps++;goto done;}
used++;budget--;steps++;goto P1c3f;
P1c3f:if(budget<3U || steps>=1024U)goto done;pc=WRAP32(pc+1U);
{uint32 const op=0x00170111U;auint const id=17U,a=1U;auint b=23U,cy=1U;
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
used+=cy;budget-=cy;steps++;goto done;}

span_barrier:pc=WRAP32(pc-1U);
done:
 c->pc=pc;c->flags=f;c->used=used;c->steps=steps;
 if(steps){
  uz_scan_prefix_entries++;
  uzebox_span_data.entries++;uzebox_span_data.instructions+=steps;uzebox_span_data.cycles+=used;
  if(steps>uzebox_span_data.max_steps){uzebox_span_data.max_steps=steps;}
  if(budget<3U){uzebox_span_data.budget_exits++;}
  else if(steps==1024U){uzebox_span_data.cap_exits++;}
  else{uzebox_span_data.barrier_exits++;}
 }
}
