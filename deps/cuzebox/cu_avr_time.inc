/* Diagnostic wall-time attribution, GPL-3.0-or-later as the parent core.
 * Only a sparse, jittered subset of complete cu_avr_run scanlines uses the
 * separately compiled decoder below. The normal decoder has no timing hooks.
 * Native buckets include admission, execution and commit. Failed probes have
 * their own bucket. Hardware/IRQ wrappers are exclusive, not double counted.
 * Scalar residual includes dispatch/pixel-cycle work and wrapper overhead.
 * accounting_us measures explicit bookkeeping intervals, NOT all perturbation.
 */
static cu_avr_time_stats_t cu_avr_time_data;
static auint cu_avr_time_rng=0xB7149E31U,cu_avr_time_until=1U;
static auint cu_avr_time_last,cu_avr_time_bin;

void cu_avr_time_clear(void)
{
 memset(&cu_avr_time_data,0,sizeof(cu_avr_time_data));
 cu_avr_time_rng=0xB7149E31U;cu_avr_time_until=1U;
}
void cu_avr_time_enable(boole enabled){cu_avr_time_data.enabled=enabled!=FALSE;}
void cu_avr_time_stats(cu_avr_time_stats_t *out){*out=cu_avr_time_data;}

static auint cu_avr_time_now(void)
{
 cu_avr_time_data.reads++;
 return cu_avr_time_clock();
}
static boole cu_avr_time_select(void)
{
 cu_avr_time_data.rows++;
 if(!cu_avr_time_data.enabled || --cu_avr_time_until!=0U){return FALSE;}
 cu_avr_time_rng^=cu_avr_time_rng<<13;
 cu_avr_time_rng^=cu_avr_time_rng>>17;
 cu_avr_time_rng^=cu_avr_time_rng<<5;
 cu_avr_time_until=128U+(cu_avr_time_rng&255U);
 return TRUE;
}
static void cu_avr_time_scalar_until(auint now)
{
 auint const elapsed=WRAP32(now-cu_avr_time_last);
 cu_avr_time_data.us[CU_TIME_SCALAR]+=elapsed;
 cu_avr_time_data.scalar_pc_us[cu_avr_time_bin]+=elapsed;
}
/* Close the scalar interval and exclude measured bookkeeping from the bucket. */
static auint cu_avr_time_enter(void)
{
 auint const now=cu_avr_time_now();
 auint after;
 cu_avr_time_scalar_until(now);
 after=cu_avr_time_now();
 cu_avr_time_data.accounting_us+=WRAP32(after-now);
 return after;
}
static void cu_avr_time_leave(auint kind,auint start)
{
 auint const now=cu_avr_time_now();
 auint after;
 cu_avr_time_data.us[kind]+=WRAP32(now-start);
 cu_avr_time_data.calls[kind]++;
 after=cu_avr_time_now();
 cu_avr_time_data.accounting_us+=WRAP32(after-now);
 cu_avr_time_last=after;
}
static boole __attribute__((noinline)) cu_avr_time_loop_run(void)
{
 auint const id=cpu_code[cpu_state.pc&32767U]&127U;
 auint const kind=id==0x11U?CU_TIME_ALU:id==0x18U?CU_TIME_BITMAP:
                  id==0x19U?CU_TIME_TILE:id==0x2DU?CU_TIME_WAIT:CU_TIME_DELAY;
 auint const start=cu_avr_time_enter();
 boole const accepted=cu_avr_loop_run();
 cu_avr_time_leave(accepted?kind:CU_TIME_REJECT,start);
 return accepted;
}
static void __attribute__((noinline)) cu_avr_time_hwexec(void)
{
 auint const start=cu_avr_time_enter();
 cu_avr_hwexec();cu_avr_time_leave(CU_TIME_EVENT,start);
}
static void __attribute__((noinline)) cu_avr_time_itcheck(void)
{
 auint const start=cu_avr_time_enter();
 cu_avr_itcheck();cu_avr_time_leave(CU_TIME_EVENT,start);
}
static void __attribute__((noinline)) cu_avr_time_interrupt(void)
{
 auint const start=cu_avr_time_enter();
 /* Original function's internal hardware calls are included here, exclusively. */
 cu_avr_interrupt();cu_avr_time_leave(CU_TIME_IRQ,start);
}
#ifdef CU_AVR_REGION_COMPARE
static boole __attribute__((noinline)) cu_avr_time_region_run(void)
{
 auint const start=cu_avr_time_enter();
 boole const accepted=cu_avr_region_run();
 cu_avr_time_leave(accepted?CU_TIME_REGION:CU_TIME_REGION_REJECT,start);
 return accepted;
}
#endif
static void __attribute__((noinline)) cu_avr_time_exec(void);
#define cu_avr_exec cu_avr_time_exec
#define cu_avr_loop_run cu_avr_time_loop_run
#define cu_avr_hwexec cu_avr_time_hwexec
#define cu_avr_itcheck cu_avr_time_itcheck
#define cu_avr_interrupt cu_avr_time_interrupt
#ifdef CU_AVR_REGION_COMPARE
#define cu_avr_region_run cu_avr_time_region_run
#endif
#include "cu_avr_n.h"
#ifdef CU_AVR_REGION_COMPARE
#undef cu_avr_region_run
#endif
#undef cu_avr_interrupt
#undef cu_avr_itcheck
#undef cu_avr_hwexec
#undef cu_avr_loop_run
#undef cu_avr_exec

static void cu_avr_time_row(void)
{
 auint const start=cu_avr_time_now();
 auint now;
 cu_avr_time_last=start;cu_avr_time_bin=(cpu_state.pc&32767U)>>10;
 while(video_cycle<VIDEO_CY_MAX){
  auint const bin=(cpu_state.pc&32767U)>>10;
  if(bin!=cu_avr_time_bin){
   now=cu_avr_time_now();cu_avr_time_scalar_until(now);
   cu_avr_time_bin=bin;cu_avr_time_last=cu_avr_time_now();
   cu_avr_time_data.accounting_us+=WRAP32(cu_avr_time_last-now);
  }
  cu_avr_time_data.calls[CU_TIME_SCALAR]++; /* dispatches, may enter native */
  cu_avr_time_exec();
 }
 now=cu_avr_time_now();cu_avr_time_scalar_until(now);
 cu_avr_time_data.sampled_us+=WRAP32(now-start);
 cu_avr_time_data.sampled_rows++;
}
