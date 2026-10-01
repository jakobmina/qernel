#ifndef HAL_GOLDEN_LAUNDER_H
#define HAL_GOLDEN_LAUNDER_H

#include <stdint.h>

/**
 * @brief GoldenLaunder HAL Controller: Bare-metal Feedback Loop for RMS Phi
 */
typedef struct {
    float target_phi;      // Goal: 1.618...
    float current_rms;     // Calculated rolling RMS
    float duty_cycle;      // Adjusted PWM duty cycle (0.0 to 1.0)
    float kp;              // Proportional Gain for correction
    uint64_t step_count;   // Total cycles processed
    float last_v;          // Last instantaneous voltage
    float rms_acc;         // Internal accumulator (V^2 integral)
} GoldenLaunder;

// HAL API
void hal_launder_init(GoldenLaunder *launder);
float hal_launder_step(GoldenLaunder *launder, float t);

#endif // HAL_GOLDEN_LAUNDER_H
