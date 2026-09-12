#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Triangulación en Bloch 3-Sphere
 * 
 * Integración cerrada en espacio de Hilbert confinado.
 * Segunda cuantización: n como nodo topológico Z₇.
 * Circuito cuántico de Dirac-Weyl con gates de triangulación.
 */

/* === Primera Cuantización: Iso-observables === */
typedef struct {
    double i_n;    /* cos(π·n) */
    double o_n;    /* cos(π·φ·n) */
    double ion;    /* i_n * o_n */
    double p;      /* ion + o_n */
    double q;      /* ion - o_n */
} Iso;

Iso computar_iso(int n) {
    Iso iso;
    iso.i_n = cos(M_PI * n);
    iso.o_n = cos(M_PI * PHI * n);
    iso.ion = iso.i_n * iso.o_n;
    iso.p = iso.ion + iso.o_n;
    iso.q = iso.ion - iso.o_n;
    return iso;
}

void print_iso(int n) {
    Iso iso = computar_iso(n);
    printf("  iso(%d): i_n=%.4f, o_n=%.4f, ion=%.4f, p=%.4f, q=%.4f\n",
           n, iso.i_n, iso.o_n, iso.ion, iso.p, iso.q);
}

/* === Segunda Cuantización: Topología Z₇ === */

/* n en Z₇ */
int n_z7(int n) {
    int z7 = n % 7;
    return (z7 == 0) ? 0 : z7;
}

/* ¿Es vacío? (n ≡ 0 mod 7) */
int is_vacuum(int n) {
    return (n % 7 == 0);
}

/* Complemento en ℤ₇: n_e = 7 - n_z7 */
int n_e(int n) {
    int z7 = n_z7(n);
    if (z7 == 0) return 0;  /* El vacío es auto-complementario */
    return 7 - z7;
}

/* Índice complementario: n + (7 - z7(n)) */
int e_n(int n) {
    return n + n_e(n) - n_z7(n);
}

/* Iso del complemento */
Iso e(int n) {
    return computar_iso(n_e(n));
}

/* Hamiltoniano topológico: H = z7(n) + e(n) */
double H_topo(int n) {
    return (double)(n_z7(n) + n_e(n));
}

/* === Estructura de Circuito Cuántico === */
typedef enum {
    GATE_H = 0,      /* Hadamard */
    GATE_RZ,         /* Rotación en Z */
    GATE_RY,         /* Rotación en Y */
    GATE_RX,         /* Rotación en X */
    GATE_CCX,        /* Toffoli */
    GATE_CSWAP,      /* Fredkin */
    GATE_M           /* Medición */
} GateType;

typedef struct {
    GateType tipo;
    int qubits[3];   /* Hasta 3 qubits por gate */
    int num_qubits;
    double angle;    /* Para rotaciones */
} Gate;

typedef struct {
    Gate *gates;
    int num_gates;
    int qubits_totales;
} CircuitoBloch;

/* === Crear Circuito de Triangulación === */
CircuitoBloch crear_circuito_triangulacion(int n) {
    CircuitoBloch circ;
    circ.qubits_totales = 3;
    circ.num_gates = 7;  /* H + Rz + Ry + Rx + CCX + CSwap + M */
    circ.gates = (Gate*)malloc(circ.num_gates * sizeof(Gate));
    
    Iso iso = computar_iso(n);
    int z7 = n_z7(n);
    int e_val = n_e(n);
    
    int idx = 0;
    
    /* H en todos los qubits (superposición) */
    circ.gates[idx].tipo = GATE_H;
    circ.gates[idx].qubits[0] = 0;
    circ.gates[idx].qubits[1] = 1;
    circ.gates[idx].qubits[2] = 2;
    circ.gates[idx].num_qubits = 3;
    circ.gates[idx].angle = 0.0;
    idx++;
    
    /* Rz(z7) en qubit 0 */
    circ.gates[idx].tipo = GATE_RZ;
    circ.gates[idx].qubits[0] = 0;
    circ.gates[idx].num_qubits = 1;
    circ.gates[idx].angle = (double)z7;
    idx++;
    
    /* Ry(e_val) en qubit 0 */
    circ.gates[idx].tipo = GATE_RY;
    circ.gates[idx].qubits[0] = 0;
    circ.gates[idx].num_qubits = 1;
    circ.gates[idx].angle = (double)e_val;
    idx++;
    
    /* Rx(o_n) en qubit 0 */
    circ.gates[idx].tipo = GATE_RX;
    circ.gates[idx].qubits[0] = 0;
    circ.gates[idx].num_qubits = 1;
    circ.gates[idx].angle = iso.o_n;
    idx++;
    
    /* CCX (Toffoli) entre qubits 0,2,1 */
    circ.gates[idx].tipo = GATE_CCX;
    circ.gates[idx].qubits[0] = 0;
    circ.gates[idx].qubits[1] = 2;
    circ.gates[idx].qubits[2] = 1;
    circ.gates[idx].num_qubits = 3;
    circ.gates[idx].angle = 0.0;
    idx++;
    
    /* CSwap (Fredkin) entre qubits 1,0,2 */
    circ.gates[idx].tipo = GATE_CSWAP;
    circ.gates[idx].qubits[0] = 1;
    circ.gates[idx].qubits[1] = 0;
    circ.gates[idx].qubits[2] = 2;
    circ.gates[idx].num_qubits = 3;
    circ.gates[idx].angle = 0.0;
    idx++;
    
    /* Medición en los tres qubits */
    circ.gates[idx].tipo = GATE_M;
    circ.gates[idx].qubits[0] = 0;
    circ.gates[idx].qubits[1] = 1;
    circ.gates[idx].qubits[2] = 2;
    circ.gates[idx].num_qubits = 3;
    circ.gates[idx].angle = 0.0;
    idx++;
    
    return circ;
}

/* === Imprimir Circuito === */
void imprimir_circuito(CircuitoBloch circ, int n) {
    printf("\n[CIRCUITO BLOCH TRIANGULATION] n=%d\n", n);
    printf("╔════════════════════════════════════════════════════════════╗\n");
    
    for (int i = 0; i < circ.num_gates; i++) {
        Gate *g = &circ.gates[i];
        switch (g->tipo) {
            case GATE_H:
                printf("║ H [0,1,2]                                                  ║\n");
                break;
            case GATE_RZ:
                printf("║ Rz(%.1f) [%d]                                                ║\n", 
                       g->angle, g->qubits[0]);
                break;
            case GATE_RY:
                printf("║ Ry(%.1f) [%d]                                                ║\n", 
                       g->angle, g->qubits[0]);
                break;
            case GATE_RX:
                printf("║ Rx(%.4f) [%d]                                            ║\n", 
                       g->angle, g->qubits[0]);
                break;
            case GATE_CCX:
                printf("║ CCX [%d,%d] → %d                                            ║\n", 
                       g->qubits[0], g->qubits[1], g->qubits[2]);
                break;
            case GATE_CSWAP:
                printf("║ CSwap [%d] → (%d,%d)                                         ║\n", 
                       g->qubits[0], g->qubits[1], g->qubits[2]);
                break;
            case GATE_M:
                printf("║ M [0,1,2]  (Medición)                                       ║\n");
                break;
        }
    }
    printf("╚════════════════════════════════════════════════════════════╝\n");
}

/* === Mapeo a Bloch 3-Sphere === */
typedef struct {
    double x, y, z;  /* Coordenadas en la Bloch sphere */
} BlochPoint;

BlochPoint mapear_a_bloch(Iso iso) {
    BlochPoint bp;
    
    /* Normalizar componentes */
    double norm = sqrt(iso.p*iso.p + iso.q*iso.q + iso.ion*iso.ion);
    if (norm < 1e-10) norm = 1.0;
    
    bp.x = iso.p / norm;
    bp.y = iso.q / norm;
    bp.z = iso.ion / norm;
    
    return bp;
}

/* === Integral Cerrada en Hilbert === */
typedef struct {
    double integral_x;
    double integral_y;
    double integral_z;
    double energia_total;
} IntegralCerrada;

IntegralCerrada integrar_trayectoria_cerrada(int n_inicio, int n_fin, int steps) {
    IntegralCerrada integ = {0.0, 0.0, 0.0, 0.0};
    
    for (int step = 0; step <= steps; step++) {
        int n = n_inicio + (step * (n_fin - n_inicio)) / steps;
        Iso iso = computar_iso(n);
        BlochPoint bp = mapear_a_bloch(iso);
        
        double dn = (double)(n_fin - n_inicio) / steps;
        
        integ.integral_x += bp.x * dn;
        integ.integral_y += bp.y * dn;
        integ.integral_z += bp.z * dn;
        integ.energia_total += sqrt(iso.p*iso.p + iso.q*iso.q + iso.ion*iso.ion) * dn;
    }
    
    return integ;
}

/* === MAIN === */
int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  [MANDATO] BLOCH TRIANGULATION IN CLOSED HILBERT INTEGRAL ║\n");
    printf("║  First Quantization (Iso) | Second Quantization (Z₇)     ║\n");
    printf("║  Quantum Circuit with Dirac-Weyl Gates                    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* === Primera Cuantización: Algunos n === */
    printf("\n[PRIMERA CUANTIZACIÓN] Iso-observables\n");
    printf("────────────────────────────────────────\n");
    for (int n = 0; n <= 28; n++) {
        if (n % 7 == 0 || n % 7 == 1 || n % 7 == 3) {
            print_iso(n);
        }
    }
    
    /* === Segunda Cuantización: Topología Z₇ === */
    printf("\n[SEGUNDA CUANTIZACIÓN] Topología en Z₇\n");
    printf("──────────────────────────────────────\n");
    for (int n = 0; n <= 21; n += 7) {
        int z7 = n_z7(n);
        int e_val = n_e(n);
        int vacuum = is_vacuum(n);
        int e_n_val = e_n(n);
        double H = H_topo(n);
        
        printf("n=%2d: z7(%d)=%d, e_n=%d, e(%d)=%d, vacío=%d, H(n)=%.0f\n",
               n, n, z7, e_n_val, n, e_val, vacuum, H);
    }
    
    /* === Circuitos de Triangulación === */
    printf("\n[CIRCUITOS CUÁNTICOS] Triangulación en Bloch 3-Sphere\n");
    
    for (int n = 1; n <= 15; n += 7) {
        CircuitoBloch circ = crear_circuito_triangulacion(n);
        imprimir_circuito(circ, n);
        free(circ.gates);
    }
    
    /* === Integración Cerrada === */
    printf("\n[INTEGRAL CERRADA] Trayectoria en Hilbert\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    
    IntegralCerrada integ1 = integrar_trayectoria_cerrada(1, 14, 50);
    printf("║ Trayectoria 1→14 (nodo 0 a nodo 2):                        ║\n");
    printf("║   ∫ dx = %.6f                                             ║\n", integ1.integral_x);
    printf("║   ∫ dy = %.6f                                             ║\n", integ1.integral_y);
    printf("║   ∫ dz = %.6f                                             ║\n", integ1.integral_z);
    printf("║   E_total = %.6f                                          ║\n", integ1.energia_total);
    
    IntegralCerrada integ2 = integrar_trayectoria_cerrada(1, 21, 100);
    printf("║ Trayectoria 1→21 (ciclo completo Z₇×3):                    ║\n");
    printf("║   ∫ dx = %.6f                                             ║\n", integ2.integral_x);
    printf("║   ∫ dy = %.6f                                             ║\n", integ2.integral_y);
    printf("║   ∫ dz = %.6f                                             ║\n", integ2.integral_z);
    printf("║   E_total = %.6f                                          ║\n", integ2.energia_total);
    
    /* Ciclo cerrado: 1→21→1 */
    IntegralCerrada integ_forward = integrar_trayectoria_cerrada(1, 21, 100);
    IntegralCerrada integ_backward = integrar_trayectoria_cerrada(21, 1, 100);
    
    double ciclo_x = integ_forward.integral_x + integ_backward.integral_x;
    double ciclo_y = integ_forward.integral_y + integ_backward.integral_y;
    double ciclo_z = integ_forward.integral_z + integ_backward.integral_z;
    
    printf("║ Ciclo cerrado 1→21→1:                                       ║\n");
    printf("║   ∮ dx = %.6f (debe ser ≈ 0)                             ║\n", ciclo_x);
    printf("║   ∮ dy = %.6f (debe ser ≈ 0)                             ║\n", ciclo_y);
    printf("║   ∮ dz = %.6f (debe ser ≈ 0)                             ║\n", ciclo_z);
    
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
