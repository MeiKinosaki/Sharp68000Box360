#include "m68k_core.h"
#include <string.h>

#define M68K_SR_SUPERVISOR 0x2000u
#define M68K_SR_ZERO       0x0004u
#define M68K_SR_NEGATIVE   0x0008u
#define M68K_SR_OVERFLOW   0x0002u
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
    if (cpu == 0) return;
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
 * First interpreter slice only: NOP, RTS, MOVEQ, BRA/Bcc, absolute JMP
 * and STOP. Other opcodes latch a fault; this is not a complete 68000.
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

    if (op == 0x4E71u) {
        used = 4u; /* NOP */
    } else if (op == 0x4E75u) {
        if (cpu->bus.read16 == 0) { cpu->fault = 1u; return 0u; }
        target = ((uint32_t)cpu->bus.read16(cpu->bus.context, cpu->a[7] & 0x00FFFFFFu) << 16) |
                 cpu->bus.read16(cpu->bus.context, (cpu->a[7] + 2u) & 0x00FFFFFFu);
        cpu->a[7] += 4u;
        cpu->pc = target & 0x00FFFFFFu;
        used = 16u; /* RTS */
    } else if ((op & 0xF100u) == 0x7000u) {
        unsigned reg = (op >> 9) & 7u;
        cpu->d[reg] = (uint32_t)(int32_t)(int8_t)(op & 0xFFu);
        set_nz(cpu, cpu->d[reg]);
        cpu->sr &= (uint16_t)~(M68K_SR_CARRY | M68K_SR_OVERFLOW);
        used = 4u; /* MOVEQ */
    } else if ((op & 0xF000u) == 0x6000u) {
        unsigned z, n, c, v;
        int take;
        displacement = (int8_t)(op & 0xFFu);
        if ((op & 0xFFu) == 0u) displacement = (int16_t)fetch16(cpu);
        condition = (op >> 8) & 0x0Fu;
        z = (cpu->sr & M68K_SR_ZERO) != 0u;
        n = (cpu->sr & M68K_SR_NEGATIVE) != 0u;
        c = (cpu->sr & M68K_SR_CARRY) != 0u;
        v = (cpu->sr & M68K_SR_OVERFLOW) != 0u;
        switch (condition) {
            case 0u: take = 1; break; /* BRA */
            case 1u: take = 1; break; /* BSR; push is handled below */
            case 2u: take = !c && !z; break;   /* BHI */
            case 3u: take = c || z; break;     /* BLS */
            case 4u: take = !c; break;         /* BCC */
            case 5u: take = c; break;          /* BCS */
            case 6u: take = !z; break;         /* BNE */
            case 7u: take = z; break;          /* BEQ */
            case 8u: take = !v; break;         /* BVC */
            case 9u: take = v; break;          /* BVS */
            case 10u: take = !n; break;        /* BPL */
            case 11u: take = n; break;         /* BMI */
            case 12u: take = n == v; break;    /* BGE */
            case 13u: take = n != v; break;    /* BLT */
            case 14u: take = !z && n == v; break; /* BGT */
            default: take = z || n != v; break;   /* BLE */
        }
        if (take && condition == 1u) {
            uint32_t return_pc = cpu->pc & 0x00FFFFFFu;
            if (cpu->bus.write16 == 0) {
                cpu->fault = 1u;
                return 0u;
            }
            cpu->a[7] = (cpu->a[7] - 4u) & 0x00FFFFFFu;
            cpu->bus.write16(cpu->bus.context, cpu->a[7], (uint16_t)(return_pc >> 16));
            cpu->bus.write16(cpu->bus.context, (cpu->a[7] + 2u) & 0x00FFFFFFu, (uint16_t)return_pc);
        }
        if (take) cpu->pc = (cpu->pc + (uint32_t)displacement) & 0x00FFFFFFu;
        used = 10u;
    } else if (op == 0x4EF9u) {
        cpu->pc = fetch32(cpu) & 0x00FFFFFFu; /* JMP (xxx).L */
        used = 10u;
    } else if (op == 0x4E72u) {
        cpu->sr = fetch16(cpu);
        cpu->stopped = 1u; /* STOP #imm */
        used = 4u;
    } else {
        cpu->fault = 1u;
        used = 0u;
    }

    cpu->cycles += used;
    return used;
}
