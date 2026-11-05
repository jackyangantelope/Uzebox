/* GPL-3.0-or-later. Fixed phase4 bitmap experiment on P/N ON. */
#ifndef UZEBOX_PHASE4_H
#define UZEBOX_PHASE4_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t mode,selftest,calls; } uzebox_phase4_stats_t;
extern const volatile uint32_t uzebox_phase4_control[2];
bool uzebox_phase4_selftest(void);
void uzebox_phase4_stats(uzebox_phase4_stats_t *out);
#endif
