/*
 * int.c — IDT, PIC y teclado PS/2
 */
#include <stdbool.h>
#include "int.h"
#include "con.h"

typedef struct __attribute__((packed)) {
    uint16_t off_lo, sel;
    uint8_t  zero, flags;
    uint16_t off_hi;
} IdtEntry;

static IdtEntry idt[48];
static struct __attribute__((packed)) { uint16_t limit; uint32_t base; } idtr;
extern const uint32_t isr_table[48];

static const char *const EXC[32] = {
    "división entre cero", "depuración", "NMI", "breakpoint", "overflow", "fuera de rango",
    "opcode inválido", "sin FPU", "doble falta", "segmento de coprocesador", "TSS inválido",
    "segmento ausente", "falta de pila", "protección general", "falta de página", "reservada",
    "error x87", "alineación", "machine check", "error SIMD", "virtualización", "control",
    "reservada", "reservada", "reservada", "reservada", "reservada", "reservada",
    "hipervisor", "VMM", "seguridad", "reservada",
};

static inline void io_wait(void) { outb(0x80, 0); }

/* ------------------------------------------------------------------------ */
/* Teclado PS/2, scancodes del conjunto 1 (el BIOS deja la traducción activa) */
/* ------------------------------------------------------------------------ */

/* Mapa por posición física de US. 0x35 (la "/" de US) da "-", que es donde
 * está el guion en el teclado latinoamericano; 0x0C (guion de US) también. */
static const char KEYMAP[0x54] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '-', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.',
};

static void kbd_irq(void) {
    static bool shift, ext;
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { ext = true; return; }
    bool up = sc & 0x80;
    sc &= 0x7F;
    if (ext) {                                       /* 0xE0 0x48: flecha arriba; el resto se ignora */
        ext = false;
        if (!up && sc == 0x48) con_input(0x10);
        return;
    }
    if (sc == 0x2A || sc == 0x36) { shift = !up; return; }
    if (up || sc >= sizeof KEYMAP) return;
    char c = KEYMAP[sc];
    if (shift && c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c) con_input(c);
}

/* ------------------------------------------------------------------------ */

void isr_dispatch(IntFrame *f) {
    if (f->int_no < 32) {
        con_attr(ATTR_BRIGHT);
        kprintf("\nexcepción %u (%s) · eip 0x%08X · error 0x%X · sistema detenido\n",
                f->int_no, EXC[f->int_no], f->eip, f->err);
        for (;;) __asm__ volatile ("cli; hlt");
    }

    int irq = (int)f->int_no - 32;
    if (irq == 7) {                                  /* IRQ7 espuria: sin EOI */
        outb(0x20, 0x0B);
        if (!(inb(0x20) & 0x80)) return;
    }
    if (irq == 1) kbd_irq();
    else if (irq == 4) con_serial_irq();

    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void int_init(void) {
    for (int i = 0; i < 48; i++) {
        uint32_t a = isr_table[i];
        idt[i] = (IdtEntry){ (uint16_t)a, 0x08, 0, 0x8E, (uint16_t)(a >> 16) };
    }
    idtr.limit = sizeof idt - 1;
    idtr.base  = (uint32_t)idt;
    __asm__ volatile ("lidt %0" :: "m"(idtr));

    /* PIC: IRQ 0-7 -> 0x20, IRQ 8-15 -> 0x28 (fuera de las excepciones) */
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    outb(0x21, (uint8_t)~((1 << 1) | (1 << 4)));     /* solo teclado y COM1 */
    outb(0xA1, 0xFF);

    while (inb(0x64) & 1) (void)inb(0x60);           /* vacía el 8042 */
    __asm__ volatile ("sti");
}
