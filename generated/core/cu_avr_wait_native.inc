/* Fold a guarded closed CALL/LDS/ANDI/RET/AND/BRBS flag-polling loop.
 * Recognition follows the call target and binds operands, never a ROM PC.
 * No event/interrupt or raster boundary may occur inside the batch. SRAM
 * stack writes/read marks and the scalar decoder's truncated RET PC remain
 * observable, even though repeated identical stack operations are folded.
 */
static void cu_avr_wait_bind(cu_avr_loop_binding_t *b, auint pc)
{
 uint32 const call=cpu_code[pc];
 auint const target=cu_avr_loop_b(call);
 uint32 const ld=cpu_code[target&0x7FFFU];
 uint32 const mask=cpu_code[(target+2U)&0x7FFFU];
 uint32 const ret=cpu_code[(target+3U)&0x7FFFU];
 uint32 const test=cpu_code[(pc+2U)&0x7FFFU];
 uint32 const branch=cpu_code[(pc+3U)&0x7FFFU];
 auint const reg=cu_avr_loop_a(ld);
 if ((call&0x7FU)!=0x2DU || (ld&0x7FU)!=0x20U || reg>=32U ||
     cu_avr_loop_b(ld)<256U || (mask&0x7FU)!=0x16U ||
     cu_avr_loop_a(mask)!=reg || cu_avr_loop_b(mask)>255U ||
     (ret&0x7FU)!=0x31U || (test&0x7FU)!=0x0EU ||
     cu_avr_loop_a(test)!=reg || cu_avr_loop_b(test)!=reg ||
     (branch&0x7FU)!=0x42U || cu_avr_loop_a(branch)!=SREG_ZM ||
     cu_avr_loop_b(branch)!=0xFFFCU){ return; }
 b->kind=4U;b->count=(uint8)reg;b->pixel=(uint8)cu_avr_loop_b(mask);
 b->branch_delta=(uint16)cu_avr_loop_b(ld);
 cu_avr_loop_counters.builds++;
}

static boole cu_avr_wait_run(cu_avr_loop_binding_t const *b)
{
 cu_avr_loop_ctx_t c;
 auint eb=WRAP32(cycle_next_event-cpu_state.cycle),budget,n,cycles,sp,a0,a1,flag,ret,i;
 if (!eb){ cu_avr_loop_counters.event_rejects++;return FALSE; }
 budget=VIDEO_CY_MAX-video_cycle;
 if (budget>=eb){ budget=eb-1U; }
 n=budget/14U;
 if (n<2U){ cu_avr_loop_counters.event_rejects++;return FALSE; }
 flag=b->branch_delta&4095U;
 sp=cpu_state.iors[CU_IO_SPL]+((auint)cpu_state.iors[CU_IO_SPH]<<8);
 a0=sp&4095U;a1=(sp-1U)&4095U;
 /* CALL must not overwrite the watched flag. The register result cannot
  * alias SP (reg<32) or other I/O. A nonzero flag exits via scalar code. */
 if (flag==a0 || flag==a1 || (cpu_state.sram[flag]&b->pixel)!=0U){
  cu_avr_loop_counters.guard_rejects++;return FALSE;
 }
 cycles=n*14U;
 if(UZ_FILL_MODE){uz_fill_constant(&video_row.pixels[video_cycle],cpu_state.iors[CU_IO_PORTC],cycles);}
 else{
 memset(&c,0,sizeof(c));c.pixels=&video_row.pixels[video_cycle];
 c.pixel=cpu_state.iors[CU_IO_PORTC];c.iterations=cycles/3U;c.tail_taken=1U;
 /* Reuse the already self-tested SRAM constant-color kernel. */
 cu_avr_loop_execute(&c,2U);
 for(i=c.iterations*3U;i<cycles;i++){ c.pixels[i]=(uint8)c.pixel; }
 }
 ret=(cpu_state.pc+2U)&65535U;
 cpu_state.sram[a0]=(uint8)ret;cpu_state.sram[a1]=(uint8)(ret>>8);
 access_mem[a0]|=CU_MEM_W|CU_MEM_R;access_mem[a1]|=CU_MEM_W|CU_MEM_R;
 access_mem[flag]|=CU_MEM_R;
 cpu_state.iors[b->count]=0U;
 cpu_state.iors[CU_IO_SREG]=(cpu_state.iors[CU_IO_SREG]&(SREG_IM|SREG_TM|SREG_HM|SREG_CM))|
                           cpu_pflags[CU_AVRFG_LOG];
 /* RET truncates to 16 bits before the compiled unsigned branch offset.
  * After one iteration the full scalar PC is stable, not affine in n. */
 cpu_state.pc=ret+2U+0xFFFCU;
 cpu_state.cycle=WRAP32(cpu_state.cycle+cycles);video_cycle+=cycles;
 cu_avr_loop_counters.entries++;cu_avr_loop_counters.iterations+=n;
 cu_avr_loop_counters.cycles+=cycles;cu_avr_loop_counters.wait_entries++;
 cu_avr_loop_counters.wait_iterations+=n;cu_avr_loop_counters.wait_cycles+=cycles;
 return TRUE;
}
