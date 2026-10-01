#include "hal_golden_launder.h"
#include "qcore_metriplectic.h"

void hal_launder_init(GoldenLaunder *launder) {
    launder->target_phi = PHI;
    launder->current_rms = 0.0f;
    launder->duty_cycle = (PHI * PHI) / 25.0f; // Initial estimate (~0.1047)
    launder->kp = 0.001f;                      // Even lower gain for ultra-stable lock
    launder->step_count = 0;
    launder->last_v = 0.0f;
    launder->rms_acc = 0.0f;
}

static float apply_voltage_correction(GoldenLaunder *launder) {
    float error = launder->target_phi - launder->current_rms;

    // Proportional correction to the duty cycle
    launder->duty_cycle += error * launder->kp;

    // Physical limits [0, 5V]
    if (launder->duty_cycle < 0.01f) launder->duty_cycle = 0.01f; // Avoid dead zone
    if (launder->duty_cycle > 0.50f) launder->duty_cycle = 0.50f; // Protect solenoid

    return launder->duty_cycle;
}

float hal_launder_step(GoldenLaunder *launder, float t) {
    launder->step_count++;

    // 1. Compute quasiperiodic threshold based on Golden Operator modulation
    float duty = apply_voltage_correction(launder);
    float threshold = k_cos(PI * duty);

    // 2. Quasiperiodic PWM pulse (Reduced to 1Hz for simulation stability)
    float phase_mod = golden_operator(t) * PI;
    launder->last_v = (k_cos(t * PI * 2.0f + phase_mod) > threshold) ? 5.0f : 0.0f;

    // 3. Update rolling RMS (using exponential moving average for V^2)
    float instantaneous_v2 = launder->last_v * launder->last_v;
    launder->rms_acc = (0.9995f * launder->rms_acc) + (0.0005f * instantaneous_v2);
    launder->current_rms = k_sqrt(launder->rms_acc);

    return launder->last_v;
}
