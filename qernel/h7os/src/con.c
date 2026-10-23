/*
 * con.c — consola de H7-OS
 *
 * Todo lo que se imprime va a dos lados: COM1 (UTF-8 tal cual, \n -> \r\n)
 * y la pantalla VGA en modo texto (UTF-8 traducido a CP437). La fila 0 de la
 * VGA es una barra de estado fija; las filas 1..24 hacen scroll.
 */
#include <stdarg.h>
#include <stdbool.h>
#include "con.h"

#define COM1 0x3F8
#define VGA  ((volatile uint16_t *)0xB8000)
#define COLS 80
#define ROWS 25
#define TOP  1

static int     row = TOP, col = 0;
static uint8_t attr = ATTR_TEXT;

/* ------------------------------------------------------------------------ */
/* UTF-8 <-> CP437 (solo lo que usa el kernel)                              */
/* ------------------------------------------------------------------------ */

static const struct { uint16_t cp; uint8_t vga; } CP437[] = {
    {0x00E1, 0xA0}, {0x00E9, 0x82}, {0x00ED, 0xA1}, {0x00F3, 0xA2}, {0x00FA, 0xA3},
    {0x00F1, 0xA4}, {0x00C9, 0x90}, {0x00B7, 0xFA}, {0x00B1, 0xF1}, {0x00B2, 0xFD},
    {0x2261, 0xF0}, {0x2192, 0x1A}, {0x2190, 0x1B}, {0x2502, 0xB3}, {0x2500, 0xC4},
    {0x25A0, 0xFE}, {0x03C0, 0xE3}, {0x03C6, 0xED}, {0x00FC, 0x81}, {0x2013, '-'},
    {0x221A, 0xFB}, {0x2248, 0xF7}, {0x03C3, 0xE5}, {0x00BF, 0xA8}, {0x2265, 0xF2},
    {0x2212, '-'},
};
#define NCP437 (sizeof CP437 / sizeof CP437[0])

static uint8_t to_vga(uint32_t cp) {
    if (cp < 0x80) return (uint8_t)cp;
    for (unsigned i = 0; i < NCP437; i++) if (CP437[i].cp == cp) return CP437[i].vga;
    return '?';
}

static uint32_t utf8_next(const char **ps) {
    const uint8_t *s = (const uint8_t *)*ps;
    uint32_t cp;
    int extra;
    if (s[0] < 0x80)      { cp = s[0];        extra = 0; }
    else if (s[0] < 0xE0) { cp = s[0] & 0x1F; extra = 1; }
    else if (s[0] < 0xF0) { cp = s[0] & 0x0F; extra = 2; }
    else                  { cp = s[0] & 0x07; extra = 3; }
    s++;
    while (extra-- && (*s & 0xC0) == 0x80) cp = (cp << 6) | (*s++ & 0x3F);
    *ps = (const char *)s;
    return cp;
}

/* ------------------------------------------------------------------------ */
/* COM1                                                                     */
/* ------------------------------------------------------------------------ */

static void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* sin interrupciones */
    outb(COM1 + 3, 0x80);   /* DLAB */
    outb(COM1 + 0, 0x03);   /* divisor 3: 38400 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0x07);   /* FIFO, disparo a 1 byte */
    outb(COM1 + 4, 0x0B);   /* DTR + RTS + OUT2 (OUT2 conecta la IRQ4 en el PC) */
    outb(COM1 + 1, 0x01);   /* interrupción por dato recibido */
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) { }
    outb(COM1, (uint8_t)c);
}

static void serial_write(const char *s) {
    for (; *s; s++) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s);
    }
}

/* ------------------------------------------------------------------------ */
/* VGA                                                                      */
/* ------------------------------------------------------------------------ */

static void vga_fill_row(int r, uint8_t a) {
    for (int c = 0; c < COLS; c++) VGA[r * COLS + c] = (uint16_t)(a << 8 | ' ');
}

static void vga_newline(void) {
    col = 0;
    if (++row < ROWS) return;
    for (int i = TOP * COLS; i < (ROWS - 1) * COLS; i++) VGA[i] = VGA[i + COLS];
    vga_fill_row(ROWS - 1, ATTR_TEXT);
    row = ROWS - 1;
}

static void vga_cursor(void) {
    uint16_t pos = (uint16_t)(row * COLS + (col < COLS ? col : COLS - 1));
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)pos);
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

static void vga_write(const char *s) {
    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (cp == '\n') { vga_newline(); continue; }
        if (col == COLS) vga_newline();          /* salto diferido: 80 columnas exactas no dejan línea vacía */
        VGA[row * COLS + col++] = (uint16_t)(attr << 8 | to_vga(cp));
    }
    vga_cursor();
}

void con_init(void) {
    serial_init();
    for (int r = 0; r < ROWS; r++) vga_fill_row(r, r == 0 ? ATTR_BAR : ATTR_TEXT);
    outb(0x3D4, 0x0A); outb(0x3D5, 14);           /* cursor: líneas 14-15 de la celda */
    outb(0x3D4, 0x0B); outb(0x3D5, 15);
    vga_cursor();
}

void con_clear(void) {
    for (int r = TOP; r < ROWS; r++) vga_fill_row(r, ATTR_TEXT);
    row = TOP;
    col = 0;
    vga_cursor();
    serial_write("\033[2J\033[H");
}

/* ------------------------------------------------------------------------ */
/* Entrada: teclado y COM1 escriben en un mismo anillo desde sus IRQ        */
/* ------------------------------------------------------------------------ */

static volatile char    inbuf[256];
static volatile uint8_t ihead, itail;

void con_input(char c) {
    if ((uint8_t)(ihead + 1) != itail) inbuf[ihead++] = c;
}

void con_serial_irq(void) {
    while (inb(COM1 + 5) & 0x01) con_input((char)inb(COM1));
}

int con_getc(void) {
    __asm__ volatile ("cli" ::: "memory");
    while (ihead == itail) __asm__ volatile ("sti; hlt; cli" ::: "memory");   /* sti+hlt: sin carrera */
    char c = inbuf[itail++];
    __asm__ volatile ("sti" ::: "memory");
    return (uint8_t)c;
}

void con_echo(char c) {
    char s[2] = { c, 0 };
    serial_putc(c);
    vga_write(s);
}

void con_backspace(void) {
    serial_write("\b \b");
    if (col > 0) VGA[row * COLS + --col] = (uint16_t)(ATTR_TEXT << 8 | ' ');
    vga_cursor();
}

void con_attr(uint8_t a) { attr = a; }

void con_status(const char *s) {
    vga_fill_row(0, ATTR_BAR);
    for (int c = 1; *s && c < COLS; c++) VGA[c] = (uint16_t)(ATTR_BAR << 8 | to_vga(utf8_next(&s)));
}

void con_dump_vga(void) {
    char line[COLS * 3 + 8];
    for (int r = 0; r < ROWS; r++) {
        int n = 0, last = 0;
        line[n++] = 'v'; line[n++] = 'g'; line[n++] = 'a'; line[n++] = '|';
        for (int c = 0; c < COLS; c++) {
            uint8_t ch = (uint8_t)VGA[r * COLS + c];
            uint32_t cp = ch;
            if (ch < 0x20 || ch >= 0x80)
                for (unsigned i = 0; i < NCP437; i++) if (CP437[i].vga == ch) cp = CP437[i].cp;
            if (cp < 0x80)       { line[n++] = (char)cp; }
            else if (cp < 0x800) { line[n++] = (char)(0xC0 | cp >> 6);  line[n++] = (char)(0x80 | (cp & 0x3F)); }
            else                 { line[n++] = (char)(0xE0 | cp >> 12); line[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                                   line[n++] = (char)(0x80 | (cp & 0x3F)); }
            if (cp != ' ') last = n;
        }
        line[last > 4 ? last : 4] = '\0';
        serial_write(line);
        serial_write("\n");
    }
}

/* ------------------------------------------------------------------------ */
/* kprintf                                                                  */
/* ------------------------------------------------------------------------ */

static int utoa(char *out, uint32_t v, unsigned base, bool upper) {
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[12];
    int n = 0, len;
    do { tmp[n++] = dig[v % base]; v /= base; } while (v);
    for (len = 0; n; len++) out[len] = tmp[--n];
    return len;
}

/* %.Nf con redondeo mitad-arriba sobre el valor escalado (ver README). */
static int ftoa(char *out, double v, int prec) {
    union { double f; uint64_t i; } u = { v };
    int n = 0;
    if (u.i >> 63) { out[n++] = '-'; v = -v; }
    if (v != v || v >= 4294967295.0) {               /* NaN o fuera de 32 bits: sin dígitos */
        const char *t = (v != v) ? "nan" : "fuera de rango";
        while (*t) out[n++] = *t++;
        return n;
    }
    uint32_t scale = 1;
    for (int i = 0; i < prec; i++) scale *= 10;
    uint32_t ip = (uint32_t)v;
    double   fr = (v - ip) * scale;
    uint32_t f  = (uint32_t)fr;
    if (fr - f >= 0.5) f++;
    if (f >= scale) { ip++; f -= scale; }
    n += utoa(out + n, ip, 10, false);
    if (prec) {
        out[n++] = '.';
        char d[12];
        int k = utoa(d, f, 10, false);
        for (int i = k; i < prec; i++) out[n++] = '0';
        for (int i = 0; i < k; i++) out[n++] = d[i];
    }
    return n;
}

static void vfmt(char *buf, int size, const char *fmt, va_list ap) {
    int o = 0;
    #define PUT(ch) do { if (o < size - 1) buf[o++] = (ch); } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { PUT(*fmt); continue; }
        fmt++;
        bool left = false, zero = false;
        for (;; fmt++) {
            if (*fmt == '-') left = true;
            else if (*fmt == '0') zero = true;
            else break;
        }
        int width = 0, prec = -1;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') { prec = 0; fmt++; while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0'); }
        while (*fmt == 'l') fmt++;

        char tmp[48];
        const char *s = tmp;
        int len = 0;
        switch (*fmt) {
        case 'd': case 'i': {
            int v = va_arg(ap, int);
            if (v < 0) { tmp[len++] = '-'; len += utoa(tmp + len, (uint32_t)(-(v + 1)) + 1u, 10, false); }
            else len = utoa(tmp, (uint32_t)v, 10, false);
            break;
        }
        case 'u': len = utoa(tmp, va_arg(ap, unsigned), 10, false); break;
        case 'x': len = utoa(tmp, va_arg(ap, unsigned), 16, false); break;
        case 'X': len = utoa(tmp, va_arg(ap, unsigned), 16, true);  break;
        case 'c': tmp[len++] = (char)va_arg(ap, int); break;
        case 'f': len = ftoa(tmp, va_arg(ap, double), prec < 0 ? 6 : prec); break;
        case 's': s = va_arg(ap, const char *); if (!s) s = "(null)"; while (s[len]) len++; break;
        case '%': tmp[len++] = '%'; break;
        default:  tmp[len++] = '?'; break;
        }
        int pad = width > len ? width - len : 0;
        if (!left) {
            if (zero && *fmt != 's' && len && tmp[0] == '-' && s == tmp) { PUT('-'); s++; len--; }
            while (pad--) PUT(zero && *fmt != 's' ? '0' : ' ');
        }
        for (int i = 0; i < len; i++) PUT(s[i]);
        if (left) while (pad--) PUT(' ');
    }
    buf[o] = '\0';
    #undef PUT
}

void ksprintf(char *buf, int size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfmt(buf, size, fmt, ap);
    va_end(ap);
}

void kprintf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vfmt(buf, sizeof buf, fmt, ap);
    va_end(ap);
    serial_write(buf);
    vga_write(buf);
}
