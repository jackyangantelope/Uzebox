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
