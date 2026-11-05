/* Diagnostic function placement comparison. SPDX-License-Identifier: BSD-3-Clause */
#ifndef UZEBOX_PLACEMENT_H
#define UZEBOX_PLACEMENT_H
#include <stdint.h>
#ifdef UZEBOX_HOT_PLACEMENT
extern const volatile uint32_t uzebox_placement_control[2];
unsigned int cu_avr_run_flash(void);
unsigned int cu_avr_run_sram(void);
static inline unsigned uzebox_placement_mode(void){return uzebox_placement_control[1];}
static inline uintptr_t uzebox_placement_selected(void){
    return uzebox_placement_mode()==1u?(uintptr_t)cu_avr_run_sram:(uintptr_t)cu_avr_run_flash;
}
#endif
#endif
