/* Fixed-placement extended execution spans. SPDX-License-Identifier: BSD-3-Clause */
#ifndef UZEBOX_SPAN_H
#define UZEBOX_SPAN_H
#include <stdint.h>
#include <stdbool.h>
#ifdef UZEBOX_EXTENDED_SPAN
extern const volatile uint32_t uzebox_placement_control[2];
extern const volatile uint32_t uzebox_span_control[2];
unsigned int cu_avr_run_sram(void);
bool cu_avr_loop_run(void);
typedef struct {uint32_t entries,instructions,cycles,max_steps,budget_exits,barrier_exits,cap_exits;} uzebox_span_stats_t;
void uzebox_span_stats(uzebox_span_stats_t *out);
static inline unsigned uzebox_span_mode(void){return uzebox_span_control[1];}
#endif
#endif
