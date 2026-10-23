/*
 * qsv.h — simulador de vector de estado: 2^n amplitudes complejas, n ≤ 10
 *
 * Complementa al tablero (qsim.h): el tablero es exacto pero solo con puertas
 * de Clifford; aquí caben fases arbitrarias (rz, cu1, ...) a costa de 2^n
 * números. Sin libm: seno, coseno y raíz salen de qsv.c con solo + − × ÷ y
 * fsqrt, así que el kernel (x87 a 53 bits) y Linux (SSE2) dan los mismos bits.
 * El índice de una amplitud lleva el qubit k en su bit k.
 */
#ifndef QSV_H
#define QSV_H

#include <stdint.h>

#define QSV_MAX 10

typedef struct { double re, im; } Cx;

typedef struct {
    int n;
    Cx  a[1 << QSV_MAX];
} QState;

void   sv_init(QState *s, int n);                         /* |0...0> */
void   sv_gate(QState *s, int q, Cx u00, Cx u01, Cx u10, Cx u11);
void   sv_h(QState *s, int q);
void   sv_phase(QState *s, int q, double lambda);          /* diag(1, e^{iλ}) = u1(λ) */
void   sv_rz(QState *s, int q, double lambda);             /* diag(e^{-iλ/2}, e^{iλ/2}) */
void   sv_rx(QState *s, int q, double theta);
void   sv_ry(QState *s, int q, double theta);
void   sv_cx(QState *s, int c, int t);
void   sv_cphase(QState *s, int c, int t, double lambda);  /* cu1(λ): e^{iλ} en |11> */
void   sv_cz(QState *s, int c, int t);                     /* −1 en |11>, exacto */
void   sv_ccx(QState *s, int c1, int c2, int t);            /* Toffoli: NOT si c1=c2=1 */
void   sv_cswap(QState *s, int c, int t1, int t2);          /* Fredkin: intercambia t1,t2 si c=1 */
double sv_prob1(const QState *s, int q);
double sv_prob_all1(const QState *s, uint32_t mask);        /* P(todos los qubits de mask en 1) */
int    sv_measure(QState *s, int q);                       /* colapsa; usa q_rand() */
double sv_parity(const QState *s, uint32_t mask);          /* <Z...Z> sobre los qubits de mask */

void   q_sincos(double x, double *sn, double *cs);
double q_sqrt(double x);

#endif
