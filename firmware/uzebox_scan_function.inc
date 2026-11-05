/* GPL-3.0-or-later. Frozen screened136-word/134-node whole scan function. */
static uint32 const uz_scan_pattern[136U]={0x001c1a01U,0x00141011U,0x00031248U,0x0012100eU,0x010395a0U,0x00060001U,0x00151111U,0x0012100aU,0x00001148U,0x00151311U,0x0012100aU,0x00ff1348U,0x00151711U,0x0012100aU,0x00071748U,0x0018180fU,0x00191e11U,0x01058fa0U,0x000a0001U,0x000e0f0bU,0x00010242U,0x00ff1e48U,0x00001648U,0x0002020fU,0x000b1248U,0x001a1023U,0x00110111U,0x00801046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x00150049U,0x00000040U,0x00001827U,0x00010049U,0x00110111U,0x00401046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x80000018U,0x00000040U,0x00150049U,0x00001827U,0x00000040U,0x00010049U,0x00110111U,0x00201046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x80000018U,0x00000040U,0x00150049U,0x00001827U,0x00000040U,0x00010049U,0x00110111U,0x00101046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x80000018U,0x00000040U,0x00150049U,0x00001827U,0x00000040U,0x00010049U,0x00110111U,0x00081046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x80000018U,0x00000040U,0x00150049U,0x00001827U,0x00000040U,0x00010049U,0x00110111U,0x00041046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x80000018U,0x00000040U,0x00150049U,0x00001827U,0x00000040U,0x00010049U,0x00110111U,0x00021046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x80000018U,0x00000040U,0x00150049U,0x00001827U,0x00000040U,0x00010049U,0x00110111U,0x00011046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x80000018U,0x00000040U,0x00150049U,0x00001827U,0x00000040U,0x00010049U,0x001a1023U,0x00110111U,0x00801046U,0x00130111U,0x001e180bU,0x00010243U,0x00170111U,0x00000000U,0x00001827U,0x0000122bU,0x00150049U,0x0002120aU,0xff9e0040U,0x00000000U,0x00160049U,0x00000031U};
static boole uz_video_try_prefix(void);

#define CU_AVR_WHOLE_COPY 1
#define CU_AVR_CONST_OPCODE 1
#define cu_avr_exec uz_video_step
#include "cu_avr_n.h"
#undef cu_avr_exec
#undef CU_AVR_CONST_OPCODE
#undef CU_AVR_WHOLE_COPY
static void __attribute__((noinline,section(".text.uz_video_run"))) uz_video_run(void){
 auint steps=0;uz_scan_function_entries++;
uz_video_dispatch:
 if(video_cycle>=VIDEO_CY_MAX || steps>=1024U)return;
 switch((cpu_state.pc&32767U)-uz_video_base){
case 0U:goto V1c20;
case 1U:goto V1c21;
case 2U:goto V1c22;
case 3U:goto V1c23;
case 4U:goto V1c24;
case 6U:goto V1c26;
case 7U:goto V1c27;
case 8U:goto V1c28;
case 9U:goto V1c29;
case 10U:goto V1c2a;
case 11U:goto V1c2b;
case 12U:goto V1c2c;
case 13U:goto V1c2d;
case 14U:goto V1c2e;
case 15U:goto V1c2f;
case 16U:goto V1c30;
case 17U:goto V1c31;
case 19U:goto V1c33;
case 20U:goto V1c34;
case 21U:goto V1c35;
case 22U:goto V1c36;
case 23U:goto V1c37;
case 24U:goto V1c38;
case 25U:goto V1c39;
case 26U:goto V1c3a;
case 27U:goto V1c3b;
case 28U:goto V1c3c;
case 29U:goto V1c3d;
case 30U:goto V1c3e;
case 31U:goto V1c3f;
case 32U:goto V1c40;
case 33U:goto V1c41;
case 34U:goto V1c42;
case 35U:goto V1c43;
case 36U:goto V1c44;
case 37U:goto V1c45;
case 38U:goto V1c46;
case 39U:goto V1c47;
case 40U:goto V1c48;
case 41U:goto V1c49;
case 42U:goto V1c4a;
case 43U:goto V1c4b;
case 44U:goto V1c4c;
case 45U:goto V1c4d;
case 46U:goto V1c4e;
case 47U:goto V1c4f;
case 48U:goto V1c50;
case 49U:goto V1c51;
case 50U:goto V1c52;
case 51U:goto V1c53;
case 52U:goto V1c54;
case 53U:goto V1c55;
case 54U:goto V1c56;
case 55U:goto V1c57;
case 56U:goto V1c58;
case 57U:goto V1c59;
case 58U:goto V1c5a;
case 59U:goto V1c5b;
case 60U:goto V1c5c;
case 61U:goto V1c5d;
case 62U:goto V1c5e;
case 63U:goto V1c5f;
case 64U:goto V1c60;
case 65U:goto V1c61;
case 66U:goto V1c62;
case 67U:goto V1c63;
case 68U:goto V1c64;
case 69U:goto V1c65;
case 70U:goto V1c66;
case 71U:goto V1c67;
case 72U:goto V1c68;
case 73U:goto V1c69;
case 74U:goto V1c6a;
case 75U:goto V1c6b;
case 76U:goto V1c6c;
case 77U:goto V1c6d;
case 78U:goto V1c6e;
case 79U:goto V1c6f;
case 80U:goto V1c70;
case 81U:goto V1c71;
case 82U:goto V1c72;
case 83U:goto V1c73;
case 84U:goto V1c74;
case 85U:goto V1c75;
case 86U:goto V1c76;
case 87U:goto V1c77;
case 88U:goto V1c78;
case 89U:goto V1c79;
case 90U:goto V1c7a;
case 91U:goto V1c7b;
case 92U:goto V1c7c;
case 93U:goto V1c7d;
case 94U:goto V1c7e;
case 95U:goto V1c7f;
case 96U:goto V1c80;
case 97U:goto V1c81;
case 98U:goto V1c82;
case 99U:goto V1c83;
case 100U:goto V1c84;
case 101U:goto V1c85;
case 102U:goto V1c86;
case 103U:goto V1c87;
case 104U:goto V1c88;
case 105U:goto V1c89;
case 106U:goto V1c8a;
case 107U:goto V1c8b;
case 108U:goto V1c8c;
case 109U:goto V1c8d;
case 110U:goto V1c8e;
case 111U:goto V1c8f;
case 112U:goto V1c90;
case 113U:goto V1c91;
case 114U:goto V1c92;
case 115U:goto V1c93;
case 116U:goto V1c94;
case 117U:goto V1c95;
case 118U:goto V1c96;
case 119U:goto V1c97;
case 120U:goto V1c98;
case 121U:goto V1c99;
case 122U:goto V1c9a;
case 123U:goto V1c9b;
case 124U:goto V1c9c;
case 125U:goto V1c9d;
case 126U:goto V1c9e;
case 127U:goto V1c9f;
case 128U:goto V1ca0;
case 129U:goto V1ca1;
case 130U:goto V1ca2;
case 131U:goto V1ca3;
case 132U:goto V1ca4;
case 133U:goto V1ca5;
case 134U:goto V1ca6;
case 135U:goto V1ca7;
default:return;
}
V1c20:
 if(uz_video_try_prefix())goto uz_video_dispatch;
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001c1a01U);
 if((cpu_state.pc&32767U)==((uz_video_base+1U)&32767U))goto V1c21;
 goto uz_video_dispatch;
V1c21:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00141011U);
 if((cpu_state.pc&32767U)==((uz_video_base+2U)&32767U))goto V1c22;
 goto uz_video_dispatch;
V1c22:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00031248U);
 if((cpu_state.pc&32767U)==((uz_video_base+3U)&32767U))goto V1c23;
 goto uz_video_dispatch;
V1c23:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0012100eU);
 if((cpu_state.pc&32767U)==((uz_video_base+4U)&32767U))goto V1c24;
 goto uz_video_dispatch;
V1c24:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x010395a0U);
 if((cpu_state.pc&32767U)==((uz_video_base+6U)&32767U))goto V1c26;
 goto uz_video_dispatch;
V1c26:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00151111U);
 if((cpu_state.pc&32767U)==((uz_video_base+7U)&32767U))goto V1c27;
 goto uz_video_dispatch;
V1c27:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0012100aU);
 goto uz_video_dispatch;
V1c28:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001148U);
 if((cpu_state.pc&32767U)==((uz_video_base+9U)&32767U))goto V1c29;
 goto uz_video_dispatch;
V1c29:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00151311U);
 if((cpu_state.pc&32767U)==((uz_video_base+10U)&32767U))goto V1c2a;
 goto uz_video_dispatch;
V1c2a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0012100aU);
 goto uz_video_dispatch;
V1c2b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00ff1348U);
 if((cpu_state.pc&32767U)==((uz_video_base+12U)&32767U))goto V1c2c;
 goto uz_video_dispatch;
V1c2c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00151711U);
 if((cpu_state.pc&32767U)==((uz_video_base+13U)&32767U))goto V1c2d;
 goto uz_video_dispatch;
V1c2d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0012100aU);
 goto uz_video_dispatch;
V1c2e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00071748U);
 if((cpu_state.pc&32767U)==((uz_video_base+15U)&32767U))goto V1c2f;
 goto uz_video_dispatch;
V1c2f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0018180fU);
 if((cpu_state.pc&32767U)==((uz_video_base+16U)&32767U))goto V1c30;
 goto uz_video_dispatch;
V1c30:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00191e11U);
 if((cpu_state.pc&32767U)==((uz_video_base+17U)&32767U))goto V1c31;
 goto uz_video_dispatch;
V1c31:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x01058fa0U);
 if((cpu_state.pc&32767U)==((uz_video_base+19U)&32767U))goto V1c33;
 goto uz_video_dispatch;
V1c33:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x000e0f0bU);
 if((cpu_state.pc&32767U)==((uz_video_base+20U)&32767U))goto V1c34;
 goto uz_video_dispatch;
V1c34:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010242U);
 goto uz_video_dispatch;
V1c35:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00ff1e48U);
 if((cpu_state.pc&32767U)==((uz_video_base+22U)&32767U))goto V1c36;
 goto uz_video_dispatch;
V1c36:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001648U);
 if((cpu_state.pc&32767U)==((uz_video_base+23U)&32767U))goto V1c37;
 goto uz_video_dispatch;
V1c37:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0002020fU);
 if((cpu_state.pc&32767U)==((uz_video_base+24U)&32767U))goto V1c38;
 goto uz_video_dispatch;
V1c38:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x000b1248U);
 if((cpu_state.pc&32767U)==((uz_video_base+25U)&32767U))goto V1c39;
 goto uz_video_dispatch;
V1c39:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001a1023U);
 if((cpu_state.pc&32767U)==((uz_video_base+26U)&32767U))goto V1c3a;
 goto uz_video_dispatch;
V1c3a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+27U)&32767U))goto V1c3b;
 goto uz_video_dispatch;
V1c3b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00801046U);
 goto uz_video_dispatch;
V1c3c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+29U)&32767U))goto V1c3d;
 goto uz_video_dispatch;
V1c3d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+30U)&32767U))goto V1c3e;
 goto uz_video_dispatch;
V1c3e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c3f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+32U)&32767U))goto V1c40;
 goto uz_video_dispatch;
V1c40:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+33U)&32767U))goto V1c41;
 goto uz_video_dispatch;
V1c41:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+34U)&32767U))goto V1c42;
 goto uz_video_dispatch;
V1c42:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+35U)&32767U))goto V1c43;
 goto uz_video_dispatch;
V1c43:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+36U)&32767U))goto V1c44;
 goto uz_video_dispatch;
V1c44:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+37U)&32767U))goto V1c45;
 goto uz_video_dispatch;
V1c45:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00401046U);
 goto uz_video_dispatch;
V1c46:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+39U)&32767U))goto V1c47;
 goto uz_video_dispatch;
V1c47:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+40U)&32767U))goto V1c48;
 goto uz_video_dispatch;
V1c48:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c49:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+42U)&32767U))goto V1c4a;
 goto uz_video_dispatch;
V1c4a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x80000018U);
 if((cpu_state.pc&32767U)==((uz_video_base+43U)&32767U))goto V1c4b;
 goto uz_video_dispatch;
V1c4b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+44U)&32767U))goto V1c4c;
 goto uz_video_dispatch;
V1c4c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+45U)&32767U))goto V1c4d;
 goto uz_video_dispatch;
V1c4d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+46U)&32767U))goto V1c4e;
 goto uz_video_dispatch;
V1c4e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+47U)&32767U))goto V1c4f;
 goto uz_video_dispatch;
V1c4f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+48U)&32767U))goto V1c50;
 goto uz_video_dispatch;
V1c50:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+49U)&32767U))goto V1c51;
 goto uz_video_dispatch;
V1c51:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00201046U);
 goto uz_video_dispatch;
V1c52:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+51U)&32767U))goto V1c53;
 goto uz_video_dispatch;
V1c53:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+52U)&32767U))goto V1c54;
 goto uz_video_dispatch;
V1c54:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c55:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+54U)&32767U))goto V1c56;
 goto uz_video_dispatch;
V1c56:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x80000018U);
 if((cpu_state.pc&32767U)==((uz_video_base+55U)&32767U))goto V1c57;
 goto uz_video_dispatch;
V1c57:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+56U)&32767U))goto V1c58;
 goto uz_video_dispatch;
V1c58:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+57U)&32767U))goto V1c59;
 goto uz_video_dispatch;
V1c59:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+58U)&32767U))goto V1c5a;
 goto uz_video_dispatch;
V1c5a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+59U)&32767U))goto V1c5b;
 goto uz_video_dispatch;
V1c5b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+60U)&32767U))goto V1c5c;
 goto uz_video_dispatch;
V1c5c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+61U)&32767U))goto V1c5d;
 goto uz_video_dispatch;
V1c5d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00101046U);
 goto uz_video_dispatch;
V1c5e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+63U)&32767U))goto V1c5f;
 goto uz_video_dispatch;
V1c5f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+64U)&32767U))goto V1c60;
 goto uz_video_dispatch;
V1c60:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c61:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+66U)&32767U))goto V1c62;
 goto uz_video_dispatch;
V1c62:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x80000018U);
 if((cpu_state.pc&32767U)==((uz_video_base+67U)&32767U))goto V1c63;
 goto uz_video_dispatch;
V1c63:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+68U)&32767U))goto V1c64;
 goto uz_video_dispatch;
V1c64:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+69U)&32767U))goto V1c65;
 goto uz_video_dispatch;
V1c65:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+70U)&32767U))goto V1c66;
 goto uz_video_dispatch;
V1c66:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+71U)&32767U))goto V1c67;
 goto uz_video_dispatch;
V1c67:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+72U)&32767U))goto V1c68;
 goto uz_video_dispatch;
V1c68:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+73U)&32767U))goto V1c69;
 goto uz_video_dispatch;
V1c69:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00081046U);
 goto uz_video_dispatch;
V1c6a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+75U)&32767U))goto V1c6b;
 goto uz_video_dispatch;
V1c6b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+76U)&32767U))goto V1c6c;
 goto uz_video_dispatch;
V1c6c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c6d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+78U)&32767U))goto V1c6e;
 goto uz_video_dispatch;
V1c6e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x80000018U);
 if((cpu_state.pc&32767U)==((uz_video_base+79U)&32767U))goto V1c6f;
 goto uz_video_dispatch;
V1c6f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+80U)&32767U))goto V1c70;
 goto uz_video_dispatch;
V1c70:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+81U)&32767U))goto V1c71;
 goto uz_video_dispatch;
V1c71:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+82U)&32767U))goto V1c72;
 goto uz_video_dispatch;
V1c72:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+83U)&32767U))goto V1c73;
 goto uz_video_dispatch;
V1c73:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+84U)&32767U))goto V1c74;
 goto uz_video_dispatch;
V1c74:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+85U)&32767U))goto V1c75;
 goto uz_video_dispatch;
V1c75:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00041046U);
 goto uz_video_dispatch;
V1c76:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+87U)&32767U))goto V1c77;
 goto uz_video_dispatch;
V1c77:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+88U)&32767U))goto V1c78;
 goto uz_video_dispatch;
V1c78:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c79:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+90U)&32767U))goto V1c7a;
 goto uz_video_dispatch;
V1c7a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x80000018U);
 if((cpu_state.pc&32767U)==((uz_video_base+91U)&32767U))goto V1c7b;
 goto uz_video_dispatch;
V1c7b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+92U)&32767U))goto V1c7c;
 goto uz_video_dispatch;
V1c7c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+93U)&32767U))goto V1c7d;
 goto uz_video_dispatch;
V1c7d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+94U)&32767U))goto V1c7e;
 goto uz_video_dispatch;
V1c7e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+95U)&32767U))goto V1c7f;
 goto uz_video_dispatch;
V1c7f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+96U)&32767U))goto V1c80;
 goto uz_video_dispatch;
V1c80:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+97U)&32767U))goto V1c81;
 goto uz_video_dispatch;
V1c81:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00021046U);
 goto uz_video_dispatch;
V1c82:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+99U)&32767U))goto V1c83;
 goto uz_video_dispatch;
V1c83:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+100U)&32767U))goto V1c84;
 goto uz_video_dispatch;
V1c84:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c85:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+102U)&32767U))goto V1c86;
 goto uz_video_dispatch;
V1c86:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x80000018U);
 if((cpu_state.pc&32767U)==((uz_video_base+103U)&32767U))goto V1c87;
 goto uz_video_dispatch;
V1c87:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+104U)&32767U))goto V1c88;
 goto uz_video_dispatch;
V1c88:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+105U)&32767U))goto V1c89;
 goto uz_video_dispatch;
V1c89:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+106U)&32767U))goto V1c8a;
 goto uz_video_dispatch;
V1c8a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+107U)&32767U))goto V1c8b;
 goto uz_video_dispatch;
V1c8b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+108U)&32767U))goto V1c8c;
 goto uz_video_dispatch;
V1c8c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+109U)&32767U))goto V1c8d;
 goto uz_video_dispatch;
V1c8d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00011046U);
 goto uz_video_dispatch;
V1c8e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+111U)&32767U))goto V1c8f;
 goto uz_video_dispatch;
V1c8f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+112U)&32767U))goto V1c90;
 goto uz_video_dispatch;
V1c90:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c91:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+114U)&32767U))goto V1c92;
 goto uz_video_dispatch;
V1c92:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x80000018U);
 if((cpu_state.pc&32767U)==((uz_video_base+115U)&32767U))goto V1c93;
 goto uz_video_dispatch;
V1c93:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+116U)&32767U))goto V1c94;
 goto uz_video_dispatch;
V1c94:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+117U)&32767U))goto V1c95;
 goto uz_video_dispatch;
V1c95:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+118U)&32767U))goto V1c96;
 goto uz_video_dispatch;
V1c96:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000040U);
 if((cpu_state.pc&32767U)==((uz_video_base+119U)&32767U))goto V1c97;
 goto uz_video_dispatch;
V1c97:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010049U);
 if((cpu_state.pc&32767U)==((uz_video_base+120U)&32767U))goto V1c98;
 goto uz_video_dispatch;
V1c98:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001a1023U);
 if((cpu_state.pc&32767U)==((uz_video_base+121U)&32767U))goto V1c99;
 goto uz_video_dispatch;
V1c99:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00110111U);
 if((cpu_state.pc&32767U)==((uz_video_base+122U)&32767U))goto V1c9a;
 goto uz_video_dispatch;
V1c9a:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00801046U);
 goto uz_video_dispatch;
V1c9b:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00130111U);
 if((cpu_state.pc&32767U)==((uz_video_base+124U)&32767U))goto V1c9c;
 goto uz_video_dispatch;
V1c9c:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x001e180bU);
 if((cpu_state.pc&32767U)==((uz_video_base+125U)&32767U))goto V1c9d;
 goto uz_video_dispatch;
V1c9d:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00010243U);
 goto uz_video_dispatch;
V1c9e:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00170111U);
 if((cpu_state.pc&32767U)==((uz_video_base+127U)&32767U))goto V1c9f;
 goto uz_video_dispatch;
V1c9f:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000000U);
 if((cpu_state.pc&32767U)==((uz_video_base+128U)&32767U))goto V1ca0;
 goto uz_video_dispatch;
V1ca0:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00001827U);
 if((cpu_state.pc&32767U)==((uz_video_base+129U)&32767U))goto V1ca1;
 goto uz_video_dispatch;
V1ca1:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0000122bU);
 if((cpu_state.pc&32767U)==((uz_video_base+130U)&32767U))goto V1ca2;
 goto uz_video_dispatch;
V1ca2:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00150049U);
 if((cpu_state.pc&32767U)==((uz_video_base+131U)&32767U))goto V1ca3;
 goto uz_video_dispatch;
V1ca3:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x0002120aU);
 goto uz_video_dispatch;
V1ca4:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0xff9e0040U);
 if((cpu_state.pc&32767U)==((uz_video_base+35U)&32767U))goto V1c43;
 goto uz_video_dispatch;
V1ca5:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000000U);
 if((cpu_state.pc&32767U)==((uz_video_base+134U)&32767U))goto V1ca6;
 goto uz_video_dispatch;
V1ca6:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00160049U);
 if((cpu_state.pc&32767U)==((uz_video_base+135U)&32767U))goto V1ca7;
 goto uz_video_dispatch;
V1ca7:
 if(video_cycle>=VIDEO_CY_MAX || steps++>=1024U)return;
 uz_video_step(0x00000031U);
 goto uz_video_dispatch;
}
static void uz_video_classify(void){
 
 auint pc,i;uz_video_valid=0U;
 for(pc=0;pc<32768U;pc++){
  for(i=0;i<136U;i++)if(cpu_code[(pc+i)&32767U]!=uz_scan_pattern[i])break;
  if(i==136U){uz_video_base=pc;uz_video_valid=1U;return;}
 }
}

