#ifndef SHARP68000_SCHEDULER_H
#define SHARP68000_SCHEDULER_H

#include <stdint.h>

typedef struct {
    uint64_t master_ticks;
    uint64_t cpu_cycles;
    uint32_t frame_number;
    uint32_t cpu_hz;
    uint32_t frame_hz;
    uint32_t frame_cycle_budget;
    uint32_t frame_cycle_remainder;
    uint64_t frame_remainder_accumulator;
} x68000_scheduler_t;

void x68000_scheduler_init(x68000_scheduler_t *scheduler, uint32_t cpu_hz, uint32_t frame_hz);
uint32_t x68000_scheduler_begin_frame(x68000_scheduler_t *scheduler);
void x68000_scheduler_account(x68000_scheduler_t *scheduler, uint32_t cpu_cycles);

#endif
