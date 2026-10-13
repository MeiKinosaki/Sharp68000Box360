#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../source/m68k_core.h"
#include "../source/x68000_bus.h"
#include "../source/x68000_scheduler.h"

static void test_bus(void)
{
    x68000_bus_state_t *state = (x68000_bus_state_t *)calloc(1, sizeof(*state));
    m68k_bus_t io;
    uint8_t rom[4] = {0x12, 0x34, 0x56, 0x78};
    assert(state != NULL);
    x68000_bus_init(state);
    io = x68000_bus_interface(state);
    io.write16(io.context, 0x100u, 0xABCDu);
    assert(io.read16(io.context, 0x100u) == 0xABCDu);
    assert(x68000_bus_load_rom(state, rom, sizeof(rom)) == 0);
    assert(io.read16(io.context, X68000_ROM_BASE) == 0x1234u);
    io.write8(io.context, X68000_ROM_BASE, 0u);
    assert(io.read8(io.context, X68000_ROM_BASE) == 0x12u);
    free(state);
}

static void test_cpu_slice(void)
{
    x68000_bus_state_t *state = (x68000_bus_state_t *)calloc(1, sizeof(*state));
    m68k_bus_t io;
    m68k_cpu_t cpu;
    assert(state != NULL);
    x68000_bus_init(state);
    io = x68000_bus_interface(state);
    io.write16(io.context, 0u, 0x0000u);
    io.write16(io.context, 2u, 0x0800u);
    io.write16(io.context, 4u, 0x0000u);
    io.write16(io.context, 6u, 0x0100u);
    io.write16(io.context, 0x100u, 0x70FFu); /* MOVEQ #-1,D0 */
    io.write16(io.context, 0x102u, 0x4E71u); /* NOP */
    io.write16(io.context, 0x104u, 0x5240u); /* ADDQ.W #1,D0 */
    io.write16(io.context, 0x106u, 0x4240u); /* CLR.W D0 */
    io.write16(io.context, 0x108u, 0x4A40u); /* TST.W D0 */
    io.write16(io.context, 0x10Au, 0x323Cu); /* MOVE.W #$1234,D1 */
    io.write16(io.context, 0x10Cu, 0x1234u);
    io.write16(io.context, 0x10Eu, 0x243Cu); /* MOVE.L #$89ABCDEF,D2 */
    io.write16(io.context, 0x110u, 0x89ABu);
    io.write16(io.context, 0x112u, 0xCDEFu);
    io.write16(io.context, 0x114u, 0x60FEu); /* BRA to itself */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    assert(cpu.a[7] == 0x800u && cpu.pc == 0x100u);
    assert(m68k_step(&cpu) == 4u && cpu.d[0] == UINT32_MAX);
    assert((cpu.sr & 0x0008u) != 0u);
    assert(m68k_step(&cpu) == 4u && cpu.pc == 0x104u);
    assert(m68k_step(&cpu) == 8u && (cpu.d[0] & 0xFFFFu) == 0u);
    assert(m68k_step(&cpu) == 4u && (cpu.d[0] & 0xFFFFu) == 0u);
    assert((cpu.sr & 0x0004u) != 0u);
    assert(m68k_step(&cpu) == 4u && (cpu.sr & 0x0004u) != 0u);
    assert(m68k_step(&cpu) == 8u && (cpu.d[1] & 0xFFFFu) == 0x1234u);
    assert(m68k_step(&cpu) == 12u && cpu.d[2] == 0x89ABCDEFu);
    assert(m68k_step(&cpu) == 10u && cpu.pc == 0x114u);
    assert(cpu.fault == 0u);
    free(state);
}

static void test_bsr_rts(void)
{
    x68000_bus_state_t *state = (x68000_bus_state_t *)calloc(1, sizeof(*state));
    m68k_bus_t io;
    m68k_cpu_t cpu;
    assert(state != NULL);
    x68000_bus_init(state);
    io = x68000_bus_interface(state);
    io.write16(io.context, 0u, 0u);
    io.write16(io.context, 2u, 0x0800u);
    io.write16(io.context, 4u, 0u);
    io.write16(io.context, 6u, 0x0100u);
    io.write16(io.context, 0x100u, 0x6102u); /* BSR +2 */
    io.write16(io.context, 0x102u, 0x4E71u); /* return address */
    io.write16(io.context, 0x104u, 0x4E75u); /* RTS */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    assert(m68k_step(&cpu) == 10u);
    assert(cpu.pc == 0x104u && cpu.a[7] == 0x7FCu);
    assert(io.read16(io.context, 0x7FCu) == 0u);
    assert(io.read16(io.context, 0x7FEu) == 0x102u);
    assert(m68k_step(&cpu) == 16u);
    assert(cpu.pc == 0x102u && cpu.a[7] == 0x800u);
    free(state);
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
    test_bsr_rts();
    test_scheduler();
    puts("Sharp68000Box360 host tests: PASS");
    return 0;
}
