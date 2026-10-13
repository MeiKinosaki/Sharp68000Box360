/*
 * Sharp68000Box360 v5.2b
 * libXenon boot shell with a synthetic Motorola 68000 smoke program.
 *
 * This is an integration milestone, not yet a compatible X68000 emulator:
 * ROM/media loading, peripherals, video, interrupts and a complete CPU ISA
 * remain unimplemented.
 */
#include <stdio.h>
#include <xenos/xenos.h>
#include <console/console.h>

#include "m68k_core.h"
#include "x68000_bus.h"
#include "x68000_scheduler.h"

static x68000_bus_state_t machine_bus;
static m68k_cpu_t machine_cpu;
static x68000_scheduler_t scheduler;

static void install_smoke_program(void)
{
    m68k_bus_t io = x68000_bus_interface(&machine_bus);

    /* Synthetic reset vectors: initial SSP=0x800, PC=0x100. */
    io.write16(io.context, 0x000000u, 0x0000u);
    io.write16(io.context, 0x000002u, 0x0800u);
    io.write16(io.context, 0x000004u, 0x0000u);
    io.write16(io.context, 0x000006u, 0x0100u);

    /* MOVEQ #-1,D0; NOP; BRA back to itself. */
    io.write16(io.context, 0x000100u, 0x70FFu);
    io.write16(io.context, 0x000102u, 0x4E71u);
    io.write16(io.context, 0x000104u, 0x60FEu);
}

int main(void)
{
    unsigned frame_cycles;

    xenos_init(VIDEO_MODE_AUTO);
    console_init();

    x68000_bus_init(&machine_bus);
    install_smoke_program();
    m68k_init(&machine_cpu, &(m68k_bus_t){
        .context = &machine_bus,
        .read8 = 0,
        .read16 = 0,
        .write8 = 0,
        .write16 = 0
    });
    {
        m68k_bus_t io = x68000_bus_interface(&machine_bus);
        m68k_init(&machine_cpu, &io);
    }
    x68000_scheduler_init(&scheduler, 10000000u, 50u);
    (void)m68k_reset(&machine_cpu);

    printf("Sharp68000Box360 v5.2b\n");
    printf("Xbox 360 / libXenon boot shell\n");
    printf("68000 core: initial opcode slice\n");
    printf("RAM: 1 MiB; ROM window: 256 KiB\n");
    printf("Smoke program: MOVEQ / NOP / BRA\n");
    printf("X68000 peripherals: pending\n");

    for (;;) {
        unsigned executed = 0u;
        frame_cycles = x68000_scheduler_begin_frame(&scheduler);
        while (executed < frame_cycles) {
            unsigned used = m68k_step(&machine_cpu);
            if (used == 0u) break;
            x68000_scheduler_account(&scheduler, used);
            executed += used;
        }
        if (machine_cpu.fault) {
            printf("CPU fault at PC=%06lx\n", (unsigned long)machine_cpu.pc);
            for (;;) { __asm__ volatile("" ::: "memory"); }
        }
    }

    return 0;
}
