#ifndef UZEBOX_Y_H
#define UZEBOX_Y_H
#include <stdbool.h>
#include <stdint.h>
extern const volatile uint32_t uzebox_y_control[2];
extern uint32_t uzebox_y_ready;
typedef struct {uint32_t mode,selftest,prefix,base,calls;} uzebox_y_stats_t;
static inline bool uzebox_y_active(void){return uzebox_y_ready==1u&&uzebox_y_control[1]==1u;}
bool uzebox_y_selftest(void);
void uzebox_y_fail(void);
void uzebox_y_stats(uzebox_y_stats_t *out);
#endif
