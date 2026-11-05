/* Loop dispatcher placement experiment. SPDX-License-Identifier: BSD-3-Clause */
#ifndef UZEBOX_LOOP_PLACEMENT_H
#define UZEBOX_LOOP_PLACEMENT_H
#include <stdint.h>
#include <stdbool.h>
#ifdef UZEBOX_LOOP_PLACEMENT
extern const volatile uint32_t uzebox_placement_control[2];
extern const volatile uint32_t uzebox_loop_placement_control[2];
unsigned int cu_avr_run_sram(void);
bool cu_avr_loop_run_flash(void);
bool cu_avr_loop_run_sram(void);
static inline unsigned uzebox_loop_placement_mode(void){return uzebox_loop_placement_control[1];}
static inline uintptr_t uzebox_loop_placement_selected(void){
    return uzebox_loop_placement_mode()==1u?(uintptr_t)cu_avr_loop_run_sram:(uintptr_t)cu_avr_loop_run_flash;
}
#endif
#endif
