#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../source/m68k_core.h"
#include "../source/x68000_bus.h"
#include "../source/x68000_scheduler.h"

static void put16(x68000_bus_state_t *bus, uint32_t address, uint16_t value)
{
    m68k_bus_t io = x68000_bus_interface(bus);
    io.write16(io.context, address, value);
}

static void test_bus(void)
{
    x68000_bus_state_t *bus = (x68000_bus_state_t *)calloc(1, sizeof(*bus));
    m68k_bus_t io;
    uint8_t rom[4] = {0x12, 0x34, 0x56, 0x78};
    assert(bus != NULL);
    x68000_bus_init(bus);
    io = x68000_bus_interface(bus);
    io.write16(io.context, 0x100u, 0xABCDu);
    assert(io.read16(io.context, 0x100u) == 0xABCDu);
    assert(x68000_bus_load_rom(bus, rom, sizeof(rom)) == 0);
    assert(io.read16(io.context, X68000_ROM_BASE) == 0x1234u);
    io.write8(io.context, X68000_ROM_BASE, 0u);
    assert(io.read8(io.context, X68000_ROM_BASE) == 0x12u);
    free(bus);
}

static void test_cpu_slice(void)
{
    x68000_bus_state_t *bus = (x68000_bus_state_t *)calloc(1, sizeof(*bus));
    m68k_bus_t io;
    m68k_cpu_t cpu;
    assert(bus != NULL);
    x68000_bus_init(bus);
    io = x68000_bus_interface(bus);
    io.write16(io.context, 0u, 0x0000u);
    io.write16(io.context, 2u, 0x0800u);
    io.write16(io.context, 4u, 0x0000u);
    io.write16(io.context, 6u, 0x0100u);
    io.write16(io.context, 0x100u, 0x70FFu); /* MOVEQ #-1,D0 */
    io.write16(io.context, 0x102u, 0x4E71u); /* NOP */
    io.write16(io.context, 0x104u, 0x60FEu); /* BRA to itself */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    assert(cpu.a[7] == 0x800u && cpu.pc == 0x100u);
    assert(m68k_step(&cpu) == 4u && cpu.d[0] == UINT32_MAX);
    assert((cpu.sr & 0x0008u) != 0u);
    assert(m68k_step(&cpu) == 4u && cpu.pc == 0x104u);
    assert(m68k_step(&cpu) == 10u && cpu.pc == 0x104u);
    assert(cpu.fault == 0u);
    free(bus);
}

static void test_scheduler(void)
{
    x68000_scheduler_t s;
    x68000_scheduler_init(&s, 10000000u, 50u);
    assert(x68000_scheduler_begin_frame(&s) == 200000u);
    x68000_scheduler_account(&s, 20u);
    assert(s.frame_number == 1u && s.cpu_cycles == 20u && s.master_ticks == 20u);
}

int main(void)
{
    test_bus();
    test_cpu_slice();
    test_scheduler();
    puts("Sharp68000Box360 host tests: PASS");
    return 0;
}
