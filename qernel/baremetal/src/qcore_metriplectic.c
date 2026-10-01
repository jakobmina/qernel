#include "qcore_metriplectic.h"

float k_mod_2pi(float x) {
    while (x > 2.0f * PI) x -= 2.0f * PI;
    while (x < 0.0f) x += 2.0f * PI;
    return x;
}

float k_sin(float x) {
    x = k_mod_2pi(x);
    if (x > PI) return -k_sin(x - PI);
    if (x > PI/2.0f) return k_cos(x - PI/2.0f);
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f - x2 * (1.0f/5040.0f))));
}

float k_cos(float x) {
    x = k_mod_2pi(x);
    if (x > PI) return -k_cos(x - PI);
    if (x > PI/2.0f) return -k_sin(x - PI/2.0f);
    float x2 = x * x;
    return 1.0f - x2 * (0.5f - x2 * (1.0f/24.0f - x2 * (1.0f/720.0f - x2 * (1.0f/40320.0f))));
}

float k_exp(float x) {
    // Basic e^x approximation
    return 1.0f + x + (x*x)/2.0f + (x*x*x)/6.0f;
}

float k_sqrt(float x) {
    if (x <= 0) return 0;
    float res = x;
    for(int i=0; i<4; i++) res = 0.5f * (res + x/res);
    return res;
}

float k_phase_lock(float n) {
    // n: Parámetro de evolución (tiempo o índice de nodo)
    // La restricción PI ancla el eje vertical (evita spin espurio)
    // La restricción PHI ancla el eje azimutal (evita resonancia destructiva)
    // Se incluye cos(PI * n) como paridad
    return k_cos(PI * n) * k_cos(PI_PHI_CONST * n);
}

float golden_operator(float n) {
    // Usamos el candado de fase para garantizar coherencia
    // Esto es un operador de segundo orden: Coherencia sobre Coherencia
    return k_phase_lock(n) * k_phase_lock(n * PHI);
}

void init_system(SystemState *state) {
    state->time = 0.0f;
    state->kink_amplitude = 10.0f;
    state->stability = 50.0f;
    state->shear_flow = 10.0f; // Default to Mach 10 "Canal Open"

    state->sync_clock_c = 0.0f;
    state->global_identity = 0.0f;
    state->gamma_strobe = 0.2f;
    state->breathing_state = 0.0f;

    state->node_density = 0.0f;
    state->bit_stream = 0.0f;
    state->causal_flux = 0.0f;
    state->golden_filter = PHI;

    // Solenoid HAL Initialization
    hal_launder_init(&state->launder);
    state->solenoid_filter = 1.0f;

    // Thermal Initialization
    state->temperature = 22.0f; // Ambient Room Temp
    state->entropy_rate = 0.0f;
    state->power_draw = 0.0f;

    // Protocol Alpha Initialization
    state->rayleigh_raw = 0.0f;
    state->l2_error = 0.0f;
    state->thermal_eff = 1.0f;

    // Barbashin-LaSalle Initialization
    state->lyapunov_v = 0.0f;
    state->lyapunov_dot = 0.0f;
    state->is_lasalle_locked = 0;

    state->vortex_z = 0.0f;

    // Acoustic Dynamics (no HAL on this target: stays at rest)
    state->audio_energy = 0.0f;
    state->audio_coherence = 0.0f;

    // Initialize Bus
    for(int i=0; i<4; i++) state->bus.core_sync[i] = 0.0f;
    state->bus.bus_throughput = 0.0f;
    state->bus.packet_loss = 0.0f;

    // Initialize Φ on T^2 with a quasiperiodic wave
    for (int i = 0; i < TORUS_DIM; i++) {
        for (int j = 0; j < TORUS_DIM; j++) {
            float theta = (float)i * 2.0f * PI / TORUS_DIM;
            float phi = (float)j * 2.0f * PI / TORUS_DIM;
            state->phi_re[i][j] = k_cos(theta);
            state->phi_im[i][j] = k_cos(phi * PHI); // Parity-Locked Mapping
        }
    }
}

float compute_sync_clock(SystemState *state) {
    float sum = 0.0f;
    float On = golden_operator(state->time);
    float energy_on = On * On; // Use energy density for observable c

    for (int i = 0; i < TORUS_DIM; i++) {
        for (int j = 0; j < TORUS_DIM; j++) {
            float intensity = state->phi_re[i][j]*state->phi_re[i][j] +
                             state->phi_im[i][j]*state->phi_im[i][j];
            sum += intensity * energy_on;
        }
    }
    return sum / (float)(TORUS_DIM * TORUS_DIM);
}

void apply_breathing_projector(SystemState *state, float dt) {
    float On = golden_operator(state->time);
    float dtheta = On * dt * 2.0f; // Angular evolution

    float cos_dt = k_cos(dtheta);
    float sin_dt = k_sin(dtheta);

    float decay = (100.0f - state->stability) * 0.002f;
    float pump = (state->shear_flow / 10.0f) * 0.1f; // Target intensity drive

    for (int i = 0; i < TORUS_DIM; i++) {
        for (int j = 0; j < TORUS_DIM; j++) {
            float r = state->phi_re[i][j];
            float im = state->phi_im[i][j];

            // 1. Unitary Rotation (Hamiltonian / Reversible)
            state->phi_re[i][j] = r * cos_dt - im * sin_dt;
            state->phi_im[i][j] = r * sin_dt + im * cos_dt;

            // 2. Metriplectic Drive (Metric / Irreversible)
            // Pulls intensity towards 1.0, modulated by stability/decay
            float intensity = state->phi_re[i][j]*state->phi_re[i][j] +
                             state->phi_im[i][j]*state->phi_im[i][j];
            float drive = (1.0f - intensity) * pump;

            state->phi_re[i][j] += state->phi_re[i][j] * (drive - decay) * dt;
            state->phi_im[i][j] += state->phi_im[i][j] * (drive - decay) * dt;
        }
    }
}

void solve_step(SystemState *state, float dt) {
    state->time += dt;

    // 1. Classical Canal (Shear Flow)
    float target_stability = (state->shear_flow >= 9.9f) ? 100.0f : (state->shear_flow * 8.0f);

    // 2. Toroidal Modulation
    apply_breathing_projector(state, dt);
    state->sync_clock_c = compute_sync_clock(state);

    if (state->sync_clock_c > 0.5f) {
        state->global_identity += state->sync_clock_c * dt * 0.1f;
    }

    // 3. Metriplectic Coupling
    // La estabilidad solo aumenta si estamos en "Fase Segura"
    float phase_coherence = k_phase_lock(state->time);
    float stability_gate = phase_coherence * phase_coherence; // Cuadrado para rectificar (energía)

    float tor_boost = (state->sync_clock_c > 0.0f) ? state->sync_clock_c * 10.0f : 0.0f;

    // El término de corrección ahora está modulado por el operador phi-pi
    float d_metr = ((target_stability - state->stability) * 0.2f + tor_boost) * stability_gate;
    state->stability += d_metr * dt;

    // 5. Solenoid HAL & RMS Control (The "Physical Filter")
    float v_pulse = hal_launder_step(&state->launder, state->time);

    // The filter is the magnetic field effect: B = mu * I
    state->solenoid_filter = 1.0f / (1.0f + (v_pulse * 0.1f));
    state->causal_flux *= state->solenoid_filter;

    // 6. Thermal & Acoustic Dynamics (Metriplectic Dissipation)
    // Joule Heating: Q_in = V^2 / R. Assume R_info = 10.0
    state->power_draw = (v_pulse * v_pulse) / 10.0f;
    float heating = state->power_draw;

    // Acoustic Perturbation: Strong sound increases informational entropy
    float acoustic_load = state->audio_energy * 20.0f;
    heating += acoustic_load;

    // Dissipative Cooling: Q_out = k * (T - T_amb)
    float cooling = (state->temperature - 22.0f) * 0.05f;

    state->entropy_rate = heating + cooling; // Metriplectic coupling term
    state->temperature += (heating - cooling) * dt;

    // Thermal & Acoustic Stability Feedback
    if (state->temperature > 60.0f) {
        state->stability -= (state->temperature - 60.0f) * 0.01f * dt;
    }

    // Acoustic Coherence: "Golden Healing"
    // If the sound is coherent (tonic), it helps stabilization
    float healing_boost = state->audio_coherence * 5.0f * dt;
    state->stability += healing_boost;

    // Sudden noise shock
    if (state->audio_energy > 0.5f) {
        state->stability -= state->audio_energy * 10.0f * dt;
    }

    // 7. Nodal Synthesis z(t) = sum(Am cos(wm t + phm))
    // Science decided: Use k_phase_lock for efficiency and tanh-like saturation for Z-restriction
    float nodal_sum = 0.0f;
    float m_amplitudes[] = {0.8f, 0.4f, 0.2f, 0.1f}; // Am for modes 2, 4, 8, 16
    for (int m = 0; m < 4; m++) {
        float mode_idx = (float)(2 << m); // 2, 4, 8, 16
        nodal_sum += m_amplitudes[m] * k_phase_lock(state->time * mode_idx);
    }
    float z_limit = 2.0f;
    state->vortex_z = nodal_sum / k_sqrt(1.0f + (nodal_sum*nodal_sum)/(z_limit*z_limit));

    // 8. Protocol Alpha: Benchmark Analysis
    float ns_baseline = (state->shear_flow >= 9.9f) ? 0.0625f : (state->shear_flow / 10.0f) * 0.0625f;
    float diff = ns_baseline - state->sync_clock_c;
    state->l2_error = (0.995f * state->l2_error) + (0.005f * (diff * diff));

    float heat_penalty = (state->temperature - 22.0f) * 0.1f;
    state->thermal_eff = (state->stability * 1.5f) / (1.0f + heat_penalty + state->entropy_rate);

    // 9. Barbashin-LaSalle Diagnostics
    // Lyapunov Candidate V = 0.5*(100-rho)^2 + 0.5*(V_rms - PHI)^2
    float rho_err = 100.0f - state->stability;
    float phi_err = state->launder.current_rms - PHI;
    float v_new = 0.5f * (rho_err * rho_err + phi_err * phi_err);

    state->lyapunov_dot = (v_new - state->lyapunov_v) / dt;
    state->lyapunov_v = v_new;

    // Convergence Criteria for Maximal Invariant Set
    state->is_lasalle_locked = (state->stability > 98.0f && (phi_err * phi_err) < 0.001f);

    // 9. Inter-core Interaction
    for(int i=0; i<4; i++) {
        // Each core synchronizes based on the golden operator phase shift
        float core_phase = state->time + (float)i * (PI / 2.0f);
        float core_op = k_cos(PI * core_phase) * k_cos(PI * PHI * core_phase);
        state->bus.core_sync[i] = (state->stability / 100.0f) * (core_op * 0.5f + 0.5f);
    }

    // Bus throughput is maximized when core_sync is balanced and stability is high
    state->bus.bus_throughput = state->node_density * state->bus.core_sync[0];
    state->bus.packet_loss = (100.0f - state->stability) / 100.0f;

    if (state->stability < 0) state->stability = 0;
    if (state->stability > 100) state->stability = 100;
}
