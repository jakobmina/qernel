/*
 * qtext.h — piezas comunes de los experimentos (echo.c, unital.c)
 *
 * Texto: un búfer con tope (Out) y formateadores que no usan libc, para que
 * el kernel y Linux escriban exactamente los mismos bytes.
 * Azar: xorshift32 propio, sembrado por (base, modelo, realización); no toca
 * la semilla de `q seed`.
 */
#ifndef QTEXT_H
#define QTEXT_H

#include <stdint.h>

typedef struct { char *p; int left, len, full; } Out;

void qx_put(Out *o, const char *s);
void qx_puti(Out *o, int v);
void qx_putfrac(Out *o, int num, int e);                  /* num/2^e reducida: −3/8, 1/4, 0 */
void qx_putfix(Out *o, double v, int dec);                /* v ≥ 0, dec ≤ 6 decimales */
void qx_putsfix(Out *o, double v, int dec);               /* con columna de signo: " 0.28" o "-0.28" */
void qx_putpad(Out *o, const char *s, int width);         /* ancho en caracteres UTF-8 */
void qx_qubit(Out *o, const char *reg, int j);            /* reg[j] */

void qx_seed(uint32_t base, int m, int i);
int  qx_draw24(void);                                     /* entero uniforme en [0, 2^24) */

#endif
