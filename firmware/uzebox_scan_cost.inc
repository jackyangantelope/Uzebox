/* GPL-3.0-or-later. Inclusive samples; nested categories must not be summed.
 * Windows 1..6 have no clock reads; window 7 samples one call in sixteen.
 * Counters clear at each phase boundary. Guest cycles/events are untouched. */
#include "uzebox_scan_cost.h"
static uzebox_scan_cost_t uz_scan_cost;
static uint32_t uz_scan_cost_ticks[4];
static boole uz_scan_cost_sample(auint kind){
    return uz_scan_cost.sampling && !uz_scan_testing &&
        (((uz_scan_cost_ticks[kind]++)&15U)==0U);
}
static void uz_scan_cost_finish(auint kind,uint32_t start){
    uz_scan_cost.us[kind]+=uzebox_scan_clock()-start;
    uz_scan_cost.samples[kind]++;
}
void uzebox_scan_cost_set(uint32_t config){
    static uint8_t const masks[8]={0U,1U,0U,2U,0U,4U,0U,7U};
    uint32_t phase=config&7U,warm=(config>>8)&1U,window=(config>>16)&7U;
    if(phase!=uz_scan_cost.phase || warm!=uz_scan_cost.warm){
        memset(&uz_scan_cost,0,sizeof(uz_scan_cost));
        memset(uz_scan_cost_ticks,0,sizeof(uz_scan_cost_ticks));
    }
    uz_scan_cost.phase=phase;uz_scan_cost.window=window;uz_scan_cost.warm=warm;
    uz_scan_cost.mask=warm?masks[phase]:0U;
    uz_scan_cost.sampling=warm && window==7U;
}
void uzebox_scan_cost_stats(uzebox_scan_cost_t *out){*out=uz_scan_cost;}
