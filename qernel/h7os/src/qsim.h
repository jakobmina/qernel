/*
 * qsim.h — tablero de estabilizadores (Aaronson–Gottesman, 2004)
 *
 * Simula exactamente circuitos de Clifford (H, S, CNOT, Paulis, medición en Z)
 * sobre hasta 32 qubits, sin números reales: cada fila es un operador de Pauli
 * guardado como dos máscaras de bits (x, z) y un bit de signo r.
 * Filas 0..n-1: desestabilizadores; n..2n-1: estabilizadores; 2n: auxiliar.
 */
#ifndef QSIM_H
#define QSIM_H

#include <stdint.h>

#define QMAX 32

typedef struct {
    int      n;
    uint32_t x[2 * QMAX + 1];
    uint32_t z[2 * QMAX + 1];
    uint8_t  r[2 * QMAX + 1];
} Tableau;

void     tab_init(Tableau *t, int n);            /* |0...0> */
void     tab_h(Tableau *t, int a);
void     tab_s(Tableau *t, int a);
void     tab_cx(Tableau *t, int c, int tg);
void     tab_x(Tableau *t, int a);
void     tab_y(Tableau *t, int a);
void     tab_z(Tableau *t, int a);
int      tab_measure(Tableau *t, int a, int *random);   /* devuelve 0/1; *random = 1 si el resultado fue al azar */
void     tab_reset(Tableau *t, int a);

void     q_seed(uint32_t s);
uint32_t q_rand(void);

#endif
