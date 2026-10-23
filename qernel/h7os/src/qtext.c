/*
 * qtext.c — texto y azar comunes de los experimentos (ver qtext.h)
 */
#include "qtext.h"

void qx_put(Out *o, const char *s) {
    for (; *s; s++) {
        if (o->left <= 1) { o->full = 1; return; }
        *o->p++ = *s; o->left--; o->len++;
    }
    *o->p = '\0';
}

void qx_puti(Out *o, int v) {
    char t[12];
    int n = 0;
    unsigned u = v < 0 ? 0u - (unsigned)v : (unsigned)v;
    do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) qx_put(o, "-");
    char s[12];
    for (int k = 0; k < n; k++) s[k] = t[n - 1 - k];
    s[n] = '\0';
    qx_put(o, s);
}

void qx_putfrac(Out *o, int num, int e) {
    while (e > 0 && num % 2 == 0) { num /= 2; e--; }
    qx_puti(o, num);
    if (num && e) { qx_put(o, "/"); qx_puti(o, 1 << e); }
}

void qx_putfix(Out *o, double v, int dec) {
    int scale = 1;
    for (int k = 0; k < dec; k++) scale *= 10;
    int n = (int)(v * scale + 0.5);
    qx_puti(o, n / scale);
    if (!dec) return;
    qx_put(o, ".");
    char s[8];
    int f = n % scale;
    for (int k = dec - 1; k >= 0; k--) { s[k] = (char)('0' + f % 10); f /= 10; }
    s[dec] = '\0';
    qx_put(o, s);
}

void qx_putsfix(Out *o, double v, int dec) {
    int scale = 1;
    for (int k = 0; k < dec; k++) scale *= 10;
    double a = v < 0 ? -v : v;
    int neg = v < 0 && (int)(a * scale + 0.5) != 0;   /* −0.0000 se escribe sin signo */
    qx_put(o, neg ? "-" : " ");
    qx_putfix(o, a, dec);
}

void qx_putpad(Out *o, const char *s, int width) {
    int chars = 0;
    for (const char *p = s; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    qx_put(o, s);
    for (; chars < width; chars++) qx_put(o, " ");
}

void qx_qubit(Out *o, const char *reg, int j) { qx_put(o, reg); qx_put(o, "["); qx_puti(o, j); qx_put(o, "]"); }

static uint32_t rng;

void qx_seed(uint32_t base, int m, int i) {
    uint32_t x = base + (uint32_t)m * 0x10000u + (uint32_t)i;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    rng = x ? x : 1;
}

int qx_draw24(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (int)(rng >> 8);
}
