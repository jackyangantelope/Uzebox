/* GPL-3.0-or-later. Reuse the original static mixer graph up to its original
 * OCR2A checkpoint. UART/other suffixes continue in the original ISA. */
#include "uzebox_y.h"
const volatile uint32_t uzebox_y_control[2]
 __attribute__((section(".rodata.uzebox_y"),used))={0x59323356U,0xffffffffU};
uint32_t uzebox_y_ready;
static auint cu_y_testing;
static volatile auint cu_y_prefix_calls;
#define CU_Y_ENABLED (cu_y_testing || (uzebox_y_ready==1U && uzebox_y_control[1]==1U))
void uzebox_y_fail(void){uzebox_y_ready=2U;}
