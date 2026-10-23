/*
 * qasm_sv.c — corre un OpenQASM 2.0 (stdin) en el vector de estado del kernel
 * (src/qasm.c + src/qsv.c) y escribe las amplitudes justo antes de la
 * primera medición, una por línea: «re im» con 17 cifras.
 */
#include <stdio.h>
#include "qasm.h"

int main(void) {
    static char src[1 << 20];
    static QasmRun run;
    size_t n = fread(src, 1, sizeof src - 1, stdin);
    src[n] = '\0';
    run.backend = QASM_SV;
    run.stop_at_measure = 1;
    if (qasm_run(src, &run) < 0) { fprintf(stderr, "qasm: sentencia %d: %s\n", run.stmt, run.err); return 1; }
    for (int i = 0; i < (1 << run.sv.n); i++) printf("%.17g %.17g\n", run.sv.a[i].re, run.sv.a[i].im);
    return 0;
}
