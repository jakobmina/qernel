/*
 * ghz_exp.c — la lectura GHZ de `q ghz` en Linux, con el mismo código del
 * kernel (src/ghz.c, qasm.c, qsv.c, h7.c).
 *
 *   ghz_exp                 la tabla de `q ghz` (n = 0..13)
 *   ghz_exp n               lo que imprime `q ghz n`
 *   ghz_exp qasm n [enc]    el circuito; con enc, solo la codificación
 *   ghz_exp raw a b         «n lectura P estado_ok» para n = a..b
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ghz.h"

static void emit(const char *line) { puts(line); }

int main(int argc, char **argv) {
    static char buf[1024];
    const char *err;
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "qasm") && strcmp(argv[1], "raw"))) {
        int all = argc == 1;
        if (ghz_report(all ? 0 : (int32_t)atoi(argv[1]), all, emit, &err) < 0) { fprintf(stderr, "ghz: %s\n", err); return 1; }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "qasm")) {
        if (ghz_qasm(buf, sizeof buf, (int32_t)atoi(argv[2]), !(argc > 3 && !strcmp(argv[3], "enc"))) < 0) return 1;
        fputs(buf, stdout);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "raw")) {
        for (int32_t n = atoi(argv[2]); n <= atoi(argv[3]); n++) {
            int code, ok;
            double p;
            if (ghz_read(n, &code, &p, &ok, &err) < 0) { fprintf(stderr, "ghz: %s\n", err); return 1; }
            printf("%d %d %.17g %d\n", (int)n, code, p, ok);
        }
        return 0;
    }
    fprintf(stderr, "uso: ghz_exp [n | qasm n [enc] | raw a b]\n");
    return 2;
}
