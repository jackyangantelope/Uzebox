/* GPL-3.0-or-later. Diagnostic-only wall-time attribution. */
#ifndef UZEBOX_SCAN_COST_H
#define UZEBOX_SCAN_COST_H
#include <stdint.h>
typedef struct {
    uint32_t phase,window,warm,mask,sampling;
    uint32_t samples[4],us[4];
} uzebox_scan_cost_t;
/* Packed phase | warm<<8 | window<<16; call only between complete frames. */
void uzebox_scan_cost_set(uint32_t config);
void uzebox_scan_cost_stats(uzebox_scan_cost_t *out);
uint32_t uzebox_scan_clock(void);
#endif
