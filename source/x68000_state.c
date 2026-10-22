#include "x68000_state.h"
#include <string.h>

#define STATE_MAGIC 0x53363853u /* "S68S" */
#define CPU_BYTES 80u
#define SCHEDULER_BYTES 44u
#define BUS_META_BYTES 9u
#define STATE_PAYLOAD_SIZE (CPU_BYTES + SCHEDULER_BYTES + BUS_META_BYTES + X68000_STATE_RAM_SIZE)
#define STATE_TOTAL_SIZE (X68000_STATE_HEADER_SIZE + STATE_PAYLOAD_SIZE)

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static void put64(uint8_t *p, uint64_t v)
{
    put32(p, (uint32_t)(v >> 32)); put32(p + 4, (uint32_t)v);
}
static uint64_t get64(const uint8_t *p)
{
    return ((uint64_t)get32(p) << 32) | get32(p + 4);
}
static uint32_t checksum(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned bit;
        crc ^= p[i];
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}

size_t x68000_state_size(void) { return (size_t)STATE_TOTAL_SIZE; }

int x68000_state_save(const m68k_cpu_t *cpu, const x68000_bus_state_t *bus,
                      const x68000_scheduler_t *s, uint8_t *out, size_t cap)
{
    size_t at = X68000_STATE_HEADER_SIZE;
    unsigned i;
    if (!cpu || !bus || !s || !out || cap < STATE_TOTAL_SIZE) return -1;
    put32(out, STATE_MAGIC);
    put32(out + 4, X68000_STATE_VERSION);
    put32(out + 8, (uint32_t)STATE_TOTAL_SIZE);
    put32(out + 12, 0u);
    put32(out + 16, 0u);
    for (i = 0; i < 8u; ++i, at += 4u) put32(out + at, cpu->d[i]);
    for (i = 0; i < 8u; ++i, at += 4u) put32(out + at, cpu->a[i]);
    put32(out + at, cpu->pc); at += 4u;
    out[at++] = (uint8_t)(cpu->sr >> 8); out[at++] = (uint8_t)cpu->sr;
    put64(out + at, cpu->cycles); at += 8u;
    out[at++] = cpu->stopped; out[at++] = cpu->fault;
    put64(out + at, s->master_ticks); at += 8u;
    put64(out + at, s->cpu_cycles); at += 8u;
    put32(out + at, s->frame_number); at += 4u;
    put32(out + at, s->cpu_hz); at += 4u;
    put32(out + at, s->frame_hz); at += 4u;
    put32(out + at, s->frame_cycle_budget); at += 4u;
    put32(out + at, s->frame_cycle_remainder); at += 4u;
    put64(out + at, s->frame_remainder_accumulator); at += 8u;
    out[at++] = bus->rom_loaded;
    put32(out + at, bus->unmapped_reads); at += 4u;
    put32(out + at, bus->ignored_writes); at += 4u;
    memcpy(out + at, bus->ram, X68000_STATE_RAM_SIZE);
    put32(out + 12, checksum(out + X68000_STATE_HEADER_SIZE, STATE_PAYLOAD_SIZE));
    return (int)STATE_TOTAL_SIZE;
}

int x68000_state_load(m68k_cpu_t *cpu, x68000_bus_state_t *bus,
                      x68000_scheduler_t *s, const uint8_t *in, size_t length)
{
    m68k_cpu_t next_cpu;
    x68000_scheduler_t next_scheduler;
    size_t at = X68000_STATE_HEADER_SIZE;
    unsigned i;
    uint8_t rom_loaded;
    uint32_t unmapped_reads, ignored_writes;
    if (!cpu || !bus || !s || !in || length != STATE_TOTAL_SIZE) return -1;
    if (get32(in) != STATE_MAGIC || get32(in + 4) != X68000_STATE_VERSION ||
        get32(in + 8) != STATE_TOTAL_SIZE || get32(in + 16) != 0u) return -2;
    if (checksum(in + X68000_STATE_HEADER_SIZE, STATE_PAYLOAD_SIZE) != get32(in + 12)) return -3;
    rom_loaded = in[at + CPU_BYTES + SCHEDULER_BYTES];
    if (rom_loaded > 1u || rom_loaded != bus->rom_loaded) return -4;

    next_cpu = *cpu;
    next_scheduler = *s;
    for (i = 0; i < 8u; ++i, at += 4u) next_cpu.d[i] = get32(in + at);
    for (i = 0; i < 8u; ++i, at += 4u) next_cpu.a[i] = get32(in + at);
    next_cpu.pc = get32(in + at); at += 4u;
    next_cpu.sr = (uint16_t)(((uint16_t)in[at] << 8) | in[at + 1u]); at += 2u;
    next_cpu.cycles = get64(in + at); at += 8u;
    next_cpu.stopped = in[at++]; next_cpu.fault = in[at++];
    if (next_cpu.stopped > 1u || next_cpu.fault > 1u) return -5;
    next_scheduler.master_ticks = get64(in + at); at += 8u;
    next_scheduler.cpu_cycles = get64(in + at); at += 8u;
    next_scheduler.frame_number = get32(in + at); at += 4u;
    next_scheduler.cpu_hz = get32(in + at); at += 4u;
    next_scheduler.frame_hz = get32(in + at); at += 4u;
    next_scheduler.frame_cycle_budget = get32(in + at); at += 4u;
    next_scheduler.frame_cycle_remainder = get32(in + at); at += 4u;
    next_scheduler.frame_remainder_accumulator = get64(in + at); at += 8u;
    at++; /* rom_loaded was validated above */
    unmapped_reads = get32(in + at); at += 4u;
    ignored_writes = get32(in + at); at += 4u;

    /* All validation precedes mutation, so rejected files leave the machine intact. */
    memcpy(bus->ram, in + at, X68000_STATE_RAM_SIZE);
    bus->unmapped_reads = unmapped_reads;
    bus->ignored_writes = ignored_writes;
    *cpu = next_cpu;
    cpu->bus = (m68k_bus_t){ bus, cpu->bus.read8, cpu->bus.read16, cpu->bus.write8, cpu->bus.write16 };
    *s = next_scheduler;
    return 0;
}
