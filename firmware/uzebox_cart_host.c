/* Host execution test for real external Uzebox UZE games. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef UZEBOX_HOST_PERF
#include <time.h>
#endif
#include "cu_avr.h"
#include "cu_ctr.h"
#include "jack_uzebox_uze.h"
#ifdef CU_AVR_REGION_COMPARE
#include "uzebox_compare_schedule.h"
#ifdef CU_AVR_FUSED
volatile uint32_t uzebox_g_control[3]={255u,0u,150u};
#endif
#endif
#ifdef CU_AVR_TIME_PROFILE
/* Synthetic wrapping clock for architectural tests, never a speed benchmark. */
auint cu_avr_time_clock(void){static auint ticks=0xFFFFF000U;return ticks++;}
#endif
#ifndef UZEBOX_TEST_FRAMES
#define UZEBOX_TEST_FRAMES 180U
#endif
#ifdef UZEBOX_HOST_PERF
#if UZEBOX_TEST_FRAMES == 0
#error "UZEBOX_HOST_PERF requires UZEBOX_TEST_FRAMES > 0"
#endif
#define UZEBOX_HOST_WARMUP_FRAMES 60U
static uint64_t clock_ns(clockid_t clock_id){
    struct timespec now;
    if(clock_gettime(clock_id,&now)!=0){perror("clock_gettime");exit(2);}
    return (uint64_t)now.tv_sec*1000000000u+(uint64_t)now.tv_nsec;
}
#endif
#ifdef CU_AVR_BOARD_PROFILE
static auint host_board_pc_counts[CU_AVR_BOARD_PROFILE_PCS];
#endif
static uint32_t mix(uint32_t h,uint8_t value){return (h^value)*16777619u;}
#ifdef CU_AVR_OPCODE_PROFILE
typedef struct {auint pc;auint count;} hot_pc_t;
static int compare_hot_pc(const void *a,const void *b){
    const hot_pc_t *left=(const hot_pc_t *)a,*right=(const hot_pc_t *)b;
    if(left->count!=right->count)return left->count<right->count?1:-1;
    return left->pc>right->pc?1:left->pc<right->pc?-1:0;
}
static void print_hotset_profile(const char *name,const auint *pcs,uint64_t total){
    static hot_pc_t hot[32768];
    unsigned unique=0u;
    for(unsigned pc=0u;pc<32768u;++pc)if(pcs[pc]!=0u){hot[unique].pc=pc;hot[unique].count=pcs[pc];++unique;}
    qsort(hot,unique,sizeof(hot[0]),compare_hot_pc);
    const unsigned budgets[]={256u,512u,1024u,2048u,4096u,8192u};
    unsigned budget_index=0u,cover90=0u,cover95=0u,cover99=0u;
    uint64_t covered=0u;
    for(unsigned rank=0u;rank<unique;++rank){
        covered+=hot[rank].count;
        if(cover90==0u&&covered*100u>=total*90u)cover90=rank+1u;
        if(cover95==0u&&covered*100u>=total*95u)cover95=rank+1u;
        if(cover99==0u&&covered*100u>=total*99u)cover99=rank+1u;
        while(budget_index<sizeof(budgets)/sizeof(budgets[0])&&rank+1u==budgets[budget_index]){
            printf("UZEBOX_HOTSET name=%.32s pcs=%u cover10=%llu budget=%u total=%llu\n",
                name,unique,(unsigned long long)(covered*1000u/total),budgets[budget_index],(unsigned long long)total);
            ++budget_index;
        }
    }
    printf("UZEBOX_HOTSET name=%.32s pcs=%u cover10=1000 budget=all total=%llu p90=%u p95=%u p99=%u\n",
        name,unique,(unsigned long long)total,cover90,cover95,cover99);
}
static void print_opcode_profile(const char *name){
    const auint *counts=cu_avr_profile_counts();
    bool used[128]={false};uint64_t total=0u;
    for(unsigned op=0u;op<128u;++op)total+=counts[op];
    for(unsigned rank=0u;rank<12u;++rank){
        unsigned best=128u;
        for(unsigned op=0u;op<128u;++op)if(!used[op]&&(best==128u||counts[op]>counts[best]))best=op;
        if(best==128u||counts[best]==0u)break;
        used[best]=true;
        printf("UZEBOX_OPCODE name=%.32s rank=%u op=%02x count=%lu share10=%lu total=%llu\n",
            name,rank+1u,best,(unsigned long)counts[best],
            total?(unsigned long)((uint64_t)counts[best]*1000u/total):0ul,(unsigned long long)total);
    }
    const auint *pcs=cu_avr_profile_pc_counts();bool pc_used[32768]={false};
    print_hotset_profile(name,pcs,total);
    auint blocks=0u,long_instructions=0u,events=0u,max_block=0u;
    cu_avr_profile_flow(&blocks,&long_instructions,&events,&max_block);
    printf("UZEBOX_FLOW name=%.32s blocks=%lu instructions=%llu average10=%llu long_share10=%llu events=%lu max_block=%lu\n",
        name,(unsigned long)blocks,(unsigned long long)total,
        blocks?(unsigned long long)(total*10u/blocks):0ull,
        total?(unsigned long long)((uint64_t)long_instructions*1000u/total):0ull,
        (unsigned long)events,(unsigned long)max_block);
    for(unsigned rank=0u;rank<12u;++rank){
        unsigned best=32768u;
        for(unsigned pc=0u;pc<32768u;++pc)if(!pc_used[pc]&&(best==32768u||pcs[pc]>pcs[best]))best=pc;
        if(best==32768u||pcs[best]==0u)break;
        pc_used[best]=true;
        const auint code=cu_avr_profile_pc_compiled(best);
        printf("UZEBOX_PC name=%.32s rank=%u pc=%04x op=%02lx arg1=%02lx arg2=%04lx count=%lu share10=%lu\n",
            name,rank+1u,best,(unsigned long)(code&0x7fu),(unsigned long)((code>>8u)&0xffu),(unsigned long)(code>>16u),
            (unsigned long)pcs[best],total?(unsigned long)((uint64_t)pcs[best]*1000u/total):0ul);
    }
}
#endif
static bool load_file(const char *path,uint8_t **data,size_t *size){
    FILE *f=fopen(path,"rb");if(!f)return false;if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}long n=ftell(f);
    if(n<=0||fseek(f,0,SEEK_SET)!=0){fclose(f);return false;}*data=(uint8_t *)malloc((size_t)n);if(!*data){fclose(f);return false;}
    *size=(size_t)n;const bool ok=fread(*data,1,*size,f)==*size;fclose(f);if(!ok){free(*data);*data=NULL;}return ok;
}
static bool run_game(const char *path){
    uint8_t *file=NULL;size_t size=0;jack_uzebox_uze_info info;
    if(!load_file(path,&file,&size)||!jack_uzebox_parse_uze(file,size,&info)){free(file);fprintf(stderr,"UZE parse failed: %s\n",path);return false;}
    cu_state_cpu_t *cpu=cu_avr_get_state();memset(cpu,0,sizeof(*cpu));memcpy(cpu->crom,info.program,info.program_size);
#ifdef UZEBOX_HOST_PERF
    memset(cpu->eepr,0xff,sizeof(cpu->eepr));
#endif
#ifdef UZEBOX_TEST_HW_IDLE
    cpu->wd_seed=jack_uzebox_crc32(0u,file,size);
#else
    cpu->wd_seed=0x13579bdfu;
#endif
    cu_avr_autofuse(FALSE);cu_avr_reset();
#ifdef CU_AVR_TIME_PROFILE
    cu_avr_time_clear();cu_avr_time_enable(TRUE);
#ifdef CU_AVR_REGION_COMPARE
    uzebox_compare_schedule host_compare={0};
    cu_avr_time_enable(FALSE);cu_avr_region_enable(FALSE);
#ifdef CU_AVR_FUSED
    cu_avr_fused_enable(FALSE);
    cu_avr_fused_stats_t fused_before;cu_avr_fused_stats(&fused_before);
#endif
#endif
#endif
#ifdef CU_AVR_BOARD_PROFILE
    cu_avr_board_profile_attach(host_board_pc_counts);
#endif
#ifdef CU_AVR_OPCODE_PROFILE
    cu_avr_profile_clear();
#endif
    uint32_t video=2166136261u,audio=2166136261u;unsigned frames=0,rows=0,calls=0;bool saw_picture=false;
#ifdef UZEBOX_HOST_PERF
    const unsigned target_frames=UZEBOX_HOST_WARMUP_FRAMES+UZEBOX_TEST_FRAMES;
    uint64_t measured_start_ns=0u,measured_end_ns=0u,core_ns=0u;
#else
    const unsigned target_frames=UZEBOX_TEST_FRAMES;
#endif
#ifdef UZEBOX_FULL_ROW_HASH
    uint32_t video_full=2166136261u;
#endif
    while(frames<target_frames&&calls<2000000u){
        unsigned buttons=0u;
#ifdef UZEBOX_TEST_GAMEPLAY
        /* Enter PLAY without DOWN, move/edit, run/pause, insert a glider.
         * The normal cyclic fixture may select HOW TO PLAY instead. */
        if(strcmp(info.name,"Game of Life")==0){
            if((frames>=120u&&frames<128u)||(frames>=360u&&frames<368u)||
               (frames>=600u&&frames<608u)||(frames>=780u&&frames<788u)||
               (frames>=960u&&frames<968u))buttons=CU_CTR_SNES_M_START;
            else if(frames>=240u&&frames<272u)buttons=CU_CTR_SNES_M_RIGHT;
            else if(frames>=300u&&frames<308u)buttons=CU_CTR_SNES_M_A;
            else if(frames>=720u&&frames<728u)buttons=CU_CTR_SNES_M_SELECT;
        }
#else
#ifndef UZEBOX_TEST_HW_IDLE
        const unsigned phase=frames%120u;if(phase<15u)buttons=CU_CTR_SNES_M_RIGHT;else if(phase<30u)buttons=CU_CTR_SNES_M_DOWN;else if(phase<45u)buttons=CU_CTR_SNES_M_A;else if(phase<60u)buttons=CU_CTR_SNES_M_START;
#endif
#endif
        cu_ctr_setsnes(0u,buttons);
#ifdef UZEBOX_HOST_PERF
        const bool measuring=frames>=UZEBOX_HOST_WARMUP_FRAMES;
        const uint64_t core_start_ns=measuring?clock_ns(CLOCK_THREAD_CPUTIME_ID):0u;
#endif
        const auint flags=cu_avr_run();++calls;
#ifdef UZEBOX_HOST_PERF
        if(measuring)core_ns+=clock_ns(CLOCK_THREAD_CPUTIME_ID)-core_start_ns;
#endif
        if(flags&CU_GET_ROW){const cu_row_t *row=cu_avr_get_row();
#ifdef UZEBOX_HOST_PERF
            if(measuring){
#endif
            audio=mix(audio,(uint8_t)row->sample);
#ifdef UZEBOX_FULL_ROW_HASH
            for(unsigned x=0;x<1920u;++x)video_full=mix(video_full,row->pixels[x]);
#endif
            if(row->pno>=19u&&row->pno<244u){for(unsigned x=0;x<320u;x+=8u){const unsigned src=291u+(x*1456u)/320u;const uint8_t pixel=row->pixels[src];video=mix(video,pixel);saw_picture|=pixel!=0u;}++rows;}
#ifdef UZEBOX_HOST_PERF
            }
#endif
        }
        if(flags&CU_GET_FRAME){++frames;
#ifdef CU_AVR_TIME_PROFILE
#ifdef CU_AVR_REGION_COMPARE
            if(frames%8u==0u&&uzebox_compare_next(&host_compare,frames)){
                cu_avr_region_enable(uzebox_compare_region(&host_compare));
                cu_avr_time_enable(uzebox_compare_timing(&host_compare));
#ifdef CU_AVR_FUSED
                cu_avr_fused_enable(uzebox_compare_fused(&host_compare));
#endif
            }
#else
            if(frames%120u==0u)cu_avr_time_enable(((frames/120u)&1u)==0u);
#endif
#endif
#ifdef UZEBOX_TEST_GAMEPLAY
            if(frames==120u||frames==240u||frames==360u||frames==600u||
               frames==720u||frames==840u||frames==960u||frames==target_frames){
                printf("UZEBOX_GAMEPLAY_FRAME frames=%u video=%08x audio=%08x\n",frames,video,audio);
#if defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)
                cu_avr_loop_stats_t p;cu_avr_loop_stats(&p);
                printf("UZEBOX_LOOP_PHASE frames=%u bitmap=%lu bitmap_cycles=%lu tile=%lu cycles=%lu avr_cycle=%lu resume=%lu wait=%lu wait_cycles=%lu alu=%lu alu_cycles=%lu\n",
                    frames,(unsigned long)p.bitmap_entries,(unsigned long)p.bitmap_cycles,
                    (unsigned long)p.tile_entries,(unsigned long)p.cycles,(unsigned long)cpu->cycle,
                    (unsigned long)p.bitmap_resumes,(unsigned long)p.wait_entries,(unsigned long)p.wait_cycles,
                    (unsigned long)p.alu_entries,(unsigned long)p.alu_cycles);
                if(strcmp(info.name,"Game of Life")==0){
                    if(frames==120u&&p.bitmap_entries!=0u)return false;
                    if(frames>=360u&&p.bitmap_entries<10000u){
                        fprintf(stderr,"GameOfLife bitmap gameplay was not reached\n");return false;
                    }
                }
#endif
            }
#endif
#ifdef CU_AVR_LOOP_QUALIFY
            if(frames==60u)cu_avr_loop_qualify_begin();
#endif
#ifdef UZEBOX_HOST_PERF
            if(frames==UZEBOX_HOST_WARMUP_FRAMES)measured_start_ns=clock_ns(CLOCK_MONOTONIC);
            if(frames==target_frames)measured_end_ns=clock_ns(CLOCK_MONOTONIC);
#endif
#ifdef UZEBOX_TEST_RECOMPILE
#ifdef UZEBOX_HOST_PERF
            if(frames==UZEBOX_HOST_WARMUP_FRAMES+UZEBOX_TEST_FRAMES/2U)cu_avr_crom_update(0u,65536u);
#else
            if(frames==UZEBOX_TEST_FRAMES/2U)cu_avr_crom_update(0u,65536u);
#endif
#endif
        }
    }
    const bool ok=frames==target_frames&&rows>UZEBOX_TEST_FRAMES*166u&&saw_picture;
#ifdef UZEBOX_HOST_PERF
    const unsigned reported_frames=frames>=UZEBOX_HOST_WARMUP_FRAMES?frames-UZEBOX_HOST_WARMUP_FRAMES:0u;
#else
    const unsigned reported_frames=frames;
#endif
    printf("UZEBOX_CART_HOST %s name=%.32s frames=%u rows=%u video=%08x audio=%08x bytes=%lu crc=%08x file_crc=%08x\n",ok?"PASS":"FAIL",info.name,reported_frames,rows,video,audio,(unsigned long)info.program_size,info.header_crc32,jack_uzebox_crc32(0u,file,size));
#ifdef UZEBOX_HOST_PERF
    if(measured_start_ns!=0u&&measured_end_ns>=measured_start_ns){
        /* wall_us includes row hashing; core_cpu_us sums cu_avr_run() CPU time. */
        const uint64_t wall_us=(measured_end_ns-measured_start_ns)/1000u;
        const uint64_t core_us=core_ns/1000u;
        printf("UZEBOX_HOST_PERF name=%.32s warmup=%u measured=%u wall_us=%llu core_cpu_us=%llu wall_us_per_frame=%llu core_cpu_us_per_frame=%llu\n",
            info.name,UZEBOX_HOST_WARMUP_FRAMES,UZEBOX_TEST_FRAMES,
            (unsigned long long)wall_us,(unsigned long long)core_us,
            (unsigned long long)(wall_us/UZEBOX_TEST_FRAMES),
            (unsigned long long)(core_us/UZEBOX_TEST_FRAMES));
    }
#endif
#ifdef UZEBOX_FULL_ROW_HASH
    printf("UZEBOX_FULL_ROW name=%.32s hash=%08x\n",info.name,video_full);
#endif
#ifdef UZEBOX_STATE_HASH
    uint32_t state=2166136261u;
    for(size_t i=0u;i<sizeof(cpu->crom);++i)state=mix(state,cpu->crom[i]);
    for(size_t i=0u;i<sizeof(cpu->sram);++i)state=mix(state,cpu->sram[i]);
    for(size_t i=0u;i<sizeof(cpu->eepr);++i)state=mix(state,cpu->eepr[i]);
    for(size_t i=0u;i<sizeof(cpu->iors);++i)state=mix(state,cpu->iors[i]);
    for(unsigned shift=0u;shift<32u;shift+=8u){state=mix(state,(uint8_t)(cpu->pc>>shift));state=mix(state,(uint8_t)(cpu->cycle>>shift));}
    printf("UZEBOX_STATE name=%.32s pc=%04lx cycle=%lu hash=%08x\n",info.name,(unsigned long)cpu->pc,(unsigned long)cpu->cycle,state);
    uint32_t access=2166136261u;
    const uint8_t *meminfo=cu_avr_get_meminfo();
    const uint8_t *ioinfo=cu_avr_get_ioinfo();
    for(size_t i=0u;i<4096u;++i)access=mix(access,meminfo[i]);
    for(size_t i=0u;i<256u;++i)access=mix(access,ioinfo[i]);
    printf("UZEBOX_ACCESS name=%.32s hash=%08x\n",info.name,access);
#endif
#ifdef CU_AVR_OPCODE_PROFILE
    print_opcode_profile(info.name);
#endif
#ifdef CU_AVR_BOARD_PROFILE
    const cu_avr_board_profile_t *profile=cu_avr_board_profile_state();
    uint64_t sampled_opcodes=0u;
    uint64_t sampled_hot=0u;
    for(unsigned op=0u;op<128u;++op)sampled_opcodes+=profile->opcodes[op];
    for(unsigned pc=0u;pc<CU_AVR_BOARD_PROFILE_PCS;++pc)sampled_hot+=profile->pc_counts[pc];
    const bool profile_ok=profile->samples>0u&&sampled_opcodes==profile->samples&&sampled_hot==profile->samples;
    printf("UZEBOX_BOARD_SAMPLE %s name=%.32s samples=%lu opcode_total=%llu pc_total=%llu\n",
        profile_ok?"PASS":"FAIL",info.name,(unsigned long)profile->samples,
        (unsigned long long)sampled_opcodes,(unsigned long long)sampled_hot);
#endif
#ifdef CU_AVR_LOOP_QUALIFY
    const cu_avr_loop_qualify_stats_t *loopq=cu_avr_loop_qualify_state();
    const unsigned long candidate_pm=loopq->samples?(unsigned long)((uint64_t)loopq->candidate*1000u/loopq->samples):0ul;
    const unsigned long eligible_pm=loopq->samples?(unsigned long)((uint64_t)loopq->eligible*1000u/loopq->samples):0ul;
    const unsigned long mean_repeat10=loopq->repeat_samples?(unsigned long)((uint64_t)loopq->repeat_iterations*10u/loopq->repeat_samples):0ul;
    printf("UZEBOX_LOOPQ name=%.32s v=6 samples=%lu candidate=%lu eligible=%lu candidate_pm=%lu eligible_pm=%lu exact=%lu affine=%lu exact_candidate=%lu affine_candidate=%lu exact_eligible=%lu affine_eligible=%lu window=%lu event=%lu row=%lu verified_repeats10=%lu capped=%lu diverged=%lu cycle_mismatch=%lu repeat_guard_stops=%lu pm=sampled_entry_instructions\n",
        info.name,(unsigned long)loopq->samples,(unsigned long)loopq->candidate,(unsigned long)loopq->eligible,candidate_pm,eligible_pm,
        (unsigned long)loopq->closed_exact,(unsigned long)loopq->closed_affine,
        (unsigned long)loopq->exact_candidate,(unsigned long)loopq->affine_candidate,
        (unsigned long)loopq->exact_eligible,(unsigned long)loopq->affine_eligible,
        (unsigned long)loopq->window_limit,(unsigned long)loopq->event_bound,
        (unsigned long)loopq->row_bound,mean_repeat10,(unsigned long)loopq->repeat_capped,
        (unsigned long)loopq->diverged,(unsigned long)loopq->cycle_mismatch,
        (unsigned long)loopq->repeat_guard_stops);
    bool used_opcodes[128]={false};
    printf("UZEBOX_LOOPQ_BLOCK name=%.32s op=",info.name);
    for(unsigned rank=0u;rank<6u;++rank){
        unsigned best=128u;
        for(unsigned op=0u;op<128u;++op){
            if(used_opcodes[op]||loopq->blocked_opcode[op]==0u)continue;
            if(best==128u||loopq->blocked_opcode[op]>loopq->blocked_opcode[best])best=op;
        }
        if(best==128u)break;
        used_opcodes[best]=true;
        printf("%s%02x:%lu",rank?",":"",best,(unsigned long)loopq->blocked_opcode[best]);
    }
    printf("\n");
#endif
#ifdef CU_AVR_BLOCK_STATS
    auint attempts=0u,hits=0u,instructions=0u;
    cu_avr_block_cache_stats(&attempts,&hits,&instructions);
    printf("UZEBOX_BLOCK name=%.32s attempts=%lu hits=%lu instructions=%lu\n",
        info.name,(unsigned long)attempts,(unsigned long)hits,(unsigned long)instructions);
#endif
#ifdef CU_AVR_NATIVE_REFERENCE
    auint native_blocks=0u,native_instructions=0u,native_rebuilds=0u;
    cu_avr_thumb_stats(&native_blocks,&native_instructions,&native_rebuilds);
    printf("UZEBOX_NATIVE name=%.32s blocks=%lu instructions=%lu rebuilds=%lu\n",
        info.name,(unsigned long)native_blocks,(unsigned long)native_instructions,
        (unsigned long)native_rebuilds);
#endif
#ifdef CU_AVR_TRACE_REFERENCE
    cu_avr_trace_stats_t trace={0};
    cu_avr_trace_stats(&trace);
    printf("UZEBOX_TRACE name=%.32s hits=%lu builds=%lu instructions=%lu replayed=%lu diverged=%lu max=%lu unique=%lu evictions=%lu hot8=%lu hot32=%lu highpc=%lu maxpc=%08lx\n",
        info.name,(unsigned long)trace.hits,(unsigned long)trace.builds,
        (unsigned long)trace.instructions,(unsigned long)trace.replayed,
        (unsigned long)trace.diverged,(unsigned long)trace.max_length,
        (unsigned long)trace.unique_starts,(unsigned long)trace.evictions,
        (unsigned long)trace.hot8_replayed,(unsigned long)trace.hot32_replayed,
        (unsigned long)trace.high_pc_starts,(unsigned long)trace.max_pc);
#endif
#ifdef CU_AVR_NATIVE_TRACE_REFERENCE
    auint native_builds=0u,native_entries=0u,native_instructions=0u,native_exits=0u,native_compiled=0u,native_short=0u,native_codecap=0u;
    cu_avr_native_trace_stats(&native_builds,&native_entries,&native_instructions,&native_exits,&native_compiled,&native_short);
    cu_avr_native_trace_codecap_stats(&native_codecap);
    printf("UZEBOX_NTRACE name=%.32s builds=%lu entries=%lu instructions=%lu exits=%lu compiled=%lu short=%lu codecap=%lu\n",
        info.name,(unsigned long)native_builds,(unsigned long)native_entries,
        (unsigned long)native_instructions,(unsigned long)native_exits,
        (unsigned long)native_compiled,(unsigned long)native_short,(unsigned long)native_codecap);
#ifdef CU_AVR_NATIVE_TRACE_LD
    auint native_ld_ok=0u,native_ld_io=0u;
    cu_avr_native_trace_ld_stats(&native_ld_ok,&native_ld_io);
    printf("UZEBOX_NLD name=%.32s guard_ok=%lu guard_io=%lu\n",
        info.name,(unsigned long)native_ld_ok,(unsigned long)native_ld_io);
#endif
#ifdef CU_AVR_NATIVE_TRACE_CHAIN
    auint native_chain_hits=0u,native_chain_segments=0u;
    cu_avr_native_trace_chain_stats(&native_chain_hits,&native_chain_segments);
    printf("UZEBOX_NCHAIN name=%.32s hits=%lu segments=%lu\n",
        info.name,(unsigned long)native_chain_hits,(unsigned long)native_chain_segments);
#endif
#endif
#if defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)
    cu_avr_loop_stats_t loops={0};cu_avr_loop_stats(&loops);
    printf("UZEBOX_LOOP_HOST name=%.32s entries=%lu iterations=%lu optimized_avr_cycles=%lu tile=%lu delay=%lu builds=%lu invalidations=%lu bitmap=%lu bitmap_cycles=%lu resume=%lu wait=%lu wait_cycles=%lu alu=%lu alu_instructions=%lu alu_cycles=%lu\n",
        info.name,(unsigned long)loops.entries,(unsigned long)loops.iterations,
        (unsigned long)loops.cycles,(unsigned long)loops.tile_entries,
        (unsigned long)loops.delay_entries,(unsigned long)loops.builds,
        (unsigned long)loops.invalidations,(unsigned long)loops.bitmap_entries,
        (unsigned long)loops.bitmap_cycles,(unsigned long)loops.bitmap_resumes,
        (unsigned long)loops.wait_entries,(unsigned long)loops.wait_cycles,
        (unsigned long)loops.alu_entries,(unsigned long)loops.alu_instructions,(unsigned long)loops.alu_cycles);
#endif
#ifdef CU_AVR_TIME_PROFILE
    cu_avr_time_stats_t timing;cu_avr_time_stats(&timing);
    uint64_t sum=timing.accounting_us,bins=0;
    for(unsigned i=0;i<CU_TIME_KINDS;i++)sum+=timing.us[i];
    for(unsigned i=0;i<32;i++)bins+=timing.scalar_pc_us[i];
#ifdef CU_AVR_FUSED
    if(timing.sampled_rows||timing.reads||sum||bins){
#else
    if(!timing.sampled_rows||sum!=timing.sampled_us||bins!=timing.us[CU_TIME_SCALAR]){
#endif
        fprintf(stderr,"Time attribution conservation failed\n");free(file);return false;
    }
#ifdef CU_AVR_DISABLE_ALU
    if(loops.alu_entries||timing.calls[CU_TIME_ALU]){free(file);return false;}
#endif
    printf("UZEBOX_TIME_HOST rows=%u sampled=%u span=%llu accounted=%llu bins=%llu\n",
        timing.rows,timing.sampled_rows,(unsigned long long)timing.sampled_us,
        (unsigned long long)sum,(unsigned long long)bins);
#ifdef CU_AVR_FUSED
    cu_avr_fused_stats_t fused;cu_avr_fused_stats(&fused);
    printf("UZEBOX_FUSED_HOST entries=%u instructions=%u cycles=%u pc=%u\n",
        fused.entries-fused_before.entries,fused.instructions-fused_before.instructions,
        fused.cycles-fused_before.cycles,fused.pc);
#ifdef UZEBOX_TEST_GAMEPLAY
    if(fused.entries==fused_before.entries){fprintf(stderr,"No fused gameplay execution\n");free(file);return false;}
#endif
#endif
#endif
    free(file);
#ifdef CU_AVR_BOARD_PROFILE
    return ok&&profile_ok;
#else
    return ok;
#endif
}
int main(int argc,char **argv){if(argc<2){fprintf(stderr,"usage: %s game.uze [...]\n",argv[0]);return 2;}bool ok=true;for(int i=1;i<argc;++i)ok=run_game(argv[i])&&ok;return ok?0:1;}
