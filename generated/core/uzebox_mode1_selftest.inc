/* Runs after the original loop selftest. Original ISA is the boot oracle. */
bool uzebox_mode1_selftest(void){
 uint8 saved_iors[256],input[256],expected[256],pixels[80],expected_pixels[80];
 uint8 romlo[8],romhi[8],ram[4],marks[6],expected_marks[6];
 uint32 code[20];cu_avr_loop_binding_t b;
 auint pc=cpu_state.pc,cycle=cpu_state.cycle,row=video_cycle,event=cycle_next_event;
 boole it=event_it,enter=event_it_enter;auint calls=cu_mode1_calls,iterations=cu_mode1_iterations,builds=cu_avr_loop_counters.builds;
 auint i,j,k,used,epc,ecy,erv,start,initial_pc;bool ok=true;
 static uint32 const program[20]={0x1019U,0x100049U,0x1a1423U,0x1019U,0x100049U,0x1a1523U,0x1019U,0x100049U,0x40U,0x1019U,0x100049U,0x181409U,0x19150dU,0x1019U,0x100049U,0x1019U,0x141e01U,0x122bU,0x100049U,0xffec0243U};
 if(cu_avr_loop_disabled){cu_mode1_ready=2U;return false;}
 if(cu_mode1_ready==2U)return false;
 if(cu_mode1_ready==1U)return true;
 memcpy(saved_iors,cpu_state.iors,256U);memcpy(pixels,video_row.pixels,80U);memcpy(code,cpu_code,sizeof(code));
 memcpy(romlo,cpu_state.crom,8U);memcpy(romhi,cpu_state.crom+65528U,8U);
 ram[0]=cpu_state.sram[256];ram[1]=cpu_state.sram[257];ram[2]=cpu_state.sram[4094];ram[3]=cpu_state.sram[4095];
 for(j=0;j<4;j++)marks[j]=access_mem[256U+j];marks[4]=access_mem[4094];marks[5]=access_mem[4095];
 memcpy(cpu_code,program,sizeof(program));
 for(i=0;i<8;i++){cpu_state.crom[i]=(uint8)(i*31U);cpu_state.crom[65528U+i]=(uint8)(i*27U+3U);}
 cu_mode1_testing=1U;
 for(i=0;i<36U&&ok;i++){
  memset(&b,0,sizeof(b));b.pc=0;b.valid=1;
  if(!cu_avr_mode1_bind(&b,0)){ok=false;break;}
  for(j=0;j<256;j++)cpu_state.iors[j]=(uint8)(0x20U+j);
  cpu_state.iors[16]=0x35U;cpu_state.iors[18]=(i%3U==0)?1U:((i%3U==1)?2U:40U);
  cpu_state.iors[24]=(uint8)(i*17U);cpu_state.iors[25]=(uint8)(i*9U);
  cpu_state.iors[26]=(i&1U)?0xfeU:0U;cpu_state.iors[27]=(i&1U)?0xffU:1U;
  cpu_state.iors[30]=0xfcU;cpu_state.iors[31]=0xffU;
  cpu_state.iors[CU_IO_DDRC]=(i%3U==0)?0U:((i%3U==1)?255U:0x5aU);
  cpu_state.iors[CU_IO_PORTC]=0xa5U;cpu_state.iors[CU_IO_SREG]=(uint8)(i*17U);
  cpu_state.sram[256]=0xfeU;cpu_state.sram[257]=0xffU;cpu_state.sram[4094]=0xfeU;cpu_state.sram[4095]=0xffU;
  access_mem[256]=access_mem[258]=access_mem[4094]=0x10U;access_mem[257]=access_mem[259]=access_mem[4095]=0x20U;
  memset(video_row.pixels,0xd3,80U);memcpy(input,cpu_state.iors,256U);
  initial_pc=(i&2U)?0xffff0000U:0U;start=0xffffffe0U;
  cpu_state.pc=initial_pc;cpu_state.cycle=start;video_cycle=0;cycle_next_event=WRAP32(start+73U);event_it=FALSE;event_it_enter=FALSE;
  if(!cu_avr_mode1_run(&b)){ok=false;break;}
  used=WRAP32(cpu_state.cycle-start);epc=cpu_state.pc;ecy=cpu_state.cycle;erv=video_cycle;
  if(!used||used>72U){ok=false;break;}
  memcpy(expected,cpu_state.iors,256U);memcpy(expected_pixels,video_row.pixels,80U);
  for(j=0;j<4;j++)expected_marks[j]=access_mem[256U+j];expected_marks[4]=access_mem[4094];expected_marks[5]=access_mem[4095];
  memcpy(cpu_state.iors,input,256U);memset(video_row.pixels,0xd3,80U);
  access_mem[256]=access_mem[258]=access_mem[4094]=0x10U;access_mem[257]=access_mem[259]=access_mem[4095]=0x20U;
  cpu_state.pc=initial_pc;cpu_state.cycle=start;video_cycle=0;
  for(k=0;k<100U && WRAP32(cpu_state.cycle-start)<used;k++)cu_avr_exec();
  ok=cpu_state.pc==epc&&cpu_state.cycle==ecy&&video_cycle==erv&&
     memcmp(expected,cpu_state.iors,256U)==0&&memcmp(expected_pixels,video_row.pixels,80U)==0&&
     memcmp(expected_marks,access_mem+256U,4U)==0&&expected_marks[4]==access_mem[4094]&&expected_marks[5]==access_mem[4095];
 }
 memcpy(cpu_state.iors,saved_iors,256U);memcpy(video_row.pixels,pixels,80U);memcpy(cpu_code,code,sizeof(code));
 memcpy(cpu_state.crom,romlo,8U);memcpy(cpu_state.crom+65528U,romhi,8U);
 cpu_state.sram[256]=ram[0];cpu_state.sram[257]=ram[1];cpu_state.sram[4094]=ram[2];cpu_state.sram[4095]=ram[3];
 for(j=0;j<4;j++)access_mem[256U+j]=marks[j];access_mem[4094]=marks[4];access_mem[4095]=marks[5];
 cpu_state.pc=pc;cpu_state.cycle=cycle;video_cycle=row;cycle_next_event=event;event_it=it;event_it_enter=enter;
 cu_mode1_calls=calls;cu_mode1_iterations=iterations;cu_avr_loop_counters.builds=builds;
 cu_mode1_testing=0U;cu_mode1_ready=ok?1U:2U;return ok;
}
void uzebox_mode1_stats(uzebox_mode1_stats_t *out){
 out->mode=uzebox_mode1_control[1];out->selftest=cu_mode1_ready;
 out->calls=cu_mode1_calls;out->iterations=cu_mode1_iterations;
}
