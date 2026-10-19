#include "m68k_core.h"
#include <string.h>

#define M68K_SR_EXTEND    0x0010u
#define M68K_SR_SUPERVISOR 0x2000u
#define M68K_SR_ZERO      0x0004u
#define M68K_SR_NEGATIVE  0x0008u
#define M68K_SR_OVERFLOW  0x0002u
#define M68K_SR_CARRY     0x0001u
#define M68K_SR_NZVC      (M68K_SR_NEGATIVE | M68K_SR_ZERO | M68K_SR_OVERFLOW | M68K_SR_CARRY)

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

static uint32_t size_mask(unsigned size)
{
    return size == 0u ? 0xFFu : size == 1u ? 0xFFFFu : 0xFFFFFFFFu;
}

static uint32_t size_sign(unsigned size)
{
    return size == 0u ? 0x80u : size == 1u ? 0x8000u : 0x80000000u;
}

static void set_nz_size(m68k_cpu_t *cpu, uint32_t value, unsigned size)
{
    uint32_t mask = size_mask(size);
    uint32_t sign = size_sign(size);
    value &= mask;
    cpu->sr &= (uint16_t)~(M68K_SR_NEGATIVE | M68K_SR_ZERO);
    if (value == 0u) cpu->sr |= M68K_SR_ZERO;
    if ((value & sign) != 0u) cpu->sr |= M68K_SR_NEGATIVE;
}

static void set_add_flags(m68k_cpu_t *cpu, uint32_t lhs, uint32_t rhs,
                          uint32_t result, unsigned size)
{
    uint32_t mask = size_mask(size);
    uint32_t sign = size_sign(size);
    uint64_t wide = (uint64_t)(lhs & mask) + (uint64_t)(rhs & mask);
    uint32_t out = result & mask;
    cpu->sr &= (uint16_t)~(M68K_SR_NZVC | M68K_SR_EXTEND);
    if (out == 0u) cpu->sr |= M68K_SR_ZERO;
    if ((out & sign) != 0u) cpu->sr |= M68K_SR_NEGATIVE;
    if (((~(lhs ^ rhs) & (lhs ^ out)) & sign) != 0u) cpu->sr |= M68K_SR_OVERFLOW;
    if ((wide & ((uint64_t)mask + 1u)) != 0u)
        cpu->sr |= (M68K_SR_CARRY | M68K_SR_EXTEND);
}

static void set_sub_flags(m68k_cpu_t *cpu, uint32_t lhs, uint32_t rhs,
                          uint32_t result, unsigned size)
{
    uint32_t mask = size_mask(size);
    uint32_t sign = size_sign(size);
    uint32_t a = lhs & mask, b = rhs & mask, out = result & mask;
    cpu->sr &= (uint16_t)~(M68K_SR_NZVC | M68K_SR_EXTEND);
    if (out == 0u) cpu->sr |= M68K_SR_ZERO;
    if ((out & sign) != 0u) cpu->sr |= M68K_SR_NEGATIVE;
    if ((((a ^ b) & (a ^ out)) & sign) != 0u) cpu->sr |= M68K_SR_OVERFLOW;
    if (b > a) cpu->sr |= (M68K_SR_CARRY | M68K_SR_EXTEND);
}

static void set_logic_flags(m68k_cpu_t *cpu, uint32_t value, unsigned size)
{
    cpu->sr &= (uint16_t)~M68K_SR_NZVC;
    set_nz_size(cpu, value, size);
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
 * Experimental interpreter slice. Supported instructions:
 * NOP, RTS, MOVEQ, MOVE data-register and selected memory forms,
 * MOVEA.W/L Dn,An, LEA (An), d16(An), absolute short/long and PC-relative displacement, BRA/Bcc, BSR,
 * absolute JMP, STOP, ADDQ/SUBQ on Dn, CLR Dn and TST Dn.
 * Unsupported opcodes fault.
 * This is intentionally not presented as a complete 68000 core.
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
        target = ((uint32_t)cpu->bus.read16(cpu->bus.context, cpu->a[7] & 0x00FFFFFFu) << 16) |
                 cpu->bus.read16(cpu->bus.context, (cpu->a[7] + 2u) & 0x00FFFFFFu);
        cpu->a[7] += 4u;
        cpu->pc = target & 0x00FFFFFFu;
        used = 16u; /* RTS */
    } else if ((op & 0xF100u) == 0x7000u) {
        unsigned reg = (op >> 9) & 7u;
        cpu->d[reg] = (uint32_t)(int32_t)(int8_t)(op & 0xFFu);
        set_logic_flags(cpu, cpu->d[reg], 2u);
        cpu->sr &= (uint16_t)~(M68K_SR_CARRY | M68K_SR_OVERFLOW);
        used = 4u; /* MOVEQ */
    } else if (((op >> 6) & 7u) == 0u &&
               ((((op >> 3) & 7u) >= 2u && ((op >> 3) & 7u) <= 4u) ||
                (((op >> 3) & 7u) == 7u && (op & 7u) <= 1u)) &&
               ((op >> 12) == 1u || (op >> 12) == 2u || (op >> 12) == 3u)) {
        unsigned top = (op >> 12) & 0x0Fu;
        unsigned size = top == 1u ? 0u : top == 3u ? 1u : 2u;
        unsigned dst = (op >> 9) & 7u;
        unsigned src_reg = op & 7u;
        unsigned src_mode = (op >> 3) & 7u;
        uint32_t address = cpu->a[src_reg];
        uint32_t value;
        uint32_t mask = size_mask(size);
        if (src_mode == 7u) {
            address = src_reg == 0u
                ? (uint32_t)(int32_t)(int16_t)fetch16(cpu)
                : fetch32(cpu);
            address &= 0x00FFFFFFu;
        }
        if (src_mode == 4u) {
            uint32_t step = (size == 0u && src_reg == 7u) ? 2u : (size == 2u ? 4u : (size == 1u ? 2u : 1u));
            address = (address - step) & 0x00FFFFFFu;
            cpu->a[src_reg] = address;
        }
        if (size == 0u) {
            if (cpu->bus.read8 == 0) { cpu->fault = 1u; return 0u; }
            value = cpu->bus.read8(cpu->bus.context, address);
        } else if (size == 1u) {
            if (cpu->bus.read16 == 0) { cpu->fault = 1u; return 0u; }
            value = cpu->bus.read16(cpu->bus.context, address);
        } else {
            if (cpu->bus.read16 == 0) { cpu->fault = 1u; return 0u; }
            value = ((uint32_t)cpu->bus.read16(cpu->bus.context, address) << 16) |
                    cpu->bus.read16(cpu->bus.context, (address + 2u) & 0x00FFFFFFu);
        }
        cpu->d[dst] = (cpu->d[dst] & ~mask) | (value & mask);
        if (src_mode == 3u) {
            uint32_t step = (size == 0u && src_reg == 7u) ? 2u : (size == 2u ? 4u : (size == 1u ? 2u : 1u));
            cpu->a[src_reg] = (cpu->a[src_reg] + step) & 0x00FFFFFFu;
        }
        set_logic_flags(cpu, value, size);
        used = src_mode == 7u
            ? (src_reg == 0u ? (size == 2u ? 16u : 12u) : (size == 2u ? 20u : 16u))
            : (size == 2u ? 12u : 8u); /* MOVE from (An), +/-An, or absolute short/long to Dn */
    } else if (((op >> 12) == 1u || (op >> 12) == 2u || (op >> 12) == 3u) &&
               ((op >> 3) & 7u) == 0u &&
               ((((op >> 6) & 7u) >= 2u && ((op >> 6) & 7u) <= 4u) ||
                (((op >> 6) & 7u) == 7u && ((op >> 9) & 7u) <= 1u))) {
        unsigned top = (op >> 12) & 0x0Fu;
        unsigned size = top == 1u ? 0u : top == 3u ? 1u : 2u;
        unsigned src = op & 7u;
        unsigned dst_reg = (op >> 9) & 7u;
        unsigned dst_mode = (op >> 6) & 7u;
        uint32_t address = cpu->a[dst_reg];
        uint32_t value = cpu->d[src] & size_mask(size);
        if (dst_mode == 7u) {
            address = dst_reg == 0u
                ? (uint32_t)(int32_t)(int16_t)fetch16(cpu)
                : fetch32(cpu);
            address &= 0x00FFFFFFu;
        }
        if (dst_mode == 4u) {
            uint32_t step = (size == 0u && dst_reg == 7u) ? 2u : (size == 2u ? 4u : (size == 1u ? 2u : 1u));
            address = (address - step) & 0x00FFFFFFu;
            cpu->a[dst_reg] = address;
        }
        if (size == 0u) {
            if (cpu->bus.write8 == 0) { cpu->fault = 1u; return 0u; }
            cpu->bus.write8(cpu->bus.context, address, (uint8_t)value);
        } else if (size == 1u) {
            if (cpu->bus.write16 == 0) { cpu->fault = 1u; return 0u; }
            cpu->bus.write16(cpu->bus.context, address, (uint16_t)value);
        } else {
            if (cpu->bus.write16 == 0) { cpu->fault = 1u; return 0u; }
            cpu->bus.write16(cpu->bus.context, address, (uint16_t)(value >> 16));
            cpu->bus.write16(cpu->bus.context, (address + 2u) & 0x00FFFFFFu, (uint16_t)value);
        }
        if (dst_mode == 3u) {
            uint32_t step = (size == 0u && dst_reg == 7u) ? 2u : (size == 2u ? 4u : (size == 1u ? 2u : 1u));
            cpu->a[dst_reg] = (cpu->a[dst_reg] + step) & 0x00FFFFFFu;
        }
        set_logic_flags(cpu, value, size);
        used = dst_mode == 7u
            ? (dst_reg == 0u ? (size == 2u ? 16u : 12u) : (size == 2u ? 20u : 16u))
            : (size == 2u ? 12u : 8u); /* MOVE Dn to (An), +/-An, or absolute short/long */
    } else if ((op & 0xF1F8u) == 0x2040u ||
               (op & 0xF1F8u) == 0x3040u) {
        unsigned top = (op >> 12) & 0x0Fu;
        unsigned dst = (op >> 9) & 7u;
        unsigned src = op & 7u;
        if (top == 3u) {
            cpu->a[dst] = (uint32_t)(int32_t)(int16_t)(cpu->d[src] & 0xFFFFu);
        } else {
            cpu->a[dst] = cpu->d[src];
        }
        used = 4u; /* MOVEA.W/L Dn,An; flags are not affected */
    } else if ((op & 0xF1F8u) == 0x1000u ||
               (op & 0xF1F8u) == 0x2000u ||
               (op & 0xF1F8u) == 0x3000u) {
        unsigned top = (op >> 12) & 0x0Fu;
        unsigned size = top == 1u ? 0u : top == 3u ? 1u : 2u;
        unsigned src = op & 7u;
        unsigned dst = (op >> 9) & 7u;
        uint32_t mask = size_mask(size);
        uint32_t value = cpu->d[src] & mask;
        cpu->d[dst] = (cpu->d[dst] & ~mask) | value;
        set_logic_flags(cpu, value, size);
        used = 4u; /* MOVE.B/W/L Dn,Dn */
    } else if ((op & 0xF1FFu) == 0x103Cu ||
               (op & 0xF1FFu) == 0x303Cu ||
               (op & 0xF1FFu) == 0x203Cu) {
        unsigned reg = (op >> 9) & 7u;
        unsigned size = ((op >> 12) & 0x0Fu) == 1u ? 0u :
                        ((op >> 12) & 0x0Fu) == 3u ? 1u : 2u;
        uint32_t mask = size_mask(size);
        uint32_t immediate = size == 2u ? fetch32(cpu) : fetch16(cpu);
        cpu->d[reg] = (cpu->d[reg] & ~mask) | (immediate & mask);
        set_logic_flags(cpu, immediate, size);
        used = size == 2u ? 12u : 8u; /* MOVE.B/W/L #imm,Dn */
    } else if ((op & 0xF000u) == 0x5000u &&
               (((op >> 3) & 7u) == 0u || ((op >> 3) & 7u) == 1u) &&
               ((op >> 6) & 3u) != 3u &&
               !(((op >> 3) & 7u) == 1u && ((op >> 6) & 3u) == 0u)) {
        unsigned reg = op & 7u;
        unsigned size = (op >> 6) & 3u;
        unsigned ea_mode = (op >> 3) & 7u;
        uint32_t mask = size_mask(size);
        uint32_t quick = (op >> 9) & 7u;
        uint32_t old_value = ea_mode == 1u ? cpu->a[reg] : (cpu->d[reg] & mask);
        uint32_t result;
        int subtract = (op & 0x0100u) != 0u;
        if (quick == 0u) quick = 8u;
        if (subtract) {
            result = (old_value - quick) & (ea_mode == 1u ? 0xFFFFFFFFu : mask);
            if (ea_mode == 0u) set_sub_flags(cpu, old_value, quick, result, size);
        } else {
            result = (old_value + quick) & (ea_mode == 1u ? 0xFFFFFFFFu : mask);
            if (ea_mode == 0u) set_add_flags(cpu, old_value, quick, result, size);
        }
        if (ea_mode == 1u) {
            cpu->a[reg] = result; /* word-sized quick arithmetic still updates full An */
            used = 8u; /* ADDQ/SUBQ.W/L An; condition codes are unaffected */
        } else {
            cpu->d[reg] = (cpu->d[reg] & ~mask) | result;
            used = size == 2u ? 8u : 4u; /* ADDQ/SUBQ Dn */
        }
    } else if ((op & 0xFF00u) == 0x4200u && (op & 0x0038u) == 0u && ((op >> 6) & 3u) != 3u) {
        unsigned reg = op & 7u;
        unsigned size = (op >> 6) & 3u;
        uint32_t mask = size_mask(size);
        cpu->d[reg] &= ~mask;
        set_logic_flags(cpu, 0u, size);
        used = size == 2u ? 6u : 4u; /* CLR Dn */
    } else if ((op & 0xFF00u) == 0x4A00u && (op & 0x0038u) == 0u && ((op >> 6) & 3u) != 3u) {
        unsigned reg = op & 7u;
        unsigned size = (op >> 6) & 3u;
        set_logic_flags(cpu, cpu->d[reg], size);
        used = size == 2u ? 4u : 4u; /* TST Dn */
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
            case 1u: take = 1; break; /* BSR */
            case 2u: take = !c && !z; break;
            case 3u: take = c || z; break;
            case 4u: take = !c; break;
            case 5u: take = c; break;
            case 6u: take = !z; break;
            case 7u: take = z; break;
            case 8u: take = !v; break;
            case 9u: take = v; break;
            case 10u: take = !n; break;
            case 11u: take = n; break;
            case 12u: take = n == v; break;
            case 13u: take = n != v; break;
            case 14u: take = !z && n == v; break;
            default: take = z || n != v; break;
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
    } else if ((op & 0xF1C0u) == 0x41C0u) {
        unsigned dst = (op >> 9) & 7u;
        unsigned ea_mode = (op >> 3) & 7u;
        unsigned ea_reg = op & 7u;
        uint32_t address = 0u;
        if (ea_mode == 2u) {
            address = cpu->a[ea_reg];
            used = 4u; /* LEA (An),An */
        } else if (ea_mode == 5u) {
            int16_t offset = (int16_t)fetch16(cpu);
            address = cpu->a[ea_reg] + (int32_t)offset;
            used = 8u; /* LEA d16(An),An */
        } else if (ea_mode == 7u && ea_reg == 0u) {
            address = (uint32_t)(int32_t)(int16_t)fetch16(cpu);
            used = 8u; /* LEA absolute short */
        } else if (ea_mode == 7u && ea_reg == 1u) {
            address = fetch32(cpu);
            used = 12u; /* LEA absolute long */
        } else if (ea_mode == 7u && ea_reg == 2u) {
            uint32_t pc_base = cpu->pc;
            int16_t offset = (int16_t)fetch16(cpu);
            address = pc_base + (int32_t)offset;
            used = 8u; /* LEA d16(PC),An */
        } else {
            cpu->fault = 1u;
            return 0u; /* indexed and unsupported LEA effective addresses */
        }
        cpu->a[dst] = address & 0x00FFFFFFu; /* LEA does not change flags */
    } else if (op == 0x4EF9u) {
        cpu->pc = fetch32(cpu) & 0x00FFFFFFu; /* JMP (xxx).L */
        used = 10u;
    } else if (op == 0x4E72u) {
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
