#include "x68000_scheduler.h"
#include <string.h>

void x68000_scheduler_init(x68000_scheduler_t *s, uint32_t cpu_hz, uint32_t frame_hz)
{
    if (s == 0) return;
    memset(s, 0, sizeof(*s));
    s->cpu_hz = cpu_hz;
    s->frame_hz = frame_hz;
    s->frame_cycle_budget = (frame_hz == 0u) ? 0u : cpu_hz / frame_hz;
}

uint32_t x68000_scheduler_begin_frame(x68000_scheduler_t *s)
{
    if (s == 0) return 0u;
    s->frame_number++;
    return s->frame_cycle_budget;
}

void x68000_scheduler_account(x68000_scheduler_t *s, uint32_t cycles)
{
    if (s == 0) return;
    s->cpu_cycles += cycles;
    s->master_ticks += cycles;
}
