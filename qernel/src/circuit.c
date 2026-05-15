#include <stdio.h>
#include <math.h>
#include <string.h>
#include "metriplectic.h"


/* [MANDATO] Aquora Circuit - 3 Qubits → 8 Amplitudes
 * 
 * Espacio: {|000⟩, |001⟩, |010⟩, |011⟩, |100⟩, |101⟩, |110⟩, |111⟩}
 * Mapeo directo: psi[i] = amplitud de estado |i⟩
 * 
 * Observables Pauli sobre 3 qubits:
 * - IZZ: Z₁Z₂ (primeros dos qubits)
 * - ZIZ: Z₀Z₂ (primero y tercero)
 * - ZZI: Z₀Z₁ (dos primeros)
 */

typedef struct {
    double psi[8];              /* Amplitudes {|000⟩....|111⟩} */
    double prob[8];             /* Probabilidades |ψ|² */
    double observables[3];      /* IZZ, ZIZ, ZZI */
    double covariance[3][3];    /* Matriz de covarianza 3×3 */
    double phi;                 /* Parámetro de control */
    int n_iterations;
} AquoraState;

/**
 * Extraer bit i-ésimo de número n
 */
static inline int get_bit(int n, int i) {
    return (n >> i) & 1;
}

/**
 * Operador Pauli Z en posición i:
 * Z|0⟩ = |0⟩, Z|1⟩ = -|1⟩
 * Eigenvalue: +1 si bit=0, -1 si bit=1
 */
static inline double pauli_z_eigenvalue(int state, int qubit_pos) {
    return get_bit(state, qubit_pos) == 0 ? 1.0 : -1.0;
}

/**
 * Calcular observable ⟨O⟩ = Σ_i P(i) * eigenvalue_O(i)
 */
double compute_observable(double *prob, int (*observable_fn)(int)) {
    double expectation = 0.0;
    for (int i = 0; i < 8; i++) {
        expectation += prob[i] * observable_fn(i);
    }
    return expectation;
}

int observable_IZZ(int state) {
    /* Z en qubit 1, Z en qubit 2 → Z₁Z₂ */
    return pauli_z_eigenvalue(state, 1) * pauli_z_eigenvalue(state, 2);
}

int observable_ZIZ(int state) {
    /* Z en qubit 0, Z en qubit 2 → Z₀Z₂ */
    return pauli_z_eigenvalue(state, 0) * pauli_z_eigenvalue(state, 2);
}

int observable_ZZI(int state) {
    /* Z en qubit 0, Z en qubit 1 → Z₀Z₁ */
    return pauli_z_eigenvalue(state, 0) * pauli_z_eigenvalue(state, 1);
}

/**
 * Calcular covarianza entre dos observables sin SVD
 * Cov(A, B) = ⟨AB⟩ - ⟨A⟩⟨B⟩
 */
double compute_covariance(double *prob, 
                         int (*obs_a)(int), 
                         int (*obs_b)(int)) {
    /* ⟨A⟩ */
    double exp_a = 0.0;
    for (int i = 0; i < 8; i++) {
        exp_a += prob[i] * obs_a(i);
    }
    
    /* ⟨B⟩ */
    double exp_b = 0.0;
    for (int i = 0; i < 8; i++) {
        exp_b += prob[i] * obs_b(i);
    }
    
    /* ⟨AB⟩ */
    double exp_ab = 0.0;
    for (int i = 0; i < 8; i++) {
        exp_ab += prob[i] * obs_a(i) * obs_b(i);
    }
    
    /* Cov(A,B) = ⟨AB⟩ - ⟨A⟩⟨B⟩ */
    return exp_ab - exp_a * exp_b;
}

/**
 * Muestreo de probabilidades asimétricas
 * φ controla la asimetría destructiva↔simétrica
 */
void apply_phase_modulation(AquoraState *state) {
    for (int i = 0; i < 8; i++) {
        /* Fase acumulada en Z₇ */
        int n_mod = i % 7;
        double phase = M_PI * state->phi * n_mod;
        
        /* Modular amplitud por fase */
        state->psi[i] *= cos(phase);
    }
    
    /* Renormalizar */
    double norm_sq = 0.0;
    for (int j = 0; j < 8; j++) {
        norm_sq += state->psi[j] * state->psi[j];
    }
    double norm = sqrt(norm_sq);
    if (norm > 1e-10) {
        for (int j = 0; j < 8; j++) {
            state->psi[j] /= norm;
        }
    }
}

/**
 * Calcular probabilidades desde amplitudes
 */
void compute_probabilities(AquoraState *state) {
    for (int i = 0; i < 8; i++) {
        state->prob[i] = state->psi[i] * state->psi[i];
    }
}

/**
 * Pipeline completo: amplitudes → observables → covarianza
 */
void aquora_step(AquoraState *state) {
    state->n_iterations++;
    
    /* 1. Modular amplitudes por φ */
    apply_phase_modulation(state);
    
    /* 2. Extraer probabilidades (asimétricas por φ) */
    compute_probabilities(state);
    
    /* 3. Calcular observables esperados */
    state->observables[0] = compute_observable(state->prob, observable_IZZ);
    state->observables[1] = compute_observable(state->prob, observable_ZIZ);
    state->observables[2] = compute_observable(state->prob, observable_ZZI);
    
    /* 4. Matriz de covarianza 3×3 */
    state->covariance[0][0] = compute_covariance(state->prob, observable_IZZ, observable_IZZ);
    state->covariance[0][1] = compute_covariance(state->prob, observable_IZZ, observable_ZIZ);
    state->covariance[0][2] = compute_covariance(state->prob, observable_IZZ, observable_ZZI);
    
    state->covariance[1][0] = state->covariance[0][1];
    state->covariance[1][1] = compute_covariance(state->prob, observable_ZIZ, observable_ZIZ);
    state->covariance[1][2] = compute_covariance(state->prob, observable_ZIZ, observable_ZZI);
    
    state->covariance[2][0] = state->covariance[0][2];
    state->covariance[2][1] = state->covariance[1][2];
    state->covariance[2][2] = compute_covariance(state->prob, observable_ZZI, observable_ZZI);
}

int main() {
    AquoraState state;
    state.phi = 0.3624;
    state.n_iterations = 0;
    
    /* Inicializar con amplitudes asimétricas (ejemplo) */
    double amplitudes[8] = {
        0.362,   /* |000⟩ */
        0.730,   /* |001⟩ */
        0.500,   /* |010⟩ */
        0.300,   /* |011⟩ */
        -0.362,  /* |100⟩ */
        -0.730,  /* |101⟩ */
        -0.500,  /* |110⟩ */
        -0.300   /* |111⟩ */
    };
    
    /* Normalizar */
    double norm = 0.0;
    for (int i = 0; i < 8; i++) {
        norm += amplitudes[i] * amplitudes[i];
    }
    norm = sqrt(norm);
    for (int i = 0; i < 8; i++) {
        state.psi[i] = amplitudes[i] / norm;
    }
    
    printf("=== AQUORA 3-Qubit VQE ===\n");
    printf("iter,phi,P000,P001,P010,P011,P100,P101,P110,P111,");
    printf("⟨IZZ⟩,⟨ZIZ⟩,⟨ZZI⟩,");
    printf("Cov(IZZ,ZIZ),Cov(IZZ,ZZI),Cov(ZIZ,ZZI)\n");
    
    for (int iter = 0; iter < 100; iter++) {
        aquora_step(&state);
        
        if (iter % 10 == 0) {
            printf("%d,%.4f,", iter, state.phi);
            for (int i = 0; i < 8; i++) {
                printf("%.4f,", state.prob[i]);
            }
            printf("%.4f,%.4f,%.4f,", 
                   state.observables[0], 
                   state.observables[1], 
                   state.observables[2]);
            printf("%.4f,%.4f,%.4f\n",
                   state.covariance[0][1],
                   state.covariance[0][2],
                   state.covariance[1][2]);
        }
    }
    
    return 0;
}
