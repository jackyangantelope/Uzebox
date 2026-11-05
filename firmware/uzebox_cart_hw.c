/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "hardware/clocks.h"
#include "hardware/psram.h"
#include "hardware/uart.h"
#include "hardware/vreg.h"
#ifdef UZEBOX_MODE1
#include "uzebox_mode1.h"
#endif
#ifdef UZEBOX_POLL
#include "uzebox_poll.h"
#endif
#ifdef UZEBOX_Y
#include "uzebox_y.h"
#include "uzebox_render_fast.h"
static volatile uint32_t uzebox_y_render_calls;
#endif
#ifndef UZEBOX_SYS_KHZ
#define UZEBOX_SYS_KHZ 252000
#endif
#if UZEBOX_SYS_KHZ == 378000
#include "hardware/structs/qmi.h"
#include "hardware/sync.h"

static bool __no_inline_not_in_flash_func(uzebox_clock_prepare_qmi)(uint32_t old_hz) {
    const uint32_t irq=save_and_disable_interrupts();
    uint32_t timing[2];
    for(unsigned i=0;i<2u;++i){
        const uint32_t t=qmi_hw->m[i].timing;
        uint32_t div=(t&QMI_M0_TIMING_CLKDIV_BITS)>>QMI_M0_TIMING_CLKDIV_LSB;
        const uint32_t rx=(t&QMI_M0_TIMING_RXDELAY_BITS)>>QMI_M0_TIMING_RXDELAY_LSB;
        if(!div)div=256u;
        const uint32_t next=i==0u?9u:(uint32_t)(((uint64_t)div*378000000u+old_hz-1u)/old_hz);
        const uint32_t delay=i==0u?3u:(uint32_t)(((uint64_t)rx*378000000u+old_hz-1u)/old_hz);
        if(next>255u||delay>(QMI_M0_TIMING_RXDELAY_BITS>>QMI_M0_TIMING_RXDELAY_LSB)){
            restore_interrupts(irq);return false;
        }
        timing[i]=(t&~(QMI_M0_TIMING_CLKDIV_BITS|QMI_M0_TIMING_RXDELAY_BITS))|
            (next<<QMI_M0_TIMING_CLKDIV_LSB)|(delay<<QMI_M0_TIMING_RXDELAY_LSB);
    }
    qmi_hw->m[0].timing=timing[0];qmi_hw->m[1].timing=timing[1];
    __dsb();__isb();restore_interrupts(irq);return true;
}
#endif
#include "pico/stdlib.h"
#include "ff.h"
#include "tf_card.h"
#include "hstx.h"
#include "video_output.h"
#include "cu_avr.h"
#include "uzebox_aa_probe.h"
#include "cu_ctr.h"
#include "cu_spir.h"
#include "jack_audio.h"
#ifdef UZEBOX_AUDIO_BATCH
#include "uzebox_audio_batch.h"
#ifdef UZEBOX_HOT_REGION
#include "uzebox_hot_region.h"
#ifdef UZEBOX_SCAN_FUNCTION
#include "uzebox_scan.h"
#ifdef UZEBOX_MENU_DIAG
#include "uzebox_menu_diag.h"
#endif
#ifdef UZEBOX_SCAN_ATTRIBUTION
#include "uzebox_scan_cost.h"
#define UZEBOX_SPAN_LOG_TAG "R0616"
#else
#define UZEBOX_SPAN_LOG_TAG "Q0615"
#endif
#else
#ifdef UZEBOX_PHASE4
#include "uzebox_phase4.h"
#ifdef UZEBOX_CONST_FILL
#include "uzebox_fill.h"
#define UZEBOX_SPAN_LOG_TAG "T0618"
#else
#define UZEBOX_SPAN_LOG_TAG "S0617"
#endif
#else
#define UZEBOX_SPAN_LOG_TAG "P0614"
#endif
#endif
#else
#define UZEBOX_SPAN_LOG_TAG "N0613"
#endif
#else
#define UZEBOX_SPAN_LOG_TAG "M0612"
#endif
#include "jack_fatfs_stream.h"
#include "jack_input_devices.h"
#include "jack_memory.h"
#include "jack_nespad_input.h"
#include "jack_storage_paths.h"
#include "jack_tinyusb_input.h"
#include "jack_uzebox_uze.h"
#include "font_8x8.h"
#include "uzebox_sampler.h"
#include "uzebox_placement.h"
#include "uzebox_loop_placement.h"
#include "uzebox_span.h"
#define UZEBOX_CART_DIR "/roms/UZEBOX"
#define UZEBOX_MENU_CARTS 64u
#define UZEBOX_MENU_NAME 256u
#define UZEBOX_MENU_ROWS 9u
#ifdef UZEBOX_MODE1
#define UZEBOX_SOURCE_AUDIO_RATE 15720u
#else
#define UZEBOX_SOURCE_AUDIO_RATE 15734u
#endif
#ifdef UZEBOX_AUDIO_CLOCK
#include "uzebox_audio_clock.h"
static uz_audio_clock_t output_audio_clock=UZ_AUDIO_CLOCK_INIT;
#endif
#ifndef UZEBOX_ASYNC_NES
#define UZEBOX_ASYNC_NES 1
#endif
#ifndef UZEBOX_BUILD_VERSION
#define UZEBOX_BUILD_VERSION "0.5.1-async-io"
#endif
static FATFS filesystem;
static uint8_t hot_memory[8u*1024u];
static jack_audio_frame audio_storage[2048];
static jack_audio_ring audio_ring;
static char eeprom_path[96];
static bool eeprom_path_ready;
static uint16_t keyboard_pad;
#ifndef UZEBOX_AUDIO_CLOCK
static uint32_t source_audio_accum;
#endif
static uint32_t frames;
#if defined(UZEBOX_HOT_PLACEMENT) || defined(UZEBOX_LOOP_PLACEMENT) || defined(UZEBOX_EXTENDED_SPAN)
static uint32_t placement_rom,placement_seed,placement_eeprom;
#endif
static uint32_t rows;
static uint32_t sync_errors;
static uint32_t completed_frames;
static uint32_t row_cap_hits;
static uint32_t call_cap_hits;
static uint32_t profiled_frames;
static uint32_t profile_total_us;
static uint32_t profile_core_us;
static uint32_t profile_render_us;
static uint32_t profile_source_audio_us;
static uint32_t profile_avr_cycles;
static uint32_t max_frame_us;
static uint64_t all_frame_time_us;
static uint32_t min_frame_us=UINT32_MAX;

static uint32_t all_frame_buckets[4];
static uint32_t io_input_us,io_audio_us,io_telemetry_us;
static uint32_t frame_input_us,frame_audio_us,frame_telemetry_us;
#ifdef UZEBOX_PC_SAMPLER
static uz_sample_report sample_report;
static uint32_t sample_rom_crc,sample_seed,sample_eeprom_crc;
#endif

#ifdef UZEBOX_PC_SAMPLER
static char report_buffer[5120];
#elif defined(CU_AVR_REGION_COMPARE)
static char report_buffer[4096];
#else
static char report_buffer[3072];
#endif
static size_t report_length,report_cursor,report_build_length;
static bool report_building,report_overflow;
static uint32_t report_dropped,report_bytes_sent;
static uint16_t rgb332_palette[256];
#ifdef UZEBOX_Y
static const uint16_t render_source_x[320]={
#include "uzebox_render_indices.inc"
};
#else
static uint16_t render_source_x[320];
#endif
static char (*cart_names)[UZEBOX_MENU_NAME];
static unsigned cart_count;
static jack_input_devices *service_devices;
static jack_input_queue *service_queue;
static bool service_usb,service_nes,service_ready;
static uint64_t service_audio_deadline;
static void service_io(void);
static void report_printf(const char *format,...);
#ifdef CU_AVR_TIME_PROFILE
static cu_avr_time_stats_t time_report;
#ifdef CU_AVR_REGION_COMPARE
#include "uzebox_compare_schedule.h"
static uzebox_compare_schedule compare_schedule;
#ifdef CU_AVR_FUSED
#ifndef UZEBOX_FUSED_MODE_MASK
#define UZEBOX_FUSED_MODE_MASK 150u
#endif
volatile uint32_t uzebox_g_control[3]={255u,0u,UZEBOX_FUSED_MODE_MASK};
#endif
#else
static bool time_warmed;
static uint64_t time_epoch;
#endif
auint cu_avr_time_clock(void){return time_us_32();}
static void report_core_time(void){
    cu_avr_time_stats(&time_report);
#ifndef UZEBOX_PC_SAMPLER
    report_printf("UZEBOX_TIME gate=%u rows=%lu sampled=%lu span_us=%llu scalar_us=%llu tile_us=%llu bitmap_us=%llu delay_us=%llu wait_us=%llu alu_us=%llu reject_us=%llu event_us=%llu irq_us=%llu accounting_us=%llu reads=%lu\n",
        (unsigned)time_report.enabled,(unsigned long)time_report.rows,(unsigned long)time_report.sampled_rows,
        (unsigned long long)time_report.sampled_us,
        (unsigned long long)time_report.us[CU_TIME_SCALAR],(unsigned long long)time_report.us[CU_TIME_TILE],
        (unsigned long long)time_report.us[CU_TIME_BITMAP],(unsigned long long)time_report.us[CU_TIME_DELAY],
        (unsigned long long)time_report.us[CU_TIME_WAIT],(unsigned long long)time_report.us[CU_TIME_ALU],
        (unsigned long long)time_report.us[CU_TIME_REJECT],(unsigned long long)time_report.us[CU_TIME_EVENT],
        (unsigned long long)time_report.us[CU_TIME_IRQ],(unsigned long long)time_report.accounting_us,
        (unsigned long)time_report.reads);
    report_printf("UZEBOX_TIME_CALLS dispatches=%lu tile=%lu bitmap=%lu delay=%lu wait=%lu alu=%lu reject=%lu event=%lu irq=%lu\n",
        (unsigned long)time_report.calls[CU_TIME_SCALAR],(unsigned long)time_report.calls[CU_TIME_TILE],
        (unsigned long)time_report.calls[CU_TIME_BITMAP],(unsigned long)time_report.calls[CU_TIME_DELAY],
        (unsigned long)time_report.calls[CU_TIME_WAIT],(unsigned long)time_report.calls[CU_TIME_ALU],
        (unsigned long)time_report.calls[CU_TIME_REJECT],(unsigned long)time_report.calls[CU_TIME_EVENT],
        (unsigned long)time_report.calls[CU_TIME_IRQ]);
    report_printf("UZEBOX_TIME_PC bin_words=1024 us=");
    for(unsigned i=0;i<32;i++)if(time_report.scalar_pc_us[i])
        report_printf("%u:%llu,",i,(unsigned long long)time_report.scalar_pc_us[i]);
    report_printf("\n");
#ifdef CU_AVR_REGION_COMPARE
    report_printf("UZEBOX_COMPARE_TIME region_us=%llu region_reject_us=%llu region_calls=%lu region_reject_calls=%lu\n",
        (unsigned long long)time_report.us[CU_TIME_REGION],
        (unsigned long long)time_report.us[CU_TIME_REGION_REJECT],
        (unsigned long)time_report.calls[CU_TIME_REGION],
        (unsigned long)time_report.calls[CU_TIME_REGION_REJECT]);
#endif
#endif
#ifdef CU_AVR_REGION_COMPARE
    report_printf("UZEBOX_COMPARE phase=%lu window=%lu epoch=%lu warm=%u clean=%u region=%u timing=%u sys_hz=%lu hstx_hz=%lu\n",
        (unsigned long)compare_schedule.phase,(unsigned long)compare_schedule.window,
        (unsigned long)compare_schedule.epoch,compare_schedule.warmed?1u:0u,
        uzebox_compare_clean(&compare_schedule)?1u:0u,
        cu_avr_region_enabled()?1u:0u,(unsigned)time_report.enabled,
        (unsigned long)clock_get_hz(clk_sys),(unsigned long)clock_get_hz(clk_hstx));
#ifdef CU_AVR_FUSED
    cu_avr_fused_stats_t fused;cu_avr_fused_stats(&fused);
    report_printf("UZEBOX_FUSED entries=%lu instructions=%lu cycles=%lu rejected=%lu pc=%lu memory=%lu enabled=%u region_mask=%lu timing_mask=%lu fused_mask=%lu\n",
        (unsigned long)fused.entries,(unsigned long)fused.instructions,(unsigned long)fused.cycles,
        (unsigned long)fused.rejected,(unsigned long)fused.pc,(unsigned long)fused.memory,(unsigned)fused.enabled,
        (unsigned long)uzebox_g_control[0],(unsigned long)uzebox_g_control[1],(unsigned long)uzebox_g_control[2]);
#endif
#endif
}
#endif
#ifdef UZEBOX_HOT_PLACEMENT
static void report_placement(void){
    report_printf("UZEBOX_PLACEMENT v=K0610 mode=%u selected=%08lx flash=%08lx sram=%08lx rom=%08lx seed=%08lx eeprom=%08lx sampler=0\n",
        uzebox_placement_mode(),(unsigned long)uzebox_placement_selected(),
        (unsigned long)(uintptr_t)cu_avr_run_flash,(unsigned long)(uintptr_t)cu_avr_run_sram,
        (unsigned long)placement_rom,(unsigned long)placement_seed,(unsigned long)placement_eeprom);
}
#endif
#ifdef UZEBOX_LOOP_PLACEMENT
static void report_loop_placement(void){
    report_printf("UZEBOX_LOOP_PLACEMENT v=L0611 mode=%u run_mode=%lu run=%08lx selected=%08lx flash=%08lx sram=%08lx rom=%08lx seed=%08lx eeprom=%08lx sampler=0\n",
        uzebox_loop_placement_mode(),(unsigned long)uzebox_placement_control[1],(unsigned long)(uintptr_t)cu_avr_run_sram,
        (unsigned long)uzebox_loop_placement_selected(),(unsigned long)(uintptr_t)cu_avr_loop_run_flash,
        (unsigned long)(uintptr_t)cu_avr_loop_run_sram,(unsigned long)placement_rom,(unsigned long)placement_seed,(unsigned long)placement_eeprom);
}
#endif
#ifdef UZEBOX_EXTENDED_SPAN
#ifdef UZEBOX_AUDIO_BATCH
static void report_audio_batch(void){
#ifdef UZEBOX_MODE1
    uzebox_mode1_stats_t w;uzebox_mode1_stats(&w);
    report_printf("UZEBOX_MODE1 v=W0621 mode=%lu selftest=%lu calls=%lu iterations=%lu pcm=original events=strict\n",
        (unsigned long)w.mode,(unsigned long)w.selftest,(unsigned long)w.calls,(unsigned long)w.iterations);
#endif
#ifdef UZEBOX_POLL
    uzebox_poll_stats_t x;uzebox_poll_stats(&x);
    report_printf("UZEBOX_POLL v=X0622 mode=%lu selftest=%lu calls=%lu iterations=%lu pcm=original events=strict\n",
        (unsigned long)x.mode,(unsigned long)x.selftest,(unsigned long)x.calls,(unsigned long)x.iterations);
#endif
#ifdef UZEBOX_Y
    uzebox_y_stats_t y;uzebox_y_stats(&y);
    report_printf("UZEBOX_Y v=Y0623 mode=%lu selftest=%lu prefix=%lu base=%08lx calls=%lu render=%lu pcm=original events=strict\n",
        (unsigned long)y.mode,(unsigned long)y.selftest,(unsigned long)y.prefix,(unsigned long)y.base,(unsigned long)y.calls,(unsigned long)uzebox_y_render_calls);
#endif
#ifdef UZEBOX_AUDIO_CLOCK
    report_printf("UZEBOX_AUDIO_CLOCK v=U0619 source_hz=%lu phase=%lu epoch_rows=%lu epoch_frames=%lu changes=%lu invalid=%lu primed=%lu prefill=441 queued=%lu highwater=%lu\n",
        (unsigned long)output_audio_clock.rate,(unsigned long)output_audio_clock.phase,
        (unsigned long)output_audio_clock.epoch_rows,(unsigned long)output_audio_clock.epoch_frames,
        (unsigned long)output_audio_clock.changes,(unsigned long)output_audio_clock.invalid,
        (unsigned long)output_audio_clock.started,(unsigned long)audio_ring.count,(unsigned long)audio_ring.high_water);
#endif
    uzebox_audio_batch_stats_t a;uzebox_audio_batch_stats(&a);
    report_printf("UZEBOX_AUDIO_BATCH v=N0613 mode=%lu calls=%lu samples=%lu real=%lu zero=%lu batches=%lu peak=%lu packets=%lu queued=%lu highdrop=%lu pushfail=%lu pending=%lu phase=%lu\n",
        (unsigned long)uzebox_audio_batch_mode(),(unsigned long)a.calls,(unsigned long)a.samples,
        (unsigned long)a.real,(unsigned long)a.zero,(unsigned long)a.batches,(unsigned long)a.peak,
        (unsigned long)a.packets,(unsigned long)a.queued,(unsigned long)a.highdrop,(unsigned long)a.pushfail,
        (unsigned long)a.pending,(unsigned long)a.phase);
#ifdef UZEBOX_HOT_REGION
    uzebox_hot_region_stats_t h;uzebox_hot_region_stats(&h);
    report_printf("UZEBOX_HOT_REGION v=P0614 mode=%lu selftest=%lu matched=%lu base=%08lx entries=%lu instructions=%lu cycles=%lu pcm=original events=strict\n",
        (unsigned long)h.mode,(unsigned long)h.selftest,(unsigned long)h.matched,(unsigned long)h.base,
        (unsigned long)h.entries,(unsigned long)h.instructions,(unsigned long)h.cycles);
#endif
#ifdef UZEBOX_PHASE4
    uzebox_phase4_stats_t b;uzebox_phase4_stats(&b);
    report_printf("UZEBOX_PHASE4 v=S0617 mode=%lu selftest=%lu calls=%lu pcm=original events=strict\n",
        (unsigned long)b.mode,(unsigned long)b.selftest,(unsigned long)b.calls);
#endif
#ifdef UZEBOX_CONST_FILL
    uzebox_fill_stats_t f;uzebox_fill_stats(&f);
    report_printf("UZEBOX_FILL v=T0618 mode=%lu selftest=%lu calls=%lu bytes=%lu pcm=original events=strict\n",
        (unsigned long)f.mode,(unsigned long)f.selftest,(unsigned long)f.calls,(unsigned long)f.bytes);
#endif
#ifdef UZEBOX_SCAN_FUNCTION
    uzebox_scan_stats_t q;uzebox_scan_stats(&q);
    report_printf("UZEBOX_SCAN v=Q0615 mode=%lu selftest=%lu matched=%lu base=%08lx functions=%lu prefixes=%lu bitmaps=%lu pcm=original events=strict\n",
        (unsigned long)q.mode,(unsigned long)q.selftest,(unsigned long)q.matched,(unsigned long)q.base,
        (unsigned long)q.functions,(unsigned long)q.prefixes,(unsigned long)q.bitmaps);
#endif
#ifdef UZEBOX_SCAN_ATTRIBUTION
    uzebox_scan_cost_t c;uzebox_scan_cost_stats(&c);
    report_printf("UZEBOX_SCAN_COST v=R0616 phase=%lu window=%lu warm=%lu mask=%lu sampling=%lu f_samples=%lu f_us=%lu p_samples=%lu p_us=%lu b_samples=%lu b_us=%lu old_samples=%lu old_us=%lu inclusive=1\n",
        (unsigned long)c.phase,(unsigned long)c.window,(unsigned long)c.warm,(unsigned long)c.mask,(unsigned long)c.sampling,
        (unsigned long)c.samples[0],(unsigned long)c.us[0],(unsigned long)c.samples[1],(unsigned long)c.us[1],
        (unsigned long)c.samples[2],(unsigned long)c.us[2],(unsigned long)c.samples[3],(unsigned long)c.us[3]);
#endif
}
#endif
static void report_span(void){
    uzebox_span_stats_t s;uzebox_span_stats(&s);
    report_printf("UZEBOX_SPAN v=" UZEBOX_SPAN_LOG_TAG " mode=%u run_mode=1 run=%08lx loop_mode=0 loop=%08lx rom=%08lx seed=%08lx eeprom=%08lx sampler=0 limit=1024 entries=%lu instructions=%lu cycles=%lu peak=%lu budget=%lu barrier=%lu cap=%lu\n",
        uzebox_span_mode(),(unsigned long)(uintptr_t)cu_avr_run_sram,(unsigned long)(uintptr_t)cu_avr_loop_run,
        (unsigned long)placement_rom,(unsigned long)placement_seed,(unsigned long)placement_eeprom,
        (unsigned long)s.entries,(unsigned long)s.instructions,(unsigned long)s.cycles,(unsigned long)s.max_steps,
        (unsigned long)s.budget_exits,(unsigned long)s.barrier_exits,(unsigned long)s.cap_exits);
}
#endif
#ifdef UZEBOX_PC_SAMPLER
static void report_sampler(void){
    const uz_sample_report *r=&sample_report;
    report_printf("UZEBOX_SAMPLE v=" UZ_SAMPLE_ID " rom=%08lx seed=%08lx eeprom=%08lx mode=%lu available=%lu bus=%lu map=%08lx total=%lu invalid=%lu isr=%lu late=%lu maxlate=%lu body_us=%lu maxbody=%lu guest=%lu guest_bad=%lu timing=%u reads=%lu\n",
        (unsigned long)sample_rom_crc,(unsigned long)sample_seed,(unsigned long)sample_eeprom_crc,
        (unsigned long)r->mode,(unsigned long)r->available,(unsigned long)r->bus_available,(unsigned long)r->map_id,
        (unsigned long)r->pc.total,(unsigned long)r->pc.invalid,(unsigned long)r->pc.isr,
        (unsigned long)r->pc.late,(unsigned long)r->pc.max_late_us,(unsigned long)r->pc.body_us,
        (unsigned long)r->pc.max_body_us,(unsigned long)r->pc.guest_samples,(unsigned long)r->pc.guest_invalid,
        (unsigned)time_report.enabled,(unsigned long)time_report.reads);
    report_printf("UZEBOX_SAMPLE_PC counts=");
    for(unsigned i=0;i<UZ_SAMPLE_KINDS;i++)report_printf("%s%lu",i?",":"",(unsigned long)r->pc.kinds[i]);
    report_printf(" stages=");
    for(unsigned i=0;i<UZ_SAMPLE_STAGES;i++)report_printf("%s%lu",i?",":"",(unsigned long)r->pc.stages[i]);
    report_printf(" guest_bins=");
    for(unsigned i=0;i<32;i++)report_printf("%s%lu",i?",":"",(unsigned long)r->pc.guest[i]);
    report_printf("\n");
    for(unsigned g=0;g<4u;g++){
        const uz_bus_counts *b=&r->bus[g];
        report_printf("UZEBOX_BUS group=%u frames=%lu sat=%lu us=%llu counts=%llu,%llu,%llu,%llu\n",g,
            (unsigned long)b->frames,(unsigned long)b->saturated,(unsigned long long)b->us,
            (unsigned long long)b->counters[0],(unsigned long long)b->counters[1],
            (unsigned long long)b->counters[2],(unsigned long long)b->counters[3]);
    }
    report_printf("UZEBOX_XIP frames=%lu sat=%lu hit=%llu access=%llu\n",
        (unsigned long)r->xip_frames,(unsigned long)r->xip_saturated,
        (unsigned long long)r->xip_hit,(unsigned long long)r->xip_access);
    uz_sample_health h;uint32_t alarm,irq,priority,armed,inte,intf;
    uzebox_sampler_health_snapshot(&h,&alarm,&irq,&priority,&armed,&inte,&intf);
    report_printf("UZEBOX_SAMPLE_HEALTH requested=%lu transitions=%lu irq_total=%lu irq_exits=%lu burst=%lu guard=%lu probe=%lu alarm=%lu irq=%lu priority=%lu armed=%lx inte=%lx intf=%lx last_exc=%08lx last_pc=%08lx last_sp=%08lx last_psr=%08lx\n",
        (unsigned long)h.requested,(unsigned long)h.transitions,(unsigned long)h.irq_total,(unsigned long)h.irq_exits,
        (unsigned long)h.burst,(unsigned long)h.guard,(unsigned long)h.probe_ok,(unsigned long)alarm,(unsigned long)irq,(unsigned long)priority,
        (unsigned long)armed,(unsigned long)inte,(unsigned long)intf,(unsigned long)h.last_exc,(unsigned long)h.last_pc,(unsigned long)h.last_sp,(unsigned long)h.last_psr);
}
#endif
static bool poll_nes(void){
#if UZEBOX_ASYNC_NES
    return jack_nespad_input_service(time_us_32());
#else
    return jack_nespad_input_poll();
#endif
}
static void report_begin(void){
    report_building=report_cursor==report_length;report_build_length=0u;report_overflow=false;
    if(!report_building)++report_dropped;
}
static void report_printf(const char *format,...){
    if(!report_building||report_overflow)return;
    va_list args;va_start(args,format);
    const int count=vsnprintf(report_buffer+report_build_length,
        sizeof(report_buffer)-report_build_length,format,args);
    va_end(args);
    if(count<0||(size_t)count>=sizeof(report_buffer)-report_build_length){report_overflow=true;return;}
    report_build_length+=(size_t)count;
}
static void report_publish(void){
    if(!report_building)return;
    if(report_overflow){++report_dropped;report_length=0u;report_cursor=0u;}
    else{report_length=report_build_length;report_cursor=0u;}
    report_building=false;
}
static void report_pump(void){
    unsigned remaining=16u;
    while(report_cursor<report_length&&remaining&&uart_is_writable(uart_default)){
        --remaining;
        uart_putc_raw(uart_default,report_buffer[report_cursor++]);++report_bytes_sent;
    }
    if(report_cursor==report_length && uzebox_aa_pending())uzebox_aa_pump(remaining);
}
static void boot_stage(const char *stage){printf("UZEBOX_BOOT %s\n",stage);stdio_flush();}
static uint16_t rgb332_to_rgb555(uint8_t color){
    const uint16_t r=(uint16_t)(((color&7u)*31u)/7u);
    const uint16_t g=(uint16_t)((((color>>3u)&7u)*31u)/7u);
    const uint16_t b=(uint16_t)((((color>>6u)&3u)*31u)/3u);
    return (uint16_t)((r<<10u)|(g<<5u)|b);
}
static void prepare_render_tables(void){
    for(unsigned color=0u;color<256u;++color)rgb332_palette[color]=rgb332_to_rgb555((uint8_t)color);
#ifndef UZEBOX_Y
    for(unsigned x=0u;x<320u;++x){
        unsigned src=291u+(x*1456u)/320u;
        if(src>2031u)src=2031u;
        render_source_x[x]=(uint16_t)src;
    }
#endif
}
static void clear_video(void){memset(hstx_getframebuffer(),0,320u*240u*sizeof(uint16_t));}
#ifdef UZEBOX_Y
#define UZ_RENDER_ATTR __attribute__((noinline,noclone))
#else
#define UZ_RENDER_ATTR
#endif
static void UZ_RENDER_ATTR render_row(const cu_row_t *row){
    if(row->pno<19u||row->pno>=244u)return;
#ifdef UZEBOX_Y
    if(uzebox_y_active()){
        uzebox_render_fast(row->pixels,(unsigned)row->pno,hstx_getframebuffer(),rgb332_palette);
        uzebox_y_render_calls++;return;
    }
#endif
    const unsigned y=7u+(unsigned)row->pno-19u;uint16_t *dst=(uint16_t *)hstx_getframebuffer()+y*320u;
    for(unsigned x=0;x<320u;++x)dst[x]=rgb332_palette[row->pixels[render_source_x[x]]];
}
#undef UZ_RENDER_ATTR
#ifdef UZEBOX_Y
#include "uzebox_render_selftest.inc"
#endif
static void queue_source_audio(uint8_t sample){
#ifdef UZEBOX_AUDIO_CLOCK
    const int16_t value=(int16_t)(((int)sample-128)*256);
    const uint32_t count=uz_audio_clock_emit(&output_audio_clock);
    for(uint32_t i=0;i<count;++i)(void)jack_audio_push(&audio_ring,(jack_audio_frame){value,value});
#else
    const int16_t value=(int16_t)(((int)sample-128)*256);source_audio_accum+=44100u;
    while(source_audio_accum>=UZEBOX_SOURCE_AUDIO_RATE){source_audio_accum-=UZEBOX_SOURCE_AUDIO_RATE;(void)jack_audio_push(&audio_ring,(jack_audio_frame){value,value});}
#endif
}
static bool uze_name(const char *name){
    const size_t length=strlen(name);
    if(length<5u||length>=UZEBOX_MENU_NAME)return false;
    const char *suffix=name+length-4u;
    return suffix[0]=='.'&&
        (suffix[1]=='u'||suffix[1]=='U')&&
        (suffix[2]=='z'||suffix[2]=='Z')&&
        (suffix[3]=='e'||suffix[3]=='E');
}
static FRESULT scan_carts(void){
    cart_count=0u;
    DIR dir;FILINFO file;
    FRESULT result=f_opendir(&dir,UZEBOX_CART_DIR);
    if(result!=FR_OK)return result;
    while((result=f_readdir(&dir,&file))==FR_OK&&file.fname[0]){
        if((file.fattrib&AM_DIR)||!uze_name(file.fname))continue;
        if(cart_count<UZEBOX_MENU_CARTS){strcpy(cart_names[cart_count],file.fname);++cart_count;}
    }
    f_closedir(&dir);
    for(unsigned i=1u;i<cart_count;++i){
        char name[UZEBOX_MENU_NAME];strcpy(name,cart_names[i]);
        unsigned j=i;
        while(j>0u&&strcmp(cart_names[j-1u],name)>0){strcpy(cart_names[j],cart_names[j-1u]);--j;}
        strcpy(cart_names[j],name);
    }
    return result;
}
static void menu_text(uint16_t *pixels,unsigned x,unsigned y,const char *text,uint16_t color,unsigned limit){
    for(unsigned i=0u;text[i]&&i<limit&&x+8u<=320u;++i,x+=8u){
        unsigned c=(unsigned char)text[i];if(c<32u||c>126u)c='?';
        for(unsigned row=0u;row<8u;++row){
            const uint8_t bits=(uint8_t)font_8x8[row*95u+c-32u];
            for(unsigned col=0u;col<8u;++col)
                if(bits&(1u<<col))pixels[(y+row)*320u+x+col]=color;
        }
    }
}
static void draw_cart_menu(unsigned selected,FRESULT scan_result){
    uint16_t *pixels=(uint16_t *)hstx_getframebuffer();
    for(unsigned i=0u;i<320u*240u;++i)pixels[i]=0x0421u;
    menu_text(pixels,24u,20u,"UZEBOX CARTRIDGES",0x7fffu,35u);
    char status[40];snprintf(status,sizeof(status),"%u GAME%s",cart_count,cart_count==1u?"":"S");
    menu_text(pixels,24u,38u,status,0x3defu,35u);
    if(scan_result!=FR_OK){menu_text(pixels,24u,85u,"SD DIRECTORY ERROR",0x7c00u,35u);return;}
    if(cart_count==0u){
        menu_text(pixels,24u,85u,"NO .UZE FILES",0x7c00u,35u);
        menu_text(pixels,24u,102u,"COPY TO /ROMS/UZEBOX/",0x7fffu,35u);
    }else{
        const unsigned page=selected/UZEBOX_MENU_ROWS*UZEBOX_MENU_ROWS;
        for(unsigned row=0u;row<UZEBOX_MENU_ROWS&&page+row<cart_count;++row){
            const unsigned y=61u+row*17u;
            const bool active=page+row==selected;
            if(active)for(unsigned x=20u;x<300u;++x)
                for(unsigned dy=0u;dy<12u;++dy)pixels[(y-2u+dy)*320u+x]=0x03e0u;
            menu_text(pixels,25u,y,active?">":" ",active?0x0000u:0x7fffu,1u);
            menu_text(pixels,41u,y,cart_names[page+row],active?0x0000u:0x7fffu,32u);
        }
        snprintf(status,sizeof(status),"%u / %u",selected+1u,cart_count);
        menu_text(pixels,24u,215u,status,0x3defu,35u);
    }
    menu_text(pixels,130u,215u,"UP/DOWN  A/X START",0x3defu,23u);
}
static auint pad_to_snes(uint16_t pad){
    auint out=0u;
    if(pad&JACK_PAD_B)out|=CU_CTR_SNES_M_B;
    if(pad&JACK_PAD_Y)out|=CU_CTR_SNES_M_Y;
    if(pad&JACK_PAD_SELECT)out|=CU_CTR_SNES_M_SELECT;
    if(pad&JACK_PAD_START)out|=CU_CTR_SNES_M_START;
    if(pad&JACK_PAD_UP)out|=CU_CTR_SNES_M_UP;
    if(pad&JACK_PAD_DOWN)out|=CU_CTR_SNES_M_DOWN;
    if(pad&JACK_PAD_LEFT)out|=CU_CTR_SNES_M_LEFT;
    if(pad&JACK_PAD_RIGHT)out|=CU_CTR_SNES_M_RIGHT;
    if(pad&JACK_PAD_A)out|=CU_CTR_SNES_M_A;
    if(pad&JACK_PAD_X)out|=CU_CTR_SNES_M_X;
    if(pad&JACK_PAD_L)out|=CU_CTR_SNES_M_LSH;
    if(pad&JACK_PAD_R)out|=CU_CTR_SNES_M_RSH;
    return out;
}
static void route_key(const jack_input_event *event){
    if(event->type!=JACK_INPUT_KEY_DOWN&&event->type!=JACK_INPUT_KEY_UP)return;
    const bool down=event->type==JACK_INPUT_KEY_DOWN;uint16_t bit=0u;
    switch(event->code){case JACK_KEY_LEFT:bit=JACK_PAD_LEFT;break;case JACK_KEY_RIGHT:bit=JACK_PAD_RIGHT;break;
    case JACK_KEY_UP:bit=JACK_PAD_UP;break;case JACK_KEY_DOWN:bit=JACK_PAD_DOWN;break;case JACK_KEY_ENTER:bit=JACK_PAD_START;break;
    case JACK_KEY_ESCAPE:bit=JACK_PAD_SELECT;break;case 'z':case 'Z':bit=JACK_PAD_B;break;case 'x':case 'X':bit=JACK_PAD_A;break;
    case 'a':case 'A':bit=JACK_PAD_Y;break;case 's':case 'S':bit=JACK_PAD_X;break;default:return;}
    if(down)keyboard_pad|=bit;else keyboard_pad&=(uint16_t)~bit;
}
static int select_cart(jack_input_devices *devices,jack_input_queue *queue,bool usb_ok,bool nes_ok,FRESULT scan_result){
    unsigned selected=0u;uint16_t previous=0u;
    uint32_t last_frame=hstx_getframecounter();
#ifdef UZEBOX_MENU_DIAG
    uint32_t next_diag=time_us_32(),loops=0u;
    printf("UZEBOX_MENU_DIAG v=QD1 stage=draw_begin usb=%u nes=%u second=%d hstx=%lu\n",usb_ok?1u:0u,nes_ok?1u:0u,jack_tinyusb_second_port_result(),(unsigned long)last_frame);stdio_flush();
    uzebox_menu_phase(1u);
#endif
    draw_cart_menu(selected,scan_result);
#ifdef UZEBOX_MENU_DIAG
    printf("UZEBOX_MENU_DIAG v=QD1 stage=draw_end hstx=%lu\n",(unsigned long)hstx_getframecounter());stdio_flush();
#endif
    while(true){
#ifdef UZEBOX_MENU_DIAG
        const bool first=loops++==0u;
        if(first){printf("UZEBOX_MENU_DIAG v=QD1 stage=usb_poll_begin\n");stdio_flush();}
        uzebox_menu_phase(2u);
#endif
        if(usb_ok)jack_tinyusb_input_poll();
#ifdef UZEBOX_MENU_DIAG
        if(first){printf("UZEBOX_MENU_DIAG v=QD1 stage=usb_poll_end nes_poll_begin\n");stdio_flush();}
        uzebox_menu_phase(3u);
#endif
        if(nes_ok)(void)poll_nes();
#ifdef UZEBOX_MENU_DIAG
        if(first){printf("UZEBOX_MENU_DIAG v=QD1 stage=nes_poll_end\n");stdio_flush();}
        uzebox_menu_phase(4u);
#endif
        jack_input_event event;while(jack_input_pop(queue,&event))route_key(&event);
        const uint32_t frame=hstx_getframecounter();
#ifdef UZEBOX_MENU_DIAG
        if((int32_t)(time_us_32()-next_diag)>=0){
            uint32_t sp;__asm volatile("mrs %0, msp":"=r"(sp));
            jack_nespad_input_stats ns;jack_nespad_input_get_stats(&ns);
            printf("UZEBOX_MENU_DIAG v=QD1 stage=alive loops=%lu hstx=%lu prev=%lu pads=%04x,%04x keys=%04x starts=%lu completed=%lu pending=%u selected=%u sp=%08lx\n",
                (unsigned long)loops,(unsigned long)frame,(unsigned long)last_frame,devices->pads[0],devices->pads[1],keyboard_pad,
                (unsigned long)ns.starts,(unsigned long)ns.completed,ns.pending?1u:0u,selected,(unsigned long)sp);stdio_flush();
            next_diag=time_us_32()+1000000u;
        }
#endif
        if(frame==last_frame){tight_loop_contents();continue;}
        last_frame=frame;
        const uint16_t buttons=(uint16_t)(devices->pads[0]|keyboard_pad);
        const uint16_t pressed=(uint16_t)(buttons&~previous);
        previous=buttons;
        if(cart_count){
            if(pressed&JACK_PAD_UP)selected=selected?selected-1u:cart_count-1u;
            if(pressed&JACK_PAD_DOWN)selected=(selected+1u)%cart_count;
            if(pressed&(JACK_PAD_A|JACK_PAD_B|JACK_PAD_X|JACK_PAD_START))return (int)selected;
        }
#ifdef UZEBOX_MENU_DIAG
        uzebox_menu_phase(5u);
#endif
        draw_cart_menu(selected,scan_result);
    }
}
static bool load_game(jack_memory_layout *memory,const char *name,jack_uzebox_uze_info *info,uint32_t *content_crc){
    char path[sizeof(UZEBOX_CART_DIR)+UZEBOX_MENU_NAME];
    const int path_len=snprintf(path,sizeof(path),UZEBOX_CART_DIR "/%s",name);
    if(path_len<0||(size_t)path_len>=sizeof(path))return false;
    jack_stream stream;jack_fatfs_file file;if(!jack_fatfs_open_read(&stream,&file,path))return false;
    const uint64_t length=jack_stream_size(&stream);if(length<513u||length>66047u){jack_stream_close(&stream);return false;}
    uint8_t *data=(uint8_t *)jack_memory_alloc_bulk(memory,(size_t)length,4u);if(!data){jack_stream_close(&stream);return false;}
    const bool read_ok=jack_stream_seek(&stream,0u)&&jack_stream_read_exact(&stream,data,(size_t)length);jack_stream_close(&stream);
    if(!read_ok||!jack_uzebox_parse_uze(data,(size_t)length,info))return false;
    cu_state_cpu_t *cpu=cu_avr_get_state();memset(cpu,0,sizeof(*cpu));memcpy(cpu->crom,info->program,info->program_size);memset(cpu->eepr,0xff,sizeof(cpu->eepr));
    *content_crc=jack_uzebox_crc32(0u,data,(size_t)length);return true;
}
static bool load_eeprom(uint32_t crc){
    eeprom_path_ready=jack_storage_save_path(eeprom_path,sizeof(eeprom_path),"UZEBOX",crc,".eeprom")&&jack_fatfs_ensure_save_dir("UZEBOX",crc)==FR_OK;
    if(!eeprom_path_ready)return false;
    jack_stream stream;
    jack_fatfs_file file;
    if(!jack_fatfs_open_read(&stream,&file,eeprom_path))return true;
    cu_state_cpu_t *cpu=cu_avr_get_state();const bool ok=jack_stream_size(&stream)==sizeof(cpu->eepr)&&jack_stream_read_exact(&stream,cpu->eepr,sizeof(cpu->eepr));jack_stream_close(&stream);return ok;
}
static bool save_eeprom(void){
    if(!eeprom_path_ready)return false;
    jack_stream stream;
    jack_fatfs_file file;
    if(!jack_fatfs_open_write(&stream,&file,eeprom_path))return false;
    const cu_state_cpu_t *cpu=cu_avr_get_state();const bool ok=jack_stream_write_exact(&stream,cpu->eepr,sizeof(cpu->eepr))&&jack_stream_flush(&stream);jack_stream_close(&stream);return ok;
}
static void run_frame(void){
    uzebox_sampler_frame_begin();
    const bool profile=(frames&7u)==0u;
    const uint32_t frame_start=time_us_32();
    uzebox_aa_frame_begin(frame_start);
    const uint32_t cycle_start=profile?cu_avr_getcycle():0u;
    const uint32_t input_start=io_input_us,audio_start=io_audio_us,telemetry_start=io_telemetry_us;
    unsigned generated=0u,calls=0u;bool complete=false;
    for(;calls<20000u&&generated<268u;++calls){
        if((calls&3u)==0u)service_io();
        uint32_t stage_start=profile?time_us_32():0u;
        uzebox_sampler_stage(UZ_CORE);
        const auint flags=cu_avr_run();
        if(profile)profile_core_us+=time_us_32()-stage_start;
        if(flags&CU_GET_ROW){
            const cu_row_t *row=cu_avr_get_row();
            stage_start=profile?time_us_32():0u;
            uzebox_sampler_stage(UZ_RENDER);render_row(row);
            if(profile)profile_render_us+=time_us_32()-stage_start;
            stage_start=profile?time_us_32():0u;
            uzebox_sampler_stage(UZ_SOURCE);queue_source_audio((uint8_t)row->sample);
            if(profile)profile_source_audio_us+=time_us_32()-stage_start;
            ++generated;++rows;if(flags&CU_SYNCERR)++sync_errors;
        }
        if(flags&CU_GET_FRAME){complete=true;break;}
    }
    uzebox_sampler_stage(UZ_IDLE);
    uzebox_sampler_frame_end();
#ifdef UZEBOX_AUDIO_CLOCK
    uz_audio_clock_frame(&output_audio_clock,generated,complete);
#endif
    if(complete)++completed_frames;
    else if(generated>=268u)++row_cap_hits;
    else ++call_cap_hits;
    ++frames;
    const uint32_t elapsed=time_us_32()-frame_start;
    all_frame_time_us+=elapsed;
    if(elapsed<min_frame_us)min_frame_us=elapsed;
    if(elapsed>max_frame_us)max_frame_us=elapsed;
    if(elapsed<20000u)++all_frame_buckets[0];
    else if(elapsed<100000u)++all_frame_buckets[1];
    else if(elapsed<180000u)++all_frame_buckets[2];
    else ++all_frame_buckets[3];
    frame_input_us+=io_input_us-input_start;frame_audio_us+=io_audio_us-audio_start;
    frame_telemetry_us+=io_telemetry_us-telemetry_start;
    if(profile){
        ++profiled_frames;
        profile_total_us+=elapsed;
        profile_avr_cycles+=(uint32_t)(cu_avr_getcycle()-cycle_start);
    }
    uzebox_aa_frame_end();
}
#ifndef UZEBOX_AUDIO_BATCH
static void audio_sink(void *context,int16_t left,int16_t right){(void)context;hstx_push_audio_sample(left,right);}
#endif
static void service_io(void){
    if(!service_ready)return;
    uzebox_sampler_stage(UZ_INPUT);
    uint32_t stage_start=time_us_32();
    if(service_usb)jack_tinyusb_input_poll();
    if(service_nes)(void)poll_nes();
    jack_input_event event;while(jack_input_pop(service_queue,&event))route_key(&event);
    cu_ctr_setsnes(0u,pad_to_snes((uint16_t)(service_devices->pads[0]|keyboard_pad)));
    cu_ctr_setsnes(1u,pad_to_snes(service_devices->pads[1]));
    io_input_us+=time_us_32()-stage_start;stage_start=time_us_32();
    uzebox_sampler_stage(UZ_DRAIN);
    uint64_t now=time_us_64()*44100u;
#ifdef UZEBOX_AUDIO_CLOCK
    const size_t due=uz_audio_clock_service(&output_audio_clock,(uint32_t)audio_ring.count,now,&service_audio_deadline);
#else
    if(now>service_audio_deadline+1000000ull*2048)service_audio_deadline=now;
    size_t due=0;
    while(service_audio_deadline<=now&&due<512){service_audio_deadline+=1000000u;++due;}
#endif
#ifdef UZEBOX_AUDIO_BATCH
    if(due)(void)uzebox_audio_drain(&audio_ring,due);
#else
    if(due)(void)jack_audio_drain(&audio_ring,due,audio_sink,NULL);
#endif
    io_audio_us+=time_us_32()-stage_start;stage_start=time_us_32();
    uzebox_sampler_stage(UZ_UART);
    report_pump();io_telemetry_us+=time_us_32()-stage_start;
    uzebox_sampler_stage(UZ_IDLE);
}
#ifdef CU_AVR_BOARD_PROFILE
static void report_avr_hotspots(void){
    const cu_avr_board_profile_t *profile=cu_avr_board_profile_state();
    auint best_pc[16]={0u},best_count[16]={0u};
    bool used_opcodes[128]={false};
    if(!profile->pc_counts)return;
    for(auint pc=0u;pc<CU_AVR_BOARD_PROFILE_PCS;++pc){
        const auint count=profile->pc_counts[pc];
        if(count==0u||count<=best_count[15])continue;
        unsigned rank=0u;while(rank<16u&&count<=best_count[rank])++rank;
        for(unsigned i=15u;i>rank;--i){best_count[i]=best_count[i-1u];best_pc[i]=best_pc[i-1u];}
        best_count[rank]=count;best_pc[rank]=pc;
    }
    report_printf("UZEBOX_HOT samples=%lu pc=",(unsigned long)profile->samples);
    for(unsigned rank=0u;rank<16u&&best_count[rank]!=0u;++rank){
        report_printf("%s%04lx:%02lx:%lu",rank?",":"",(unsigned long)best_pc[rank],
            (unsigned long)cu_avr_board_profile_opcode(best_pc[rank]),(unsigned long)best_count[rank]);
    }
    report_printf("\nUZEBOX_HOT_OP samples=%lu op=",(unsigned long)profile->samples);
    for(unsigned rank=0u;rank<16u;++rank){
        unsigned best=128u;
        for(unsigned i=0u;i<128u;++i){
            if(used_opcodes[i]||profile->opcodes[i]==0u)continue;
            if(best==128u||profile->opcodes[i]>profile->opcodes[best])best=i;
        }
        if(best==128u)break;
        used_opcodes[best]=true;
        report_printf("%s%02x:%lu",rank?",":"",best,(unsigned long)profile->opcodes[best]);
    }
    report_printf("\n");
}
#endif
#ifdef CU_AVR_LOOP_QUALIFY
static void report_avr_loop_qualification(unsigned input_active_frames){
    const cu_avr_loop_qualify_stats_t *s=cu_avr_loop_qualify_state();
    const unsigned long candidate_pm=s->samples?(unsigned long)((uint64_t)s->candidate*1000u/s->samples):0ul;
    const unsigned long eligible_pm=s->samples?(unsigned long)((uint64_t)s->eligible*1000u/s->samples):0ul;
    const unsigned long mean_len10=s->candidate?(unsigned long)((uint64_t)s->candidate_length_sum*10u/s->candidate):0ul;
    const unsigned long mean_repeat10=s->repeat_samples?(unsigned long)((uint64_t)s->repeat_iterations*10u/s->repeat_samples):0ul;
    bool used[128]={false};
    report_printf("UZEBOX_LOOPQ v=6 samples=%lu candidate=%lu eligible=%lu candidate_pm=%lu eligible_pm=%lu exact=%lu affine=%lu exact_candidate=%lu affine_candidate=%lu exact_eligible=%lu affine_eligible=%lu window=%lu event=%lu row=%lu len10=%lu verified_repeats10=%lu capped=%lu diverged=%lu cycle_mismatch=%lu repeat_event=%lu repeat_row=%lu repeat_guard_stops=%lu rom_updates=%lu rom_aborts=%lu input_active=%u\n",
        (unsigned long)s->samples,(unsigned long)s->candidate,(unsigned long)s->eligible,candidate_pm,eligible_pm,
        (unsigned long)s->closed_exact,(unsigned long)s->closed_affine,
        (unsigned long)s->exact_candidate,(unsigned long)s->affine_candidate,
        (unsigned long)s->exact_eligible,(unsigned long)s->affine_eligible,
        (unsigned long)s->window_limit,(unsigned long)s->event_bound,(unsigned long)s->row_bound,
        mean_len10,mean_repeat10,(unsigned long)s->repeat_capped,(unsigned long)s->diverged,
        (unsigned long)s->cycle_mismatch,
        (unsigned long)s->repeat_event_stops,(unsigned long)s->repeat_row_stops,
        (unsigned long)s->repeat_guard_stops,
        (unsigned long)s->rom_updates,(unsigned long)s->rom_aborts,input_active_frames);
    report_printf("UZEBOX_LOOPQ_BLOCK op=");
    for(unsigned rank=0u;rank<6u;++rank){
        unsigned best=128u;
        for(unsigned op=0u;op<128u;++op){
            if(used[op]||s->blocked_opcode[op]==0u)continue;
            if(best==128u||s->blocked_opcode[op]>s->blocked_opcode[best])best=op;
        }
        if(best==128u)break;
        used[best]=true;
        report_printf("%s%02x:%lu",rank?",":"",best,(unsigned long)s->blocked_opcode[best]);
    }
    report_printf("\n");
}
#endif
#if defined(UZEBOX_SCAN_FUNCTION) || defined(UZEBOX_PHASE4)
static int __attribute__((noinline)) uzebox_boot(void){
#else
int main(void){
#endif
    stdio_init_all();
#ifdef CU_AVR_LOOP_NATIVE
    boot_stage(UZEBOX_BUILD_VERSION " loop-native entry; clock begin");
#elif defined(CU_AVR_LOOP_QUALIFY)
    boot_stage("diag20 interpreter loop qualification v6 entry; clock begin");
#elif defined(CU_AVR_BOARD_PROFILE)
    boot_stage("diag12 exact-board-profile entry; clock begin");
#elif defined(CU_AVR_TRACE_REFERENCE)
    boot_stage("diag13 canonical-trace reference entry; clock begin");
#elif defined(CU_AVR_NATIVE_TRACE)
#ifdef CU_AVR_NATIVE_TRACE_LD
#ifdef CU_AVR_NATIVE_TRACE_CHAIN
    boot_stage("diag19 native chained SRAM-LD trace entry; clock begin");
#else
    boot_stage("diag18 native SRAM-LD trace entry; clock begin");
#endif
#else
#ifdef CU_AVR_NATIVE_TRACE_CHAIN
    boot_stage("diag17 native chained trace entry; clock begin");
#else
    boot_stage("diag16 native cross-branch trace entry; clock begin");
#endif
#endif
#elif defined(CU_AVR_NATIVE_THUMB)
#ifdef CU_AVR_NATIVE_VERIFY
    boot_stage("diag10 native-thumb min3 verified entry; clock begin");
#else
    boot_stage("diag9 native-thumb lpm-pixel entry; clock begin");
#endif
#elif defined(CU_AVR_BLOCK_CACHE)
    boot_stage("diag7 block-cache entry; clock begin");
#else
    boot_stage(UZEBOX_BUILD_VERSION " interpreter entry; clock begin");
#endif
    vreg_set_voltage(VREG_VOLTAGE_1_30);sleep_ms(10);
#if UZEBOX_SYS_KHZ == 378000
    if(!uzebox_clock_prepare_qmi(clock_get_hz(clk_sys)))return 2;
#endif
    if(!set_sys_clock_khz(UZEBOX_SYS_KHZ,true))return 2;
    stdio_init_all();sleep_ms(200);boot_stage("clock ready; psram begin");
#if UZEBOX_SYS_KHZ == 378000
    printf("UZEBOX_CLOCK_CONFIG cpu_hz=%lu target_khz=378000 vreg_mv=1300 hstx_target_hz=126000000 qmi0=%08lx qmi1=%08lx experiment=1 T=ON\n",
        (unsigned long)clock_get_hz(clk_sys),(unsigned long)qmi_hw->m[0].timing,(unsigned long)qmi_hw->m[1].timing);stdio_flush();
#endif
#ifdef UZEBOX_HOT_PLACEMENT
    if(uzebox_placement_control[0]!=0x4b313056u||uzebox_placement_mode()>1u)panic("Unbound K placement image");
    printf("UZEBOX_PLACEMENT_CONFIG version=%s mode=%u target=cu_avr_run flash=%08lx sram=%08lx selected=%08lx matched_layout=1 sampler=0 input=live\n",
        UZEBOX_BUILD_VERSION,uzebox_placement_mode(),(unsigned long)(uintptr_t)cu_avr_run_flash,
        (unsigned long)(uintptr_t)cu_avr_run_sram,(unsigned long)uzebox_placement_selected());stdio_flush();
#endif
#ifdef UZEBOX_LOOP_PLACEMENT
    if(uzebox_placement_control[0]!=0x4b313056u||uzebox_placement_control[1]!=1u||
       uzebox_loop_placement_control[0]!=0x4c313156u||uzebox_loop_placement_mode()>1u)panic("Unbound L loop placement image");
    printf("UZEBOX_LOOP_PLACEMENT_CONFIG version=%s mode=%u target=cu_avr_loop_run run_mode=%lu run=%08lx flash=%08lx sram=%08lx selected=%08lx matched_layout=1 sampler=0 input=live\n",
        UZEBOX_BUILD_VERSION,uzebox_loop_placement_mode(),(unsigned long)uzebox_placement_control[1],
        (unsigned long)(uintptr_t)cu_avr_run_sram,(unsigned long)(uintptr_t)cu_avr_loop_run_flash,
        (unsigned long)(uintptr_t)cu_avr_loop_run_sram,(unsigned long)uzebox_loop_placement_selected());stdio_flush();
#endif
#ifdef UZEBOX_EXTENDED_SPAN
    if(uzebox_placement_control[0]!=0x4b313056u||uzebox_placement_control[1]!=1u||
       uzebox_span_control[0]!=0x4d313256u||uzebox_span_mode()>1u)panic("Unbound M span image");
    printf("UZEBOX_SPAN_CONFIG version=%s mode=%u target=register_span run_mode=1 run=%08lx loop_mode=0 loop=%08lx limit=1024 matched_layout=1 sampler=0 input=live\n",
        UZEBOX_BUILD_VERSION,uzebox_span_mode(),(unsigned long)(uintptr_t)cu_avr_run_sram,
        (unsigned long)(uintptr_t)cu_avr_loop_run);stdio_flush();
#endif
#ifdef UZEBOX_AUDIO_BATCH
    if(uzebox_audio_batch_control[0]!=0x4e313356u||uzebox_audio_batch_mode()>1u||uzebox_span_mode()!=1u)panic("Unbound N audio batch image");
    printf("UZEBOX_AUDIO_BATCH_CONFIG version=%s mode=%lu span_mode=1 pcm=original chunk=4 max_due=512 matched_layout=1 input=live\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_audio_batch_mode());stdio_flush();
#endif
    printf("UZEBOX_IO_CONFIG version=%s async_nes=%u nes_interval_us=%u telemetry=uart_fifo telemetry_budget=16\n",
        UZEBOX_BUILD_VERSION,(unsigned)UZEBOX_ASYNC_NES,UZEBOX_ASYNC_NES?5000u:0u);stdio_flush();
#ifdef CU_AVR_REGION
    printf("UZEBOX_REGION_CONFIG version=%s enabled=1 max_instructions=128 io=scalar events=strict original_isa=1\n",UZEBOX_BUILD_VERSION);stdio_flush();
#endif
#ifdef CU_AVR_TIME_PROFILE
#ifdef CU_AVR_REGION_COMPARE
#if defined(UZEBOX_PC_SAMPLER) || defined(UZEBOX_HOT_PLACEMENT) || defined(UZEBOX_LOOP_PLACEMENT) || defined(UZEBOX_EXTENDED_SPAN)
    printf("UZEBOX_COMPARE_CONFIG version=%s image=same alu=1 phases=8 reports_per_phase=8 settle_reports=1 warmup_frames=60 legacy_timing=0 input=live\n",UZEBOX_BUILD_VERSION);stdio_flush();
#else
    printf("UZEBOX_COMPARE_CONFIG version=%s image=same alu=1 phases=8 reports_per_phase=8 settle_reports=1 warmup_frames=60 row_gap=128..383 units=wall_us input=live\n",UZEBOX_BUILD_VERSION);stdio_flush();
#endif
#ifdef CU_AVR_FUSED
    printf("UZEBOX_FUSED_CONFIG version=%s words=174 match=opcode_structure io=original region_mask=%lu timing_mask=%lu fused_mask=%lu\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_g_control[0],(unsigned long)uzebox_g_control[1],(unsigned long)uzebox_g_control[2]);stdio_flush();
#ifdef CU_AVR_SCALAR_INLINE
    printf("UZEBOX_INLINE_CONFIG version=%s ordinary=forced timed=unchanged isa=original\n",UZEBOX_BUILD_VERSION);stdio_flush();
#endif
    if(uzebox_g_control[0]!=255u||uzebox_g_control[1]!=0u)panic("Invalid G control data");
#endif
#else
    printf("UZEBOX_TIME_CONFIG version=%s alu=%u row_gap=128..383 gate_us=5000000 units=wall_us native=inclusive scalar_pc_bin_words=1024\n",
        UZEBOX_BUILD_VERSION,
#ifdef CU_AVR_DISABLE_ALU
        0u
#else
        1u
#endif
    );stdio_flush();
#endif
#endif
#ifdef CU_AVR_LOOP_NATIVE
    boot_stage("resident loop selftest begin");
    const bool loop_ok=cu_avr_loop_selftest();
    printf("UZEBOX_BOOT loop selftest=%u fallback=%u\n",loop_ok?1u:0u,loop_ok?0u:1u);stdio_flush();
#ifdef UZEBOX_HOT_REGION
    if(uzebox_hot_region_control[0]!=0x50313456u || uzebox_hot_region_control[1]>1u || uzebox_audio_batch_mode()!=1u)
        panic("Unbound P hot-region image");
    boot_stage("static hot-region selftest begin");
    const bool hot_ok=uzebox_hot_region_selftest();
    printf("UZEBOX_HOT_REGION_CONFIG version=%s v=P0614 mode=%lu selftest=%u fallback=%u N=ON span=1 words=132 blocks=66 pcm=original events=strict matched_layout=1\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_hot_region_control[1],hot_ok?1u:0u,hot_ok?0u:1u);stdio_flush();
#endif
#ifdef UZEBOX_PHASE4
    if(uzebox_phase4_control[0]!=0x53313756u || uzebox_phase4_control[1]>1u || uzebox_hot_region_control[1]!=1u)
        panic("Unbound S phase4 image");
    boot_stage("phase4 selftest begin");
    const bool phase4_ok=uzebox_phase4_selftest();
    printf("UZEBOX_PHASE4_CONFIG version=%s v=S0617 mode=%lu selftest=%u fallback=%u P=ON N=ON guard_words=98 pcm=original events=strict fixed=1\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_phase4_control[1],phase4_ok?1u:0u,phase4_ok?0u:1u);stdio_flush();
#endif
#ifdef UZEBOX_CONST_FILL
    if(uzebox_fill_control[0]!=0x54313856u || uzebox_fill_control[1]>1u || uzebox_phase4_control[1]!=1u)
        panic("Unbound T constant-fill image");
    boot_stage("constant-fill selftest begin");
    const bool fill_ok=uzebox_fill_selftest();
    printf("UZEBOX_FILL_CONFIG version=%s v=T0618 mode=%lu selftest=%u fallback=%u S=ON P=ON N=ON vectors=420 pcm=original events=strict fixed=1\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_fill_control[1],fill_ok?1u:0u,fill_ok?0u:1u);stdio_flush();
#ifdef UZEBOX_MODE1
    if(uzebox_mode1_control[0]!=0x57323156u||uzebox_mode1_control[1]>1u||uzebox_fill_control[1]!=1u)panic("Unbound W Mode1 image");
    boot_stage("Mode1 tile selftest begin");
    const bool mode1_ok=uzebox_mode1_selftest();
    printf("UZEBOX_MODE1_CONFIG version=%s v=W0621 mode=%lu selftest=%u fallback=%u T=ON S=ON P=ON N=ON vectors=36 words=20 pcm=original events=strict\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_mode1_control[1],mode1_ok?1u:0u,mode1_ok?0u:1u);stdio_flush();
#endif
#ifdef UZEBOX_POLL
    if(uzebox_poll_control[0]!=0x58323256u||uzebox_poll_control[1]>1u||uzebox_mode1_control[1]!=1u)panic("Unbound X polling image");
    boot_stage("idle polling selftest begin");
    const bool poll_ok=uzebox_poll_selftest();
    printf("UZEBOX_POLL_CONFIG version=%s v=X0622 mode=%lu selftest=%u fallback=%u W=ON T=ON S=ON P=ON N=ON vectors=36 pcm=original events=strict\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_poll_control[1],poll_ok?1u:0u,poll_ok?0u:1u);stdio_flush();
#endif
#ifdef UZEBOX_Y
    if(uzebox_y_control[0]!=0x59323356u||uzebox_y_control[1]>1u||uzebox_poll_control[1]!=1u)panic("Unbound Y mixer/render image");
    boot_stage("mixer prefix and projection selftest begin");
    const bool y_core_ok=uzebox_y_selftest(),y_render_ok=uzebox_y_render_selftest();
    if(!y_render_ok)uzebox_y_fail();
    printf("UZEBOX_Y_CONFIG version=%s v=Y0623 mode=%lu selftest=%u fallback=%u core=%u render=%u words=172 X=ON W=ON T=ON S=ON P=ON N=ON pcm=original events=strict\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_y_control[1],(y_core_ok&&y_render_ok)?1u:0u,(y_core_ok&&y_render_ok)?0u:1u,y_core_ok?1u:0u,y_render_ok?1u:0u);stdio_flush();
#endif
#endif
#ifdef UZEBOX_SCAN_FUNCTION
    if(uzebox_scan_control[0]!=0x51313556u || uzebox_scan_control[1]>1u || uzebox_hot_region_control[1]!=1u)
        panic("Unbound Q scan-function image");
    boot_stage("static scan-function selftest begin");
    const bool scan_ok=uzebox_scan_selftest();
    printf("UZEBOX_SCAN_CONFIG version=%s v=Q0615 mode=%lu selftest=%u fallback=%u P=ON N=ON words=136 nodes=134 pcm=original events=strict matched_layout=1\n",
        UZEBOX_BUILD_VERSION,(unsigned long)uzebox_scan_control[1],scan_ok?1u:0u,scan_ok?0u:1u);stdio_flush();
#ifdef UZEBOX_SCAN_ATTRIBUTION
    if(uzebox_scan_control[1]!=1u)panic("R requires bound scan umbrella ON");
    printf("UZEBOX_SCAN_COST_CONFIG v=R0616 phase_masks=0,1,0,2,0,4,0,7 reports=8 untimed=1..6 sample_window=7 stride=16 inclusive=1 frame_boundary=1 pcm=original events=strict input=live\n");stdio_flush();
#endif
#endif
#ifdef CU_AVR_FUSED
    cu_avr_fused_enable(FALSE);
#endif
#endif
#ifdef CU_AVR_NATIVE_THUMB
    boot_stage("native selftest begin");
    const bool native_ok=cu_avr_thumb_selftest();
    printf("UZEBOX_BOOT native selftest=%u\n",native_ok?1u:0u);stdio_flush();
    if(!native_ok)panic("Uzebox native Thumb selftest failed");
#endif
#ifdef CU_AVR_NATIVE_TRACE
    boot_stage("native trace selftest begin");
    const bool trace_ok=cu_avr_native_trace_selftest();
    printf("UZEBOX_BOOT native trace selftest=%u\n",trace_ok?1u:0u);stdio_flush();
    if(!trace_ok)panic("Uzebox native trace selftest failed");
#if defined(CU_AVR_NTRACE_SLOTS) && defined(CU_AVR_NTRACE_WAYS)
    printf("UZEBOX_NCACHE_CONFIG slots=%u ways=%u sets=%u scan=single\n",
        (unsigned)CU_AVR_NTRACE_SLOTS,(unsigned)CU_AVR_NTRACE_WAYS,
        (unsigned)(CU_AVR_NTRACE_SLOTS/CU_AVR_NTRACE_WAYS));stdio_flush();
#endif
#endif
#if defined(UZEBOX_SCAN_FUNCTION) || defined(UZEBOX_PHASE4)
    return 0;
}
static int __attribute__((noinline)) uzebox_main_loop(void){
#endif
    const bool psram_present=psram_is_available();
    const int psram_params=psram_present?psram_configure_params(PICO_DEFAULT_PSRAM_MAX_FREQ,PICO_DEFAULT_PSRAM_MAX_SELECT,PICO_DEFAULT_PSRAM_MIN_DESELECT):-1;
    const int psram_reinit=psram_params==PICO_OK?psram_reinitialize():-1;
    const bool psram_ok=psram_present&&psram_params==PICO_OK&&psram_reinit==PICO_OK;
    const size_t psram_size=psram_get_size();
    printf("UZEBOX_BOOT psram present=%u params=%d reinit=%d bytes=%lu\n",psram_present?1u:0u,psram_params,psram_reinit,(unsigned long)psram_size);stdio_flush();
    jack_memory_layout memory;
    const bool memory_ok=psram_ok&&jack_memory_init(&memory,hot_memory,sizeof(hot_memory),(void *)0x11000000u,psram_size);
    if(!memory_ok)panic("Uzebox requires PSRAM");
    uint8_t *spi_ram=(uint8_t *)jack_memory_alloc_bulk(&memory,0x20000u,4u);if(!spi_ram)panic("Uzebox SPI RAM allocation failed");cu_spir_set_ram(spi_ram);
    cart_names=(char (*)[UZEBOX_MENU_NAME])jack_memory_alloc_bulk(&memory,UZEBOX_MENU_CARTS*UZEBOX_MENU_NAME,4u);
    if(!cart_names)panic("Uzebox cartridge menu allocation failed");
#ifdef CU_AVR_BOARD_PROFILE
    auint *board_pc_counts=(auint *)jack_memory_alloc_bulk(&memory,CU_AVR_BOARD_PROFILE_PCS*sizeof(auint),4u);
    if(!board_pc_counts)panic("Uzebox board profile PSRAM allocation failed");
    cu_avr_board_profile_attach(board_pc_counts);
#endif
    boot_stage("sd configure begin");
    pico_fatfs_spi_config_t sd_config={SDCARD_SPI,CLK_SLOW_DEFAULT,CLK_FAST_DEFAULT_PIO,
        SDCARD_PIN_MISO,SDCARD_PIN_CS,SDCARD_PIN_SCK,SDCARD_PIN_MOSI,true};
    if(!pico_fatfs_set_config(&sd_config))panic("SD pin configuration rejected");
    printf("UZEBOX_BOOT sd configured SPI1 cs=%u sck=%u mosi=%u miso=%u\n",
        (unsigned)SDCARD_PIN_CS,(unsigned)SDCARD_PIN_SCK,(unsigned)SDCARD_PIN_MOSI,(unsigned)SDCARD_PIN_MISO);stdio_flush();
    boot_stage("sd begin");const FRESULT sd=jack_fatfs_mount(&filesystem);
    printf("UZEBOX_BOOT sd result=%d\n",(int)sd);stdio_flush();
    boot_stage("hdmi begin");hstx_init(false);clear_video();prepare_render_tables();
    printf("UZEBOX_BOOT hdmi ready hstx_hz=%lu resync=%d\n",(unsigned long)clock_get_hz(clk_hstx),get_video_output_resync_count());stdio_flush();
    boot_stage("menu scan begin " UZEBOX_CART_DIR);
    const FRESULT scan_result=sd==FR_OK?scan_carts():sd;
    printf("UZEBOX_BOOT menu scan=%d carts=%u\n",(int)scan_result,cart_count);stdio_flush();
    jack_input_queue queue;jack_input_devices devices;jack_input_init(&queue);const bool input_ok=jack_input_devices_init(&devices,&queue);
#ifdef UZEBOX_MENU_DIAG
    uzebox_menu_phase(10u);
#endif
    boot_stage("usb begin");const bool usb_ok=input_ok&&jack_tinyusb_input_init(&devices);
    boot_stage("usb ready; nes begin");const bool nes_ok=input_ok&&jack_nespad_input_init(&devices,UZEBOX_SYS_KHZ);
    boot_stage("menu begin");const int selected=select_cart(&devices,&queue,usb_ok,nes_ok,scan_result);
    printf("UZEBOX_BOOT menu selected=%d name=%s\n",selected,cart_names[selected]);stdio_flush();
    clear_video();
    jack_uzebox_uze_info info;uint32_t content_crc=0u;
    boot_stage("cart begin");
    const bool game_ok=sd==FR_OK&&load_game(&memory,cart_names[selected],&info,&content_crc);
    printf("UZEBOX_BOOT cart ok=%u name=%.32s bytes=%lu crc=%08lx\n",game_ok?1u:0u,game_ok?info.name:"",game_ok?(unsigned long)info.program_size:0ul,(unsigned long)content_crc);stdio_flush();
    const bool eeprom_ok=game_ok&&load_eeprom(content_crc);
    if(game_ok){boot_stage("avr reset begin");cu_state_cpu_t *cpu=cu_avr_get_state();cpu->wd_seed=content_crc;cu_avr_autofuse(FALSE);cu_avr_reset();boot_stage("avr reset ready");}
    if(game_ok){const cu_state_cpu_t *cpu=cu_avr_get_state();
        printf("UZEBOX_RUN_CONFIG rom_crc=%08lx seed=%08lx eeprom_crc=%08lx eeprom_ok=%u input=live\n",
            (unsigned long)content_crc,(unsigned long)cpu->wd_seed,
            (unsigned long)jack_uzebox_crc32(0u,cpu->eepr,sizeof(cpu->eepr)),eeprom_ok?1u:0u);}
#if defined(UZEBOX_HOT_PLACEMENT) || defined(UZEBOX_LOOP_PLACEMENT) || defined(UZEBOX_EXTENDED_SPAN)
    if(game_ok){const cu_state_cpu_t *cpu=cu_avr_get_state();
        placement_rom=content_crc;placement_seed=cpu->wd_seed;
        placement_eeprom=jack_uzebox_crc32(0u,cpu->eepr,sizeof(cpu->eepr));}
#endif
#ifdef UZEBOX_PC_SAMPLER
    if(game_ok){
        const cu_state_cpu_t *cpu=cu_avr_get_state();
        sample_rom_crc=content_crc;sample_seed=cpu->wd_seed;
        sample_eeprom_crc=jack_uzebox_crc32(0u,cpu->eepr,sizeof(cpu->eepr));
        uzebox_sampler_init(&cpu->pc);
        printf("UZEBOX_SAMPLE_CONFIG version=%s phases=0,1,0,3,0,4,0,5 reports=8 units=samples low_us=512..1535 dense_us=128..383 priority=lowest bus=separate core=H_fixed input=live\n",UZEBOX_BUILD_VERSION);stdio_flush();
        printf("UZEBOX_SAMPLE_PROBE begin irq_only=1 duration_us=10000\n");stdio_flush();
        const bool probe_ok=uzebox_sampler_probe();
        printf("UZEBOX_SAMPLE_PROBE done ok=%u irq_total=%lu irq_exits=%lu guard=%lu\n",probe_ok?1u:0u,
            (unsigned long)uzebox_sample_health.irq_total,(unsigned long)uzebox_sample_health.irq_exits,(unsigned long)uzebox_sample_health.guard);stdio_flush();
    }
#endif
#ifdef CU_AVR_LOOP_QUALIFY
    if(game_ok){const cu_state_cpu_t *cpu=cu_avr_get_state();
        printf("UZEBOX_LOOPQ_CONFIG v=6 rom_crc=%08lx seed=%08lx eeprom_crc=%08lx eeprom_ok=%u input=live sample_gap=4096..8191 window=64 max_iterations=8 ld_guard=old_sram pm=sampled_entry_instructions\n",
            (unsigned long)content_crc,(unsigned long)cpu->wd_seed,
            (unsigned long)jack_uzebox_crc32(0u,cpu->eepr,sizeof(cpu->eepr)),eeprom_ok?1u:0u);}
#endif
    boot_stage("audio begin");const bool audio_ok=jack_audio_init(&audio_ring,audio_storage,sizeof(audio_storage)/sizeof(audio_storage[0]),44100u);
#ifdef UZEBOX_AUDIO_CLOCK
    printf("UZEBOX_AUDIO_CLOCK_CONFIG v=U0619 source_hz=15720 output_hz=44100 video_hz=60 epoch_frames=60 prefill=441 original_guest_pcm=1\n");stdio_flush();
#endif
    printf("UZEBOX_BOOT psram=%u sd=%d game=%u name=%.32s bytes=%lu crc=%08lx eeprom=%u audio=%u usb=%u nes=%u app=%08x\n",psram_ok?1u:0u,(int)sd,game_ok?1u:0u,game_ok?info.name:"",game_ok?(unsigned long)info.program_size:0ul,(unsigned long)content_crc,eeprom_ok?1u:0u,audio_ok?1u:0u,usb_ok?1u:0u,nes_ok?1u:0u,(unsigned)JACK_APP_BASE);
    uint32_t last=hstx_getframecounter(),report_frames=0u,report_completed=0u,report_rows=0u,report_hstx=last;
    uint32_t report_row_caps=0u,report_call_caps=0u,previous_report_us=0u;
#ifdef CU_AVR_BOARD_PROFILE
    unsigned board_profile_reports=0u;bool board_profile_warmed=false;
#endif
#ifdef CU_AVR_LOOP_QUALIFY
    unsigned loopq_reports=0u,loopq_input_active=0u;bool loopq_warmed=false;
#endif
    uint64_t report_time=time_us_64(),next_report=report_time+1000000u;
    service_devices=&devices;service_queue=&queue;service_usb=usb_ok;service_nes=nes_ok;
    uzebox_aa_init();
    service_audio_deadline=time_us_64()*44100u;service_ready=true;
    bool eeprom_dirty=false;boot_stage("main loop begin");
    while(true){
        service_io();
        const uint32_t current=hstx_getframecounter();if(last!=current){last=current;if(game_ok){
            if(frames==0u)boot_stage("first frame begin");
            cu_ctr_setsnes(0u,pad_to_snes((uint16_t)(devices.pads[0]|keyboard_pad)));cu_ctr_setsnes(1u,pad_to_snes(devices.pads[1]));
#ifdef CU_AVR_LOOP_QUALIFY
            if(loopq_warmed&&(devices.pads[0]|devices.pads[1]|keyboard_pad)!=0u)++loopq_input_active;
#endif
            run_frame();
#if defined(CU_AVR_TIME_PROFILE) && !defined(CU_AVR_REGION_COMPARE)
            if(!time_warmed&&completed_frames>=60u){
                cu_avr_time_clear();cu_avr_time_enable(TRUE);
                time_epoch=time_us_64();time_warmed=true;
                printf("UZEBOX_TIME warmup_done frames=%lu\n",(unsigned long)completed_frames);
            }
#endif
#ifdef CU_AVR_BOARD_PROFILE
            if(!board_profile_warmed&&completed_frames>=60u){cu_avr_board_profile_clear();board_profile_warmed=true;printf("UZEBOX_HOT warmup_done frames=%lu\n",(unsigned long)completed_frames);}
#endif
#ifdef CU_AVR_LOOP_QUALIFY
            if(!loopq_warmed&&completed_frames>=60u){cu_avr_loop_qualify_begin();loopq_warmed=true;printf("UZEBOX_LOOPQ warmup_done frames=%lu\n",(unsigned long)completed_frames);}
#endif
            if(frames==1u){printf("UZEBOX_BOOT first frame done rows=%lu syncerr=%lu\n",(unsigned long)rows,(unsigned long)sync_errors);stdio_flush();}
            eeprom_dirty|=cu_avr_eeprom_ischanged(TRUE)!=FALSE;}}
        service_io();
        const uint64_t now=time_us_64();if(now>=next_report){bool save_ok=true;if(eeprom_dirty){uzebox_sampler_stage(UZ_SAVE);save_ok=save_eeprom();if(save_ok)eeprom_dirty=false;}
            uzebox_sampler_stage(UZ_REPORT);
#ifdef UZEBOX_PC_SAMPLER
            uzebox_sampler_take(&sample_report);
#endif
            const uint32_t report_start=time_us_32();
            report_begin();
            const uint32_t hstx_now=hstx_getframecounter();
            const unsigned long fps10=(unsigned long)((uint64_t)(frames-report_frames)*10000000u/(now-report_time));
            const unsigned long real_fps10=(unsigned long)((uint64_t)(completed_frames-report_completed)*10000000u/(now-report_time));
            const unsigned long hstx_fps10=(unsigned long)((uint64_t)(hstx_now-report_hstx)*10000000u/(now-report_time));
            uint32_t nonblack=0u;const uint16_t *video=(const uint16_t *)hstx_getframebuffer();
            for(size_t i=0u;i<320u*240u;++i)nonblack+=video[i]!=0u;
            report_printf("UZEBOX_ALIVE frames=%lu fps10=%lu hstx=%lu rows=%lu rows_sec=%lu nonblack=%lu fb=%04x,%04x,%04x syncerr=%lu audio=%lu under=%lu over=%lu resync=%d save=%u psram=%lu\n",
                (unsigned long)frames,fps10,(unsigned long)(hstx_now-report_hstx),(unsigned long)rows,(unsigned long)(rows-report_rows),(unsigned long)nonblack,
                (unsigned)video[0],(unsigned)video[120u*320u+160u],(unsigned)video[239u*320u+319u],
                (unsigned long)sync_errors,(unsigned long)audio_ring.count,(unsigned long)audio_ring.underruns,(unsigned long)audio_ring.overruns,
                get_video_output_resync_count(),save_ok?1u:0u,(unsigned long)memory.bulk.high_water);
            const uint32_t samples=profiled_frames;
            report_printf("UZEBOX_PROFILE real_fps10=%lu hstx_fps10=%lu samples=%lu frame_us=%lu max_us=%lu core_us=%lu render_us=%lu srcaudio_us=%lu avr_cycles=%lu input_us=%lu drain_us=%lu prev_report_us=%lu rowcap=%lu callcap=%lu\n",
                real_fps10,hstx_fps10,(unsigned long)samples,
                samples?(unsigned long)(profile_total_us/samples):0ul,(unsigned long)max_frame_us,
                samples?(unsigned long)(profile_core_us/samples):0ul,
                samples?(unsigned long)(profile_render_us/samples):0ul,
                samples?(unsigned long)(profile_source_audio_us/samples):0ul,
                samples?(unsigned long)(profile_avr_cycles/samples):0ul,
                (unsigned long)io_input_us,(unsigned long)io_audio_us,(unsigned long)previous_report_us,
                (unsigned long)(row_cap_hits-report_row_caps),(unsigned long)(call_cap_hits-report_call_caps));
            const uint32_t all_samples=frames-report_frames;
            report_printf("UZEBOX_FRAME_ALL samples=%lu completed=%lu elapsed_us=%llu work_us=%llu avr_cycle=%lu avg_us=%lu min_us=%lu max_us=%lu lt20=%lu b20_99=%lu b100_179=%lu ge180=%lu inframe_input_us=%lu inframe_drain_us=%lu inframe_log_us=%lu\n",
                (unsigned long)all_samples,
                (unsigned long)(completed_frames-report_completed),(unsigned long long)(now-report_time),
                (unsigned long long)all_frame_time_us,(unsigned long)cu_avr_getcycle(),
                all_samples?(unsigned long)(all_frame_time_us/all_samples):0ul,
                all_samples?(unsigned long)min_frame_us:0ul,(unsigned long)max_frame_us,
                (unsigned long)all_frame_buckets[0],(unsigned long)all_frame_buckets[1],
                (unsigned long)all_frame_buckets[2],(unsigned long)all_frame_buckets[3],
                all_samples?(unsigned long)(frame_input_us/all_samples):0ul,
                all_samples?(unsigned long)(frame_audio_us/all_samples):0ul,
                all_samples?(unsigned long)(frame_telemetry_us/all_samples):0ul);
            jack_nespad_input_stats nes_stats;jack_nespad_input_get_stats(&nes_stats);
            report_printf("UZEBOX_IO async_nes=%u starts=%lu completed=%lu pending=%u max_latency_us=%lu log_us=%lu log_sent=%lu log_dropped=%lu\n",
                (unsigned)UZEBOX_ASYNC_NES,(unsigned long)nes_stats.starts,(unsigned long)nes_stats.completed,
                nes_stats.pending?1u:0u,(unsigned long)nes_stats.max_latency_us,
                (unsigned long)io_telemetry_us,(unsigned long)report_bytes_sent,(unsigned long)report_dropped);
#ifdef CU_AVR_LOOP_NATIVE
            cu_avr_loop_stats_t loop_stats;cu_avr_loop_stats(&loop_stats);
            report_printf("UZEBOX_LOOP builds=%lu entries=%lu iterations=%lu cycles=%lu tile=%lu delay=%lu guard_reject=%lu event_reject=%lu row_reject=%lu invalidations=%lu bitmap=%lu bitmap_cycles=%lu resume=%lu wait=%lu wait_cycles=%lu alu=%lu alu_instructions=%lu alu_cycles=%lu\n",
                (unsigned long)loop_stats.builds,(unsigned long)loop_stats.entries,
                (unsigned long)loop_stats.iterations,(unsigned long)loop_stats.cycles,
                (unsigned long)loop_stats.tile_entries,(unsigned long)loop_stats.delay_entries,
                (unsigned long)loop_stats.guard_rejects,(unsigned long)loop_stats.event_rejects,
                (unsigned long)loop_stats.row_rejects,(unsigned long)loop_stats.invalidations,
                (unsigned long)loop_stats.bitmap_entries,(unsigned long)loop_stats.bitmap_cycles,
                (unsigned long)loop_stats.bitmap_resumes,(unsigned long)loop_stats.wait_entries,
                (unsigned long)loop_stats.wait_cycles,(unsigned long)loop_stats.alu_entries,
                (unsigned long)loop_stats.alu_instructions,(unsigned long)loop_stats.alu_cycles);
#endif
#ifdef CU_AVR_REGION
            cu_avr_region_stats_t region_stats;cu_avr_region_stats(&region_stats);
            report_printf("UZEBOX_REGION entries=%lu instructions=%lu cycles=%lu\n",
                (unsigned long)region_stats.entries,(unsigned long)region_stats.instructions,(unsigned long)region_stats.cycles);
#endif
#ifdef CU_AVR_BOARD_PROFILE
            if(board_profile_warmed&&++board_profile_reports%5u==0u)report_avr_hotspots();
#endif
#ifdef CU_AVR_LOOP_QUALIFY
            if(loopq_warmed&&++loopq_reports%5u==0u)report_avr_loop_qualification(loopq_input_active);
#endif
#ifdef CU_AVR_TRACE_REFERENCE
            cu_avr_trace_stats_t trace={0};cu_avr_trace_stats(&trace);
            report_printf("UZEBOX_TRACE hits=%lu builds=%lu instructions=%lu replayed=%lu diverged=%lu unique=%lu evictions=%lu hot8=%lu hot32=%lu highpc=%lu maxpc=%08lx\n",
                (unsigned long)trace.hits,(unsigned long)trace.builds,
                (unsigned long)trace.instructions,(unsigned long)trace.replayed,
                (unsigned long)trace.diverged,(unsigned long)trace.unique_starts,
                (unsigned long)trace.evictions,(unsigned long)trace.hot8_replayed,
                (unsigned long)trace.hot32_replayed,(unsigned long)trace.high_pc_starts,
                (unsigned long)trace.max_pc);
#endif
#ifdef CU_AVR_NATIVE_TRACE
            auint nt_builds=0u,nt_entries=0u,nt_instructions=0u,nt_exits=0u,nt_compiled=0u,nt_short=0u,nt_codecap=0u;
            cu_avr_native_trace_stats(&nt_builds,&nt_entries,&nt_instructions,&nt_exits,&nt_compiled,&nt_short);
            cu_avr_native_trace_codecap_stats(&nt_codecap);
            report_printf("UZEBOX_NTRACE builds=%lu entries=%lu instructions=%lu exits=%lu compiled=%lu short=%lu codecap=%lu\n",
                (unsigned long)nt_builds,(unsigned long)nt_entries,(unsigned long)nt_instructions,
                (unsigned long)nt_exits,(unsigned long)nt_compiled,(unsigned long)nt_short,(unsigned long)nt_codecap);
            auint cache_hits=0u,cache_misses=0u,cache_short=0u,cache_cold=0u,cache_full=0u,cache_evictions=0u;
            cu_avr_native_trace_cache_stats(&cache_hits,&cache_misses,&cache_short,&cache_cold,&cache_full,&cache_evictions);
            report_printf("UZEBOX_NCACHE hits=%lu misses=%lu short_hits=%lu cold_reject=%lu full_reject=%lu evictions=%lu\n",
                (unsigned long)cache_hits,(unsigned long)cache_misses,(unsigned long)cache_short,
                (unsigned long)cache_cold,(unsigned long)cache_full,(unsigned long)cache_evictions);
#ifdef CU_AVR_NATIVE_TRACE_LD
            auint nl_ok=0u,nl_io=0u;
            cu_avr_native_trace_ld_stats(&nl_ok,&nl_io);
            report_printf("UZEBOX_NLD guard_ok=%lu guard_io=%lu\n",
                (unsigned long)nl_ok,(unsigned long)nl_io);
#endif
#ifdef CU_AVR_NATIVE_TRACE_CHAIN
            auint nc_hits=0u,nc_segments=0u;
            cu_avr_native_trace_chain_stats(&nc_hits,&nc_segments);
            report_printf("UZEBOX_NCHAIN hits=%lu segments=%lu\n",
                (unsigned long)nc_hits,(unsigned long)nc_segments);
#endif
#ifdef CU_AVR_NATIVE_TRACE_VERIFY
            auint nt_checks=0u,nt_bad=0u,nt_pc=0u,nt_index=0u,nt_got=0u,nt_expected=0u;
            cu_avr_native_trace_verify_stats(&nt_checks,&nt_bad,&nt_pc,&nt_index,&nt_got,&nt_expected);
            report_printf("UZEBOX_NVERIFY checks=%lu bad=%lu pc=%04lx index=%lu got=%lu expected=%lu\n",
                (unsigned long)nt_checks,(unsigned long)nt_bad,(unsigned long)nt_pc,
                (unsigned long)nt_index,(unsigned long)nt_got,(unsigned long)nt_expected);
#endif
#endif
#ifdef CU_AVR_NATIVE_THUMB
            auint native_blocks=0u,native_instructions=0u,native_rebuilds=0u;
            cu_avr_thumb_stats(&native_blocks,&native_instructions,&native_rebuilds);
            report_printf("UZEBOX_NATIVE blocks=%lu instructions=%lu rebuilds=%lu\n",
                (unsigned long)native_blocks,(unsigned long)native_instructions,
                (unsigned long)native_rebuilds);
#ifdef CU_AVR_NATIVE_VERIFY
            auint checks=0u,mismatches=0u,first_pc=0u,first_index=0u,first_got=0u,first_expected=0u,event_rejects=0u,row_rejects=0u;
            cu_avr_thumb_verify_stats(&checks,&mismatches,&first_pc,&first_index,&first_got,&first_expected,&event_rejects,&row_rejects);
            report_printf("UZEBOX_VERIFY checks=%lu bad=%lu pc=%04lx index=%lu got=%lu expected=%lu events=%lu rows=%lu\n",
                (unsigned long)checks,(unsigned long)mismatches,(unsigned long)first_pc,
                (unsigned long)first_index,(unsigned long)first_got,(unsigned long)first_expected,
                (unsigned long)event_rejects,(unsigned long)row_rejects);
#endif
#endif
#ifdef CU_AVR_BLOCK_STATS
            auint block_attempts=0u,block_hits=0u,block_instructions=0u;
            cu_avr_block_cache_stats(&block_attempts,&block_hits,&block_instructions);
            report_printf("UZEBOX_BLOCK attempts=%lu hits=%lu instructions=%lu\n",
                (unsigned long)block_attempts,(unsigned long)block_hits,(unsigned long)block_instructions);
#endif
#ifdef CU_AVR_TIME_PROFILE
            report_core_time();
#endif
#ifdef UZEBOX_PC_SAMPLER
            report_sampler();
#endif
#ifdef UZEBOX_HOT_PLACEMENT
            report_placement();
#endif
#ifdef UZEBOX_LOOP_PLACEMENT
            report_loop_placement();
#endif
#ifdef UZEBOX_EXTENDED_SPAN
            report_span();
#ifdef UZEBOX_AUDIO_BATCH
            report_audio_batch();
#endif
#endif
            report_publish();uzebox_aa_report();previous_report_us=time_us_32()-report_start;
#ifdef CU_AVR_TIME_PROFILE
#ifdef CU_AVR_REGION_COMPARE
            if(uzebox_compare_next(&compare_schedule,completed_frames)){
                cu_avr_region_enable(uzebox_compare_region(&compare_schedule));
                cu_avr_time_enable(uzebox_compare_timing(&compare_schedule));
#ifdef CU_AVR_FUSED
                cu_avr_fused_enable(uzebox_compare_fused(&compare_schedule));
#endif
#ifdef UZEBOX_PC_SAMPLER
                uzebox_sampler_set_mode(uz_sample_mode(compare_schedule.phase,compare_schedule.warmed));
#endif
            }
#ifdef UZEBOX_SCAN_ATTRIBUTION
            uzebox_scan_cost_set(compare_schedule.phase|((uint32_t)compare_schedule.warmed<<8)|(compare_schedule.window<<16));
#endif
#else
            if(time_warmed)cu_avr_time_enable((((now-time_epoch)/5000000u)&1u)==0u);
#endif
#endif
            report_frames=frames;report_completed=completed_frames;report_rows=rows;report_hstx=hstx_now;
            report_row_caps=row_cap_hits;report_call_caps=call_cap_hits;report_time=now;next_report=now+1000000u;
            profiled_frames=0u;profile_total_us=0u;profile_core_us=0u;profile_render_us=0u;
            profile_source_audio_us=0u;profile_avr_cycles=0u;max_frame_us=0u;
            io_input_us=0u;io_audio_us=0u;io_telemetry_us=0u;
            frame_input_us=0u;frame_audio_us=0u;frame_telemetry_us=0u;
            all_frame_time_us=0u;min_frame_us=UINT32_MAX;memset(all_frame_buckets,0,sizeof(all_frame_buckets));
            uzebox_sampler_stage(UZ_IDLE);
            }
        tight_loop_contents();
    }
}
#if defined(UZEBOX_SCAN_FUNCTION) || defined(UZEBOX_PHASE4)
int main(void){
    const int status=uzebox_boot();
    return status?status:uzebox_main_loop();
}
#endif
#ifdef UZEBOX_SCAN_ATTRIBUTION
uint32_t uzebox_scan_clock(void){return time_us_32();}
#endif
