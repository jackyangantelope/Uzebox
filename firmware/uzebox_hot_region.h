/* GPL-3.0-or-later; guarded CUzeBox static hot-region interface. */
#ifndef UZEBOX_HOT_REGION_H
#define UZEBOX_HOT_REGION_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
 uint32_t mode,selftest,matched,base,entries,instructions,cycles;
} uzebox_hot_region_stats_t;
extern const volatile uint32_t uzebox_hot_region_control[2];
bool uzebox_hot_region_selftest(void);
void uzebox_hot_region_stats(uzebox_hot_region_stats_t *out);
#endif
