/*
 * con.h — consola de H7-OS: VGA texto 80x25 (fila 0 = barra de estado) + COM1
 */
#ifndef CON_H
#define CON_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t v) { __asm__ volatile ("outb %0, %1" :: "a"(v), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t v; __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }

#define ATTR_TEXT   0x07   /* gris sobre negro */
#define ATTR_BRIGHT 0x0F   /* blanco sobre negro */
#define ATTR_BAR    0x70   /* negro sobre gris: barra de estado */

void con_init(void);
void con_attr(uint8_t attr);
void con_status(const char *s);          /* reescribe la fila 0 (solo VGA) */
void con_clear(void);                    /* limpia filas 1..24 y la terminal serial */
void con_dump_vga(void);                 /* copia la pantalla VGA al serial */
void kprintf(const char *fmt, ...);      /* %d %u %x %X %s %c %f, ancho, '-', '0', precisión */
void ksprintf(char *buf, int size, const char *fmt, ...);

/* Entrada */
void con_input(char c);                  /* lo llaman las IRQ */
void con_serial_irq(void);
int  con_getc(void);                     /* bloquea con hlt hasta que llegue un carácter */
void con_echo(char c);
void con_backspace(void);

#endif
