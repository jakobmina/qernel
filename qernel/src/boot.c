#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "metriplectic.h"

/**
 * BOOT.C - El Orquestador Metriplético
 * Regla 1: Este proceso actúa como el sumidero y generador de flujo inicial.
 */

void print_banner() {
    printf("   __  _______   __  __ _____ _____ \n");
    printf("  / / / /__  /  /  |/  / ___// ___/ \n");
    printf(" / /_/ /  / /  / /|_/ / __ \ \__ \  \n");
    printf("/ __  /  / /  / /  / / /_/ /___/ /  \n");
    printf("/_/ /_/  /_/  /_/  /_/\____//____/   (Qernel Boot)\n");
    printf("--------------------------------------------\n");
}

/* Regla 3.1: Lagrangiano de Arranque */
LagrangianComponents compute_boot_lagrangian(double uptime, double components_ready) {
    LagrangianComponents L;
    /* H: Esfuerzo de inicialización (Energía cinética de arranque) */
    L.H = components_ready * 10.0; 
    /* S: Disipación de errores de configuración (Relajación al estado estable) */
    L.S = 1.0 / (uptime + 1.0); 
    return L;
}

int main() {
    print_banner();
    
    printf("[BOOT] Iniciando Operador Áureo (O_n)... \n");
    double stability = golden_operator(0.0);
    printf("[BOOT] Estabilidad inicial: %.4f\n", stability);

    /* Regla 1.3: Prohibición de singularidades */
    if (stability == 0) {
        fprintf(stderr, "[FATAL] El vacío es plano. Abortando.\n");
        return EXIT_FAILURE;
    }

    printf("[BOOT] Lanzando Pauli Daemon (Disipación)... \n");
    pid_t pauli_pid = fork();
    if (pauli_pid == 0) {
        execl("./pauli_daemon", "pauli_daemon", NULL);
        perror("execl pauli_daemon");
        exit(EXIT_FAILURE);
    }

    sleep(1); // Esperar a que el socket se cree

    printf("[BOOT] Lanzando H7 Server (Conservativo)... \n");
    pid_t h7_pid = fork();
    if (h7_pid == 0) {
        execl("./h7_server", "h7_server", NULL);
        perror("execl h7_server");
        exit(EXIT_FAILURE);
    }

    printf("[BOOT] Qernel en equilibrio metriplético.\n");
    
    while(1) {
        sleep(5);
        
        int status;
        pid_t result = waitpid(pauli_pid, &status, WNOHANG);
        
        if (result != 0) {
            printf("[ALERT] Pauli Daemon (PID %d) ha colapsado o terminado. Reiniciando...\n", pauli_pid);
            unlink("/tmp/qnn_pauli.socket"); // Limpieza de seguridad
            
            pauli_pid = fork();
            if (pauli_pid == 0) {
                execl("./pauli_daemon", "pauli_daemon", NULL);
                exit(EXIT_FAILURE);
            }
            sleep(1); // Estabilización
        }

        pid_t h7_result = waitpid(h7_pid, &status, WNOHANG);
        if (h7_result != 0) {
            printf("[ALERT] H7 Server (PID %d) ha colapsado. Reiniciando...\n", h7_pid);
            h7_pid = fork();
            if (h7_pid == 0) {
                execl("./h7_server", "h7_server", NULL);
                exit(EXIT_FAILURE);
            }
            sleep(1);
        }
    }

    return EXIT_SUCCESS;
}
