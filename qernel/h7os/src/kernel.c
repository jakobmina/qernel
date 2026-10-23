/*
 * kernel.c — H7-OS v0.7
 *
 * Arranque: GDT (boot.S) -> consola -> IDT + PIC + teclado/COM1 -> almacén Z7
 * sembrado y autoprobado -> script opcional -> shell.
 *
 * Línea de comandos (qemu -append): "vgadump" copia la VGA al serial en exit;
 * "run=cmd;cmd;..." ejecuta comandos al arrancar (va al final: se lee hasta
 * el fin de la línea).
 */
#include <stdbool.h>
#include "con.h"
#include "int.h"
#include "rs.h"
#include "shell.h"

typedef struct {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
} MultibootInfo;

extern char _kernel_start[], _kernel_end[];

/* gcc puede emitir llamadas a estas aun en freestanding */
void *memcpy(void *d, const void *s, unsigned n) { char *a = d; const char *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, unsigned n) { char *a = d; while (n--) *a++ = (char)c; return d; }

static const char *find(const char *hay, const char *needle) {
    for (; hay && *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*n && *h == *n) { h++; n++; }
        if (!*n) return hay;
    }
    return 0;
}

void kernel_main(uint32_t magic, const MultibootInfo *mb) {
    con_init();
    con_status("H7-OS v0.7 │ arrancando");
    int_init();

    const char *cmdline = (magic == 0x2BADB002 && (mb->flags & 4)) ? (const char *)mb->cmdline : "";
    uint16_t fpu_cw;
    __asm__ volatile ("fnstcw %0" : "=m"(fpu_cw));

    con_attr(ATTR_BRIGHT);
    kprintf("H7-OS v0.7 · núcleo H7 · almacén Z7 + Hamming · Steane · eco · unitalidad · GHZ\n");
    con_attr(ATTR_TEXT);
    label("boot");
    kprintf("multiboot %s · memoria %u KiB + %u KiB\n", magic == 0x2BADB002 ? "ok" : "??",
            (mb->flags & 1) ? mb->mem_lower : 0, (mb->flags & 1) ? mb->mem_upper : 0);
    label("kern");
    kprintf("0x%08X..0x%08X · %u B\n", (unsigned)_kernel_start, (unsigned)_kernel_end,
            (unsigned)(_kernel_end - _kernel_start));
    label("cpu");
    kprintf("GDT plana · IDT 48 · PIC en 0x20/0x28 · IRQ1 teclado · IRQ4 COM1\n");
    label("fpu");
    kprintf("x87 · control 0x%04X · 53 bits, redondeo como double IEEE\n", fpu_cw);

    shell_seed();
    kprintf("escribe help para ver los comandos\n");

    shell_vgadump = find(cmdline, "vgadump") != 0;
    const char *run = find(cmdline, "run=");
    if (run) shell_run_script(run + 4);

    shell_loop();
}
