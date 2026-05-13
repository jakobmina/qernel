#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Motor de Entrenamiento QNN con Normalización de Manifold
 */

QNNGrid initialize_qnn_grid() {
    QNNGrid grid;
    grid.learning_rate = 0.5; /* Aprendizaje muy agresivo para el demo */
    
    int pairs[QNN_LAYERS][2] = {{0,7}, {1,6}, {2,5}, {3,4}};
    for (int i = 0; i < QNN_LAYERS; i++) {
        grid.layers[i].weight = 1.0;
        grid.layers[i].bias = 0.0;
        grid.layers[i].pair_indices[0] = pairs[i][0];
        grid.layers[i].pair_indices[1] = pairs[i][1];
    }
    return grid;
}

void normalize_state(EstadoCuantico *e) {
    double total = 0;
    for(int i=0; i<HILBERT_DIM; i++) {
        if(e->psi[i] < 0) e->psi[i] = 0;
        total += e->psi[i];
    }
    if(total > 1e-10) {
        for(int i=0; i<HILBERT_DIM; i++) e->psi[i] /= total;
    }
}

void forward_pass(QNNGrid *grid, EstadoCuantico *input, EstadoCuantico *output, double On) {
    for (int l = 0; l < QNN_LAYERS; l++) {
        int i1 = grid->layers[l].pair_indices[0];
        int i2 = grid->layers[l].pair_indices[1];
        
        double sum = input->psi[i1] + input->psi[i2];
        double diff = input->psi[i1] - input->psi[i2];
        
        double new_diff = diff * grid->layers[l].weight + (grid->layers[l].bias * On);
        
        output->psi[i1] = (sum + new_diff) / 2.0;
        output->psi[i2] = (sum - new_diff) / 2.0;
    }
    normalize_state(output);
}

double calculate_loss(EstadoCuantico *pred, EstadoCuantico *target) {
    double loss = 0;
    for (int i = 0; i < HILBERT_DIM; i++) {
        double err = pred->psi[i] - target->psi[i];
        loss += err * err;
    }
    return loss;
}

void train_step(QNNGrid *grid, EstadoCuantico *input, EstadoCuantico *target, double On) {
    EstadoCuantico pred;
    forward_pass(grid, input, &pred, On);
    
    for (int l = 0; l < QNN_LAYERS; l++) {
        int i1 = grid->layers[l].pair_indices[0];
        int i2 = grid->layers[l].pair_indices[1];
        
        double diff_old = input->psi[i1] - input->psi[i2];
        double grad = 0.5 * ((pred.psi[i1] - target->psi[i1]) - (pred.psi[i2] - target->psi[i2]));
        
        grid->layers[l].weight -= grid->learning_rate * grad * diff_old;
        grid->layers[l].bias -= grid->learning_rate * grad * On;
    }
}

int main() {
    QNNGrid grid = initialize_qnn_grid();
    
    EstadoCuantico target = encode_char('H');
    EstadoCuantico input = target;
    
    /* Ruido: Alteramos un poco los pesos iniciales en lugar de las amplitudes */
    /* para simular una red no entrenada ante una entrada limpia */
    for(int i=0; i<QNN_LAYERS; i++) {
        grid.layers[i].weight = 0.1; /* Red "colapsada" */
    }

    printf("[MANDATO] Entrenamiento de Reconstrucción (Estado Colapsado -> 'H')\n");
    
    for (int epoch = 0; epoch <= 3000; epoch++) {
        double On = golden_operator(epoch * 0.001);
        train_step(&grid, &input, &target, On);
        
        if (epoch % 1000 == 0) {
            EstadoCuantico pred;
            forward_pass(&grid, &input, &pred, On);
            double loss = calculate_loss(&pred, &target);
            printf("Época %4d | Loss: %.8f | Resultado: '%c'\n", 
                   epoch, loss, decode_char(pred));
        }
    }
    
    return 0;
}
