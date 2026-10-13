#include "m68k_core.h"
#include <string.h>

#define M68K_SR_SUPERVISOR 0x2000u
#define M68K_SR_ZERO       0x0004u
#define M68K_SR_NEGATIVE   0x0008u
#define M68K_SR_CARRY      0x0001u

static uint16_t fetch16(m68k_cpu_t *cpu)
{
    uint16_t value = cpu->bus.read16(cpu->bus.context, cpu->pc & 0x00FFFFFFu);
    cpu->pc = (cpu->pc + 2u) & 0x00FFFFFFu;
    return value;
}

static uint32_t fetch32(m68k_cpu_t *cpu)
{
    uint32_t high = fetch16(cpu);
    return (high << 16) | fetch16(cpu);
}

static void set_nz(m68k_cpu_t *cpu, uint32_t value)
{
    cpu->sr &= (uint16_t)~(M68K_SR_ZERO | M68K_SR_NEGATIVE);
    if (value == 0u) cpu->sr |= M68K_SR_ZERO;
    if ((value & 0x80000000u) != 0u) cpu->sr |= M68K_SR_NEGATIVE;
}

void m68k_init(m68k_cpu_t *cpu, const m68k_bus_t *bus)
{
    memset(cpu, 0, sizeof(*cpu));
    if (bus != 0) cpu->bus = *bus;
}

int m68k_reset(m68k_cpu_t *cpu)
{
    if (cpu == 0 || cpu->bus.read16 == 0) return -1;
    cpu->sr = M68K_SR_SUPERVISOR | 0x0700u;
    cpu->a[7] = ((uint32_t)cpu->bus.read16(cpu->bus.context, 0u) << 16) |
                cpu->bus.read16(cpu->bus.context, 2u);
    cpu->pc = (((uint32_t)cpu->bus.read16(cpu->bus.context, 4u) << 16) |
               cpu->bus.read16(cpu->bus.context, 6u)) & 0x00FFFFFFu;
    cpu->stopped = 0u;
    cpu->fault = 0u;
    cpu->cycles = 0u;
    return 0;
}

/*
 * Deliberately small first interpreter slice: NOP, RTS, MOVEQ, BRA/Bcc,
 * absolute JMP and STOP. Unimplemented opcodes latch a fault instead of
 * silently pretending to execute a complete 68000 instruction set.
 */
unsigned m68k_step(m68k_cpu_t *cpu)
{
    uint16_t op;
    unsigned used = 4u;
    uint32_t target;
    int32_t displacement;
    unsigned condition;

    if (cpu == 0 || cpu->fault || cpu->stopped || cpu->bus.read16 == 0) return 0u;
    op = fetch16(cpu);

    if (op == 0x4E71u) { /* NOP */
        used = 4u;
    } else if (op == 0x4E75u) { /* RTS */
        target = ((uint32_t)cpu->bus.read16(cpu->bus.context, cpu->a[7] & 0x00FFFFFFu) << 16) |
                 cpu->bus.read16(cpu->bus.context, (cpu->a[7] + 2u) & 0x00FFFFFFu);
        cpu->a[7] += 4u;
        cpu->pc = target & 0x00FFFFFFu;
        used = 16u;
    } else if ((op & 0xF100u) == 0x7000u) { /* MOVEQ #imm8,Dn */
        unsigned reg = (op >> 9) & 7u;
        cpu->d[reg] = (uint32_t)(int32_t)(int8_t)(op & 0xFFu);
        set_nz(cpu, cpu->d[reg]);
        cpu->sr &= (uint16_t)~M68K_SR_CARRY;
        used = 4u;
    } else if ((op & 0xF000u) == 0x6000u) { /* BRA/Bcc, byte or word displacement */
        displacement = (int8_t)(op & 0xFFu);
        if ((op & 0xFFu) == 0u) displacement = (int16_t)fetch16(cpu);
        condition = (op >> 8) & 0x0Fu;
        {
            unsigned z = (cpu->sr & M68K_SR_ZERO) != 0u;
            unsigned n = (cpu->sr & M68K_SR_NEGATIVE) != 0u;
            unsigned c = (cpu->sr & M68K_SR_CARRY) != 0u;
            unsigned v = 0u; /* overflow flag is not implemented in this slice */
            int take = condition == 0u || condition == 1u ||
                (condition == 2u && !c && !z) || (condition == 3u && (c || z)) ||
                (condition == 4u && !c) || (condition == 5u && c) ||
                (condition == 6u && !z) || (condition == 7u && z) ||
                (condition == 8u && !v) || (condition == 9u && v) ||
                (condition == 10u && !n) || (condition == 11u && n) ||
                (condition == 12u && n == v) || (condition == 13u && n != v) ||
                (condition == 14u && !z && n == v) || (condition == 15u && (z || n != v));
            if (take) cpu->pc = (cpu->pc + (uint32_t)displacement) & 0x00FFFFFFu;
        }
        used = 10u;
    } else if (op == 0x4EF9u) { /* JMP (xxx).L */
        cpu->pc = fetch32(cpu) & 0x00FFFFFFu;
        used = 10u;
    } else if (op == 0x4E72u) { /* STOP #imm */
        cpu->sr = fetch16(cpu);
        cpu->stopped = 1u;
        used = 4u;
    } else {
        cpu->fault = 1u;
        used = 0u;
    }

    cpu->cycles += used;
    return used;
}
