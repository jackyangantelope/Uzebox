#ifndef UZEBOX_POLL_H
#define UZEBOX_POLL_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t mode,selftest,calls,iterations;} uzebox_poll_stats_t;
extern const volatile uint32_t uzebox_poll_control[2];
bool uzebox_poll_selftest(void);
void uzebox_poll_stats(uzebox_poll_stats_t *out);
#endif
