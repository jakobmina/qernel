/*
 * qnnhash.h — réplica exacta (sin Aer, sin shots) de generate_metriplectic_hash()
 *             de h7_qnn_hash.py / h7_bridge.py, para el pipeline H7 + Ollama de Jako
 *
 * Qué queda igual: n_z7 (convención de h7_qnn_hash.py, NO la de h7.c — el 0
 * no cuenta como múltiplo "vacío" aquí), o_n = cos(πn), i_n = rotate_pow(n)
 * = cos(πφn) (misma función que h7.c), el circuito de 3 qubits de
 * run_qnn_step, y la recurrencia de generate_metriplectic_hash.
 *
 * Qué cambia a propósito: covariance/asymmetry salen de probabilidades
 * exactas (Born, vía qsv.c) en vez de 512 *shots* de AerSimulator — mismo
 * circuito, sin ruido de muestreo y reproducible bit a bit entre el kernel
 * y Linux, igual que el resto de H7-OS.
 *
 * Qué se deja fuera a propósito (no alimenta psi/energy/torsion en la
 * salida real de generate_metriplectic_hash, así que no hace falta
 * portarlo): su2_original/el cuaternión, QNNGrid, EstadoCuantico, y la
 * exportación a JSON/binario de h7_bridge.py. "torsion" se expone tal
 * cual es: una constante (2π−7), no un valor derivado de la semilla.
 */
#ifndef QNNHASH_H
#define QNNHASH_H

#include <stdint.h>

#define QNN_MAX_ITER 8

typedef struct {
    int64_t n;                     /* entrada de este paso */
    int     n_z7, c_n_z7;          /* convención de h7_qnn_hash.py */
    double  o_n, i_n;              /* cos(pi n), cos(pi phi n) = rotate_pow(n) */
    double  particle_class_value;  /* o_n*i_n + i_n */
    double  d_s;                   /* n + c_n_z7 - n_z7 */
    double  covariance;            /* E(q1 q2) - E(q1)*E(q2), exacto */
    double  asymmetry;             /* P(q2=1,q1=0) - P(q2=0,q1=1) = E(q2)-E(q1) */
    double  dist_alt;              /* sqrt(covariance^2 + asymmetry^2) */
    int64_t delta_n, next_n;
} QnnStep;

typedef struct {
    int64_t seed, final_n;
    int     iterations;                 /* ≤ QNN_MAX_ITER */
    QnnStep step[QNN_MAX_ITER];         /* las iteraciones de recalibración */
    QnnStep final_step;                 /* el paso extra sobre final_n */
    double  psi;                        /* = final_step.i_n */
    double  energy;                     /* = final_step.particle_class_value */
    double  torsion;                    /* = 2*pi - 7, constante: no depende de n */
} QnnHash;

/* Un paso de run_qnn_step: construye el circuito de 3 qubits y mide
 * covariance/asymmetry de forma exacta (sin azar). */
void qnn_step(int64_t n, QnnStep *out);

/* generate_metriplectic_hash(seed, iterations): hasta QNN_MAX_ITER pasos. */
void qnn_hash(int64_t seed, int iterations, QnnHash *out);

#endif
