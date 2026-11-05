/* GPL-3.0-or-later. Fixed S ON; exact constant-port batch output. */
#ifndef UZEBOX_FILL_H
#define UZEBOX_FILL_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {uint32_t mode,selftest,calls,bytes;} uzebox_fill_stats_t;
extern const volatile uint32_t uzebox_fill_control[2];
bool uzebox_fill_selftest(void);
void uzebox_fill_stats(uzebox_fill_stats_t *out);
#endif
