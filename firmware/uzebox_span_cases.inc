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
   case 0x1CU:case 0x20U: /* STS / LDS, including LDS metadata */
    if(id==0x20U){a&=127U;}
    if(a>=32U || b<256U){goto span_barrier;}
    v=b;pc=WRAP32(pc+1U);goto span_memory;
   case 0x1DU:case 0x21U: /* ST / LD with displacement */
    d=b&255U;
    if(a>=32U || d>=31U){goto span_barrier;}
    v=(r[d]+((auint)r[d+1U]<<8)+(b>>8))&65535U;
    if(v<256U){goto span_barrier;}
    goto span_memory;
   case 0x1EU:case 0x1FU:case 0x22U:case 0x23U: /* pre/post ST / LD */
    if(a>=32U || b>=31U){goto span_barrier;}
    v=r[b]+((auint)r[b+1U]<<8);
    if(id==0x1EU || id==0x22U){v=WRAP32(v-1U);}
    if(v<256U){goto span_barrier;}
    /* Original pre-decrement at zero uses unsigned 0xffffffff, then
     * mirrored SRAM. Pointer updates precede aliased source/dest use. */
    d=(id==0x1FU || id==0x23U)?WRAP32(v+1U):v;
    r[b]=(uint8)d;r[b+1U]=(uint8)(d>>8);
span_memory:
    if(id<=0x1FU){cpu_state.sram[v&4095U]=r[a];access_mem[v&4095U]|=CU_MEM_W;}
    else{r[a]=cpu_state.sram[v&4095U];access_mem[v&4095U]|=CU_MEM_R;}
    cy=2U;break;
