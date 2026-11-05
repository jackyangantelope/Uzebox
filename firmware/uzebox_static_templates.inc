/* GPL-3.0-or-later; static event-free CUzeBox ISA templates.
 * Included in an isolated span function with PC/flags/budget/used/steps live.
 * No runtime trace cache, context-call ABI or ROM-specific PCs. */
#define UZ_STATIC_COMMIT(n,cy_) do{used+=(cy_);budget-=(cy_);steps+=(n);goto uz_static_dispatch;}while(0)
uz_static_mov:
 a=(op>>8)&255U;b=(op>>16)&32767U;
#if UZEBOX_STATIC_FUSE
 if(budget>=4U && steps<=1022U){
  uint32 const q=code[pc&32767U];auint const qb=q>>16;
  if((q&127U)==0x0EU && ((q>>8)&255U)==a && qb<32U){
   v=r[b];v&=qb==a?v:r[qb];
#if UZEBOX_STATIC_FUSE >= 2
   if(budget>=6U && steps<=1020U){
    uint32 const q2=code[(pc+1U)&32767U],q3=code[(pc+2U)&32767U];
    auint const t=(q2>>8)&255U,source3=(q2>>16)&32767U;
    if((q2&127U)==0x11U && (source3==a || (UZEBOX_STATIC_FUSE>=3 && source3<32U)) && t<32U &&
       (q3&127U)==0x29U && ((q3>>8)&255U)==t){
     auint const shifted=source3==a?v:r[source3];
     r[a]=(uint8)v;r[t]=(uint8)(shifted>>1);
     /* MOV doesn't alter flags, so the intermediate AND flags are dead. */
     f=(f&(SREG_IM|SREG_TM|SREG_HM))|pf[CU_AVRFG_SHR+((shifted&1U)<<8)+(shifted>>1)];
     if(budget>=7U && steps<=1019U){
      uint32 const q4=code[(pc+3U)&32767U];auint const x=(q4>>8)&255U,y=q4>>16;
      if((q4&127U)==0x0AU && x<32U && y<32U){
#if UZEBOX_STATIC_FUSE >= 3
       if(budget>=8U && steps<=1018U){
        uint32 const q5=code[(pc+4U)&32767U];
        if((q5&255U)==0x40U){
         if(r[x]==r[y]){pc=WRAP32(pc+5U);UZ_STATIC_COMMIT(5U,6U);}
         pc=WRAP32(pc+5U+(q5>>16));UZ_STATIC_COMMIT(6U,7U);
        }
       }
#endif
       auint const skip=r[x]==r[y]?1U+((code[(pc+4U)&32767U]>>7)&1U):0U;
       pc=WRAP32(pc+4U+skip);UZ_STATIC_COMMIT(5U,5U+skip);
      }
     }
     pc=WRAP32(pc+3U);UZ_STATIC_COMMIT(4U,4U);
    }
   }
#endif
   r[a]=(uint8)v;f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
   pc=WRAP32(pc+1U);UZ_STATIC_COMMIT(2U,2U);
  }
 }
#endif
 r[a]=r[b];UZ_STATIC_COMMIT(1U,1U);

uz_static_and:
 a=(op>>8)&255U;b=op>>16;v=r[a]&r[b];r[a]=(uint8)v;
 f=(f&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|pf[CU_AVRFG_LOG+v];
 UZ_STATIC_COMMIT(1U,1U);

uz_static_cpse:
 a=(op>>8)&255U;b=op>>16;
#if UZEBOX_STATIC_FUSE
 if(budget>=4U && steps<=1022U){
  uint32 const q=code[pc&32767U];
  if((q&255U)==0x40U){
   if(r[a]==r[b]){pc=WRAP32(pc+1U);UZ_STATIC_COMMIT(1U,2U);}
   pc=WRAP32(pc+1U+(q>>16));UZ_STATIC_COMMIT(2U,3U);
  }
 }
#endif
 if(r[a]==r[b]){cy=2U+((code[pc&32767U]>>7)&1U);pc=WRAP32(pc+cy-1U);}
 else{cy=1U;}
 UZ_STATIC_COMMIT(1U,cy);

uz_static_brbs:
 a=(op>>8)&255U;b=op>>16;
 if(f&a){pc=WRAP32(pc+b);UZ_STATIC_COMMIT(1U,2U);}
 UZ_STATIC_COMMIT(1U,1U);
uz_static_brbc:
 a=(op>>8)&255U;b=op>>16;
 if(!(f&a)){pc=WRAP32(pc+b);UZ_STATIC_COMMIT(1U,2U);}
 UZ_STATIC_COMMIT(1U,1U);
uz_static_rjmp:
 pc=WRAP32(pc+(op>>16));UZ_STATIC_COMMIT(1U,2U);
uz_static_ldi:
 r[(op>>8)&255U]=(uint8)(op>>16);UZ_STATIC_COMMIT(1U,1U);
uz_static_nop:
 UZ_STATIC_COMMIT(1U,1U);
#undef UZ_STATIC_COMMIT
