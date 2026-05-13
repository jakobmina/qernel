#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "metriplectic.h"

#define PUERTO 8000
#define TRAJECTORY_SIZE 50

/* Estado Global del Actor AI */
GenerativeActor ai_actor = {0, 0, 0.1, 0.1, 0};
GenerativeActor history[TRAJECTORY_SIZE];
int history_idx = 0;
char last_thought = '?';
int step_count = 0;

void process_thought() {
    /* Mapear la trayectoria a un estado cuántico */
    EstadoCuantico thought_state = {0};
    double dx_total = 0, dy_total = 0, winding = 0;
    
    for(int i=0; i < TRAJECTORY_SIZE - 1; i++) {
        double dx = history[i+1].x - history[i].x;
        double dy = history[i+1].y - history[i].y;
        dx_total += fabs(dx);
        dy_total += fabs(dy);
        /* Calcular el "giro" (winding) */
        winding += (history[i].x * history[i+1].y - history[i+1].x * history[i].y);
    }
    
    /* Inyectar observables de trayectoria en amplitudes */
    thought_state.psi[0] = dx_total / 10.0;
    thought_state.psi[7] = dy_total / 10.0;
    thought_state.psi[1] = fabs(winding) / 2.0;
    thought_state.psi[6] = 0.1; /* Bias de torsión */
    
    /* Decodificar usando el Codec de Torsión */
    char c = decode_char(thought_state);
    if (c >= 32 && c <= 126) last_thought = c;
}

void handle_request(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    
    if (strstr(buffer, "GET /api/actor")) {
        uint8_t mock_screen[256];
        for(int i=0; i<256; i++) mock_screen[i] = rand() % 256;
        TorsionObservables obs = feel_screen(mock_screen, 16, 16);
        
        double On = golden_operator(step_count++ * 0.01);
        ai_actor = update_actor(ai_actor, obs, On);
        
        /* Guardar en historial para "pensar" */
        history[history_idx] = ai_actor;
        history_idx = (history_idx + 1) % TRAJECTORY_SIZE;
        if (history_idx == 0) process_thought();

        char json[1024];
        sprintf(json, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n"
                      "{\"x\": %.4f, \"y\": %.4f, \"energy\": %.4f, \"thought\": \"%c\", \"On\": %.4f}",
                ai_actor.x, ai_actor.y, ai_actor.energy, last_thought, On);
        send(client_socket, json, strlen(json), 0);
        
    } else if (strstr(buffer, "GET /api/feeling")) {
        uint8_t mock_screen[256];
        for(int i=0; i<256; i++) mock_screen[i] = rand() % 256;
        TorsionObservables obs = feel_screen(mock_screen, 16, 16);
        char json[1024];
        sprintf(json, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n"
                      "{\"energy\": %.4f, \"entropy\": %.4f, \"torsion\": %.4f, \"chirality\": %.4f}",
                obs.energy_density, obs.entropy_gradient, obs.spatial_torsion, obs.chirality);
        send(client_socket, json, strlen(json), 0);
    } else {
        char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>H7 Cognitive Actor Active</h1>";
        send(client_socket, response, strlen(response), 0);
    }
    close(client_socket);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PUERTO);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    printf("[MANDATO] H7 Cognitive Server en Puerto %d\n", PUERTO);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        handle_request(new_socket);
    }
    return 0;
}
