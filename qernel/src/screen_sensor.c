#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Sensor de Torsión de Pantalla (Librería Core)
 */

TorsionObservables feel_screen(uint8_t *buffer, int width, int height) {
    TorsionObservables obs = {0};
    int total_pixels = width * height;
    
    double sum = 0;
    double var_sum = 0;
    double torsion_sum = 0;
    double chirality_sum = 0;
    
    for (int i = 0; i < total_pixels; i++) sum += buffer[i];
    obs.energy_density = sum / (total_pixels * 255.0);
    
    for (int i = 0; i < total_pixels - 1; i++) {
        var_sum += fabs((buffer[i] / 255.0) - obs.energy_density);
        torsion_sum += fabs((double)buffer[i] - (double)buffer[i+1]) / 255.0;
        if (i + width < total_pixels) {
            chirality_sum += ((double)buffer[i] - (double)buffer[i+width]) / 255.0;
        }
    }
    
    obs.entropy_gradient = var_sum / total_pixels;
    obs.spatial_torsion = torsion_sum / total_pixels;
    obs.chirality = (chirality_sum > 0) ? 1.0 : -1.0;
    
    return obs;
}

EstadoCuantico map_observables_to_hilbert(TorsionObservables obs) {
    EstadoCuantico estado = {0};
    
    estado.psi[0] = obs.energy_density / 2.0;
    estado.psi[7] = obs.energy_density / 2.0;
    
    double x_mag = obs.spatial_torsion + 0.5;
    estado.psi[1] = (2.0 + obs.chirality * x_mag) / 4.0;
    estado.psi[6] = (2.0 - obs.chirality * x_mag) / 4.0;
    
    estado.psi[2] = (1.0 + obs.entropy_gradient) / 2.0;
    estado.psi[5] = (1.0 - obs.entropy_gradient) / 2.0;
    
    estado.psi[3] = (1.0 + obs.chirality) / 2.0;
    estado.psi[4] = (1.0 - obs.chirality) / 2.0;
    
    double total = 0;
    for(int i=0; i<HILBERT_DIM; i++) total += estado.psi[i];
    if(total > 0) {
        for(int i=0; i<HILBERT_DIM; i++) estado.psi[i] /= total;
    }
    return estado;
}
