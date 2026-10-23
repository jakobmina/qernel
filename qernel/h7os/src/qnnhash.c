/*
 * qnnhash.c — ver qnnhash.h
 */
#include "qnnhash.h"
#include "qsv.h"
#include "h7.h"

#define QNN_PI 0x1.921fb54442d18p+1   /* mismos bits que PI en qasm.c y que math.pi en Python */

void qnn_step(int64_t n, QnnStep *out) {
    /* n_z7(n) de h7.c YA es n mod 7 de verdad, en [0,6], sin división de 64
     * bits (h7_fold7 pliega octales); h7_qnn_hash.py solo cambia qué hacer
     * cuando ese resto es 0: aquí 0 sigue siendo 0 únicamente si n==0, y
     * cualquier otro múltiplo de 7 se reporta como 7. No es la convención
     * de is_vacuum() del resto de H7-OS — es la de h7_qnn_hash.py a propósito. */
    int m = n_z7(n);
    int nz7 = (m == 0 && n != 0) ? 7 : m;
    int c_nz7 = 7 - nz7;

    double o_n = (n & 1) ? -1.0 : 1.0;     /* cos(pi*n) */
    double i_n = rotate_pow(n);            /* cos(pi*phi*n): misma función que h7.c */
    double pcv = o_n * i_n + i_n;
    double d_s = (double)(n + c_nz7 - nz7);

    /* el mismo circuito de 3 qubits que run_qnn_step en h7_qnn_hash.py:
     * h en los tres, rz/ry/rx en el 0 con d_s/d_s/i_n, cswap(0,2,1),
     * ccx(2,1,0), ccx(1,0,2). su2_original se calcula ahí pero no se
     * aplica al circuito (no afecta covariance/asymmetry), así que no
     * hace falta portarlo para estos dos números. */
    QState s;
    sv_init(&s, 3);
    sv_h(&s, 0); sv_h(&s, 1); sv_h(&s, 2);
    sv_rz(&s, 0, d_s);
    sv_ry(&s, 0, d_s);
    sv_rx(&s, 0, i_n);
    sv_cswap(&s, 0, 2, 1);
    sv_ccx(&s, 2, 1, 0);
    sv_ccx(&s, 1, 0, 2);

    /* Born exacto en vez de 512 *shots* de AerSimulator: mismo circuito,
     * sin ruido de muestreo. */
    double Eq1   = sv_prob1(&s, 1);
    double Eq2   = sv_prob1(&s, 2);
    double Eq1q2 = sv_prob_all1(&s, (1u << 1) | (1u << 2));

    out->n = n;
    out->n_z7 = nz7;
    out->c_n_z7 = c_nz7;
    out->o_n = o_n;
    out->i_n = i_n;
    out->particle_class_value = pcv;
    out->d_s = d_s;
    out->covariance = Eq1q2 - Eq1 * Eq2;
    /* P(q2=1,q1=0) - P(q2=0,q1=1) = (Eq2-Eq1q2) - (Eq1-Eq1q2) = Eq2-Eq1:
     * Eq1q2 se cancela algebraicamente, no hace falta guardarlo aparte. */
    out->asymmetry = Eq2 - Eq1;
    out->dist_alt = q_sqrt(out->covariance * out->covariance + out->asymmetry * out->asymmetry);
    out->delta_n = 0;
    out->next_n  = 0;
}

void qnn_hash(int64_t seed, int iterations, QnnHash *out) {
    if (iterations < 0) iterations = 0;
    if (iterations > QNN_MAX_ITER) iterations = QNN_MAX_ITER;

    double phi = 0.5 * (1.0 + q_sqrt(5.0));  /* mismos bits que (1+sqrt(5))/2 en Python */
    int64_t current_n = seed;

    out->seed = seed;
    out->iterations = iterations;

    for (int i = 0; i < iterations; i++) {
        QnnStep *st = &out->step[i];
        qnn_step(current_n, st);

        double delta_f = (st->dist_alt > 1e-9) ? st->dist_alt * 1000.0 * phi
                                                : st->covariance * 1000.0 * phi;
        int64_t delta_n = (int64_t)delta_f;          /* int(): trunca hacia 0, como Python */
        int64_t next_n = current_n + delta_n + st->c_n_z7;
        if (next_n < 0) next_n = -next_n;

        st->delta_n = delta_n;
        st->next_n  = next_n;
        current_n = next_n;
    }

    out->final_n = current_n;
    qnn_step(current_n, &out->final_step);

    /* lo que de verdad expone generate_metriplectic_hash: psi y energy sí
     * dependen de la semilla (vía final_n); torsion es la constante que es
     * en h7_bridge.py (2π-7), no algo derivado de n — se reporta tal cual. */
    out->psi     = out->final_step.i_n;
    out->energy  = out->final_step.particle_class_value;
    out->torsion = 2.0 * QNN_PI - 7.0;
}
