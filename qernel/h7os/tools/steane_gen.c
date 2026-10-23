/*
 * steane_gen.c — herramienta de Linux: imprime el circuito de Steane en
 * OpenQASM 2.0 con el mismo generador que usa el kernel (src/steane.c).
 *
 *   steane_gen                 |0_L>, sin errores
 *   steane_gen + z 5           |+_L>, error Z en la posición 5
 *   steane_gen x 1 x 2         dos errores
 */
#include <stdio.h>
#include <stdlib.h>
#include "steane.h"

int main(int argc, char **argv) {
    static char buf[4096];
    QErr e[STEANE_MAX_ERR];
    int i = 1, ne = 0, plus = 0;
    if (i < argc && (argv[i][0] == '+' || argv[i][0] == '0') && !argv[i][1]) plus = argv[i++][0] == '+';
    while (i + 1 < argc) {
        if (ne == STEANE_MAX_ERR) { fprintf(stderr, "máximo %d errores\n", STEANE_MAX_ERR); return 2; }
        e[ne].pauli = argv[i][0];
        e[ne].pos = atoi(argv[i + 1]);
        if ((e[ne].pauli != 'x' && e[ne].pauli != 'y' && e[ne].pauli != 'z') || e[ne].pos < 1 || e[ne].pos > 7) {
            fprintf(stderr, "uso: steane_gen [0|+] [x|y|z pos]...\n");
            return 2;
        }
        ne++;
        i += 2;
    }
    if (steane_qasm(buf, sizeof buf, plus, e, ne) < 0) return 1;
    fputs(buf, stdout);
    return 0;
}
