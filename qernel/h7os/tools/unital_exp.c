/*
 * unital_exp.c — herramienta de Linux para la prueba de unitalidad, con el
 * mismo código que corre el kernel (src/unital.c, qasm.c, qsv.c).
 *
 *   unital_exp                    la tabla de `q unital`
 *   unital_exp raw                t_x t_y t_z w z0 z1 |t| y máx |t| por realización
 *   unital_exp qasm E z [i]       el circuito de una realización (lectura x, y o z)
 *   unital_exp dump E z           los M circuitos de un modelo, uno tras otro
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unital.h"

static void emit(const char *line) { puts(line); }

static int usage(void) {
    fprintf(stderr, "uso: unital_exp [raw | qasm|dump A..F x|y|z [i]]\n");
    return 2;
}

int main(int argc, char **argv) {
    static char buf[8192];
    const char *err;
    if (argc == 1) {
        if (uni_report(-1, emit, &err) < 0) { fprintf(stderr, "unital: %s\n", err); return 1; }
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "raw")) {
        for (int m = 0; m < UNI_MODELS; m++) {
            UniResult r;
            if (uni_measure(m, &r, &err) < 0) { fprintf(stderr, "unital: %s\n", err); return 1; }
            printf("%c %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", uni_model[m].id,
                   r.t[0], r.t[1], r.t[2], r.w, r.z0, r.z1, r.tn, r.tmax);
        }
        return 0;
    }
    if (argc < 4 || (strcmp(argv[1], "qasm") && strcmp(argv[1], "dump"))) return usage();
    int m = argv[2][1] ? -1 : uni_find(argv[2][0]);
    const char *bs = "xyz", *b = argv[3][1] ? 0 : strchr(bs, argv[3][0]);
    if (m < 0 || !b || !*b) return usage();
    int i0 = argc > 4 ? atoi(argv[4]) : 0, i1 = argv[1][0] == 'd' ? uni_model[m].M : i0 + 1;
    for (int i = i0; i < i1; i++) {
        if (uni_qasm(buf, sizeof buf, m, (int)(b - bs), i) < 0) { fprintf(stderr, "unital: el circuito no cupo\n"); return 1; }
        fputs(buf, stdout);
    }
    return 0;
}
