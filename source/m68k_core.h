#ifndef SHARP68000_M68K_CORE_H
#define SHARP68000_M68K_CORE_H

#include <stdint.h>

typedef uint8_t (*m68k_read8_fn)(void *context, uint32_t address);
typedef uint16_t (*m68k_read16_fn)(void *context, uint32_t address);
typedef void (*m68k_write8_fn)(void *context, uint32_t address, uint8_t value);
typedef void (*m68k_write16_fn)(void *context, uint32_t address, uint16_t value);

typedef struct {
    void *context;
    m68k_read8_fn read8;
    m68k_read16_fn read16;
    m68k_write8_fn write8;
    m68k_write16_fn write16;
} m68k_bus_t;

typedef struct {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
    uint64_t cycles;
    uint8_t stopped;
    uint8_t fault;
    m68k_bus_t bus;
} m68k_cpu_t;

void m68k_init(m68k_cpu_t *cpu, const m68k_bus_t *bus);
int m68k_reset(m68k_cpu_t *cpu);
unsigned m68k_step(m68k_cpu_t *cpu);

#endif
