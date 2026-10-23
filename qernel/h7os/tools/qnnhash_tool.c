/*
 * qnnhash_tool.c — herramienta de Linux para qnnhash.c (ver qnnhash.h), con
 * el mismo código que correría el kernel (src/qnnhash.c, qsv.c, h7.c).
 *
 *   qnnhash_tool <n> [iter]       reporte legible (como el __main__ de
 *                                 h7_qnn_hash.py), iter por defecto 3
 *   qnnhash_tool raw <n> [iter]   una línea por paso, para comparar con
 *                                 test/qnnhash_ref.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qnnhash.h"

static void print_step(const QnnStep *st, int idx) {
    printf("--- iteracion %d ---\n", idx + 1);
    printf("  n=%lld (n_z7=%d c_n_z7=%d)\n", (long long)st->n, st->n_z7, st->c_n_z7);
    printf("  cov=%.9f asym=%.9f dist_alt=%.9f\n", st->covariance, st->asymmetry, st->dist_alt);
    printf("  delta_n=%lld -> next_n=%lld\n", (long long)st->delta_n, (long long)st->next_n);
}

int main(int argc, char **argv) {
    int raw = argc > 1 && !strcmp(argv[1], "raw");
    int ai = raw ? 2 : 1;
    if (argc <= ai) {
        fprintf(stderr, "uso: qnnhash_tool [raw] <n> [iteraciones=3]\n");
        return 2;
    }
    int64_t seed = (int64_t)strtoll(argv[ai], NULL, 10);
    int iterations = argc > ai + 1 ? atoi(argv[ai + 1]) : 3;

    QnnHash h;
    qnn_hash(seed, iterations, &h);

    if (raw) {
        printf("%lld %d", (long long)h.seed, h.iterations);
        for (int i = 0; i < h.iterations; i++) {
            const QnnStep *st = &h.step[i];
            printf(" %d %d %.17g %.17g %.17g %.17g %.17g %.17g %lld %lld",
                   st->n_z7, st->c_n_z7, st->o_n, st->i_n, st->particle_class_value,
                   st->covariance, st->asymmetry, st->dist_alt,
                   (long long)st->delta_n, (long long)st->next_n);
        }
        printf(" %lld %.17g %.17g %.17g\n", (long long)h.final_n, h.psi, h.energy, h.torsion);
        return 0;
    }

    printf("semilla n=%lld, %d iteraciones\n", (long long)h.seed, h.iterations);
    for (int i = 0; i < h.iterations; i++) print_step(&h.step[i], i);
    printf("\n[!] convergio en n_final = %lld\n", (long long)h.final_n);
    printf("\n==== METRIPLECTIC QUANTUM HASH ====\n");
    printf("  Campo Psi:        %.8f\n", h.psi);
    printf("  Energia (H):      %.8f\n", h.energy);
    printf("  Torsion Espacial (S): %.8f  (constante, no depende de n)\n", h.torsion);
    return 0;
}
