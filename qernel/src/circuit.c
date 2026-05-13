#include <stdio.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Simulación del Oscilador Metripléctico H7
 */

LagrangianComponents compute_lagrangian(MetriplecticState *u) {
    LagrangianComponents L;
    L.H = 0.5 * (u->v * u->v + u->psi * u->psi); /* Energía Conservativa */
    L.S = 0.25 * pow(u->psi * u->psi - 1.0, 2);  /* Potencial Disipativo */
    return L;
}

int main() {
    MetriplecticState u = {1.0, 0.0, 0.0};
    double dt = 0.01;
    
    printf("step,t,psi,H,S,On\n");

    for (int i = 0; i < 1000; i++) {
        double t = i * dt;
        LagrangianComponents L = compute_lagrangian(&u);
        double On = golden_operator(t);

        /* Regla 1.1: Componente Simpléctica */
        double d_psi_symp = u.v;
        double d_v_symp = -u.psi;

        /* Regla 1.2: Componente Métrica (Relajación) */
        double d_psi_metr = (u.psi - pow(u.psi, 3));

        /* Evolución Total */
        u.psi += (d_psi_symp + d_psi_metr * On) * dt;
        u.v   += d_v_symp * dt;

        if (i % 10 == 0) {
            printf("%d,%.3f,%.4f,%.4f,%.4f,%.4f\n", i, t, u.psi, L.H, L.S, On);
        }
    }

    return 0;
}
