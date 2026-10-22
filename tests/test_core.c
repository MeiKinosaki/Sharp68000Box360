#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../source/m68k_core.h"
#include "../source/x68000_bus.h"
#include "../source/x68000_scheduler.h"
#include "../source/x68000_state.h"
#include "../source/x68000_library.h"

static void test_bus(void)
{
    x68000_bus_state_t *state = (x68000_bus_state_t *)calloc(1, sizeof(*state));
    m68k_bus_t io;
    uint8_t rom[4] = {0x12, 0x34, 0x56, 0x78};
    assert(state != NULL);
    x68000_bus_init(0); /* null-safe initialization */
    x68000_bus_init(state);
    io = x68000_bus_interface(state);
    io.write16(io.context, 0x100u, 0xABCDu);
    assert(io.read16(io.context, 0x100u) == 0xABCDu);
    assert(x68000_bus_load_rom(state, rom, sizeof(rom)) == 0);
    assert(io.read16(io.context, X68000_ROM_BASE) == 0x1234u);
    io.write8(io.context, X68000_ROM_BASE, 0u);
    assert(io.read8(io.context, X68000_ROM_BASE) == 0x12u);
    assert(state->ignored_writes == 1u); /* ROM remains read-only */

    io.write8(io.context, 0x01000100u, 0x5Au); /* 24-bit address bus alias */
    assert(io.read8(io.context, 0x000100u) == 0x5Au);
    assert(x68000_bus_load_rom(state, rom, X68000_ROM_SIZE + 1u) == -1);
    assert(io.read16(io.context, X68000_ROM_BASE) == 0x1234u); /* failed load preserves prior ROM */
    (void)io.read8(io.context, 0x200000u);
    assert(state->unmapped_reads == 1u);
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
    io.write16(io.context, 0x114u, 0x1602u); /* MOVE.B D2,D3 */
    io.write16(io.context, 0x116u, 0x3802u); /* MOVE.W D2,D4 */
    io.write16(io.context, 0x118u, 0x2A02u); /* MOVE.L D2,D5 */
    io.write16(io.context, 0x11Au, 0x3242u); /* MOVEA.W D2,A1 */
    io.write16(io.context, 0x11Cu, 0x2042u); /* MOVEA.L D2,A0 */
    io.write16(io.context, 0x11Eu, 0x60FEu); /* BRA to itself */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    assert(cpu.a[7] == 0x800u && cpu.pc == 0x100u);
    assert(m68k_step(&cpu) == 4u && cpu.d[0] == UINT32_MAX);
    assert((cpu.sr & 0x0008u) != 0u);
    assert(m68k_step(&cpu) == 4u && cpu.pc == 0x104u);
    assert(m68k_step(&cpu) == 4u && (cpu.d[0] & 0xFFFFu) == 0u);
    assert(m68k_step(&cpu) == 4u && (cpu.d[0] & 0xFFFFu) == 0u);
    assert((cpu.sr & 0x0004u) != 0u);
    assert(m68k_step(&cpu) == 4u && (cpu.sr & 0x0004u) != 0u);
    assert(m68k_step(&cpu) == 8u && (cpu.d[1] & 0xFFFFu) == 0x1234u);
    assert(m68k_step(&cpu) == 12u && cpu.d[2] == 0x89ABCDEFu);
    cpu.d[3] = 0xAABBCCDDu;
    cpu.d[4] = 0x11223344u;
    assert(m68k_step(&cpu) == 4u && cpu.d[3] == 0xAABBCCEFu);
    assert(m68k_step(&cpu) == 4u && cpu.d[4] == 0x1122CDEFu);
    assert(m68k_step(&cpu) == 4u && cpu.d[5] == 0x89ABCDEFu);
    assert(m68k_step(&cpu) == 4u && cpu.a[1] == 0xFFFFCDEFu);
    assert(m68k_step(&cpu) == 4u && cpu.a[0] == 0x89ABCDEFu);
    assert(m68k_step(&cpu) == 10u && cpu.pc == 0x11Eu);
    assert(cpu.fault == 0u);
    free(state);
}

static void test_quick_arithmetic_flags(void)
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
    io.write16(io.context, 0x100u, 0x5200u); /* ADDQ.B #1,D0 */
    io.write16(io.context, 0x102u, 0x5200u);
    io.write16(io.context, 0x104u, 0x5301u); /* SUBQ.B #1,D1 */
    io.write16(io.context, 0x106u, 0x5301u);
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);

    cpu.d[0] = 0x7Fu;
    assert(m68k_step(&cpu) == 4u && (cpu.d[0] & 0xFFu) == 0x80u);
    assert((cpu.sr & 0x0008u) != 0u); /* N */
    assert((cpu.sr & 0x0002u) != 0u); /* V */
    assert((cpu.sr & 0x0011u) == 0u); /* no C or X */

    cpu.d[0] = 0xFFu;
    assert(m68k_step(&cpu) == 4u && (cpu.d[0] & 0xFFu) == 0u);
    assert((cpu.sr & 0x0015u) == 0x0015u); /* X, Z, C */

    cpu.d[1] = 0u;
    assert(m68k_step(&cpu) == 4u && (cpu.d[1] & 0xFFu) == 0xFFu);
    assert((cpu.sr & 0x0019u) == 0x0019u); /* X, N, C */

    cpu.d[1] = 1u;
    assert(m68k_step(&cpu) == 4u && (cpu.d[1] & 0xFFu) == 0u);
    assert((cpu.sr & 0x0015u) == 0x0004u); /* Z; no X or C */
    assert(cpu.fault == 0u);
    free(state);
}

static void test_quick_address_registers(void)
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
    io.write16(io.context, 0x100u, 0x5249u); /* ADDQ.W #1,A1 */
    io.write16(io.context, 0x102u, 0x558Au); /* SUBQ.L #2,A2 */
    io.write16(io.context, 0x104u, 0x504Bu); /* ADDQ.W #8,A3 */
    io.write16(io.context, 0x106u, 0x514Cu); /* SUBQ.W #8,A4 */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    cpu.sr = 0x2015u;
    cpu.a[1] = 0x0000FFFFu;
    cpu.a[2] = 0x00001000u;
    cpu.a[3] = 0x00000010u;
    cpu.a[4] = 0x00010000u;
    assert(m68k_step(&cpu) == 8u && cpu.a[1] == 0x00010000u);
    assert(cpu.sr == 0x2015u);
    assert(m68k_step(&cpu) == 8u && cpu.a[2] == 0x00000FFEu);
    assert(cpu.sr == 0x2015u);
    assert(m68k_step(&cpu) == 8u && cpu.a[3] == 0x00000018u);
    assert(cpu.sr == 0x2015u);
    assert(m68k_step(&cpu) == 8u && cpu.a[4] == 0x0000FFF8u);
    assert(cpu.sr == 0x2015u && cpu.fault == 0u);
    free(state);
}

static void test_illegal_opcode_fault(void)
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
    io.write16(io.context, 0x100u, 0xFFFFu); /* unsupported opcode in this slice */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    assert(m68k_step(&cpu) == 0u);
    assert(cpu.fault == 1u && cpu.pc == 0x102u && cpu.cycles == 0u);
    assert(m68k_step(&cpu) == 0u && cpu.pc == 0x102u);
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

static void test_move_memory_modes(void)
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
    io.write16(io.context, 0x100u, 0x1C12u); /* MOVE.B (A2),D6 */
    io.write16(io.context, 0x102u, 0x3E1Au); /* MOVE.W (A2)+,D7 */
    io.write16(io.context, 0x104u, 0x2A22u); /* MOVE.L -(A2),D5 */
    io.write16(io.context, 0x300u, 0x7F12u);
    io.write16(io.context, 0x304u, 0xABCDu);
    io.write16(io.context, 0x306u, 0xCAFEu);
    io.write16(io.context, 0x308u, 0xEF01u);
    io.write16(io.context, 0x30Au, 0x2345u);
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    cpu.a[2] = 0x300u;
    assert(m68k_step(&cpu) == 8u);
    assert((cpu.d[6] & 0xFFu) == 0x7Fu && cpu.a[2] == 0x300u);
    cpu.a[2] = 0x304u;
    assert(m68k_step(&cpu) == 8u);
    assert((cpu.d[7] & 0xFFFFu) == 0xABCDu && cpu.a[2] == 0x306u);
    cpu.a[2] = 0x30Cu;
    assert(m68k_step(&cpu) == 12u);
    assert(cpu.d[5] == 0xEF012345u && cpu.a[2] == 0x308u);
    assert(cpu.fault == 0u);
    free(state);
}

static void test_move_to_memory(void)
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
    io.write16(io.context, 0x100u, 0x3682u); /* MOVE.W D2,(A3) */
    io.write16(io.context, 0x102u, 0x26C2u); /* MOVE.L D2,(A3)+ */
    io.write16(io.context, 0x104u, 0x3702u); /* MOVE.W D2,-(A3) */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    cpu.d[2] = 0x89ABCDEFu;
    cpu.a[3] = 0x400u;
    assert(m68k_step(&cpu) == 8u);
    assert(io.read16(io.context, 0x400u) == 0xCDEFu && cpu.a[3] == 0x400u);
    assert(m68k_step(&cpu) == 12u);
    assert(io.read16(io.context, 0x400u) == 0x89ABu);
    assert(io.read16(io.context, 0x402u) == 0xCDEFu && cpu.a[3] == 0x404u);
    cpu.a[3] = 0x500u;
    assert(m68k_step(&cpu) == 8u);
    assert(io.read16(io.context, 0x4FEu) == 0xCDEFu && cpu.a[3] == 0x4FEu);
    assert(cpu.fault == 0u);
    free(state);
}

static void test_move_absolute_modes(void)
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

    io.write16(io.context, 0x100u, 0x3038u); /* MOVE.W ($0500).W,D0 */
    io.write16(io.context, 0x102u, 0x0500u);
    io.write16(io.context, 0x104u, 0x2239u); /* MOVE.L ($00000520).L,D1 */
    io.write16(io.context, 0x106u, 0x0000u);
    io.write16(io.context, 0x108u, 0x0520u);
    io.write16(io.context, 0x10Au, 0x31C2u); /* MOVE.W D2,($0502).W */
    io.write16(io.context, 0x10Cu, 0x0502u);
    io.write16(io.context, 0x10Eu, 0x23C2u); /* MOVE.L D2,($00000524).L */
    io.write16(io.context, 0x110u, 0x0000u);
    io.write16(io.context, 0x112u, 0x0524u);

    io.write16(io.context, 0x500u, 0x1234u);
    io.write16(io.context, 0x520u, 0x89ABu);
    io.write16(io.context, 0x522u, 0xCDEFu);
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    assert(m68k_step(&cpu) == 12u && (cpu.d[0] & 0xFFFFu) == 0x1234u);
    assert(m68k_step(&cpu) == 20u && cpu.d[1] == 0x89ABCDEFu);
    cpu.d[2] = 0x76543210u;
    assert(m68k_step(&cpu) == 12u && io.read16(io.context, 0x502u) == 0x3210u);
    assert(m68k_step(&cpu) == 20u);
    assert(io.read16(io.context, 0x524u) == 0x7654u);
    assert(io.read16(io.context, 0x526u) == 0x3210u);
    assert(cpu.fault == 0u);
    free(state);
}

static void test_lea_absolute_modes(void)
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
    io.write16(io.context, 0x100u, 0x43F8u); /* LEA ($0500).W,A1 */
    io.write16(io.context, 0x102u, 0x0500u);
    io.write16(io.context, 0x104u, 0x45F9u); /* LEA ($00000600).L,A2 */
    io.write16(io.context, 0x106u, 0x0000u);
    io.write16(io.context, 0x108u, 0x0600u);
    io.write16(io.context, 0x10Au, 0x47FAu); /* LEA d16(PC),A3 */
    io.write16(io.context, 0x10Cu, 0x0010u); /* PC base 0x10C -> 0x11C */
    io.write16(io.context, 0x10Eu, 0x49D2u); /* LEA (A2),A4 */
    io.write16(io.context, 0x110u, 0x4BE9u); /* LEA d16(A1),A5 */
    io.write16(io.context, 0x112u, 0xFFF0u); /* A1 - 16 */
    io.write16(io.context, 0x114u, 0x4DF0u); /* LEA d8(A0,D1.W),A6 */
    io.write16(io.context, 0x116u, 0x1004u); /* D1.W + 4 */
    io.write16(io.context, 0x118u, 0x4FFBu); /* LEA d8(PC,D3.L),A7 */
    io.write16(io.context, 0x11Au, 0x38FCu); /* PC base 0x11C + D3 + (-4) */
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    cpu.sr = 0x2015u;
    assert(m68k_step(&cpu) == 8u);
    assert(cpu.a[1] == 0x500u && cpu.sr == 0x2015u);
    assert(m68k_step(&cpu) == 12u);
    assert(cpu.a[2] == 0x600u && cpu.sr == 0x2015u);
    assert(m68k_step(&cpu) == 8u);
    assert(cpu.a[3] == 0x11Cu && cpu.sr == 0x2015u);
    cpu.a[2] = 0x12345678u;
    cpu.a[1] = 0x00000100u;
    assert(m68k_step(&cpu) == 4u && cpu.a[4] == 0x345678u);
    assert(m68k_step(&cpu) == 8u && cpu.a[5] == 0xF0u);
    cpu.a[0] = 0x00000200u;
    cpu.d[1] = 0x00000020u;
    assert(m68k_step(&cpu) == 10u && cpu.a[6] == 0x224u);
    cpu.d[3] = 0x00000030u;
    assert(m68k_step(&cpu) == 10u && cpu.a[7] == 0x146u);
    assert(cpu.sr == 0x2015u && cpu.fault == 0u);

    io.write16(io.context, 0x11Cu, 0x4DF0u); /* LEA indexed, reserved extension bits */
    io.write16(io.context, 0x11Eu, 0x1104u); /* reserved bit 8 */
    assert(m68k_step(&cpu) == 0u && cpu.fault == 1u);
    cpu.fault = 0u;
    cpu.pc = 0x11Cu;
    io.write16(io.context, 0x11Eu, 0x1204u); /* reserved bit 9 / scale field */
    assert(m68k_step(&cpu) == 0u && cpu.fault == 1u);
    free(state);
}

static void test_scheduler(void)
{
    x68000_scheduler_t s;
    x68000_scheduler_init(&s, 10000000u, 50u);
    assert(x68000_scheduler_begin_frame(&s) == 200000u);
    x68000_scheduler_account(&s, 20u);
    assert(s.frame_number == 1u && s.cpu_cycles == 20u && s.master_ticks == 20u);

    x68000_scheduler_init(&s, 10u, 3u);
    assert(x68000_scheduler_begin_frame(&s) == 3u);
    assert(x68000_scheduler_begin_frame(&s) == 3u);
    assert(x68000_scheduler_begin_frame(&s) == 4u);
    assert(x68000_scheduler_begin_frame(&s) == 3u);

    x68000_scheduler_init(&s, 2u, 5u);
    assert(x68000_scheduler_begin_frame(&s) == 0u);
    assert(x68000_scheduler_begin_frame(&s) == 0u);
    assert(x68000_scheduler_begin_frame(&s) == 1u);
    assert(x68000_scheduler_begin_frame(&s) == 0u);
    assert(x68000_scheduler_begin_frame(&s) == 1u);
}


static void test_save_load_state(void)
{
    x68000_bus_state_t *bus = (x68000_bus_state_t *)calloc(1, sizeof(*bus));
    m68k_cpu_t cpu, restored;
    x68000_scheduler_t sched, restored_sched;
    m68k_bus_t io;
    uint8_t *blob;
    size_t size = x68000_state_size();
    assert(bus != NULL);
    blob = (uint8_t *)malloc(size);
    assert(blob != NULL);
    x68000_bus_init(bus);
    io = x68000_bus_interface(bus);
    io.write16(io.context, 0u, 0u);
    io.write16(io.context, 2u, 0x0800u);
    io.write16(io.context, 4u, 0u);
    io.write16(io.context, 6u, 0x0100u);
    m68k_init(&cpu, &io);
    assert(m68k_reset(&cpu) == 0);
    cpu.d[0] = 0x12345678u;
    cpu.a[3] = 0x00000400u;
    cpu.pc = 0x00001234u;
    cpu.cycles = 9876u;
    cpu.sr = 0x2015u;
    bus->ram[0x400u] = 0xA5u;
    x68000_scheduler_init(&sched, 10000000u, 50u);
    (void)x68000_scheduler_begin_frame(&sched);
    x68000_scheduler_account(&sched, 44u);
    assert(x68000_state_save(&cpu, bus, &sched, blob, size) == (int)size);

    cpu.d[0] = 0u; cpu.a[3] = 0u; cpu.pc = 0u; cpu.cycles = 0u; cpu.sr = 0u;
    bus->ram[0x400u] = 0u;
    memset(&sched, 0, sizeof(sched));
    m68k_init(&restored, &io);
    x68000_scheduler_init(&restored_sched, 1u, 1u);
    assert(x68000_state_load(&restored, bus, &restored_sched, blob, size) == 0);
    assert(restored.d[0] == 0x12345678u && restored.a[3] == 0x400u);
    assert(restored.pc == 0x1234u && restored.cycles == 9876u && restored.sr == 0x2015u);
    assert(bus->ram[0x400u] == 0xA5u);
    assert(restored_sched.cpu_cycles == 44u && restored_sched.frame_number == 1u);
    assert(restored.bus.context == bus && restored.bus.read16 != NULL);

    /* Corrupt data must be rejected before any machine state is mutated. */
    blob[size - 1u] ^= 0x80u;
    restored.d[0] = 0xCAFEBABEu;
    assert(x68000_state_load(&restored, bus, &restored_sched, blob, size) == -3);
    assert(restored.d[0] == 0xCAFEBABEu);
    assert(x68000_state_load(&restored, bus, &restored_sched, blob, size - 1u) == -1);
    free(blob);
    free(bus);
}


static void test_game_library_favorites(void)
{
    x68000_library_t *lib = (x68000_library_t *)calloc(1, sizeof(*lib));
    const x68000_game_entry_t *entry;
    assert(lib != NULL);
    x68000_library_init(lib);
    assert(x68000_library_add(lib, "Test Game", "Hdd1:/ROMs/test.xdf", "") == 0);
    assert(x68000_library_add(lib, "Test Game", "Hdd1:/ROMs/test.xdf", "") == -2);
    assert(x68000_library_add(lib, "Too Long", "", "") == -3);
    assert(lib->count == 1u && x68000_library_favorite_count(lib) == 0u);
    assert(x68000_library_toggle_favorite(lib, "Hdd1:/ROMs/test.xdf") == 1);
    assert(x68000_library_favorite_count(lib) == 1u);
    entry = x68000_library_find(lib, "Hdd1:/ROMs/test.xdf");
    assert(entry != NULL && entry->favorite == 1u);
    assert(strcmp(entry->title, "Test Game") == 0 && entry->cover_path[0] == '\\0');
    assert(x68000_library_set_favorite(lib, "Hdd1:/ROMs/test.xdf", 0) == 0);
    assert(x68000_library_favorite_count(lib) == 0u);
    assert(x68000_library_remove(lib, "Hdd1:/ROMs/test.xdf") == 0);
    assert(lib->count == 0u && x68000_library_find(lib, "Hdd1:/ROMs/test.xdf") == NULL);
    assert(x68000_library_remove(lib, "missing") == -1);
    free(lib);
}

int main(void)
{
    test_bus();
    test_cpu_slice();
    test_quick_arithmetic_flags();
    test_quick_address_registers();
    test_illegal_opcode_fault();
    test_bsr_rts();
    test_move_memory_modes();
    test_move_to_memory();
    test_move_absolute_modes();
    test_lea_absolute_modes();
    test_scheduler();
    test_save_load_state();
    test_game_library_favorites();
    puts("Sharp68000Box360 host tests: PASS");
    return 0;
}
