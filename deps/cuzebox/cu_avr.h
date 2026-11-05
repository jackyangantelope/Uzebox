/*
 *  AVR microcontroller emulation
 *
 *  Copyright (C) 2016
 *    Sandor Zsuga (Jubatian)
 *  Uzem (the base of CUzeBox) is copyright (C)
 *    David Etherton,
 *    Eric Anderton,
 *    Alec Bourque (Uze),
 *    Filipe Rinaldi,
 *    Sandor Zsuga (Jubatian),
 *    Matt Pandina (Artcfox)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#ifndef CU_AVR_H
#define CU_AVR_H



#include "cu_types.h"

#ifdef CU_AVR_TIME_PROFILE
enum { CU_TIME_SCALAR, CU_TIME_TILE, CU_TIME_BITMAP, CU_TIME_DELAY,
       CU_TIME_WAIT, CU_TIME_ALU, CU_TIME_REJECT, CU_TIME_EVENT, CU_TIME_IRQ,
#ifdef CU_AVR_REGION_COMPARE
       CU_TIME_REGION, CU_TIME_REGION_REJECT,
#endif
       CU_TIME_KINDS };
typedef struct {
 uint64_t us[CU_TIME_KINDS], scalar_pc_us[32U], sampled_us, accounting_us;
 auint calls[CU_TIME_KINDS], rows, sampled_rows, reads, enabled;
} cu_avr_time_stats_t;
/* Monotonic wrapping microseconds supplied by the platform, never guest cycles. */
auint cu_avr_time_clock(void);
void cu_avr_time_clear(void);
void cu_avr_time_enable(boole enabled);
void cu_avr_time_stats(cu_avr_time_stats_t *out);
#endif


/*
** Auto-fuses the CPU based on ROM contents and boot priority. The bootpri
** flag requests prioritizing the bootloader when set TRUE, otherwise game is
** prioritized (used when both a game and a bootloader appears to be present
** in the ROM). This should be called before reset unless the CPU state is
** already loaded (which includes fuse bits).
*/
void  cu_avr_autofuse(boole bootpri);


/*
** Resets the CPU as if it was power-cycled. It properly initializes
** everything from the state as if cu_avr_crom_update() and cu_avr_io_update()
** was called.
*/
void  cu_avr_reset(void);


/*
** Run emulation. Returns according to the return values defined in cu_types
** (emulating up to about 2050 cycles).
*/
auint cu_avr_run(void);


/*
** Returns emulator's cycle counter. It may be used to time emulation when it
** doesn't generate proper video signal. This is the cycle member of the CPU
** state (32 bits wrapping).
*/
auint cu_avr_getcycle(void);

#if defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)
typedef struct {
 auint builds, entries, iterations, cycles, tile_entries, delay_entries;
 auint guard_rejects, event_rejects, row_rejects, invalidations;
 auint bitmap_entries, bitmap_cycles;
 auint wait_entries, wait_iterations, wait_cycles;
 auint bitmap_resumes;
 auint alu_entries,alu_instructions,alu_cycles;
} cu_avr_loop_stats_t;
void cu_avr_loop_stats(cu_avr_loop_stats_t *out);
#ifdef CU_AVR_REGION
typedef struct {auint entries,instructions,cycles;} cu_avr_region_stats_t;
void cu_avr_region_stats(cu_avr_region_stats_t *out);
#ifdef CU_AVR_FUSED
typedef struct {auint entries,instructions,cycles,rejected,pc,memory,enabled;} cu_avr_fused_stats_t;
void cu_avr_fused_stats(cu_avr_fused_stats_t *out);
void cu_avr_fused_enable(boole enabled);
boole cu_avr_fused_enabled(void);
#endif
#ifdef CU_AVR_REGION_COMPARE
void cu_avr_region_enable(boole enabled);
boole cu_avr_region_enabled(void);
#endif
#endif
#ifdef CU_AVR_LOOP_NATIVE
boole cu_avr_loop_selftest(void);
#endif
#endif

#ifdef CU_AVR_NATIVE_THUMB
boole cu_avr_thumb_selftest(void);
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
void cu_avr_thumb_stats(auint *blocks, auint *instructions, auint *rebuilds);
#endif
#ifdef CU_AVR_NATIVE_VERIFY
void cu_avr_thumb_verify_stats(auint *checks, auint *mismatches,
                              auint *pc, auint *index, auint *got,
                              auint *expected, auint *events, auint *rows);
#endif

#ifdef CU_AVR_OPCODE_PROFILE
void cu_avr_profile_clear(void);
auint const* cu_avr_profile_counts(void);
auint const* cu_avr_profile_pc_counts(void);
auint cu_avr_profile_pc_compiled(auint pc);
void cu_avr_profile_flow(auint *blocks, auint *instructions_in_long_blocks,
                         auint *events, auint *max_block);
#endif
#ifdef CU_AVR_BOARD_PROFILE
#define CU_AVR_BOARD_PROFILE_PCS 32768U
typedef struct{
 auint samples;
 auint opcodes[128U];
 auint *pc_counts;
} cu_avr_board_profile_t;
void cu_avr_board_profile_attach(auint *pc_counts);
void cu_avr_board_profile_clear(void);
cu_avr_board_profile_t const *cu_avr_board_profile_state(void);
auint cu_avr_board_profile_opcode(auint pc);
#endif
#ifdef CU_AVR_LOOP_QUALIFY
typedef struct{
 auint samples;
 auint closed_exact;
 auint closed_affine;
 auint candidate;
 auint exact_candidate;
 auint affine_candidate;
 auint eligible;
 auint exact_eligible;
 auint affine_eligible;
 auint candidate_length_sum;
 auint window_limit;
 auint event_bound;
 auint row_bound;
 auint repeat_samples;
 auint repeat_iterations;
 auint repeat_capped;
 auint repeat_event_stops;
 auint repeat_row_stops;
 auint repeat_guard_stops;
 auint diverged;
 auint cycle_mismatch;
 auint rom_updates;
 auint rom_aborts;
 auint blocked_opcode[128U];
} cu_avr_loop_qualify_stats_t;
void cu_avr_loop_qualify_begin(void);
cu_avr_loop_qualify_stats_t const *cu_avr_loop_qualify_state(void);
#endif
#ifdef CU_AVR_BLOCK_STATS
void cu_avr_block_cache_stats(auint *attempts, auint *hits, auint *instructions);
#endif
#ifdef CU_AVR_TRACE_REFERENCE
typedef struct{
 auint hits;
 auint builds;
 auint instructions;
 auint replayed;
 auint diverged;
 auint max_length;
 auint unique_starts;
 auint evictions;
 auint hot8_replayed;
 auint hot32_replayed;
 auint high_pc_starts;
 auint max_pc;
} cu_avr_trace_stats_t;
void cu_avr_trace_stats(cu_avr_trace_stats_t *out);
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
void cu_avr_native_trace_stats(auint *builds, auint *entries,
                               auint *instructions, auint *side_exits,
                               auint *compiled_builds, auint *short_builds);
/* Cached successor hits and successfully executed extra chain segments. */
void cu_avr_native_trace_chain_stats(auint *hits, auint *segments);
/* Trace builds stopped because the next supported opcode exceeded code space. */
void cu_avr_native_trace_codecap_stats(auint *stops);
/* Cache lookups and admissions, cumulative since boot. Short hits are a
 * subset of hits; full rejects occur only when all ways are occupied. */
void cu_avr_native_trace_cache_stats(auint *hits, auint *misses,
                                      auint *short_hits, auint *cold_rejects,
                                      auint *full_rejects, auint *evictions);
/* Completed guarded SRAM loads and IO-address exits (per reset). */
void cu_avr_native_trace_ld_stats(auint *guard_pass, auint *guard_fail_io);
#ifdef CU_AVR_NATIVE_TRACE
boole cu_avr_native_trace_selftest(void);
#endif
#ifdef CU_AVR_NATIVE_TRACE_VERIFY
void cu_avr_native_trace_verify_stats(auint *checks, auint *mismatches,
                                      auint *pc, auint *index, auint *got,
                                      auint *expected);
#endif
#endif


/*
** Return current row. Note that continuing emulation will modify the returned
** structure's contents.
*/
cu_row_t const* cu_avr_get_row(void);


/*
** Return frame info. Note that continuing emulation will modify the returned
** structure's contents.
*/
cu_frameinfo_t const* cu_avr_get_frameinfo(void);


/*
** Returns memory access info block. It can be written (with zeros) to clear
** flags which are only set by the emulator. Note that the highest 256 bytes
** of the RAM come first here! (so address 0x0100 corresponds to AVR address
** 0x0100)
*/
uint8* cu_avr_get_meminfo(void);


/*
** Returns I/O register access info block. It can be written (with zeros) to
** clear flags which are only set by the emulator. It doesn't reflect implicit
** accesses, only those explicitly performed by read or write operations.
*/
uint8* cu_avr_get_ioinfo(void);


/*
** Returns whether the EEPROM changed since the last clear of this indicator.
** Calling cu_avr_io_update() clears this indicator (as well as resetting by
** cu_avr_reset()). Passing TRUE also clears it. This can be used to save
** EEPROM state to persistent storage when it changes.
*/
boole cu_avr_eeprom_ischanged(boole clear);


/*
** Returns whether the Code ROM changed since the last clear of this
** indicator. Calling cu_avr_io_update() clears this indicator (as well as
** resetting by cu_avr_reset()). Passing TRUE also clears it. This can be used
** to save Code ROM state to persistent storage when it changes.
*/
boole cu_avr_crom_ischanged(boole clear);


/*
** Returns whether the Code ROM was modified since reset. This can be used to
** determine if it is necessary to include the Code ROM in a save state.
** Internal Code ROM writes and the cu_avr_crom_update() function can set it.
*/
boole cu_avr_crom_ismod(void);


/*
** Returns AVR CPU state structure. It may be written, the Code ROM must be
** recompiled (by cu_avr_crom_update()) if anything in that area was updated
** or freshly written, and the IO space needs to be updated (by
** cu_avr_io_update()) if anything in that area was modified.
*/
cu_state_cpu_t* cu_avr_get_state(void);


/*
** Updates a section of the Code ROM. This must be called after writing into
** the Code ROM so the emulator recompiles the affected instructions. The
** "base" and "len" parameters specify the range to update in bytes.
*/
void  cu_avr_crom_update(auint base, auint len);


/*
** Updates the I/O area. If any change is performed in the I/O register
** contents (iors, 0x20 - 0xFF), this have to be called to update internal
** emulator state over it. It also updates state related to additional
** variables in the structure (such as the watchdog timer).
*/
void  cu_avr_io_update(void);


/*
** Returns last measured interval between WDR calls. Returns begin and end
** (word) addresses of WDR instructions into beg and end.
*/
auint cu_avr_get_lastwdrinterval(auint* beg, auint* end);


#endif
