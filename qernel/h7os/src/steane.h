/*
 * steane.h — código de Steane [[7,1,3]] escrito en OpenQASM 2.0
 *
 * El circuito se genera como texto QASM (lo mismo que se exporta a hardware)
 * y se ejecuta con el intérprete de qasm.c sobre el tablero de estabilizadores.
 * La posición j = 1..7 de Hamming es el qubit q[j-1].
 */
#ifndef STEANE_H
#define STEANE_H

#define STEANE_MAX_ERR 4

typedef struct { char pauli; int pos; } QErr;      /* pauli: 'x' 'y' 'z'; pos: 1..7 */

typedef struct {
    int sz, sx;        /* síndromes de 3 bits: posición del error X y del error Z */
    int m;             /* lectura final, bit k = q[k] (posición k+1) */
    int msyn;          /* síndrome clásico de m: XOR de las posiciones en 1 */
    int logical;       /* paridad de m: el valor lógico leído */
    int random;        /* mediciones al azar (las de la lectura final) */
    int ops;
} SteaneResult;

/* Escribe el circuito en buf; plus = 1 prepara |+_L> en lugar de |0_L>.
 * Devuelve la longitud, o -1 si no cupo. */
int steane_qasm(char *buf, int size, int plus, const QErr *e, int ne);

/* Genera y ejecuta el circuito. Devuelve 0, o -1 con *err apuntando al motivo. */
int steane_run(int plus, const QErr *e, int ne, SteaneResult *res, const char **err);

#endif
