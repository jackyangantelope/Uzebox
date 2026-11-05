/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef JACK_YELLOW_NESPAD_INPUT_H
#define JACK_YELLOW_NESPAD_INPUT_H
#include <stdbool.h>
#include "jack_input_devices.h"
#ifdef __cplusplus
extern "C" {
#endif
bool jack_nespad_input_init(jack_input_devices *devices,uint32_t cpu_khz);
bool jack_nespad_input_poll(void);

bool jack_nespad_input_service(uint32_t now_us);
typedef struct {
    uint32_t starts,completed,max_latency_us;
    bool pending;
} jack_nespad_input_stats;
void jack_nespad_input_get_stats(jack_nespad_input_stats *stats);
#ifdef __cplusplus
}
#endif
#endif
