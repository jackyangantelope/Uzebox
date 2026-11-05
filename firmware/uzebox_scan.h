/* GPL-3.0-or-later; fixed-P static scan experiment. */
#ifndef UZEBOX_SCAN_H
#define UZEBOX_SCAN_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
 uint32_t mode,selftest,matched,base,functions,prefixes,bitmaps;
} uzebox_scan_stats_t;
extern const volatile uint32_t uzebox_scan_control[2];
bool uzebox_scan_selftest(void);
void uzebox_scan_stats(uzebox_scan_stats_t *out);
#endif
