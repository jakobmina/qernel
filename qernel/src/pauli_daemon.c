#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "metriplectic.h"

#define SOCKET_PATH "/tmp/qnn_pauli.socket"

/* Estadísticas para el Lagrangiano del Daemon */
typedef struct {
    long total_requests;
    long flattened_count;
} DaemonStats;

DaemonStats stats = {0, 0};

/* Regla 3.1: Lagrangiano del Daemon (Monitor de Presión Informacional) */
LagrangianComponents compute_daemon_lagrangian() {
    LagrangianComponents L;
    /* H: Flujo de información (Energía del proceso) */
    L.H = (double)stats.total_requests;
    /* S: Eficiencia de aplanamiento (Entropía/Disipación) */
    L.S = (double)stats.flattened_count / (stats.total_requests + 1.0);
    return L;
}

int calcularGCD(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

uint8_t aplanarEspacioPauli(int n) {
    stats.total_requests++;
    int factor = calcularGCD(abs(n), 7);
    if (factor == 7) {
        stats.flattened_count++;
        return 0x07;
    } else {
        return 0x01;
    }
}

void daemonizar() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    /* Nota: En un entorno de desarrollo, a veces es útil loguear a un archivo en vez de cerrar todo */
}

int main() {
    /* La daemonización la gestiona el orquestador Boot o Systemd */
    // daemonizar();

    int server_fd, client_fd;
    struct sockaddr_un addr;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) exit(EXIT_FAILURE);

    unlink(SOCKET_PATH);
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) exit(EXIT_FAILURE);

    while (true) {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) continue;

        int n_recibido;
        if (read(client_fd, &n_recibido, sizeof(n_recibido)) > 0) {
            uint8_t resultado_binario = aplanarEspacioPauli(n_recibido);
            write(client_fd, &resultado_binario, sizeof(resultado_binario));
            
            /* Registro Metriplético silencioso (Regla 3.3) */
            /* En un sistema real, esto se enviaría a un log o dashboard */
        }
        close(client_fd);
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}
