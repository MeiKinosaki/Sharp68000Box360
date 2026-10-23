#include "x68000_bus.h"
#include <string.h>

static uint8_t bus_read8(void *context, uint32_t address)
{
    x68000_bus_state_t *s = (x68000_bus_state_t *)context;
    address &= X68000_ADDRESS_MASK;
    if (address < X68000_RAM_SIZE) return s->ram[address];
    if (address >= X68000_ROM_BASE && address < X68000_ROM_BASE + X68000_ROM_SIZE && s->rom_loaded)
        return s->rom[address - X68000_ROM_BASE];
    s->unmapped_reads++;
    return 0xFFu;
}

static uint16_t bus_read16(void *context, uint32_t address)
{
    uint16_t hi = bus_read8(context, address);
    uint16_t lo = bus_read8(context, (address + 1u) & X68000_ADDRESS_MASK);
    return (uint16_t)((hi << 8) | lo);
}

static void bus_write8(void *context, uint32_t address, uint8_t value)
{
    x68000_bus_state_t *s = (x68000_bus_state_t *)context;
    address &= X68000_ADDRESS_MASK;
    if (address < X68000_RAM_SIZE) {
        s->ram[address] = value;
        return;
    }
    s->ignored_writes++;
}

static void bus_write16(void *context, uint32_t address, uint16_t value)
{
    bus_write8(context, address, (uint8_t)(value >> 8));
    bus_write8(context, (address + 1u) & X68000_ADDRESS_MASK, (uint8_t)value);
}

void x68000_bus_init(x68000_bus_state_t *state)
{
    if (state == 0) return;
    memset(state, 0, sizeof(*state));
}

m68k_bus_t x68000_bus_interface(x68000_bus_state_t *state)
{
    m68k_bus_t bus;
    bus.context = state;
    bus.read8 = bus_read8;
    bus.read16 = bus_read16;
    bus.write8 = bus_write8;
    bus.write16 = bus_write16;
    return bus;
}

int x68000_bus_load_rom(x68000_bus_state_t *state, const uint8_t *data, uint32_t size)
{
    if (state == 0 || data == 0 || size == 0u || size > X68000_ROM_SIZE) return -1;
    memset(state->rom, 0xFF, sizeof(state->rom));
    memcpy(state->rom, data, size);
    state->rom_loaded = 1u;
    return 0;
}
