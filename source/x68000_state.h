#ifndef SHARP68000_STATE_H
#define SHARP68000_STATE_H

#include <stddef.h>
#include <stdint.h>
#include "m68k_core.h"
#include "x68000_bus.h"
#include "x68000_scheduler.h"

/* Versioned, portable save-state payload. ROM bytes and bus function pointers
 * are deliberately excluded; loading restores mutable machine state only. */
#define X68000_STATE_VERSION 1u
#define X68000_STATE_HEADER_SIZE 20u
#define X68000_STATE_RAM_SIZE X68000_RAM_SIZE
#define X68000_STATE_SIZE (X68000_STATE_HEADER_SIZE + 80u + 44u + 9u + X68000_STATE_RAM_SIZE)

size_t x68000_state_size(void);
int x68000_state_save(const m68k_cpu_t *cpu,
                      const x68000_bus_state_t *bus,
                      const x68000_scheduler_t *scheduler,
                      uint8_t *output, size_t capacity);
int x68000_state_load(m68k_cpu_t *cpu,
                      x68000_bus_state_t *bus,
                      x68000_scheduler_t *scheduler,
                      const uint8_t *input, size_t length);

#endif
