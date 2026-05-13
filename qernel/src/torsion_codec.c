#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Codec de Torsión Cuántica (Librería Core)
 */

EstadoCuantico encode_char(char c) {
    EstadoCuantico estado;
    uint8_t val = (uint8_t)c;
    
    int b0 = (val >> 0) & 1;
    int b1 = (val >> 1) & 1;
    int b2 = (val >> 2) & 1;
    int b3 = (val >> 3) & 1;
    int b4 = (val >> 4) & 1;
    int b5 = (val >> 5) & 1;
    int b6 = (val >> 6) & 1;
    
    double w_mag = (double)(b0 + (b1 << 1) + (b6 << 2));
    double x_mag = (double)b2 + 0.5;
    double y_mag = (double)b3;
    double z_mag = (double)b4;
    double sign = (b5 == 1) ? 1.0 : -1.0;
    
    estado.psi[0] = w_mag / 2.0;
    estado.psi[7] = w_mag / 2.0;
    estado.psi[1] = (2.0 + sign * x_mag) / 4.0;
    estado.psi[6] = (2.0 - sign * x_mag) / 4.0;
    estado.psi[2] = (1.0 + sign * y_mag) / 2.0;
    estado.psi[5] = (1.0 - sign * y_mag) / 2.0;
    estado.psi[3] = (1.0 + sign * z_mag) / 2.0;
    estado.psi[4] = (1.0 - sign * z_mag) / 2.0;
    
    double total_prob = 0.0;
    for (int i = 0; i < HILBERT_DIM; i++) total_prob += estado.psi[i];
    if (total_prob > 0.0) {
        for (int i = 0; i < HILBERT_DIM; i++) estado.psi[i] /= total_prob;
    }
    return estado;
}

char decode_char(EstadoCuantico estado) {
    double S_x = estado.psi[1] + estado.psi[6];
    if (S_x < 1e-10) return '?';
    double scale = 1.0 / S_x;
    
    int w_mag = (int)round((estado.psi[0] + estado.psi[7]) * scale);
    double sign_x_mag = 2.0 * (estado.psi[1] - estado.psi[6]) * scale;
    double sign_y_mag = (estado.psi[2] - estado.psi[5]) * scale;
    double sign_z_mag = (estado.psi[3] - estado.psi[4]) * scale;
    
    double sign = (sign_x_mag < 0.0) ? -1.0 : 1.0;
    int b5 = (sign_x_mag < 0.0) ? 0 : 1;
    
    int b2 = (int)round(sign_x_mag * sign - 0.5);
    int b3 = (int)round(sign_y_mag * sign);
    int b4 = (int)round(sign_z_mag * sign);
    int b0 = w_mag & 1;
    int b1 = (w_mag >> 1) & 1;
    int b6 = (w_mag >> 2) & 1;
    
    return (char)((b6 << 6) | (b5 << 5) | (b4 << 4) | (b3 << 3) | (b2 << 2) | (b1 << 1) | b0);
}
