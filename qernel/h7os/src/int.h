/*
 * int.h — interrupciones de H7-OS: IDT de 48 entradas, PIC 8259 remapeado,
 * teclado PS/2 (IRQ1) y COM1 (IRQ4)
 */
#ifndef INT_H
#define INT_H

#include <stdint.h>

/* Lo que isr.S deja en la pila antes de llamar a isr_dispatch. */
typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* pusha */
    uint32_t int_no, err;
    uint32_t eip, cs, eflags;
} IntFrame;

void int_init(void);
void isr_dispatch(IntFrame *f);

#endif
