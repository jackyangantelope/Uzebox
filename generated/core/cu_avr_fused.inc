
/* GPL-3.0-or-later, derived from cu_avr_n.h and the original soundMixerInline.s.
 * Matches a whole five-channel mixer graph by opcode structure and relative
 * SRAM operands. Constants fold the ORIGINAL ISA, not a replacement mixer.
 * All edges are forward; limits bound both branch outcomes to the next
 * original I/O/RET checkpoint. Pending events or row exhaustion fall back.
 * ROM recompile re-runs classification; no ROM name/CRC/entry-PC binding. */
static auint cu_avr_fused_prefix;
static auint cu_avr_fused_pc=65536U,cu_avr_fused_memory;
static const uint32 cu_avr_fused_pattern[174U]={0x121aU,0x111aU,0x101aU,0x1e1211U,0x311a0U,0x0U,0x41ea0U,0x0U,0x51fa0U,0x0U,0x110a0U,0x0U,0x101109U,0x210a0U,0x0U,0x101e0dU,0x1018U,0x41e9cU,0x0U,0x3119cU,0x0U,0x11a0U,0x0U,0x111003U,0x8U,0x911a0U,0x0U,0xa1ea0U,0x0U,0xb1fa0U,0x0U,0x710a0U,0x0U,0x101109U,0x810a0U,0x0U,0x101e0dU,0x1018U,0xa1e9cU,0x0U,0x9119cU,0x0U,0x1e01U,0x611a0U,0x0U,0x111003U,0x8U,0x1f0109U,0xf11a0U,0x0U,0x11246U,0x1253eU,0x1e000dU,0x101ea0U,0x0U,0x111fa0U,0x0U,0xd10a0U,0x0U,0x101109U,0xe10a0U,0x0U,0x101e0dU,0x1018U,0x101e9cU,0x0U,0xf119cU,0x0U,0x1e01U,0xc11a0U,0x0U,0x111003U,0x8U,0x1f0109U,0x1e000dU,0x1410a0U,0x0U,0x1511a0U,0x0U,0x121fa0U,0x0U,0x1f29U,0x12fU,0x11046U,0x1f25U,0x1e1e08U,0x1f0109U,0x1e000dU,0x161ea0U,0x0U,0x21e14U,0x40142U,0x1e18U,0x1e18U,0x1e18U,0xa0040U,0x101e11U,0x1129U,0x102aU,0x101e0fU,0x1e45U,0x61144U,0x131ea0U,0x0U,0x11e47U,0x61044U,0x14109cU,0x0U,0x15119cU,0x0U,0x161e9cU,0x0U,0x1148U,0x1b1ea0U,0x0U,0x191fa0U,0x0U,0x21246U,0x1253eU,0x1f1e09U,0x1b1e9cU,0x0U,0x1c1ea0U,0x0U,0x1d1fa0U,0x0U,0x1a10a0U,0x0U,0x101e0dU,0x111f0dU,0x2010a0U,0x0U,0x2111a0U,0x0U,0x101e0bU,0x111f07U,0x30143U,0x1018U,0x40U,0x60040U,0x1e10a0U,0x0U,0x1f11a0U,0x0U,0x101e0cU,0x111f08U,0x1c1e9cU,0x0U,0x1d1f9cU,0x0U,0x1018U,0x1e01U,0x1811a0U,0x0U,0x111003U,0x8U,0x1f0109U,0x1e000dU,0x1e01U,0x101bU,0x111bU,0x801f14U,0xff1e13U,0x30443U,0x1f48U,0x0U,0x30040U,0x1e12U,0x10242U,0xff1f48U,0xb31f9cU,0x0U,0x121bU,0x31U};
static void cu_avr_fused_classify(void){
 auint p,j,mem,candidate=65536U,candidate_mem=0U;uint32 op,pat;
 cu_avr_fused_pc=65536U;cu_avr_fused_prefix=0U;
 for(p=0;p+174U<=32768U;p++){
  if((cpu_code[p]&~CU_AVR_REGION_MARKER)!=0x121aU ||
     (cpu_code[p+1]&~CU_AVR_REGION_MARKER)!=0x111aU ||
     (cpu_code[p+2]&~CU_AVR_REGION_MARKER)!=0x101aU)continue;
  mem=(cpu_code[p+4]>>16)-3U;if(mem<256U||mem>65502U)continue;
  for(j=0;j<174U;j+=1U+((pat>>7)&1U)){
   pat=cu_avr_fused_pattern[j];op=cpu_code[p+j];
   if((op&127U)==0x1aU||(op&127U)==0x20U)op&=~CU_AVR_REGION_MARKER;
   if(((pat&127U)==0x1cU||(pat&127U)==0x20U)&&(pat>>16)<34U)pat+=mem<<16;
   if(op!=pat)break;
  }
  if(j==174U){cu_avr_fused_pc=p;cu_avr_fused_memory=mem;return;}
  if(j>=172U && candidate==65536U && CU_Y_ENABLED){candidate=p;candidate_mem=mem;}
 }
 if(candidate!=65536U){cu_avr_fused_pc=candidate;cu_avr_fused_memory=candidate_mem;cu_avr_fused_prefix=1U;}
}
#define CU_AVR_CONST_OPCODE 1
#define CU_AVR_LOOP_SCALAR_ORACLE 1
#define cu_avr_exec cu_avr_fused_io
#include "cu_avr_n.h"
#undef cu_avr_exec
#pragma push_macro("UPDATE_HARDWARE")
#pragma push_macro("UPDATE_HARDWARE_IT")
#undef UPDATE_HARDWARE
#undef UPDATE_HARDWARE_IT
#define UPDATE_HARDWARE do{region_cycles++;}while(0)
#define UPDATE_HARDWARE_IT UPDATE_HARDWARE
#define CU_AVR_REGION_COPY 1
#define event_it_enter FALSE
#define cu_avr_exec cu_avr_fused_step
#include "cu_avr_n.h"
#undef cu_avr_exec
#undef event_it_enter
#undef CU_AVR_REGION_COPY
#pragma pop_macro("UPDATE_HARDWARE_IT")
#pragma pop_macro("UPDATE_HARDWARE")
#undef CU_AVR_LOOP_SCALAR_ORACLE
#undef CU_AVR_CONST_OPCODE
static const uint16 cu_avr_fused_limits[174U]={182U,180U,178U,176U,175U,0U,173U,0U,171U,0U,169U,0U,167U,166U,0U,164U,163U,160U,0U,158U,0U,156U,0U,154U,152U,151U,0U,149U,0U,147U,0U,145U,0U,143U,142U,0U,140U,139U,136U,0U,134U,0U,132U,131U,0U,129U,127U,126U,125U,0U,123U,2U,121U,120U,0U,118U,0U,116U,0U,114U,113U,0U,111U,110U,107U,0U,105U,0U,103U,102U,0U,100U,98U,97U,96U,95U,0U,93U,0U,91U,0U,89U,88U,87U,86U,85U,84U,83U,82U,0U,80U,79U,78U,75U,72U,69U,77U,76U,75U,74U,73U,72U,71U,0U,69U,68U,67U,0U,65U,0U,63U,0U,61U,60U,0U,58U,0U,56U,2U,54U,53U,0U,51U,0U,49U,0U,47U,0U,45U,44U,43U,0U,41U,0U,39U,38U,37U,36U,33U,31U,35U,0U,33U,0U,31U,30U,29U,0U,27U,0U,25U,22U,21U,0U,19U,17U,16U,15U,14U,13U,11U,9U,8U,7U,6U,5U,4U,5U,4U,3U,2U,0U,6U,4U};
static cu_avr_fused_stats_t cu_avr_fused_data;
void cu_avr_fused_stats(cu_avr_fused_stats_t *out){
 *out=cu_avr_fused_data;out->pc=cu_avr_fused_pc;out->memory=cu_avr_fused_memory;
 out->enabled=CU_AVR_FUSED_ENABLED;
}

static boole __attribute__((noinline,section(".time_critical.cu_avr_fused_run"))) cu_avr_fused_run(void){
 auint const base=cu_avr_fused_pc,mem=cu_avr_fused_memory;
 auint const start=cpu_state.cycle;
 auint pos=(cpu_state.pc&32767U)-base,used=0U,steps=0U,cy;
 if(cu_avr_loop_disabled||!CU_AVR_REGION_ENABLED||!CU_AVR_FUSED_ENABLED||(pos>=174U || (cu_avr_fused_prefix && (pos>=172U || !CU_Y_ENABLED)))||event_it||event_it_enter)return FALSE;
 if(WRAP32(cycle_next_event-cpu_state.cycle)<=cu_avr_fused_limits[pos]||
    video_cycle>=VIDEO_CY_MAX||VIDEO_CY_MAX-video_cycle<cu_avr_fused_limits[pos]){cu_avr_fused_data.rejected++;return FALSE;}
 switch(pos){case 0U:goto i0;case 1U:goto i1;case 2U:goto i2;case 3U:goto i3;case 4U:goto i4;case 6U:goto i6;case 8U:goto i8;case 10U:goto i10;case 12U:goto i12;case 13U:goto i13;case 15U:goto i15;case 16U:goto i16;case 17U:goto i17;case 19U:goto i19;case 21U:goto i21;case 23U:goto i23;case 24U:goto i24;case 25U:goto i25;case 27U:goto i27;case 29U:goto i29;case 31U:goto i31;case 33U:goto i33;case 34U:goto i34;case 36U:goto i36;case 37U:goto i37;case 38U:goto i38;case 40U:goto i40;case 42U:goto i42;case 43U:goto i43;case 45U:goto i45;case 46U:goto i46;case 47U:goto i47;case 48U:goto i48;case 50U:goto i50;case 51U:goto i51;case 52U:goto i52;case 53U:goto i53;case 55U:goto i55;case 57U:goto i57;case 59U:goto i59;case 60U:goto i60;case 62U:goto i62;case 63U:goto i63;case 64U:goto i64;case 66U:goto i66;case 68U:goto i68;case 69U:goto i69;case 71U:goto i71;case 72U:goto i72;case 73U:goto i73;case 74U:goto i74;case 75U:goto i75;case 77U:goto i77;case 79U:goto i79;case 81U:goto i81;case 82U:goto i82;case 83U:goto i83;case 84U:goto i84;case 85U:goto i85;case 86U:goto i86;case 87U:goto i87;case 88U:goto i88;case 90U:goto i90;case 91U:goto i91;case 92U:goto i92;case 93U:goto i93;case 94U:goto i94;case 95U:goto i95;case 96U:goto i96;case 97U:goto i97;case 98U:goto i98;case 99U:goto i99;case 100U:goto i100;case 101U:goto i101;case 102U:goto i102;case 104U:goto i104;case 105U:goto i105;case 106U:goto i106;case 108U:goto i108;case 110U:goto i110;case 112U:goto i112;case 113U:goto i113;case 115U:goto i115;case 117U:goto i117;case 118U:goto i118;case 119U:goto i119;case 120U:goto i120;case 122U:goto i122;case 124U:goto i124;case 126U:goto i126;case 128U:goto i128;case 129U:goto i129;case 130U:goto i130;case 132U:goto i132;case 134U:goto i134;case 135U:goto i135;case 136U:goto i136;case 137U:goto i137;case 138U:goto i138;case 139U:goto i139;case 140U:goto i140;case 142U:goto i142;case 144U:goto i144;case 145U:goto i145;case 146U:goto i146;case 148U:goto i148;case 150U:goto i150;case 151U:goto i151;case 152U:goto i152;case 154U:goto i154;case 155U:goto i155;case 156U:goto i156;case 157U:goto i157;case 158U:goto i158;case 159U:goto i159;case 160U:goto i160;case 161U:goto i161;case 162U:goto i162;case 163U:goto i163;case 164U:goto i164;case 165U:goto i165;case 166U:goto i166;case 167U:goto i167;case 168U:goto i168;case 169U:goto i169;case 170U:goto i170;case 172U:goto i172;case 173U:goto i173; default:return FALSE;}
i0:
cy=cu_avr_fused_step(0x121aU);if(!cy)goto done;used+=cy;steps++;
i1:
cy=cu_avr_fused_step(0x111aU);if(!cy)goto done;used+=cy;steps++;
i2:
cy=cu_avr_fused_step(0x101aU);if(!cy)goto done;used+=cy;steps++;
i3:
cy=cu_avr_fused_step(0x1e1211U);if(!cy)goto done;used+=cy;steps++;
i4:
cy=cu_avr_fused_step((0x311a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i6:
cy=cu_avr_fused_step((0x41ea0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i8:
cy=cu_avr_fused_step((0x51fa0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i10:
cy=cu_avr_fused_step((0x110a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i12:
cy=cu_avr_fused_step(0x101109U);if(!cy)goto done;used+=cy;steps++;
i13:
cy=cu_avr_fused_step((0x210a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i15:
cy=cu_avr_fused_step(0x101e0dU);if(!cy)goto done;used+=cy;steps++;
i16:
cy=cu_avr_fused_step(0x1018U);if(!cy)goto done;used+=cy;steps++;
i17:
cy=cu_avr_fused_step((0x41e9cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i19:
cy=cu_avr_fused_step((0x3119cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i21:
cy=cu_avr_fused_step((0x11a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i23:
cy=cu_avr_fused_step(0x111003U);if(!cy)goto done;used+=cy;steps++;
i24:
cy=cu_avr_fused_step(0x8U);if(!cy)goto done;used+=cy;steps++;
i25:
cy=cu_avr_fused_step((0x911a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i27:
cy=cu_avr_fused_step((0xa1ea0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i29:
cy=cu_avr_fused_step((0xb1fa0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i31:
cy=cu_avr_fused_step((0x710a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i33:
cy=cu_avr_fused_step(0x101109U);if(!cy)goto done;used+=cy;steps++;
i34:
cy=cu_avr_fused_step((0x810a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i36:
cy=cu_avr_fused_step(0x101e0dU);if(!cy)goto done;used+=cy;steps++;
i37:
cy=cu_avr_fused_step(0x1018U);if(!cy)goto done;used+=cy;steps++;
i38:
cy=cu_avr_fused_step((0xa1e9cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i40:
cy=cu_avr_fused_step((0x9119cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i42:
cy=cu_avr_fused_step(0x1e01U);if(!cy)goto done;used+=cy;steps++;
i43:
cy=cu_avr_fused_step((0x611a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i45:
cy=cu_avr_fused_step(0x111003U);if(!cy)goto done;used+=cy;steps++;
i46:
cy=cu_avr_fused_step(0x8U);if(!cy)goto done;used+=cy;steps++;
i47:
cy=cu_avr_fused_step(0x1f0109U);if(!cy)goto done;used+=cy;steps++;
i48:
cy=cu_avr_fused_step((0xf11a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i50:
cy=cu_avr_fused_step(0x11246U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+52U)&32767U))goto i52;
i51:
if(used){memset(&video_row.pixels[video_cycle],cpu_state.iors[CU_IO_PORTC],used);cpu_state.cycle=WRAP32(cpu_state.cycle+used);video_cycle+=used;used=0U;}
cu_avr_fused_io(0x1253eU);steps++;
if(video_cycle>=VIDEO_CY_MAX||event_it||event_it_enter||WRAP32(cycle_next_event-cpu_state.cycle)<=121U||VIDEO_CY_MAX-video_cycle<121U)goto done;
i52:
cy=cu_avr_fused_step(0x1e000dU);if(!cy)goto done;used+=cy;steps++;
i53:
cy=cu_avr_fused_step((0x101ea0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i55:
cy=cu_avr_fused_step((0x111fa0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i57:
cy=cu_avr_fused_step((0xd10a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i59:
cy=cu_avr_fused_step(0x101109U);if(!cy)goto done;used+=cy;steps++;
i60:
cy=cu_avr_fused_step((0xe10a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i62:
cy=cu_avr_fused_step(0x101e0dU);if(!cy)goto done;used+=cy;steps++;
i63:
cy=cu_avr_fused_step(0x1018U);if(!cy)goto done;used+=cy;steps++;
i64:
cy=cu_avr_fused_step((0x101e9cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i66:
cy=cu_avr_fused_step((0xf119cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i68:
cy=cu_avr_fused_step(0x1e01U);if(!cy)goto done;used+=cy;steps++;
i69:
cy=cu_avr_fused_step((0xc11a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i71:
cy=cu_avr_fused_step(0x111003U);if(!cy)goto done;used+=cy;steps++;
i72:
cy=cu_avr_fused_step(0x8U);if(!cy)goto done;used+=cy;steps++;
i73:
cy=cu_avr_fused_step(0x1f0109U);if(!cy)goto done;used+=cy;steps++;
i74:
cy=cu_avr_fused_step(0x1e000dU);if(!cy)goto done;used+=cy;steps++;
i75:
cy=cu_avr_fused_step((0x1410a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i77:
cy=cu_avr_fused_step((0x1511a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i79:
cy=cu_avr_fused_step((0x121fa0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i81:
cy=cu_avr_fused_step(0x1f29U);if(!cy)goto done;used+=cy;steps++;
i82:
cy=cu_avr_fused_step(0x12fU);if(!cy)goto done;used+=cy;steps++;
i83:
cy=cu_avr_fused_step(0x11046U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+85U)&32767U))goto i85;
i84:
cy=cu_avr_fused_step(0x1f25U);if(!cy)goto done;used+=cy;steps++;
i85:
cy=cu_avr_fused_step(0x1e1e08U);if(!cy)goto done;used+=cy;steps++;
i86:
cy=cu_avr_fused_step(0x1f0109U);if(!cy)goto done;used+=cy;steps++;
i87:
cy=cu_avr_fused_step(0x1e000dU);if(!cy)goto done;used+=cy;steps++;
i88:
cy=cu_avr_fused_step((0x161ea0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i90:
cy=cu_avr_fused_step(0x21e14U);if(!cy)goto done;used+=cy;steps++;
i91:
cy=cu_avr_fused_step(0x40142U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+96U)&32767U))goto i96;
i92:
cy=cu_avr_fused_step(0x1e18U);if(!cy)goto done;used+=cy;steps++;
i93:
cy=cu_avr_fused_step(0x1e18U);if(!cy)goto done;used+=cy;steps++;
i94:
cy=cu_avr_fused_step(0x1e18U);if(!cy)goto done;used+=cy;steps++;
i95:
cy=cu_avr_fused_step(0xa0040U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+106U)&32767U))goto i106;
goto done;
i96:
cy=cu_avr_fused_step(0x101e11U);if(!cy)goto done;used+=cy;steps++;
i97:
cy=cu_avr_fused_step(0x1129U);if(!cy)goto done;used+=cy;steps++;
i98:
cy=cu_avr_fused_step(0x102aU);if(!cy)goto done;used+=cy;steps++;
i99:
cy=cu_avr_fused_step(0x101e0fU);if(!cy)goto done;used+=cy;steps++;
i100:
cy=cu_avr_fused_step(0x1e45U);if(!cy)goto done;used+=cy;steps++;
i101:
cy=cu_avr_fused_step(0x61144U);if(!cy)goto done;used+=cy;steps++;
i102:
cy=cu_avr_fused_step((0x131ea0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i104:
cy=cu_avr_fused_step(0x11e47U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+106U)&32767U))goto i106;
i105:
cy=cu_avr_fused_step(0x61044U);if(!cy)goto done;used+=cy;steps++;
i106:
cy=cu_avr_fused_step((0x14109cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i108:
cy=cu_avr_fused_step((0x15119cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i110:
cy=cu_avr_fused_step((0x161e9cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i112:
cy=cu_avr_fused_step(0x1148U);if(!cy)goto done;used+=cy;steps++;
i113:
cy=cu_avr_fused_step((0x1b1ea0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i115:
cy=cu_avr_fused_step((0x191fa0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i117:
cy=cu_avr_fused_step(0x21246U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+119U)&32767U))goto i119;
i118:
if(used){memset(&video_row.pixels[video_cycle],cpu_state.iors[CU_IO_PORTC],used);cpu_state.cycle=WRAP32(cpu_state.cycle+used);video_cycle+=used;used=0U;}
cu_avr_fused_io(0x1253eU);steps++;
if(video_cycle>=VIDEO_CY_MAX||event_it||event_it_enter||WRAP32(cycle_next_event-cpu_state.cycle)<=54U||VIDEO_CY_MAX-video_cycle<54U)goto done;
i119:
cy=cu_avr_fused_step(0x1f1e09U);if(!cy)goto done;used+=cy;steps++;
i120:
cy=cu_avr_fused_step((0x1b1e9cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i122:
cy=cu_avr_fused_step((0x1c1ea0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i124:
cy=cu_avr_fused_step((0x1d1fa0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i126:
cy=cu_avr_fused_step((0x1a10a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i128:
cy=cu_avr_fused_step(0x101e0dU);if(!cy)goto done;used+=cy;steps++;
i129:
cy=cu_avr_fused_step(0x111f0dU);if(!cy)goto done;used+=cy;steps++;
i130:
cy=cu_avr_fused_step((0x2010a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i132:
cy=cu_avr_fused_step((0x2111a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i134:
cy=cu_avr_fused_step(0x101e0bU);if(!cy)goto done;used+=cy;steps++;
i135:
cy=cu_avr_fused_step(0x111f07U);if(!cy)goto done;used+=cy;steps++;
i136:
cy=cu_avr_fused_step(0x30143U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+140U)&32767U))goto i140;
i137:
cy=cu_avr_fused_step(0x1018U);if(!cy)goto done;used+=cy;steps++;
i138:
cy=cu_avr_fused_step(0x40U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+139U)&32767U))goto i139;
goto done;
i139:
cy=cu_avr_fused_step(0x60040U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+146U)&32767U))goto i146;
goto done;
i140:
cy=cu_avr_fused_step((0x1e10a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i142:
cy=cu_avr_fused_step((0x1f11a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i144:
cy=cu_avr_fused_step(0x101e0cU);if(!cy)goto done;used+=cy;steps++;
i145:
cy=cu_avr_fused_step(0x111f08U);if(!cy)goto done;used+=cy;steps++;
i146:
cy=cu_avr_fused_step((0x1c1e9cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i148:
cy=cu_avr_fused_step((0x1d1f9cU+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i150:
cy=cu_avr_fused_step(0x1018U);if(!cy)goto done;used+=cy;steps++;
i151:
cy=cu_avr_fused_step(0x1e01U);if(!cy)goto done;used+=cy;steps++;
i152:
cy=cu_avr_fused_step((0x1811a0U+(mem<<16)));if(!cy)goto done;used+=cy;steps++;
i154:
cy=cu_avr_fused_step(0x111003U);if(!cy)goto done;used+=cy;steps++;
i155:
cy=cu_avr_fused_step(0x8U);if(!cy)goto done;used+=cy;steps++;
i156:
cy=cu_avr_fused_step(0x1f0109U);if(!cy)goto done;used+=cy;steps++;
i157:
cy=cu_avr_fused_step(0x1e000dU);if(!cy)goto done;used+=cy;steps++;
i158:
cy=cu_avr_fused_step(0x1e01U);if(!cy)goto done;used+=cy;steps++;
i159:
cy=cu_avr_fused_step(0x101bU);if(!cy)goto done;used+=cy;steps++;
i160:
cy=cu_avr_fused_step(0x111bU);if(!cy)goto done;used+=cy;steps++;
i161:
cy=cu_avr_fused_step(0x801f14U);if(!cy)goto done;used+=cy;steps++;
i162:
cy=cu_avr_fused_step(0xff1e13U);if(!cy)goto done;used+=cy;steps++;
i163:
cy=cu_avr_fused_step(0x30443U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+167U)&32767U))goto i167;
i164:
cy=cu_avr_fused_step(0x1f48U);if(!cy)goto done;used+=cy;steps++;
i165:
cy=cu_avr_fused_step(0x0U);if(!cy)goto done;used+=cy;steps++;
i166:
cy=cu_avr_fused_step(0x30040U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+170U)&32767U))goto i170;
goto done;
i167:
cy=cu_avr_fused_step(0x1e12U);if(!cy)goto done;used+=cy;steps++;
i168:
cy=cu_avr_fused_step(0x10242U);if(!cy)goto done;used+=cy;steps++;
if((cpu_state.pc&32767U)==((base+170U)&32767U))goto i170;
i169:
cy=cu_avr_fused_step(0xff1f48U);if(!cy)goto done;used+=cy;steps++;
i170:
if(used){memset(&video_row.pixels[video_cycle],cpu_state.iors[CU_IO_PORTC],used);cpu_state.cycle=WRAP32(cpu_state.cycle+used);video_cycle+=used;used=0U;}
cu_avr_fused_io(0xb31f9cU);steps++;
if(cu_avr_fused_prefix)goto done;
if(video_cycle>=VIDEO_CY_MAX||event_it||event_it_enter||WRAP32(cycle_next_event-cpu_state.cycle)<=6U||VIDEO_CY_MAX-video_cycle<6U)goto done;
i172:
cy=cu_avr_fused_step(0x121bU);if(!cy)goto done;used+=cy;steps++;
i173:
cy=cu_avr_fused_step(0x31U);if(!cy)goto done;used+=cy;steps++;
goto done;
done:
 if(used){memset(&video_row.pixels[video_cycle],cpu_state.iors[CU_IO_PORTC],used);
  cpu_state.cycle=WRAP32(cpu_state.cycle+used);video_cycle+=used;}
 if(!steps)return FALSE;
 if(cu_avr_fused_prefix)cu_y_prefix_calls++;
 cu_avr_fused_data.entries++;cu_avr_fused_data.instructions+=steps;
 cu_avr_fused_data.cycles+=WRAP32(cpu_state.cycle-start);return TRUE;
}

#ifdef CU_AVR_LOOP_NATIVE
/* Real kernel vs original scalar startup check, with no external sound output.
 * DDRB=0 suppresses sync edges; no emulated event is allowed during the test. */
static boole __attribute__((noinline)) cu_avr_fused_selftest(void){
 uint32 code[174U];uint8 old_io[256U],expected_io[256U],pixels[256U];
 uint8 old_ram[39U],expected_ram[39U],old_marks[39U],expected_marks[39U];
 uint8 io_marks[2U],expected_io_marks[2U];
 auint const old_pc=cpu_state.pc,old_cycle=cpu_state.cycle,old_row=video_cycle,old_event=cycle_next_event;
 auint const old_base=cu_avr_fused_pc,old_mem=cu_avr_fused_memory,old_prefix=cu_avr_fused_prefix;
 boole const old_it=event_it,old_enter=event_it_enter;
 boole const old_enable=cu_avr_fused_enabled();
#ifdef CU_AVR_REGION_COMPARE
 boole const old_region=cu_avr_region_enabled();
#endif
 cu_avr_fused_stats_t const old_stats=cu_avr_fused_data;
 auint j,addr,ref_pc,ref_cycle,ref_row,steps=0U;boole ok;
 memcpy(code,&cpu_code[0x400U],sizeof(code));memcpy(old_io,cpu_state.iors,sizeof(old_io));
 memcpy(pixels,video_row.pixels,sizeof(pixels));
 io_marks[0]=access_io[CU_IO_PORTB];io_marks[1]=access_io[CU_IO_OCR2A];
 for(j=0;j<39U;j++){
  addr=j<34U?0x200U+j:0x2FEU+j-34U;
  old_ram[j]=cpu_state.sram[addr];old_marks[j]=access_mem[addr];
  cpu_state.sram[addr]=0U;access_mem[addr]=0U;
 }
 cpu_state.sram[0x301U]=0x30U;cpu_state.sram[0x302U]=0U;
 for(j=0;j<174U;j++){
  uint32 op=cu_avr_fused_pattern[j];
  if(((op&127U)==0x1CU||(op&127U)==0x20U)&&(op>>16)<34U)op+=0x200U<<16;
  cpu_code[0x400U+j]=op;
 }
 memset(cpu_state.iors,0,256U);cpu_state.iors[CU_IO_PORTC]=0x5AU;
 cpu_state.iors[CU_IO_SPH]=3U;cpu_state.iors[30]=2U;
 cpu_state.pc=0x400U;cpu_state.cycle=0U;video_cycle=0U;cycle_next_event=1000U;
 event_it=FALSE;event_it_enter=FALSE;
 while(cpu_state.pc>=0x400U&&cpu_state.pc<0x400U+174U&&steps++<300U)cu_avr_exec();
 ref_pc=cpu_state.pc;ref_cycle=cpu_state.cycle;ref_row=video_cycle;
 memcpy(expected_io,cpu_state.iors,256U);
 for(j=0;j<39U;j++){addr=j<34U?0x200U+j:0x2FEU+j-34U;expected_ram[j]=cpu_state.sram[addr];expected_marks[j]=access_mem[addr];}
 expected_io_marks[0]=access_io[CU_IO_PORTB];expected_io_marks[1]=access_io[CU_IO_OCR2A];
 for(j=0;j<39U;j++){addr=j<34U?0x200U+j:0x2FEU+j-34U;cpu_state.sram[addr]=0U;access_mem[addr]=0U;}
 cpu_state.sram[0x301U]=0x30U;cpu_state.sram[0x302U]=0U;
 memset(cpu_state.iors,0,256U);cpu_state.iors[CU_IO_PORTC]=0x5AU;
 cpu_state.iors[CU_IO_SPH]=3U;cpu_state.iors[30]=2U;
 cpu_state.pc=0x400U;cpu_state.cycle=0U;video_cycle=0U;cycle_next_event=1000U;
 event_it=FALSE;event_it_enter=FALSE;access_io[CU_IO_PORTB]=io_marks[0];access_io[CU_IO_OCR2A]=io_marks[1];
 cu_avr_fused_pc=0x400U;cu_avr_fused_memory=0x200U;cu_avr_fused_prefix=0U;cu_avr_fused_enable(TRUE);
#ifdef CU_AVR_REGION_COMPARE
 cu_avr_region_enable(TRUE);
#endif
 ok=cu_avr_fused_run()&&ref_pc==0x3000U&&cpu_state.pc==ref_pc&&cpu_state.cycle==ref_cycle&&video_cycle==ref_row&&ref_cycle<256U;
 if(memcmp(expected_io,cpu_state.iors,256U))ok=FALSE;
 for(j=0;j<39U;j++){addr=j<34U?0x200U+j:0x2FEU+j-34U;if(cpu_state.sram[addr]!=expected_ram[j]||access_mem[addr]!=expected_marks[j])ok=FALSE;}
 if(access_io[CU_IO_PORTB]!=expected_io_marks[0]||access_io[CU_IO_OCR2A]!=expected_io_marks[1])ok=FALSE;
 for(j=0;j<ref_cycle&&j<256U;j++)if(video_row.pixels[j]!=0x5AU)ok=FALSE;
 memcpy(&cpu_code[0x400U],code,sizeof(code));memcpy(cpu_state.iors,old_io,256U);
 memcpy(video_row.pixels,pixels,sizeof(pixels));
 for(j=0;j<39U;j++){addr=j<34U?0x200U+j:0x2FEU+j-34U;cpu_state.sram[addr]=old_ram[j];access_mem[addr]=old_marks[j];}
 access_io[CU_IO_PORTB]=io_marks[0];access_io[CU_IO_OCR2A]=io_marks[1];
 cpu_state.pc=old_pc;cpu_state.cycle=old_cycle;video_cycle=old_row;cycle_next_event=old_event;
 event_it=old_it;event_it_enter=old_enter;cu_avr_fused_pc=old_base;cu_avr_fused_memory=old_mem;cu_avr_fused_prefix=old_prefix;
 cu_avr_fused_data=old_stats;cu_avr_fused_enable(old_enable);
#ifdef CU_AVR_REGION_COMPARE
 cu_avr_region_enable(old_region);
#endif
 return ok;
}
#endif
