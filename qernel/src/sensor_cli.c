#include <stdio.h>
#include <stdlib.h>
#include "metriplectic.h"

int main() {
    uint8_t buffer[256];
    for (int i = 0; i < 256; i++) buffer[i] = rand() % 256;
    
    printf("[MANDATO] CLI de Sensor de Pantalla\n");
    TorsionObservables obs = feel_screen(buffer, 16, 16);
    printf(" > Densidad de Energía (H): %.4f\n", obs.energy_density);
    printf(" > Gradiente de Entropía (S): %.4f\n", obs.entropy_gradient);
    printf(" > Torsión Espacial (Tau): %.4f\n", obs.spatial_torsion);
    
    return 0;
}
