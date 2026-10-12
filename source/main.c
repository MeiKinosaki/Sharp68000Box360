/*
 * Sharp68000Box360 v5.2b
 * Cloud-build bootstrap for Xbox 360 (libXenon).
 *
 * This is a first boot/console diagnostic, not yet the X68000 emulator core.
 */
#include <stdio.h>
#include <xenos/xenos.h>
#include <console/console.h>

int main(void)
{
    xenos_init(VIDEO_MODE_AUTO);
    console_init();

    printf("Sharp68000Box360 v5.2b\n");
    printf("X68000 emulator bootstrap\n");
    printf("libXenon target: Xbox 360\n");
    printf("CPU core: integration pending\n");
    printf("Cloud ELF build: active\n");

    /*
     * libXenon's console API does not expose console_flush().
     * Keep the diagnostic screen alive after writing the startup text.
     */
    for (;;) {
        __asm__ volatile("" ::: "memory");
    }

    return 0;
}
