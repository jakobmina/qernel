#include <stdio.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Motor de Torsión QNN - Producto de Hamilton
 */

Cuaternion hamilton_product(Cuaternion q1, Cuaternion q2) {
    Cuaternion r;
    r.w = q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z;
    r.x = q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y;
    r.y = q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x;
    r.z = q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w;
    return r;
}

LagrangianComponents compute_qnn_lagrangian(MetriplecticState *u) {
    LagrangianComponents L;
    /* Energía: Norma del Cuaternión */
    L.H = sqrt(u->q.w*u->q.w + u->q.x*u->q.x + u->q.y*u->q.y + u->q.z*u->q.z);
    /* Disipación: Parte Escalar (Entropy source) */
    L.S = u->q.w;
    return L;
}

int main() {
    /* Inicialización de dos momentos de torsión */
    Cuaternion q_a = {0.0, 0.362, 0.730, 0.500};
    Cuaternion q_b = {0.0, -0.362, -0.730, -0.500};
    
    printf("[MANDATO] QNN Torsion Engine Activo\n");
    
    Cuaternion q_final = hamilton_product(q_a, q_b);
    
    MetriplecticState state;
    state.q = q_final;
    
    LagrangianComponents L = compute_qnn_lagrangian(&state);
    
    printf("q_final = %.3f + %.3fi + %.3fj + %.3fk\n", q_final.w, q_final.x, q_final.y, q_final.z);
    printf("Energía H: %.4f | Disipación S: %.4f\n", L.H, L.S);
    
    return 0;
}
