#ifndef SHARP68000_X68000_BUS_H
#define SHARP68000_X68000_BUS_H

#include <stdint.h>
#include "m68k_core.h"

#define X68000_ADDRESS_MASK 0x00FFFFFFu
#define X68000_RAM_BASE     0x000000u
#define X68000_RAM_SIZE     (1024u * 1024u)
#define X68000_ROM_BASE     0xFC0000u
#define X68000_ROM_SIZE     (256u * 1024u)

typedef struct {
    uint8_t ram[X68000_RAM_SIZE];
    uint8_t rom[X68000_ROM_SIZE];
    uint8_t rom_loaded;
    uint32_t unmapped_reads;
    uint32_t ignored_writes;
} x68000_bus_state_t;

void x68000_bus_init(x68000_bus_state_t *state);
m68k_bus_t x68000_bus_interface(x68000_bus_state_t *state);
int x68000_bus_load_rom(x68000_bus_state_t *state, const uint8_t *data, uint32_t size);

#endif
