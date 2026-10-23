/*
 * qasm.h — intérprete de un subconjunto de OpenQASM 2.0, con dos simuladores
 *
 * Acepta: OPENQASM 2.0; include "qelib1.inc"; qreg/creg; barrier;
 * measure a -> b; reset a; if(creg==int) <puerta>; comentarios //.
 * Un registro sin índice se expande a todos sus elementos.
 *
 *   QASM_TAB  tablero de estabilizadores, hasta 32 qubits, exacto:
 *             h s sdg x y z id cx cz
 *   QASM_SV   vector de estado, hasta 10 qubits, además fases arbitrarias:
 *             t tdg rz(λ) rx(θ) ry(θ) u1(λ) p(λ) cu1(λ) cp(λ)
 *
 * Los ángulos son expresiones: números, pi, ( ), + − * / y − unario.
 * Con stop_at_measure = 1 la ejecución se detiene antes de la primera
 * medición, para leer valores esperados exactos del vector de estado.
 */
#ifndef QASM_H
#define QASM_H

#include <stdint.h>
#include "qsim.h"
#include "qsv.h"

#define QASM_MAXREG 4
#define QASM_TAB 0
#define QASM_SV  1

typedef struct { char name[12]; int base, size; } QReg;

typedef struct {
    int         backend;           /* QASM_TAB o QASM_SV: lo elige quien llama */
    int         stop_at_measure;   /* lo elige quien llama */
    Tableau     t;
    QState      sv;
    QReg        q[QASM_MAXREG], c[QASM_MAXREG];
    int         nq, nc, nqubits, ncbits;
    uint32_t    cbits;             /* bit k = bit clásico k */
    int         stmt;              /* número de sentencia donde falló */
    const char *err;               /* 0 si todo bien */
    int         random_meas;       /* mediciones que salieron al azar */
    int         ops;               /* puertas y mediciones ejecutadas */
    int         stopped;           /* se detuvo en una medición (stop_at_measure) */
} QasmRun;

int qasm_run(const char *src, QasmRun *run);             /* 0 bien, -1 error (ver run->err) */
int qasm_creg(const QasmRun *run, const char *name);     /* valor del registro (bit 0 = menos significativo) */
const QReg *qasm_qreg(const QasmRun *run, const char *name);

#endif
