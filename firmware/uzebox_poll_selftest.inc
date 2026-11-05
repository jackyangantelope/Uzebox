/* Original ISA is the boot oracle; a failure remains latched until reboot. */
bool uzebox_poll_selftest(void){
 uint8 saved_iors[256],input[256],expected[256],pixels[128],expected_pixels[128];
 uint8 ram[5],marks[5],expected_ram[5],expected_marks[5];uint32 code[34];
 auint pc=cpu_state.pc,cycle=cpu_state.cycle,row=video_cycle,event=cycle_next_event;
 boole it=event_it,enter=event_it_enter;auint calls=cu_poll_calls,iterations=cu_poll_iterations;
 auint i,j,k,used,epc,ecy,erv;cu_avr_loop_binding_t b;bool ok=true,accepted;
 static uint16 const ram_addr[5]={256U,257U,260U,510U,511U};
 static uint16 const code_addr[34]={64,65,66,67,68,69,70,80,81,82,83,127,128,129,130,131,132,133,134,135,200,201,202,203,204,205,206,207,208,209,210,211,212,213};
 static uint32 const program[34]={0x18180eU,0x50243U,0x10018a0U,0U,0x10119a0U,0U,0x31U,0x10418a0U,0U,0x11816U,0x31U,0x1848U,0x4000adU,0U,0x180e01U,0x5000adU,0U,0x18180eU,0x10243U,0x400040U,0x400e47U,0x40U,0x800e47U,0x40U,0x10f47U,0x40U,0x10e47U,0x40U,0x80e47U,0x40U,0x100e47U,0x40U,0x200e47U,0xffa90040U};
 if(cu_avr_loop_disabled){cu_poll_ready=2U;return false;}
 if(cu_poll_ready==2U)return false;if(cu_poll_ready==1U)return true;
 memcpy(saved_iors,cpu_state.iors,256U);memcpy(pixels,video_row.pixels,128U);
 for(j=0;j<34;j++){code[j]=cpu_code[code_addr[j]];cpu_code[code_addr[j]]=program[j];}
 for(j=0;j<5;j++){ram[j]=cpu_state.sram[ram_addr[j]];marks[j]=access_mem[ram_addr[j]];}
 cu_poll_testing=1U;
 for(i=0;i<36U&&ok;i++){
  memset(&b,0,sizeof(b));b.pc=128;b.valid=1;
  if(!cu_poll_bind(&b,128U)){ok=false;break;}
  for(j=0;j<256;j++)cpu_state.iors[j]=(uint8)(j+i*17U);
  cpu_state.iors[24]=0;cpu_state.iors[CU_IO_SPL]=255;cpu_state.iors[CU_IO_SPH]=1;
  cpu_state.iors[CU_IO_SREG]=(uint8)(i*17U);cpu_state.iors[CU_IO_PORTC]=(uint8)(i*19U);
  for(j=0;j<5;j++){cpu_state.sram[ram_addr[j]]=0;access_mem[ram_addr[j]]=(uint8)(0x10U+j);}
  memset(video_row.pixels,0xd3,128U);
  cpu_state.pc=(i&1U)?0xffff0080U:128U;cpu_state.cycle=0xfffffff0U;video_cycle=0;
  cycle_next_event=WRAP32(cpu_state.cycle+((i%3U==0)?53U:105U));event_it=FALSE;event_it_enter=FALSE;
  if(i%9U==0)cpu_state.sram[256]=1U;
  if(i%9U==1)cpu_state.sram[260]=1U;
  if(i%9U==2)cpu_state.iors[24]=1U;
  memcpy(input,cpu_state.iors,256U);
  accepted=cu_poll_run(&b);used=WRAP32(cpu_state.cycle-0xfffffff0U);epc=cpu_state.pc;ecy=cpu_state.cycle;erv=video_cycle;
  if(!accepted){
   ok=i%9U<3U && !used && !video_cycle && cpu_state.pc==((i&1U)?0xffff0080U:128U) && memcmp(input,cpu_state.iors,256U)==0;
   for(j=0;j<128U&&ok;j++)if(video_row.pixels[j]!=0xd3U)ok=false;
   for(j=0;j<5U&&ok;j++)if(access_mem[ram_addr[j]]!=(uint8)(0x10U+j)||cpu_state.sram[ram_addr[j]]!=((j==0&&i%9U==0)||(j==2&&i%9U==1)?1U:0U))ok=false;
   continue;
  }
  if(i%9U<3U || !used || used>104U){ok=false;break;}
  memcpy(expected,cpu_state.iors,256U);memcpy(expected_pixels,video_row.pixels,128U);
  for(j=0;j<5;j++){expected_ram[j]=cpu_state.sram[ram_addr[j]];expected_marks[j]=access_mem[ram_addr[j]];cpu_state.sram[ram_addr[j]]=0;access_mem[ram_addr[j]]=(uint8)(0x10U+j);}
  memcpy(cpu_state.iors,input,256U);memset(video_row.pixels,0xd3,128U);
  cpu_state.pc=(i&1U)?0xffff0080U:128U;cpu_state.cycle=0xfffffff0U;video_cycle=0;
  for(k=0;k<100U && WRAP32(cpu_state.cycle-0xfffffff0U)<used;k++)cu_avr_exec();
  ok=cpu_state.pc==epc&&cpu_state.cycle==ecy&&video_cycle==erv&&memcmp(expected,cpu_state.iors,256U)==0&&memcmp(expected_pixels,video_row.pixels,128U)==0;
  for(j=0;j<5U&&ok;j++)if(expected_ram[j]!=cpu_state.sram[ram_addr[j]]||expected_marks[j]!=access_mem[ram_addr[j]])ok=false;
 }
 memcpy(cpu_state.iors,saved_iors,256U);memcpy(video_row.pixels,pixels,128U);
 for(j=0;j<34;j++)cpu_code[code_addr[j]]=code[j];
 for(j=0;j<5;j++){cpu_state.sram[ram_addr[j]]=ram[j];access_mem[ram_addr[j]]=marks[j];}
 cpu_state.pc=pc;cpu_state.cycle=cycle;video_cycle=row;cycle_next_event=event;event_it=it;event_it_enter=enter;
 cu_poll_calls=calls;cu_poll_iterations=iterations;cu_poll_testing=0;cu_poll_ready=ok?1U:2U;return ok;
}
void uzebox_poll_stats(uzebox_poll_stats_t *out){
 out->mode=uzebox_poll_control[1];out->selftest=cu_poll_ready;out->calls=cu_poll_calls;out->iterations=cu_poll_iterations;
}
