/* GPL-3.0-or-later. Guarded original-ISA Mode1 tile loop. */
#ifndef UZEBOX_MODE1_H
#define UZEBOX_MODE1_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {uint32_t mode,selftest,calls,iterations;} uzebox_mode1_stats_t;
extern const volatile uint32_t uzebox_mode1_control[2];
bool uzebox_mode1_selftest(void);
void uzebox_mode1_stats(uzebox_mode1_stats_t *out);
#endif
