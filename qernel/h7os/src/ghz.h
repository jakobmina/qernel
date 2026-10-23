/*
 * ghz.h — n mod 14 en la base GHZ de 3 qubits, con lectura determinista (v0.7)
 *
 * El estado de la «Matrioska H7» (columna H del ciclo de 14):
 *     (|r⟩ + (−1)^n |r̄⟩)/√2,   r = n mod 7,   r̄ = 7 − r = r XOR 111.
 * Los 8 estados que aparecen son la base GHZ (ortonormal), así que se leen
 * sin azar: CX q2→q0, CX q2→q1, H q2 y medir.
 *   c2     = paridad de n: valor propio (−1)^n de XXX
 *   c1 c0  = bits bajos del miembro del par {r, r̄} que tiene el bit alto en 0
 * Medir en Z, en cambio, da r o r̄ al 50 % y pierde el signo.
 * La lectura junta n con 14 − n (mismo par, misma paridad); el bit r ≥ 4 dice
 * cuál de los dos es el residuo y completa n mod 14.
 * Codificar es el inverso: X según (paridad, par), luego H q2, CX q2→q1, CX q2→q0.
 * No usa nada del kernel: compila igual en Linux (tools/ghz_exp.c).
 */
#ifndef GHZ_H
#define GHZ_H

#include <stdint.h>

int ghz_qasm(char *buf, int size, int32_t n, int decode);        /* longitud, -1 si no cupo */
int ghz_read(int32_t n, int *code, double *p, int *state_ok, const char **err);
int ghz_mod14(int code, int flag);                                /* n mod 14 desde la lectura y r ≥ 4 */

/* all = 1: tabla n = 0..13 y comprobaciones; all = 0: solo n */
int ghz_report(int32_t n, int all, void (*emit)(const char *line), const char **err);

#endif
