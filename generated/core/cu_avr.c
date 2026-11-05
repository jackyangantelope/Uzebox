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



#include "cu_avr.h"
#include "cu_avrc.h"
#include "cu_avrfg.h"
#include "cu_ctr.h"
#include "cu_spi.h"

#if defined(CU_AVR_OPCODE_PROFILE) && defined(CU_AVR_BLOCK_CACHE)
#error "Opcode profiling and block-cache execution cannot be combined"
#endif
#if defined(CU_AVR_BLOCK_STATS) && !defined(CU_AVR_BLOCK_CACHE)
#error "Block-cache statistics require block-cache execution"
#endif
#if (defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)) && defined(CU_AVR_BLOCK_CACHE)
#error "The native translator and C block cache are separate experiments"
#endif
#if defined(CU_AVR_TRACE_REFERENCE) && (defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE) || defined(CU_AVR_BLOCK_CACHE) || defined(CU_AVR_OPCODE_PROFILE))
#error "Trace reference must run alone against the baseline interpreter"
#endif
#if (defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)) && \
    (defined(CU_AVR_TRACE_REFERENCE) || defined(CU_AVR_NATIVE_THUMB) || \
     defined(CU_AVR_NATIVE_REFERENCE) || defined(CU_AVR_BLOCK_CACHE) || \
     defined(CU_AVR_OPCODE_PROFILE))
#error "Native trace must run as a separate AVR execution experiment"
#endif

#if (defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)) && \
    (defined(CU_AVR_TRACE_REFERENCE) || defined(CU_AVR_NATIVE_THUMB) || \
     defined(CU_AVR_NATIVE_REFERENCE) || defined(CU_AVR_BLOCK_CACHE) || \
     defined(CU_AVR_OPCODE_PROFILE) || defined(CU_AVR_NATIVE_TRACE) || \
     defined(CU_AVR_NATIVE_TRACE_REFERENCE) || defined(CU_AVR_LOOP_QUALIFY))
#error "Closed-loop execution must run as a separate AVR experiment"
#endif




/* Marker for this compilation unit for sub-includes */
#define CU_AVR_C



/* CPU state */
cu_state_cpu_t  cpu_state;

/* Compiled AVR instructions */
uint32          cpu_code[32768];

#ifdef CU_AVR_BLOCK_CACHE
#ifndef CU_AVR_BLOCK_SLOTS
#define CU_AVR_BLOCK_SLOTS 256U
#endif
#if CU_AVR_BLOCK_SLOTS < 16U || (CU_AVR_BLOCK_SLOTS & (CU_AVR_BLOCK_SLOTS - 1U)) != 0U
#error "CU_AVR_BLOCK_SLOTS must be a power of two of at least 16"
#endif
#define CU_AVR_BLOCK_MAX 8U
typedef struct {
 auint pc;
 auint epoch;
 uint32 op[CU_AVR_BLOCK_MAX];
 uint8 op_cycles[CU_AVR_BLOCK_MAX];
 uint8 len;
 uint8 cycles;
} cu_avr_block_t;
static cu_avr_block_t cu_avr_blocks[CU_AVR_BLOCK_SLOTS];
static auint cu_avr_block_epoch = 1U;
#ifdef CU_AVR_BLOCK_STATS
static auint cu_avr_block_attempts;
static auint cu_avr_block_hits;
static auint cu_avr_block_instructions;
#endif
#endif

/* Access info structure for SRAM */
uint8           access_mem[4096U];

/* Access info structure for I/O */
uint8           access_io[256U];

/* Precalculated flags */
uint8           cpu_pflags[CU_AVRFG_SIZE];

/* Whether the flags were already precalculated */
boole           pflags_done = FALSE;

/* Row generation structure */
cu_row_t        video_row;

/* Frame information structure */
cu_frameinfo_t  video_frame;

/* Sync pulse counter (0 - 270) */
auint           video_pulsectr;

/* Cycle of previous edge of sync signal */
auint           video_pedge;

/* Cycle of previous rising edge of sync signal (used to find VSync) */
auint           video_prise;

/* Row generation flag (passes when a row has to be signalled) */
boole           video_rowflag;

/* Cycle counter within row. At most approx. 2010 */
auint           video_cycle;

/* A tiny sample FIFO to align sample output with rows cleanly */
auint           audio_samples[4U];
auint           audio_rp;
auint           audio_wp;

/* Next hardware event's cycle. It can be safely set to
** WRAP32(cpu_state.cycle + 1U) to force full processing next time. */
auint           cycle_next_event;
#ifdef CU_AVR_LOOP_QUALIFY
/* A sampled instruction must not silently cross an event inside cu_avr_exec. */
static auint    cu_avr_loopq_hw_events;
#endif
#ifdef CU_AVR_OPCODE_PROFILE
static auint    cu_avr_profile_events;
#endif

/* Timer1 TCNT1 adjustment value: WRAP32(cpu_state.cycle - timer1_base)
** gives the correct TCNT1 any time. */
auint           timer1_base;

/* Interrupt might be waiting to be serviced flag. This is set nonzero by
** any event which should trigger an IT including setting the I flag in the
** status register. It can be safely set nonzero to ask for a certain IT
** check. */
boole           event_it;

/* Interrupt entry necessary if set. */
boole           event_it_enter;

/* Vector to call when entering interrupt */
auint           event_it_vect;

/* EEPROM change indicator for saves to persistent storage */
boole           eeprom_changed;

/* Code ROM change indicator for saves to persistent storage */
boole           crom_changed;

/* Watchdog-based debug counter: Time of last WDR execution */
auint           wd_last;

/* Watchdog-based debug counter: PC of last WDR execution */
auint           wd_last_pc;

/* Watchdog-based debug counter: Smallest interval between two WDR calls in a
** frame. Index 0 is the work value, Index 1 is the latched value. */
auint           wd_interval_min[2];

/* Watchdog-based debug counter: PC of baginning WDR of interval. Index 0 is
** the work value, Index 1 is the latched value. */
auint           wd_interval_beg[2];

/* Watchdog-based debug counter: PC of ending WDR of interval. Index 0 is the
** work value, Index 1 is the latched value. */
auint           wd_interval_end[2];



/* Watchdog 16 millisecond timer base tick count */
#define WD_16MS_BASE (458176U - 1024U)

/* Watchdog timing seed mask base, used to mask for the 16 ms timer */
#define WD_SEED_MASK 2048U

/* EEPROM programming time base, assume ~1.75ms */
#define EEPROM_EWR_TIM 50000U

/* SPM programming time base, assuming ~4ms */
#define SPM_PROG_TIM 100000U

/* Maximal cycles in a video scanline (above which sync error is returned) */
#define VIDEO_CY_MAX 1920U

/* Watchdog-based debug counter: Maximal interval to display */
#define WD_INTERVAL_MAX (VIDEO_CY_MAX * 262U / 2U)


/* Flags in CU_IO_SREG */
#define SREG_I  7U
#define SREG_T  6U
#define SREG_H  5U
#define SREG_S  4U
#define SREG_V  3U
#define SREG_N  2U
#define SREG_Z  1U
#define SREG_C  0U
#define SREG_IM 0x80U
#define SREG_TM 0x40U
#define SREG_HM 0x20U
#define SREG_SM 0x10U
#define SREG_VM 0x08U
#define SREG_NM 0x04U
#define SREG_ZM 0x02U
#define SREG_CM 0x01U


/* Macros for managing the flags */

/* Clear flags by mask */
#define SREG_CLR(fl, xm) (fl &= (auint)(~((auint)(xm))))

/* Set flags by mask */
#define SREG_SET(fl, xm) (fl |= (xm))

/* Set Zero if (at most) 16 bit "val" is zero */
#define SREG_SET_Z(fl, val) (fl |= SREG_ZM & (((auint)(val) - 1U) >> 16))

/* Set Carry by bit 15 (for multiplications) */
#define SREG_SET_C_BIT15(fl, val) (fl |= ((val) >> 15) & 1U)

/* Set Carry by bit 16 (for float multiplications and adiw & sbiw) */
#define SREG_SET_C_BIT16(fl, val) (fl |= ((val) >> 16) & 1U)

/* Combine N and V into S (for all ops updating N or V) */
#define SREG_COM_NV(fl) (fl |= (((fl) << 1) ^ ((fl) << 2)) & 0x10U)

/* Get carry flag (for carry overs) */
#define SREG_GET_C(fl) ((fl) & 1U)


/* Macro for calculating a multiplication's flags */
#define PROCFLAGS_MUL(fl, res) \
 do{ \
  SREG_CLR(fl, SREG_CM | SREG_ZM); \
  SREG_SET_C_BIT15(fl, res); \
  SREG_SET_Z(fl, res & 0xFFFFU); \
 }while(0)

/* Macro for calculating a floating multiplication's flags */
#define PROCFLAGS_FMUL(fl, res) \
 do{ \
  SREG_CLR(fl, SREG_CM | SREG_ZM); \
  SREG_SET_C_BIT16(fl, res); \
  SREG_SET_Z(fl, res & 0xFFFFU); \
 }while(0)


/* Macro for updating hardware from within instructions */
#define UPDATE_HARDWARE \
 do{ \
  cpu_state.cycle = WRAP32(cpu_state.cycle + 1U); \
  if (cycle_next_event == cpu_state.cycle){ cu_avr_hwexec(); } \
  video_row.pixels[video_cycle] = cpu_state.iors[CU_IO_PORTC]; \
  video_cycle ++; \
 }while(0)


/* Macro for consuming last instruction cycle before which ITs are triggered */
#define UPDATE_HARDWARE_IT \
 do{ \
  if (event_it){ cu_avr_itcheck(); } \
  UPDATE_HARDWARE; \
 }while(0)


/* Vectors base when IVSEL is 1 (MCUCR.1; Boot loader base) */
#define VBASE_BOOT     0x7800U


/* Various vectors */

/* Reset */
#define VECT_RESET     0x0000U
/* Watchdog */
#define VECT_WDT       0x0010U
/* Timer1 Comparator A */
#define VECT_T1COMPA   0x001AU
/* Timer1 Comparator B */
#define VECT_T1COMPB   0x001CU
/* Timer1 Overflow */
#define VECT_T1OVF     0x001EU
/* SPI */
#define VECT_SPISTC    0x0026U
/* USART0_TXC */
#define VECT_USART0_TX 0x002CU



/*
** Get Watchdog timeout ticks (WDP3 ignored, should be zero)
*/
static auint cu_avr_getwdto(void)
{
 auint presc = cpu_state.iors[CU_IO_WDTCSR] & 7U;
 return ( ((auint)(WD_16MS_BASE) << presc) +
          (cpu_state.wd_seed & (((auint)(WD_SEED_MASK) << presc) - 1U)) );
}



/*
** Resets the CPU by watchdog. It initializes CPU state according to CPU reset
** without touching other hardware and timing.
*/
static void  cu_avr_reset_wd(void)
{
 auint i;

 for (i = 0U; i < 256U; i++){ /* Most I/O regs are reset to zero */
  cpu_state.iors[i] = 0U;
 }
 cpu_state.iors[CU_IO_SPL] = 0xFFU;
 cpu_state.iors[CU_IO_SPH] = 0x10U;

 cpu_state.latch = 0U;

 cpu_state.pc = (((cpu_state.fuse[1] & 1U) ^ 1U) * VBASE_BOOT) + VECT_RESET;

 cpu_state.spi_tran = FALSE;
 cpu_state.wd_end   = WRAP32(cu_avr_getwdto() + cpu_state.cycle);
 cpu_state.eep_wrte = FALSE;
 cpu_state.spm_prge = FALSE;

 cu_avr_io_update();
}



/*
** Emulates cycle-precise hardware tasks. This is called through the
** UPDATE_HARDWARE macro if cycle_next_event matches the cycle counter (a new
** HW event is to be processed).
*/
static void cu_avr_hwexec(void)
{
#ifdef CU_AVR_LOOP_QUALIFY
 cu_avr_loopq_hw_events ++;
#endif
#ifdef CU_AVR_OPCODE_PROFILE
 cu_avr_profile_events ++;
#endif
 auint nextev = ~0U;
 auint t0;
 auint t1;
 auint t2;

 /* Timer 1 */

 if ((cpu_state.iors[CU_IO_TCCR1B] & 0x07U) != 0U){ /* Timer 1 started */

  t0 = (cpu_state.cycle - timer1_base) & 0xFFFFU;   /* Current TCNT1 value */
  t1 = ( ( ((auint)(cpu_state.iors[CU_IO_OCR1AL])     ) |
           ((auint)(cpu_state.iors[CU_IO_OCR1AH]) << 8) ) + 1U) & 0xFFFFU;
  t2 = ( ( ((auint)(cpu_state.iors[CU_IO_OCR1BL])     ) |
           ((auint)(cpu_state.iors[CU_IO_OCR1BH]) << 8) ) + 1U) & 0xFFFFU;

  if ((cpu_state.iors[CU_IO_TCCR1B] & 0x08U) != 0U){ /* Timer 1 in CTC mode: Counts to OCR1A, then resets */

   if (t0 == 0x0000U){                    /* Timer overflow (might happen if it starts above Comp. A, or Comp. A is 0) */
    cpu_state.iors[CU_IO_TIFR1] |= 0x01U;
    event_it = TRUE;
   }

   if (t0 == t2){
    cpu_state.iors[CU_IO_TIFR1] |= 0x04U; /* Comparator B interrupt */
    event_it = TRUE;
   }

   if (t0 == t1){
    cpu_state.iors[CU_IO_TIFR1] |= 0x02U; /* Comparator A interrupt */
    event_it = TRUE;
    timer1_base = cpu_state.cycle;        /* Reset timer to zero */
    t0 = 0U;                              /* Also reset for event calculation */
   }

   if ( (t0 != t2) &&
        (nextev > (t2 - t0)) ){ nextev = t2 - t0; } /* Next Comp. B match */
   if ( (nextev > (t1 - t0)) ){ nextev = t1 - t0; } /* Next Comp. A match */
   if ( (nextev > (0x10000U - t0)) ){ nextev = 0x10000U - t0; } /* Next Overflow (if timer was set above Comp. A) */

  }else{ /* Timer 1 in normal mode: wrapping 16 bit counter */

   if (t0 == 0x0000U){
    cpu_state.iors[CU_IO_TIFR1] |= 0x01U; /* Overflow interrupt */
    event_it = TRUE;
    timer1_base = cpu_state.cycle;        /* Reset timer to zero */
   }

   if (nextev > (0x10000U - t0)){ nextev = 0x10000U - t0; }

  }

 }

 /* Watchdog (in Uzebox used to seed random number generators and soft reset) */

 if ((cpu_state.iors[CU_IO_WDTCSR] & 0x48U) != 0U){ /* Watchdog is operational */

  if (cpu_state.cycle == cpu_state.wd_end){ /* Watchdog timed out */
   cpu_state.iors[CU_IO_WDTCSR] |= 0x80U;   /* Watchdog interrupt flag */
   event_it = TRUE;
   cpu_state.wd_end = WRAP32(cu_avr_getwdto() + cpu_state.cycle);
   if ((cpu_state.iors[CU_IO_WDTCSR] & 0x48U) == 0x08U){ /* WDE set & WDIE clear: System reset! */
    cu_avr_reset_wd();
   }
  }

  t0 = WRAP32(cpu_state.wd_end - cpu_state.cycle);
  if (nextev > t0){ nextev = t0; }

 }

 /* USART0 peripherals (RLE mode end scanline) */

 if (cpu_state.usr0_tran){

  if (cpu_state.cycle == cpu_state.usr0_end){

   cpu_state.usr0_tran = FALSE;
   cpu_state.iors[CU_IO_UCSR0A] |= 0x40U; /* USART0 TX complete flag */
   event_it = TRUE;

  }else{

   t0 = WRAP32(cpu_state.usr0_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }

  }

 }

 /* SPI peripherals (SD card, SPI RAM) */

 if (cpu_state.spi_tran){

  if (cpu_state.cycle == cpu_state.spi_end){

   cpu_state.spi_tran = FALSE;
   cpu_state.iors[CU_IO_SPSR] |= 0x80U; /* SPI interrupt */
   event_it = TRUE;
   cpu_state.iors[CU_IO_SPDR] = cpu_state.spi_rx;
   cu_spi_send(cpu_state.spi_tx, cpu_state.cycle);

  }else{

   t0 = WRAP32(cpu_state.spi_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }

  }

 }

 /* EEPROM */

 if (cpu_state.eep_wrte){

  if (cpu_state.cycle == cpu_state.eep_end){

   cpu_state.eep_wrte = FALSE;
   t0 = cpu_state.iors[CU_IO_EECR] & 0x30U; /* EEPROM write mode */
   t1 = ( ((auint)(cpu_state.iors[CU_IO_EEARH]) << 8) |
          ((auint)(cpu_state.iors[CU_IO_EEARL])     ) ) & 0x7FFU;
   t2 = cpu_state.eepr[t1];
   if       (t0 == 0x00U){ /* Erase and Write */
    cpu_state.eepr[t1]  = cpu_state.iors[CU_IO_EEDR];
   }else if (t0 == 0x10U){ /* Erase only */
    cpu_state.eepr[t1]  = 0xFFU;
   }else if (t0 == 0x20U){ /* Write only */
    cpu_state.eepr[t1] |= cpu_state.iors[CU_IO_EEDR];
   }else{                  /* Reserved: Do nothing */
   }
   if (t2 != t1){ eeprom_changed = TRUE; }
   cpu_state.iors[CU_IO_EECR] &= ~0x02U; /* Clear EEPE (programming completed) */

  }else{

   t0 = WRAP32(cpu_state.eep_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }

  }

 }else if ((cpu_state.iors[CU_IO_EECR] & 0x04U) != 0U){

  if (cpu_state.cycle == cpu_state.eep_end){
   cpu_state.iors[CU_IO_EECR] &= ~0x04U; /* Clear EEMPE (disable programming) */
  }else{
   t0 = WRAP32(cpu_state.eep_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }
  }

 }else{}

 /* SPM */

 if (cpu_state.spm_prge){

  if (cpu_state.cycle == cpu_state.spm_end){

   cpu_state.spm_prge = FALSE; /* Completed erasing / programming */
   cpu_state.iors[CU_IO_SPMCSR] &= ~0x1FU;

  }else{

   t0 = WRAP32(cpu_state.spm_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }

  }

 }else if ((cpu_state.iors[CU_IO_SPMCSR] & 0x01U) != 0U){

  if (cpu_state.cycle == cpu_state.spm_end){
   cpu_state.iors[CU_IO_SPMCSR] &= ~0x01U; /* Cancel SPM instruction */
  }else{
   t0 = WRAP32(cpu_state.spm_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }
  }

 }else{}

 /* Calculate next event's cycle */

 cycle_next_event = WRAP32(cpu_state.cycle + nextev);
}



/*
** Enters requested interrupt (event_it_enter must be true)
*/
static void cu_avr_interrupt(void)
{
 auint tmp;

 event_it_enter = FALSE; /* Requested IT entry performed */

 SREG_CLR(cpu_state.iors[CU_IO_SREG], SREG_IM);

 tmp   = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
         ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
 cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc     ) & 0xFFU;
 access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
 tmp --;
 cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc >> 8) & 0xFFU;
 access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
 tmp --;
 cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU;
 cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU;

 cpu_state.pc = event_it_vect;

 UPDATE_HARDWARE;
 UPDATE_HARDWARE;
 UPDATE_HARDWARE;
}



/*
** Checks for interrupts and triggers if any is pending. Clears event_it when
** there are no more interrupts waiting for servicing.
*/
static void cu_avr_itcheck(void)
{
 auint vbase;

 /* Global interrupt enable? */

 if ((cpu_state.iors[CU_IO_SREG] & SREG_IM) == 0U){
  event_it = FALSE;
  return;
 }

 /* Interrupts are enabled, so check them */

 vbase = ((cpu_state.iors[CU_IO_MCUCR] >> 1) & 1U) * VBASE_BOOT;

 if       ( (cpu_state.iors[CU_IO_SPCR] &
             cpu_state.iors[CU_IO_SPSR] & 0x80U)   !=    0U ){ /* SPI */

  cpu_state.iors[CU_IO_SPSR] ^= 0x80U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_SPISTC;

 }else if ( (cpu_state.iors[CU_IO_WDTCSR] & 0xC0U) == 0xC0U ){ /* Watchdog */

  cpu_state.iors[CU_IO_WDTCSR] &= 0x7FU;             /* Always clear WDIF */
  if ((cpu_state.iors[CU_IO_WDTCSR] & 0x08U) != 0U){ /* WDE */
   cpu_state.iors[CU_IO_WDTCSR] &= 0xBFU;            /* Clear WDIE too (next WD timeout is a sys reset) */
  }
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_WDT;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x02U) !=    0U ){ /* Timer 1 Comparator A */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x02U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1COMPA;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x04U) !=    0U ){ /* Timer 1 Comparator B */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x04U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1COMPB;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x01U) !=    0U ){ /* Timer 1 Overflow */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x01U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1OVF;

 }else if ( (cpu_state.iors[CU_IO_UCSR0A] & 0x40U) !=    0U ){ /* USART0 dodgy timer interrupt */

  cpu_state.iors[CU_IO_UCSR0A] ^= 0x40U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_USART0_TX;

 }else{ /* No interrupts are pending */

  event_it = FALSE;

 }
}



/*
** Writes an I/O port
*/
static void  cu_avr_write_io(auint port, auint val)
{
 auint pval = cpu_state.iors[port]; /* Previous value */
 auint cval = val & 0xFFU;          /* Current (requested) value */
 auint pio;
 auint cio;
 auint t0;
 auint t1;

 access_io[port] |= CU_MEM_W;

 switch (port){

  case CU_IO_PORTA:   /* Controller inputs, SPI RAM Chip Select */
  case CU_IO_DDRA:

   if (port == CU_IO_PORTA){
    pio = pval & cpu_state.iors[CU_IO_DDRA];
    cio = cval & cpu_state.iors[CU_IO_DDRA];
   }else{
    pio = pval & cpu_state.iors[CU_IO_PORTA];
    cio = cval & cpu_state.iors[CU_IO_PORTA];
   }
   cpu_state.iors[CU_IO_PINA] = cu_ctr_process(pio, cio);
   cu_spi_cs_set(CU_SPI_CS_RAM, (cio & 0x10U) == 0U, cpu_state.cycle);
   break;

  case CU_IO_PORTB:   /* Sync output */
  case CU_IO_DDRB:

   if (port == CU_IO_PORTB){
    pio = pval & cpu_state.iors[CU_IO_DDRB];
    cio = cval & cpu_state.iors[CU_IO_DDRB];
   }else{
    pio = pval & cpu_state.iors[CU_IO_PORTB];
    cio = cval & cpu_state.iors[CU_IO_PORTB];
   }

   if (((pio ^ cio) & 1U) != 0U){   /* Sync edge */

    t0 = WRAP32(cpu_state.cycle - video_pedge); /* Cycles elapsed since previous edge */
    video_pedge = cpu_state.cycle;

    if ((cio & 1U) == 1U){      /* Rising edge */

     if ( (video_pulsectr <= 268U) &&
          (video_pulsectr != 251U) ){
      video_pulsectr ++;
     }
     if (video_pulsectr <  252U){
      video_frame.rowcdif ++;
     }
     if (video_pulsectr == 270U){
      video_pulsectr      = 0U;
      video_frame.rowcdif = 0U - 252U;
     }
     t1 = WRAP32(cpu_state.cycle - video_prise);
     video_prise = cpu_state.cycle;
     if       ( (t1 >=  944U) &&
                (t1 <= 1012U) && /* Sync to first normal pulse (978 cycles apart from last rise) */
                (video_pulsectr >= 252U) ){
      video_pulsectr = 270U;
     }else if ( (t1 >= 1718U) &&
                (t1 <= 1786U) ){ /* Sync to first VSync pulse (1752 cycles apart from last rise) */
      video_pulsectr = 252U;
     }else{}

     if ( (video_pulsectr < 252U) ||
          (video_pulsectr == 270U) ){ /* 0 - 251 & 270 are normal rises 136 cycles after the fall */
      video_frame.pulse[video_pulsectr].rise = t0 - 136U;
     }else{
      switch (video_pulsectr){
       case 252U:
       case 253U:
       case 254U:
       case 255U:
       case 256U:
       case 257U:                /* 251 - 257 come 68 cycles after the fall */
        video_frame.pulse[video_pulsectr].rise = t0 - 68U;
        break;
       case 258U:
       case 259U:
       case 260U:
       case 261U:
       case 262U:
       case 263U:                /* 258 - 263 come 774 cycles after the fall */
        video_frame.pulse[video_pulsectr].rise = t0 - 774U;
        break;
       case 264U:
       case 265U:
       case 266U:
       case 267U:
       case 268U:
       case 269U:                /* 264 - 269 come 68 cycles after the fall */
        video_frame.pulse[video_pulsectr].rise = t0 - 68U;
        break;
       default:                  /* Out of sync */
        break;
      }
     }

    }else{                       /* Falling edge */

     if ( (video_pulsectr < 252U) ||
          (video_pulsectr == 270U) ){ /* 0 - 251 & 270 are normal falls 1684 cycles after the rise */
      video_frame.pulse[video_pulsectr].fall = t0 - 1684U;
      video_rowflag = TRUE;      /* Trigger new row */
      video_cycle   = VIDEO_CY_MAX; /* Also flags new row */
     }else{
      switch (video_pulsectr){
       case 252U:
       case 253U:
       case 254U:
       case 255U:
       case 256U:
       case 257U:                /* 252 - 257 come 842 cycles after the rise */
        video_frame.pulse[video_pulsectr].fall = t0 - 842U;
        break;
       case 258U:
       case 259U:
       case 260U:
       case 261U:
       case 262U:
       case 263U:                /* 258 - 263 come 136 cycles after the rise */
        video_frame.pulse[video_pulsectr].fall = t0 - 136U;
        break;
       case 264U:
       case 265U:
       case 266U:
       case 267U:
       case 268U:
       case 269U:                /* 264 - 269 come 842 cycles after the rise */
        video_frame.pulse[video_pulsectr].fall = t0 - 842U;
        break;
       default:                  /* Out of sync */
        break;
      }
      if ((video_pulsectr & 1U) != 0U){
       video_rowflag = TRUE;    /* Odd pulses trigger new row */
       video_cycle   = VIDEO_CY_MAX; /* Also flags new row */
      }
     }

    }

   }

   break;

  case CU_IO_PORTC:   /* Pixel output */

   /* Special shortcut masking with the DDR register. This port tolerates a
   ** bit of inaccuracy since it is only graphics and is most frequently
   ** written "normally" (the DDR is used for fade effects on it) */
   cval &= cpu_state.iors[CU_IO_DDRC];
   break;

  case CU_IO_PORTD:   /* SD card Chip Select */
  case CU_IO_DDRD:

   if (port == CU_IO_PORTD){
    cio = cval & cpu_state.iors[CU_IO_DDRD];
   }else{
    cio = cval & cpu_state.iors[CU_IO_PORTD];
   }
   cu_spi_cs_set(CU_SPI_CS_SD, (cio & 0x40U) == 0U, cpu_state.cycle);
   break;

  case CU_IO_OCR2A:   /* PWM audio output */

   if (cpu_state.iors[CU_IO_TCCR2B] != 0U){
    audio_samples[audio_wp] = cval;
    if (audio_rp == audio_wp){ audio_rp = (audio_rp + 1U) & 0x3U; }
    audio_wp = (audio_wp + 1U) & 0x3U;
   }
   break;

  case CU_IO_TCNT1H:  /* Timer1 counter, high */

   cpu_state.latch = cval; /* Write into latch (value written to the port itself is ignored) */
   break;

  case CU_IO_TCNT1L:  /* Timer1 counter, low */

   t0    = (cpu_state.latch << 8) | cval;
   timer1_base = WRAP32(cpu_state.cycle - t0);
   cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
   break;

  case CU_IO_TIFR1:   /* Timer1 interrupt flags */

   cval  = pval & (~cval);
   break;

  case CU_IO_TCCR1B:  /* Timer1 control */
  case CU_IO_OCR1AH:  /* Timer1 comparator A, high */
  case CU_IO_OCR1AL:  /* Timer1 comparator A, low */
  case CU_IO_OCR1BH:  /* Timer1 comparator B, high */
  case CU_IO_OCR1BL:  /* Timer1 comparator B, low */

   cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
   break;


  case CU_IO_UDR0:    /* USART0 data */
   cpu_state.usr0_tran = TRUE;
   cpu_state.usr0_end  = WRAP32( cpu_state.cycle + 1304 );
   if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
        (WRAP32(cpu_state.usr0_end - cpu_state.cycle)) ){
    cycle_next_event = cpu_state.usr0_end; /* Set USR HW processing target */
   }
   break;

  case CU_IO_SPDR:    /* SPI data */

   cpu_state.iors[CU_IO_SPSR] &= ~0x80U;
   /* Note: By the doc first a read would be necessary for clearing SPIF here,
   ** but that's a very unusual use case, so ignored */
   if ((cpu_state.iors[CU_IO_SPCR] & 0x40U) != 0U){ /* SPI enabled */
    if (cpu_state.spi_tran){                /* Already sending */
     cpu_state.iors[CU_IO_SPSR] |= 0x40U;   /* Signal write collision */
    }else{
     cpu_state.spi_tran = TRUE;
     cpu_state.spi_end  = WRAP32( cpu_state.cycle +
          (16U << ( ((cpu_state.iors[CU_IO_SPCR] & 0x3U) << 1) |
                    ((cpu_state.iors[CU_IO_SPSR] & 0x1U) ^ 1U) )) );
     if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
          (WRAP32(cpu_state.spi_end - cpu_state.cycle)) ){
      cycle_next_event = cpu_state.spi_end; /* Set SPI HW processing target */
     }
     cpu_state.spi_rx = cu_spi_recv(cpu_state.cycle);
     cpu_state.spi_tx = cval;
    }
   }
   break;

  case CU_IO_SPCR:    /* SPI control */

   break;

  case CU_IO_SPSR:    /* SPI status */

   break;

  case CU_IO_EECR:    /* EEPROM control */

   if ((pval & 0x04U) == 0U){
    cval &= ~0x02U;   /* Without EEMPE, programming (EEPE) can not start */
   }else{
    cpu_state.eep_end  = WRAP32(cpu_state.cycle + 4U); /* Open EEPE window (4 cycles) */
    if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
         (WRAP32(cpu_state.eep_end - cpu_state.cycle)) ){
     cycle_next_event = cpu_state.eep_end; /* Set EEPROM HW processing target */
    }
   }

   if ( ((pval & 0x02U) == 0U) &&
        ((cval & 0x02U) != 0U) ){ /* Programming started */
    cpu_state.eep_wrte = TRUE;
    t0 = cpu_state.iors[CU_IO_EECR] & 0x30U;  /* EEPROM write mode */
    cpu_state.eep_end  = EEPROM_EWR_TIM;
    if (t0 == 0U){ cpu_state.eep_end *= 2U; } /* Erase + Write */
    cpu_state.eep_end  = WRAP32(cpu_state.eep_end + cpu_state.cycle);
    if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
         (WRAP32(cpu_state.eep_end - cpu_state.cycle)) ){
     cycle_next_event = cpu_state.eep_end; /* Set EEPROM HW processing target */
    }
    UPDATE_HARDWARE;  /* 2 cycles write stall. */
    UPDATE_HARDWARE;  /* Note: IT checks are slightly off due to this, but this inaccuracy is tolerable. */
    cval &= ~0x04U;   /* Turn off EEMPE (succesfully entered programming) */
   }

   if (cval & 0x01U){ /* EEPROM read (EERE) strobe */
    if (!cpu_state.eep_wrte){ /* During writing it can't be done */
     t0 = ( ((auint)(cpu_state.iors[CU_IO_EEARH]) << 8) |
            ((auint)(cpu_state.iors[CU_IO_EEARL])     ) ) & 0x7FFU;
     cpu_state.iors[CU_IO_EEDR] = cpu_state.eepr[t0];
     UPDATE_HARDWARE;
     UPDATE_HARDWARE;
     UPDATE_HARDWARE; /* 4 cycles read stall. */
     UPDATE_HARDWARE; /* Note: IT checks are slightly off due to this, but this inaccuracy is tolerable. */
    }
    cval &= ~0x01U;
    /* Note: The EERE bit is a little hazy, it is not described whether it is
    ** cleared after write or not, the SBI / CBI instructions might work
    ** differently on this port than read + mask + write. Clearing it however
    ** works for the documented usage (the bit is never read anyway). */
   }
   break;

  case CU_IO_EEARH:   /* EEPROM address & data registers */
  case CU_IO_EEARL:
  case CU_IO_EEDR:

   if (cpu_state.eep_wrte){
    cval = pval;      /* During EEPROM programming, these can't be modified */
   }
   break;

  case CU_IO_WDTCSR:  /* Watchdog timer control */

   if ( ((pval & 0x48U) == 0U) &&
        ((cval & 0x48U) != 0U) ){ /* Watchdog becomes enabled, so start it */
    cpu_state.wd_end = WRAP32(cu_avr_getwdto() + cpu_state.cycle);
    if ( (WRAP32(cycle_next_event - cpu_state.cycle)) >
         (WRAP32(cpu_state.wd_end - cpu_state.cycle)) ){
     cycle_next_event = cpu_state.wd_end; /* Set Watchdog timeout HW processing target */
    }
   }
   break;

  case CU_IO_SPMCSR:  /* Store program memory control & status */

   cval = (cval & (~0x40U)) | (pval & 0x40U); /* RRWSB bit can not be written */
   if ( ((pval & 0x01U) == 0U) &&
        ((cval & 0x01U) != 0U) && /* SPM enable just turned on */
        (!cpu_state.spm_prge) ){
    t0 = cval & 0x1EU;    /* SPM mode select bits */
    if ( (t0 == 0x00U) || /* Page load (filling temp buffer) */
         (t0 == 0x02U) || /* Page erase */
         (t0 == 0x04U) || /* Page write */
         (t0 == 0x08U) || /* Boot lock bit set */
         (t0 == 0x10U) ){ /* RWW section read enable */
     cpu_state.spm_mode = t0;
     cpu_state.spm_end  = WRAP32(cpu_state.cycle + 4U);
     if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
          (WRAP32(cpu_state.spm_end - cpu_state.cycle)) ){
      cycle_next_event = cpu_state.spm_end; /* Set SPM HW processing target */
     }
    }
   }
   break;

  case CU_IO_SREG:    /* Status register */

   if ((((~pval) & cval) & SREG_IM) != 0U){
    event_it = TRUE;  /* Interrupts become enabled, so check them */
   }
   break;

  default:

   break;

 }

 cpu_state.iors[port] = cval;
}



/*
** Reads from an I/O port
*/
static auint cu_avr_read_io(auint port)
{
 auint t0;
 auint ret;

 access_io[port] |= CU_MEM_R;

 switch (port){

  case CU_IO_TCNT1L:
   t0  = WRAP32(cpu_state.cycle - timer1_base); /* Current TCNT1 value */
   cpu_state.latch = (t0 >> 8) & 0xFFU;
   ret = t0 & 0xFFU;
   break;

  case CU_IO_TCNT1H:
   ret = cpu_state.latch;
   break;

  default:
   ret = cpu_state.iors[port];
   break;
 }

 return ret;
}



/*
** Emulates a single (compiled) AVR instruction and any associated hardware
** tasks.
*/
#ifdef CU_AVR_OPCODE_PROFILE
static auint cu_avr_opcode_counts[128U];
static auint cu_avr_pc_counts[32768U];
static auint cu_avr_profile_blocks;
static auint cu_avr_profile_long_instructions;
static auint cu_avr_profile_max_block;
static auint cu_avr_profile_current_block;
void cu_avr_profile_clear(void)
{
 auint i;
 for (i = 0U; i < 128U; i++){ cu_avr_opcode_counts[i] = 0U; }
 for (i = 0U; i < 32768U; i++){ cu_avr_pc_counts[i] = 0U; }
 cu_avr_profile_blocks = 0U;
 cu_avr_profile_long_instructions = 0U;
 cu_avr_profile_events = 0U;
 cu_avr_profile_max_block = 0U;
 cu_avr_profile_current_block = 0U;
}
auint const* cu_avr_profile_counts(void)
{
 return cu_avr_opcode_counts;
}
auint const* cu_avr_profile_pc_counts(void)
{
 return cu_avr_pc_counts;
}
auint cu_avr_profile_pc_compiled(auint pc)
{
 return cpu_code[pc & 0x7FFFU];
}
void cu_avr_profile_flow(auint *blocks, auint *instructions_in_long_blocks,
                         auint *events, auint *max_block)
{
 *blocks = cu_avr_profile_blocks;
 *instructions_in_long_blocks = cu_avr_profile_long_instructions;
 *events = cu_avr_profile_events;
 *max_block = cu_avr_profile_max_block;
}
#endif

#ifdef CU_AVR_BOARD_PROFILE
#include "cu_avr_board_profile.inc"
#endif

#if defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)
/* LPM, LPM+ and DEC ignore arg2. Its high bit can mark recognized loop heads
 * without adding a RAM bitmap or changing scalar instruction semantics. */
#define CU_AVR_LOOP_HEAD_MARKER ((uint32)0x80000000U)
/* CALL ignores arg1. Its bit7 is metadata, distinct from opcode bit7,
 * which preserves the decoder's two-word skip length. */
#define CU_AVR_LOOP_CALL_MARKER ((uint32)0x00008000U)
boole cu_avr_loop_run(void);
#ifdef CU_AVR_REGION
#define CU_AVR_REGION_MARKER ((uint32)0x00008000U)
#ifdef CU_AVR_REGION_COMPARE
#if !defined(CU_AVR_TIME_PROFILE) || defined(CU_AVR_DISABLE_ALU)
#error "Region comparison requires time profiling and the original enabled ALU"
#endif
static volatile auint cu_avr_region_gate;
void cu_avr_region_enable(boole enabled){cu_avr_region_gate=enabled!=FALSE;}
boole cu_avr_region_enabled(void){return cu_avr_region_gate!=0U;}
#define CU_AVR_REGION_ENABLED (cu_avr_region_gate!=0U)
#else
#define CU_AVR_REGION_ENABLED TRUE
#endif
static boole cu_avr_region_run(void);

#include "uzebox_fill.h"
static boole cu_avr_loop_disabled;
static auint uz_fill_status,uz_fill_calls,uz_fill_bytes;
const volatile uint32_t uzebox_fill_control[2]
 __attribute__((section(".rodata.uzebox_fill"),used))={0x54313856U,1U};
#define UZ_FILL_MODE (!cu_avr_loop_disabled && uz_fill_status==1U && uzebox_fill_control[1]==1U)
/* GPL-3.0-or-later. All caller admission/cycle arithmetic remains original.
 * No I/O/event/IRQ occurs inside the already accepted constant-port batch. */
static void __attribute__((noinline,noclone,section(".time_critical.uz_fill_constant")))
uz_fill_constant(uint8 *pixels,auint pixel,auint length){
 memset(pixels,(uint8)pixel,length);
 uz_fill_calls++;uz_fill_bytes+=length;
}


#include "uzebox_phase4.h"
static boole cu_avr_loop_disabled;
static auint uz_phase4_status,uz_phase4_testing,uz_phase4_calls;
const volatile uint32_t uzebox_phase4_control[2]
 __attribute__((section(".rodata.uzebox_phase4"),used))={0x53313756U,1U};
#define uz_phase4_mode (!cu_avr_loop_disabled && (uz_phase4_testing || (uz_phase4_status==1U && uzebox_phase4_control[1]==1U)))

#ifdef CU_AVR_FUSED
static auint cu_avr_fused_pc;
static boole cu_avr_fused_run(void);
static void cu_avr_fused_classify(void);
#ifdef CU_AVR_LOOP_NATIVE
static boole cu_avr_fused_selftest(void);
#endif
static volatile auint cu_avr_fused_switch[2U]={1U,1U};
#define CU_AVR_FUSED_ENABLED (cu_avr_fused_switch[1U]!=0U)
void cu_avr_fused_enable(boole enabled){cu_avr_fused_switch[1U]=enabled!=FALSE;}
boole cu_avr_fused_enabled(void){return CU_AVR_FUSED_ENABLED;}
#endif
static void cu_avr_region_classify(auint base, auint len);
#ifdef CU_AVR_LOOP_NATIVE
static boole cu_avr_region_selftest(void);
#endif
#endif
#ifdef CU_AVR_TIME_PROFILE
/* Keep both diagnostic cores identical; the switch differs only in data.
 * The nonzero first word keeps the whole object in .data for both variants. */
static volatile auint cu_avr_time_alu_switch[2U]={1U,
#ifdef CU_AVR_DISABLE_ALU
 0U
#else
 1U
#endif
};
#define CU_AVR_ALU_ENABLED (cu_avr_time_alu_switch[1U]!=0U)
#elif defined(CU_AVR_DISABLE_ALU)
#define CU_AVR_ALU_ENABLED FALSE
#else
#define CU_AVR_ALU_ENABLED TRUE
#endif
#endif

#ifdef CU_AVR_SCALAR_INLINE
/* Only the ordinary ISA copy is forced inline. Timed/oracle copies retain
 * their original compilation policy. No guest instruction semantics change. */
#define CU_AVR_EXEC_FORCE_INLINE 1
#endif
#ifdef __EMSCRIPTEN__
#ifndef FLAG_NATIVE
#include "cu_avr_e.h"
#else
#include "cu_avr_n.h"
#endif
#else
#include "cu_avr_n.h"
#endif
#undef CU_AVR_EXEC_FORCE_INLINE

#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
#include "cu_avr_thumb.inc"
#endif
#ifdef CU_AVR_TRACE_REFERENCE
#include "cu_avr_trace.inc"
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
#include "cu_avr_native_trace.inc"
#endif
#ifdef CU_AVR_LOOP_QUALIFY
#include "cu_avr_loop_qualify.inc"
#endif

#if defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)
#include "uzebox_y.inc"
#include "cu_avr_loop_native.inc"
#endif

#ifdef CU_AVR_FUSED
#include "cu_avr_fused.inc"
#endif

#ifdef CU_AVR_TIME_PROFILE
#if !defined(CU_AVR_LOOP_NATIVE) && !defined(CU_AVR_LOOP_REFERENCE)
#error "Time diagnostic requires the D closed-loop execution path"
#endif
#include "cu_avr_time.inc"
#endif

#ifdef CU_AVR_BLOCK_CACHE
/* Only register/ROM/video operations with fixed cycle counts enter this path.
** Every other instruction remains in the original cycle-accurate interpreter. */
static auint cu_avr_block_cycles(auint opid)
{
 switch (opid){
  case 0x00U: case 0x01U: case 0x09U: case 0x0CU: case 0x0DU:
  case 0x0EU: case 0x0FU: case 0x10U: case 0x11U: case 0x12U:
  case 0x14U: case 0x15U: case 0x16U: case 0x26U: case 0x27U:
  case 0x2BU: case 0x48U: case 0x49U: return 1U;
  case 0x18U: case 0x19U: case 0x2CU: case 0x0AU:
  case 0x3DU: case 0x3FU: case 0x46U: case 0x47U: return 3U;
  case 0x40U: case 0x42U: case 0x43U: return 2U;
  default: return 0U;
 }
}

static boole cu_avr_block_ends(auint opid)
{
 return opid == 0x0AU || opid == 0x2CU || opid == 0x3DU ||
        opid == 0x3FU || opid == 0x40U || opid == 0x42U ||
        opid == 0x43U || opid == 0x46U || opid == 0x47U;
}

static boole cu_avr_block_run(void)
{
 auint const pc = cpu_state.pc & 0x7FFFU;
 cu_avr_block_t *block = &cu_avr_blocks[pc & (CU_AVR_BLOCK_SLOTS - 1U)];
 auint i;
 auint x;
 auint cycles;
 auint arg1;
 auint arg2;
 auint opid;
 auint src;
 auint dst;
 auint res;
 auint tmp;
 auint vc;
 auint portc;
 auint code;
 auint next_pc;
 auint elapsed;

#ifdef CU_AVR_BLOCK_STATS
 cu_avr_block_attempts ++;
#endif
 if (block->epoch != cu_avr_block_epoch || block->pc != pc){
  block->pc = pc;
  block->epoch = cu_avr_block_epoch;
  block->len = 0U;
  block->cycles = 0U;
  for (i = 0U; i < CU_AVR_BLOCK_MAX; i++){
   code = cpu_code[(pc + i) & 0x7FFFU];
   cycles = cu_avr_block_cycles(code & 0x7FU);
   if (cycles == 0U){ break; }
   block->op[i] = code;
   block->op_cycles[i] = cycles;
   block->len ++;
   block->cycles += cycles;
   if (cu_avr_block_ends(code & 0x7FU)){ break; }
  }
 }
 if (block->len == 0U ||
     (block->len == 1U && !cu_avr_block_ends(block->op[0] & 0x7FU)) ||
     event_it || event_it_enter ||
     (WRAP32(cycle_next_event - cpu_state.cycle) <= block->cycles) ||
     ((video_cycle + block->cycles) > VIDEO_CY_MAX)){
  return FALSE;
 }

 vc = video_cycle;
 portc = cpu_state.iors[CU_IO_PORTC];
 next_pc = cpu_state.pc + block->len;
 elapsed = 0U;
 for (i = 0U; i < block->len; i++){
  code = block->op[i];
  opid = code & 0x7FU;
  arg1 = (code >> 8) & 0xFFU;
  arg2 = (code >> 16) & 0xFFFFU;
  cycles = block->op_cycles[i];
  switch (opid){
   case 0x00U: break; /* NOP */
   case 0x01U: /* MOVW */
    cpu_state.iors[arg1] = cpu_state.iors[arg2];
    cpu_state.iors[arg1 + 1U] = cpu_state.iors[arg2 + 1U];
    break;
   case 0x09U: /* ADD */
   case 0x0DU: /* ADC */
    src = cpu_state.iors[arg2];
    dst = cpu_state.iors[arg1];
    res = dst + src + ((opid == 0x0DU) ? SREG_GET_C(cpu_state.iors[CU_IO_SREG]) : 0U);
    cpu_state.iors[arg1] = res;
    cpu_state.iors[CU_IO_SREG] =
     (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM)) |
     cpu_pflags[CU_AVRFG_ADD + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)];
    break;
   case 0x0AU: /* CPSE */
    if (cpu_state.iors[arg1] != cpu_state.iors[arg2]){ cycles = 1U; }
    else{
     x = (cpu_code[(pc + i + 1U) & 0x7FFFU] >> 7) & 1U;
     next_pc = cpu_state.pc + i + 2U + x;
     cycles = 2U + x;
    }
    break;
   case 0x0CU: /* SUB */
   case 0x14U: /* SUBI */
    src = (opid == 0x14U) ? arg2 : cpu_state.iors[arg2];
    dst = cpu_state.iors[arg1];
    res = dst - src;
    cpu_state.iors[arg1] = res;
    cpu_state.iors[CU_IO_SREG] =
     (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM)) |
     cpu_pflags[CU_AVRFG_SUB + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)];
    break;
   case 0x0EU: case 0x0FU: case 0x10U: /* AND / EOR / OR */
   case 0x15U: case 0x16U: /* ORI / ANDI */
    src = (opid >= 0x15U) ? arg2 : cpu_state.iors[arg2];
    dst = cpu_state.iors[arg1];
    if (opid == 0x0EU || opid == 0x16U){ res = dst & src; }
    else if (opid == 0x0FU){ res = dst ^ src; }
    else{ res = dst | src; }
    cpu_state.iors[arg1] = res;
    cpu_state.iors[CU_IO_SREG] =
     (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
     cpu_pflags[CU_AVRFG_LOG + res];
    break;
   case 0x11U: cpu_state.iors[arg1] = cpu_state.iors[arg2]; break; /* MOV */
   case 0x12U: /* CPI */
    src = arg2;
    dst = cpu_state.iors[arg1];
    res = dst - src;
    cpu_state.iors[CU_IO_SREG] =
     (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM)) |
     cpu_pflags[CU_AVRFG_SUB + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)];
    break;
   case 0x18U: case 0x19U: /* LPM / LPM+ */
    tmp = ((auint)cpu_state.iors[30]) + (((auint)cpu_state.iors[31]) << 8);
    cpu_state.iors[arg1] = cpu_state.crom[tmp];
    if (opid == 0x19U){
     tmp ++;
     cpu_state.iors[30] = tmp & 0xFFU;
     cpu_state.iors[31] = (tmp >> 8) & 0xFFU;
    }
    break;
   case 0x26U: /* SWAP */
    res = cpu_state.iors[arg1];
    cpu_state.iors[arg1] = (res >> 4) | (res << 4);
    break;
   case 0x27U: case 0x2BU: /* INC / DEC */
    res = cpu_state.iors[arg1];
    if (opid == 0x27U){ res ++; }
    else{ res --; }
    cpu_state.iors[arg1] = res;
    cpu_state.iors[CU_IO_SREG] =
     (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
     cpu_pflags[(opid == 0x27U ? CU_AVRFG_INC : CU_AVRFG_DEC) + (res & 0xFFU)];
    break;
   case 0x2CU: /* JMP */
    next_pc = arg2;
    break;
   case 0x3DU: case 0x3FU: /* SBIC / SBIS */
    cpu_state.cycle = WRAP32(cpu_state.cycle + elapsed);
    elapsed = 0U;
    res = cu_avr_read_io(arg1) & arg2;
    if ((res != 0U) == (opid == 0x3DU)){
     cycles = 1U;
    }else{
     x = (cpu_code[(pc + i + 1U) & 0x7FFFU] >> 7) & 1U;
     next_pc = cpu_state.pc + i + 2U + x;
     cycles = 2U + x;
    }
    break;
   case 0x40U: /* RJMP */
    next_pc = cpu_state.pc + i + 1U + arg2;
    break;
   case 0x42U: case 0x43U: /* BRBS / BRBC */
    if (((cpu_state.iors[CU_IO_SREG] & arg1) != 0U) == (opid == 0x42U)){
     next_pc = cpu_state.pc + i + 1U + arg2;
    }else{ cycles = 1U; }
    break;
   case 0x46U: case 0x47U: /* SBRC / SBRS */
    if (((cpu_state.iors[arg1] & arg2) != 0U) == (opid == 0x46U)){
     cycles = 1U;
    }else{
     x = (cpu_code[(pc + i + 1U) & 0x7FFFU] >> 7) & 1U;
     next_pc = cpu_state.pc + i + 2U + x;
     cycles = 2U + x;
    }
    break;
   case 0x48U: cpu_state.iors[arg1] = arg2; break; /* LDI */
   case 0x49U: /* PIXEL */
    portc = cpu_state.iors[arg2] & cpu_state.iors[CU_IO_DDRC];
    cpu_state.iors[CU_IO_PORTC] = portc;
    break;
   default: return FALSE; /* Unreachable: builder excludes all other opcodes. */
  }
  for (x = 0U; x < cycles; x++){ video_row.pixels[vc ++] = portc; }
  elapsed += cycles;
 }
 cpu_state.pc = next_pc;
 cpu_state.cycle = WRAP32(cpu_state.cycle + elapsed);
 video_cycle = vc;
#ifdef CU_AVR_BLOCK_STATS
 cu_avr_block_hits ++;
 cu_avr_block_instructions += block->len;
#endif
 return TRUE;
}

#ifdef CU_AVR_BLOCK_STATS
void cu_avr_block_cache_stats(auint *attempts, auint *hits, auint *instructions)
{
 *attempts = cu_avr_block_attempts;
 *hits = cu_avr_block_hits;
 *instructions = cu_avr_block_instructions;
}
#endif
#endif



/*
** Auto-fuses the CPU based on ROM contents and boot priority. The bootpri
** flag requests prioritizing the bootloader when set TRUE, otherwise game is
** prioritized (used when both a game and a bootloader appears to be present
** in the ROM). This should be called before reset unless the CPU state is
** already loaded (which includes fuse bits).
*/
void  cu_avr_autofuse(boole bootpri)
{
 boole hasgame;
 boole hasboot;

 /* Default fuse settings are according to Uzebox */
 /* (For now, only the BOOTRST fuse is actually used) */

 cpu_state.fuse[0] = 0xD7U; /* CLKSEL = 0111; SUT = 01; CKOUT = 1; CKDIV = 1 */
 cpu_state.fuse[1] = 0xD2U; /* BOOTRST = 0; BOOTSZ = 01; EESAVE = 0; WDTON = 1; SPIEN = 0; JTAGEN = 1; OCDEN = 1 */
 cpu_state.fuse[2] = 0xFFU; /* BODLEVEL = 111 */

 /* Determine boot fuses according to the contents of the ROM and the boot
 ** prioritization parameter. The boot loader is used when it exists and it
 ** is prioritized or when it looks like there is no game in the Code ROM. */

 hasgame = (cpu_state.crom[                    0U] != 0U) ||
           (cpu_state.crom[                    1U] != 0U);
 hasboot = (cpu_state.crom[(VBASE_BOOT * 2U) + 0U] != 0U) ||
           (cpu_state.crom[(VBASE_BOOT * 2U) + 1U] != 0U);
 if ( (!(hasboot && bootpri)) &&
      (hasgame) ){ cpu_state.fuse[1] |= 0x01U; } /* Turn off bootloader */
}



/*
** Resets the CPU as if it was power-cycled. It properly initializes
** everything from the state as if cu_avr_crom_update() and cu_avr_io_update()
** was called.
*/
void  cu_avr_reset(void)
{
 auint i;

 for (i = 0U; i < 4096U; i++){
  access_mem[i] = 0U;
 }

 for (i = 0U; i < 256U; i++){
  access_io[i] = 0U;
 }

 for (i = 0U; i < 271U; i++){
  video_frame.pulse[i].rise = CU_NOSYNC;
  video_frame.pulse[i].fall = CU_NOSYNC;
 }

 for (i = 0U; i < 4U; i++){
  audio_samples[i] = 0x80U;
 }

 cpu_state.cycle    = 0U;
 video_pulsectr     = 0U;
 video_pedge        = cpu_state.cycle;
 video_prise        = cpu_state.cycle;
 video_rowflag      = FALSE;
 video_cycle        = 0U;
#if defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)
 memset(&cu_avr_loop_counters, 0, sizeof(cu_avr_loop_counters));
 cu_avr_loop_invalidate();
#endif
#ifdef CU_AVR_TRACE_REFERENCE
 cu_avr_trace_reset_stats();
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
 cu_avr_native_trace_invalidate();
 cu_avr_native_builds = 0U;
 cu_avr_native_entries = 0U;
 cu_avr_native_instructions = 0U;
 cu_avr_native_side_exits = 0U;
 cu_avr_native_compiled_builds = 0U;
 cu_avr_native_short_builds = 0U;
 cu_avr_native_codecap_stops = 0U;
#ifdef CU_AVR_NATIVE_TRACE_CHAIN
 cu_avr_native_chain_hits = 0U;
 cu_avr_native_chain_segments = 0U;
#endif
#ifdef CU_AVR_NATIVE_TRACE_LD
 cu_avr_native_ld_guard_pass = 0U;
 cu_avr_native_ld_guard_fail_io = 0U;
#endif
#ifdef CU_AVR_NATIVE_TRACE_VERIFY
 cu_avr_native_verify_checks = 0U;
 cu_avr_native_verify_mismatches = 0U;
 cu_avr_native_verify_first_pc = 0U;
 cu_avr_native_verify_first_index = 0U;
 cu_avr_native_verify_first_got = 0U;
 cu_avr_native_verify_first_expected = 0U;
 cu_avr_native_verify_disabled = FALSE;
#endif
#endif
#ifdef CU_AVR_BLOCK_STATS
 cu_avr_block_attempts = 0U;
 cu_avr_block_hits = 0U;
 cu_avr_block_instructions = 0U;
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
 cu_avr_thumb_blocks_run = 0U;
 cu_avr_thumb_instructions_run = 0U;
 cu_avr_thumb_cache_rebuilds = 0U;
#endif
 #ifdef CU_AVR_NATIVE_VERIFY
  cu_avr_thumb_checks = 0U;
  cu_avr_thumb_mismatches = 0U;
  cu_avr_thumb_first_pc = 0U;
  cu_avr_thumb_first_index = 0U;
  cu_avr_thumb_first_got = 0U;
  cu_avr_thumb_first_expected = 0U;
  cu_avr_thumb_event_rejects = 0U;
  cu_avr_thumb_row_rejects = 0U;
  cu_avr_thumb_disabled = FALSE;
 #endif
 audio_rp           = 0U;
 audio_wp           = 0U;
 cycle_next_event   = WRAP32(cpu_state.cycle + 1U);
 timer1_base        = cpu_state.cycle;
 event_it           = TRUE;
 event_it_enter     = FALSE;
 wd_last            = cpu_state.cycle;
 wd_last_pc         = 0U;

 wd_interval_min[0] = WD_INTERVAL_MAX;
 wd_interval_min[1] = 0U;
 wd_interval_beg[0] = 0U;
 wd_interval_beg[1] = 0U;
 wd_interval_end[0] = 0U;
 wd_interval_end[1] = 0U;

 if (!pflags_done){
  cu_avrfg_fill(&cpu_pflags[0]);
  pflags_done = TRUE;
 }

 cu_avr_crom_update(0U, 65536U);

 cu_avr_reset_wd();
 cu_ctr_reset();
 cu_spi_reset(cpu_state.cycle);

 cpu_state.crom_mod = FALSE; /* Initial code ROM state: not modified. */
}



/*
** Run emulation. Returns according to the return values defined in cu_types
** (emulating up to about 2050 cycles).
*/
__attribute__((noinline,noclone,no_icf,section(".text.uzebox_run_flash")))
auint cu_avr_run_flash(void)
{
 auint ret = 0U;
 auint i;
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
 boole native_probe = TRUE;
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
 boole trace_probe = TRUE;
#endif

#if (defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)) && \
    defined(CU_AVR_NATIVE_TRACE_CHAIN)
 cu_avr_native_trace_chain_break();
#endif
 video_rowflag = FALSE;

#ifdef CU_AVR_TIME_PROFILE
 if(cu_avr_time_select()){
  cu_avr_time_row();
 }else
#endif
 while (video_cycle < VIDEO_CY_MAX){ /* Also signals proper row end */
#ifdef CU_AVR_FUSED
  if(((cpu_state.pc&32767U)-cu_avr_fused_pc)<174U && CU_AVR_FUSED_ENABLED && cu_avr_fused_run())continue;
#endif
#ifdef CU_AVR_TRACE_REFERENCE
  cu_avr_trace_run();
  continue;
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  if (trace_probe && cu_avr_native_trace_run()){
   trace_probe = cu_avr_native_trace_continue_probe;
   continue;
  }
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
  /* Probe after control-flow changes, row entry, translated blocks, or an
   * opcode that this translator cannot include in a native block. */
  if (native_probe && cu_avr_thumb_run()){ continue; }
  native_probe = FALSE;
#endif
#ifdef CU_AVR_BLOCK_CACHE
  if (cu_avr_block_run()){ continue; }
#endif
#ifdef CU_AVR_LOOP_QUALIFY
  cu_avr_loopq_before();
#endif
#if defined(CU_AVR_OPCODE_PROFILE) || defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE) || defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  auint const old_pc = cpu_state.pc;
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  uint32 const old_op = cpu_code[old_pc & 0x7FFFU];
#endif
#if (defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)) && \
    defined(CU_AVR_NATIVE_TRACE_CHAIN)
  cu_avr_native_trace_chain_break();
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
  auint const executed_opid = cu_avr_exec();
#else
  cu_avr_exec();       /* Note: This inlines as only this single call exists */
#endif
#ifdef CU_AVR_LOOP_QUALIFY
  cu_avr_loopq_after();
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
  native_probe = cpu_state.pc != (old_pc + 1U) ||
                 !cu_avr_thumb_supported_id(executed_opid);
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  trace_probe = cpu_state.pc != (old_pc + 1U) ||
                !cu_avr_native_trace_supported(old_op) ||
                (old_op & 0x7FU) == 0x40U ||
                (old_op & 0x7FU) == 0x42U ||
                (old_op & 0x7FU) == 0x43U ||
                (old_op & 0x7FU) == 0x46U ||
                (old_op & 0x7FU) == 0x47U;
#endif
#ifdef CU_AVR_OPCODE_PROFILE
  cu_avr_profile_current_block ++;
  if (cpu_state.pc != (old_pc + 1U)){
   cu_avr_profile_blocks ++;
   if (cu_avr_profile_current_block >= 8U){
    cu_avr_profile_long_instructions += cu_avr_profile_current_block;
   }
   if (cu_avr_profile_current_block > cu_avr_profile_max_block){
    cu_avr_profile_max_block = cu_avr_profile_current_block;
   }
   cu_avr_profile_current_block = 0U;
  }
#endif
 }
#ifdef CU_AVR_LOOP_QUALIFY
 cu_avr_loopq_row_end();
#endif
 video_cycle -= VIDEO_CY_MAX; /* Next line pixels */

 if (video_rowflag){

  ret |= CU_GET_ROW;

  audio_rp = (audio_rp + 1U) & 0x3U;
  if (audio_rp == audio_wp){ audio_rp = (audio_rp - 1U) & 0x3U; }
  video_row.sample = audio_samples[audio_rp];

  video_row.pno = video_pulsectr;

  if (video_pulsectr == 270U){ /* Frame completed, can return it */

   ret |= CU_GET_FRAME;

   if (wd_interval_min[0] < WD_INTERVAL_MAX){ /* Latch WDR interval debug counter */
    wd_interval_min[1] = wd_interval_min[0];
    wd_interval_end[1] = wd_interval_end[0];
    wd_interval_beg[1] = wd_interval_beg[0];
   }
   wd_interval_min[0] = WD_INTERVAL_MAX; /* Clear for next frame */
   wd_interval_beg[0] = 0U;
   wd_interval_end[0] = 0U;

  }else if (video_pulsectr == 0U){

   for (i = 1U; i < 271U; i++){ /* Clear sync info for next frame (pulse 0 is already produced here) */
    video_frame.pulse[i].rise = CU_NOSYNC;
    video_frame.pulse[i].fall = CU_NOSYNC;
   }

  }

 }else{ /* (Note: No CU_BREAK support yet) */

  ret |= CU_GET_ROW;
  ret |= CU_SYNCERR;

 }

 return ret;
}

__attribute__((noinline,noclone,no_icf,section(".time_critical.uzebox_run_sram")))
auint cu_avr_run_sram(void)
{
 auint ret = 0U;
 auint i;
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
 boole native_probe = TRUE;
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
 boole trace_probe = TRUE;
#endif

#if (defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)) && \
    defined(CU_AVR_NATIVE_TRACE_CHAIN)
 cu_avr_native_trace_chain_break();
#endif
 video_rowflag = FALSE;

#ifdef CU_AVR_TIME_PROFILE
 if(cu_avr_time_select()){
  cu_avr_time_row();
 }else
#endif
 while (video_cycle < VIDEO_CY_MAX){ /* Also signals proper row end */
#ifdef CU_AVR_FUSED
  if(((cpu_state.pc&32767U)-cu_avr_fused_pc)<174U && CU_AVR_FUSED_ENABLED && cu_avr_fused_run())continue;
#endif
#ifdef CU_AVR_TRACE_REFERENCE
  cu_avr_trace_run();
  continue;
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  if (trace_probe && cu_avr_native_trace_run()){
   trace_probe = cu_avr_native_trace_continue_probe;
   continue;
  }
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
  /* Probe after control-flow changes, row entry, translated blocks, or an
   * opcode that this translator cannot include in a native block. */
  if (native_probe && cu_avr_thumb_run()){ continue; }
  native_probe = FALSE;
#endif
#ifdef CU_AVR_BLOCK_CACHE
  if (cu_avr_block_run()){ continue; }
#endif
#ifdef CU_AVR_LOOP_QUALIFY
  cu_avr_loopq_before();
#endif
#if defined(CU_AVR_OPCODE_PROFILE) || defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE) || defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  auint const old_pc = cpu_state.pc;
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  uint32 const old_op = cpu_code[old_pc & 0x7FFFU];
#endif
#if (defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)) && \
    defined(CU_AVR_NATIVE_TRACE_CHAIN)
  cu_avr_native_trace_chain_break();
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
  auint const executed_opid = cu_avr_exec();
#else
  cu_avr_exec();       /* Note: This inlines as only this single call exists */
#endif
#ifdef CU_AVR_LOOP_QUALIFY
  cu_avr_loopq_after();
#endif
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
  native_probe = cpu_state.pc != (old_pc + 1U) ||
                 !cu_avr_thumb_supported_id(executed_opid);
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
  trace_probe = cpu_state.pc != (old_pc + 1U) ||
                !cu_avr_native_trace_supported(old_op) ||
                (old_op & 0x7FU) == 0x40U ||
                (old_op & 0x7FU) == 0x42U ||
                (old_op & 0x7FU) == 0x43U ||
                (old_op & 0x7FU) == 0x46U ||
                (old_op & 0x7FU) == 0x47U;
#endif
#ifdef CU_AVR_OPCODE_PROFILE
  cu_avr_profile_current_block ++;
  if (cpu_state.pc != (old_pc + 1U)){
   cu_avr_profile_blocks ++;
   if (cu_avr_profile_current_block >= 8U){
    cu_avr_profile_long_instructions += cu_avr_profile_current_block;
   }
   if (cu_avr_profile_current_block > cu_avr_profile_max_block){
    cu_avr_profile_max_block = cu_avr_profile_current_block;
   }
   cu_avr_profile_current_block = 0U;
  }
#endif
 }
#ifdef CU_AVR_LOOP_QUALIFY
 cu_avr_loopq_row_end();
#endif
 video_cycle -= VIDEO_CY_MAX; /* Next line pixels */

 if (video_rowflag){

  ret |= CU_GET_ROW;

  audio_rp = (audio_rp + 1U) & 0x3U;
  if (audio_rp == audio_wp){ audio_rp = (audio_rp - 1U) & 0x3U; }
  video_row.sample = audio_samples[audio_rp];

  video_row.pno = video_pulsectr;

  if (video_pulsectr == 270U){ /* Frame completed, can return it */

   ret |= CU_GET_FRAME;

   if (wd_interval_min[0] < WD_INTERVAL_MAX){ /* Latch WDR interval debug counter */
    wd_interval_min[1] = wd_interval_min[0];
    wd_interval_end[1] = wd_interval_end[0];
    wd_interval_beg[1] = wd_interval_beg[0];
   }
   wd_interval_min[0] = WD_INTERVAL_MAX; /* Clear for next frame */
   wd_interval_beg[0] = 0U;
   wd_interval_end[0] = 0U;

  }else if (video_pulsectr == 0U){

   for (i = 1U; i < 271U; i++){ /* Clear sync info for next frame (pulse 0 is already produced here) */
    video_frame.pulse[i].rise = CU_NOSYNC;
    video_frame.pulse[i].fall = CU_NOSYNC;
   }

  }

 }else{ /* (Note: No CU_BREAK support yet) */

  ret |= CU_GET_ROW;
  ret |= CU_SYNCERR;

 }

 return ret;
}
/* Post-link data-only selection. Unbound images must not be deployed. */
const volatile uint32 uzebox_placement_control[2]
 __attribute__((section(".rodata.uzebox_placement"),used))={0x4b313056U,1U};
auint cu_avr_run(void)
{
 return uzebox_placement_control[1]==1U?cu_avr_run_sram():cu_avr_run_flash();
}




/*
** Returns emulator's cycle counter. It may be used to time emulation when it
** doesn't generate proper video signal. This is the cycle member of the CPU
** state (32 bits wrapping).
*/
auint cu_avr_getcycle(void)
{
 return cpu_state.cycle;
}



/*
** Return current row. Note that continuing emulation will modify the returned
** structure's contents.
*/
cu_row_t const* cu_avr_get_row(void)
{
 return &video_row;
}



/*
** Return frame info. Note that continuing emulation will modify the returned
** structure's contents.
*/
cu_frameinfo_t const* cu_avr_get_frameinfo(void)
{
 return &video_frame;
}


/*
** Returns memory access info block. It can be written (with zeros) to clear
** flags which are only set by the emulator. Note that the highest 256 bytes
** of the RAM come first here! (so address 0x0100 corresponds to AVR address
** 0x0100)
*/
uint8* cu_avr_get_meminfo(void)
{
 return &access_mem[0];
}


/*
** Returns I/O register access info block. It can be written (with zeros) to
** clear flags which are only set by the emulator.
*/
uint8* cu_avr_get_ioinfo(void)
{
 return &access_io[0];
}


/*
** Returns whether the EEPROM changed since the last clear of this indicator.
** Calling cu_avr_io_update() clears this indicator (as well as resetting by
** cu_avr_reset()). Passing TRUE also clears it. This can be used to save
** EEPROM state to persistent storage when it changes.
*/
boole cu_avr_eeprom_ischanged(boole clear)
{
 boole ret = eeprom_changed;
 if (clear){ eeprom_changed = FALSE; }
 return ret;
}


/*
** Returns whether the Code ROM changed since the last clear of this
** indicator. Calling cu_avr_io_update() clears this indicator (as well as
** resetting by cu_avr_reset()). Passing TRUE also clears it. This can be used
** to save Code ROM state to persistent storage when it changes.
*/
boole cu_avr_crom_ischanged(boole clear)
{
 boole ret = crom_changed;
 if (clear){ crom_changed = FALSE; }
 return ret;
}



/*
** Returns whether the Code ROM was modified since reset. This can be used to
** determine if it is necessary to include the Code ROM in a save state.
** Internal Code ROM writes and the cu_avr_crom_update() function can set it.
*/
boole cu_avr_crom_ismod(void)
{
 return cpu_state.crom_mod;
}



/*
** Returns AVR CPU state structure. It may be written, the Code ROM must be
** recompiled (by cu_avr_crom_update()) if anything in that area was updated
** or freshly written.
*/
cu_state_cpu_t* cu_avr_get_state(void)
{
 auint t0 = WRAP32(cpu_state.cycle - timer1_base); /* Current TCNT1 value */

 cpu_state.iors[CU_IO_TCNT1H] = (t0 >> 8) & 0xFFU;
 cpu_state.iors[CU_IO_TCNT1L] = (t0     ) & 0xFFU;

 return &cpu_state;
}



/*
** Updates a section of the Code ROM. This must be called after writing into
** the Code ROM so the emulator recompiles the affected instructions. The
** "base" and "len" parameters specify the range to update in bytes.
*/
void  cu_avr_crom_update(auint base, auint len)
{
#ifdef CU_AVR_BLOCK_CACHE
 cu_avr_block_epoch ++;
 if (cu_avr_block_epoch == 0U){
  auint slot;
  for (slot = 0U; slot < CU_AVR_BLOCK_SLOTS; slot++){
   cu_avr_blocks[slot].epoch = 0U;
  }
  cu_avr_block_epoch = 1U;
 }
#endif
 auint wbase = base >> 1;
 auint wlen  = (len + (base & 1U) + 1U) >> 1;
 auint i;

 if (wbase > 0x7FFFU){ wbase = 0x7FFFU; }
 if ((wbase + wlen) > 0x8000U){ wlen = 0x8000U - wbase; }

 for (i = wbase; i < (wbase + wlen); i++){
  cpu_code[i] = cu_avrc_compile(
      ((auint)(cpu_state.crom[((i << 1) + 0U) & 0xFFFFU])     ) |
      ((auint)(cpu_state.crom[((i << 1) + 1U) & 0xFFFFU]) << 8),
      ((auint)(cpu_state.crom[((i << 1) + 2U) & 0xFFFFU])     ) |
      ((auint)(cpu_state.crom[((i << 1) + 3U) & 0xFFFFU]) << 8) );
 }
#if defined(CU_AVR_NATIVE_THUMB) || defined(CU_AVR_NATIVE_REFERENCE)
 cu_avr_thumb_rebuild();
#endif
#ifdef CU_AVR_TRACE_REFERENCE
 cu_avr_trace_invalidate();
#endif
#if defined(CU_AVR_NATIVE_TRACE) || defined(CU_AVR_NATIVE_TRACE_REFERENCE)
 cu_avr_native_trace_invalidate();
#endif
#ifdef CU_AVR_LOOP_QUALIFY
 cu_avr_loopq_invalidate();
#endif
#if defined(CU_AVR_LOOP_NATIVE) || defined(CU_AVR_LOOP_REFERENCE)
 cu_avr_loop_invalidate();
#endif

 cpu_state.crom_mod = TRUE;
 crom_changed = TRUE;
}



/*
** Updates the I/O area. If any change is performed in the I/O register
** contents (iors, 0x20 - 0xFF), this have to be called to update internal
** emulator state over it. It also updates state related to additional
** variables in the structure (such as the watchdog timer).
*/
void  cu_avr_io_update(void)
{
 auint t0;

 eeprom_changed = FALSE;
 crom_changed   = FALSE;

 t0    = (cpu_state.iors[CU_IO_TCNT1H] << 8) |
         (cpu_state.iors[CU_IO_TCNT1L]     );
 timer1_base = WRAP32(cpu_state.cycle - t0);

 cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
 event_it         = TRUE; /* Request interrupt processing */
}



/*
** Returns last measured interval between WDR calls. Returns begin and end
** (word) addresses of WDR instructions into beg and end.
*/
auint cu_avr_get_lastwdrinterval(auint* beg, auint* end)
{
 *beg = wd_interval_beg[1];
 *end = wd_interval_end[1];
 return wd_interval_min[1];
}

/* GPL-3.0-or-later. Boot-only independent original bitmap reference.
 * Restore SRAM/marks/full row; no guest instruction or peripheral advances. */
static uint32 uz_phase4_hash(void const *data,auint count){
 uint8 const *p=data;uint32 h=2166136261U;
 while(count--)h=(h^*p++)*16777619U;
 return h;
}
boole __attribute__((noinline,section(".text.uzebox_phase4_selftest"))) uzebox_phase4_selftest(void){
 static uint8 const counts[]={0U,1U,2U,8U,11U},ddrs[]={0U,0x5AU,0xFFU};
 if(uz_phase4_status)return uz_phase4_status==1U;
 uz_phase4_status=2U;
 if(cu_avr_loop_disabled)return FALSE;
 cu_row_t const saved_row=video_row;
 cu_avr_bitmap_ctx_t c,e;
 uint8 saved_ram[12U],saved_marks[12U],native_marks[12U];
 auint const saved_calls=uz_phase4_calls;auint i,n,tail,d;boole ok=TRUE;
 for(i=0U;i<12U;i++){saved_ram[i]=cpu_state.sram[512U+i];saved_marks[i]=access_mem[512U+i];cpu_state.sram[512U+i]=(uint8)(0xA5U+17U*i);}
 uz_phase4_calls=0U;uz_phase4_testing=1U;
 for(n=0U;n<5U;n++)for(tail=0U;tail<2U;tail++)for(d=0U;d<3U;d++){
  memset(&c,0,sizeof(c));c.pixels=video_row.pixels;c.sram=cpu_state.sram;c.marks=access_mem;
  c.x=512U;c.n=counts[n];c.port=0xA3U;c.pixel=0x37U;c.bits=0xA5U;c.pos=7U;c.cursor=11U;
  c.dead=0x12U;c.living=0xE7U;c.cursor_color=0xF0U;c.grid=0x81U;c.ddrc=ddrs[d];c.tail_taken=tail;c.phase=4U;
  e=c;video_row=saved_row;
  for(i=0U;i<12U;i++)access_mem[512U+i]=saved_marks[i];
  cu_avr_bitmap_execute(&c);
  uint32 const hash=uz_phase4_hash(&video_row,sizeof(video_row));
  for(i=0U;i<12U;i++)native_marks[i]=access_mem[512U+i];
  video_row=saved_row;
  for(i=0U;i<12U;i++)access_mem[512U+i]=saved_marks[i];
  cu_avr_bitmap_reference(&e);
  ok=ok && memcmp(&c,&e,sizeof(c))==0 && hash==uz_phase4_hash(&video_row,sizeof(video_row));
  for(i=0U;i<12U;i++)ok=ok && native_marks[i]==access_mem[512U+i];
 }
 ok=ok && uz_phase4_calls==30U;
 for(i=0U;i<12U;i++){cpu_state.sram[512U+i]=saved_ram[i];access_mem[512U+i]=saved_marks[i];}
 video_row=saved_row;uz_phase4_calls=saved_calls;uz_phase4_testing=0U;
 if(ok)uz_phase4_status=1U;
 return ok;
}
void uzebox_phase4_stats(uzebox_phase4_stats_t *out){
 out->mode=uzebox_phase4_control[1];out->selftest=uz_phase4_status;out->calls=uz_phase4_calls;
}

/* GPL-3.0-or-later. Independent byte-loop oracle, including guard bytes. */
boole __attribute__((noinline,section(".text.uzebox_fill_selftest"))) uzebox_fill_selftest(void){
 static uint16 const lengths[]={0,1,2,3,4,5,7,8,11,12,31,32,63,64,127,128,255,256,381,384,511};
 static uint16 const colors[]={0,1,0x55,0xAA,0xFF};
 if(uz_fill_status)return uz_fill_status==1U;
 uz_fill_status=2U;if(cu_avr_loop_disabled)return FALSE;
 uint8 actual[544U],expected[544U];
 auint const old_calls=uz_fill_calls,old_bytes=uz_fill_bytes;auint n,a,c,i;boole ok=TRUE;
 for(n=0U;n<21U;n++)for(a=0U;a<4U;a++)for(c=0U;c<5U;c++){
  memset(actual,0xD3U,sizeof(actual));memset(expected,0xD3U,sizeof(expected));
  for(i=0U;i<lengths[n];i++)expected[16U+a+i]=(uint8)colors[c];
  uz_fill_constant(actual+16U+a,colors[c],lengths[n]);
  ok=ok && memcmp(actual,expected,sizeof(actual))==0;
 }
 uz_fill_calls=old_calls;uz_fill_bytes=old_bytes;
 if(ok)uz_fill_status=1U;
 return ok;
}
void uzebox_fill_stats(uzebox_fill_stats_t *out){
 out->mode=uzebox_fill_control[1];out->selftest=uz_fill_status;out->calls=uz_fill_calls;out->bytes=uz_fill_bytes;
}

#include "uzebox_mode1_selftest.inc"

#include "uzebox_poll_selftest.inc"

#include "uzebox_y_selftest.inc"
