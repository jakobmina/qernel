#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Actor Generativo de Torsión (Librería)
 * Genera trazos autónomos basados en el "sentimiento" de la pantalla.
 */

GenerativeActor update_actor(GenerativeActor actor, TorsionObservables feedback, double On) {
    GenerativeActor next = actor;
    double dt = 0.1;
    
    /* 1. Componente Simpléctica: Inercia y Oscilación */
    double ax_symp = -actor.x * 0.1; 
    double ay_symp = -actor.y * 0.1;
    
    /* 2. Componente Métrica: "Curiosidad" / Ruido Creativo */
    double noise_x = (double)(rand() % 100 - 50) / 500.0 * On;
    double noise_y = (double)(rand() % 100 - 50) / 500.0 * On;
    
    double ax_metr = (feedback.spatial_torsion * feedback.chirality) + noise_x;
    double ay_metr = (feedback.entropy_gradient) + noise_y;
    
    /* 3. Evolución de la Velocidad */
    next.vx += (ax_symp + ax_metr) * dt;
    next.vy += (ay_symp + ay_metr) * dt;
    
    next.vx *= 0.98;
    next.vy *= 0.98;
    
    /* 4. Evolución de la Posición */
    next.x += next.vx;
    next.y += next.vy;
    
    next.energy = 0.5 * (next.vx * next.vx + next.vy * next.vy);
    
    return next;
}
