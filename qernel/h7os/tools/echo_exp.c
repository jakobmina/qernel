/*
 * echo_exp.c — herramienta de Linux para el experimento del eco, con el mismo
 * código que corre el kernel (src/echo.c, qasm.c, qsv.c).
 *
 *   echo_exp                          la tabla de `q echo`
 *   echo_exp raw                      r libre y r eco de cada modelo, 15 decimales
 *   echo_exp qasm A eco yxx [i]       el circuito de una realización
 *   echo_exp dump A eco yxx           los M circuitos de un modelo, uno tras otro
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "echo.h"

static void emit(const char *line) { puts(line); }

static int usage(void) {
    fprintf(stderr, "uso: echo_exp [raw | qasm|dump A..G libre|eco xxx|yxx [i]]\n");
    return 2;
}

int main(int argc, char **argv) {
    static char buf[4096];
    const char *err;
    if (argc == 1) {
        if (echo_report(-1, emit, &err) < 0) { fprintf(stderr, "echo: %s\n", err); return 1; }
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "raw")) {
        for (int m = 0; m < ECHO_MODELS; m++) {
            double rl, re;
            if (echo_r(m, 0, &rl, &err) < 0 || echo_r(m, 1, &re, &err) < 0) { fprintf(stderr, "echo: %s\n", err); return 1; }
            printf("%c %.15f %.15f\n", echo_model[m].id, rl, re);
        }
        return 0;
    }
    if (argc < 5 || (strcmp(argv[1], "qasm") && strcmp(argv[1], "dump"))) return usage();
    int m = argv[2][1] ? -1 : echo_find(argv[2][0]);
    int eco = !strcmp(argv[3], "eco"), yxx = !strcmp(argv[4], "yxx");
    if (m < 0 || (!eco && strcmp(argv[3], "libre")) || (!yxx && strcmp(argv[4], "xxx"))) return usage();
    int i0 = argc > 5 ? atoi(argv[5]) : 0, i1 = argv[1][0] == 'd' ? echo_model[m].M : i0 + 1;
    for (int i = i0; i < i1; i++) {
        if (echo_qasm(buf, sizeof buf, m, eco, yxx, i) < 0) { fprintf(stderr, "echo: el circuito no cupo\n"); return 1; }
        fputs(buf, stdout);
    }
    return 0;
}
