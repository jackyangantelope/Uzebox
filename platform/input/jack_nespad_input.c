/* SPDX-License-Identifier: BSD-3-Clause */

#include "jack_nespad_input.h"
#include <string.h>
bool jack_nespad_input_init(jack_input_devices*d,uint32_t khz){(void)d;(void)khz;return false;}
bool jack_nespad_input_poll(void){return false;}
bool jack_nespad_input_service(uint32_t now){(void)now;return false;}
void jack_nespad_input_get_stats(jack_nespad_input_stats*s){memset(s,0,sizeof *s);}
