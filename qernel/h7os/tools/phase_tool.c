/*
 * phase_tool.c — la fase áurea de src/h7.c en Linux, para compararla con una
 * referencia de precisión arbitraria (test/phase_ref.py).
 *
 *   phase_tool          lista fija: casos borde + 4000 enteros de 64 bits
 *   phase_tool n...     solo esos n
 *
 * Una línea por n:  n  h7_label en hex  h7_golden  rotate_pow
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "h7.h"

static void line(int64_t n) {
    printf("%lld %016llx %.17g %.17g\n", (long long)n, (unsigned long long)h7_label(n),
           h7_golden(n), rotate_pow(n));
}

int main(int argc, char **argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) line((int64_t)strtoull(argv[i], 0, 0));  /* 0x8000000000000000 = INT64_MIN */
        return 0;
    }
    static const int64_t edge[] = {
        0, 1, 2, 7, 8, 14, 15, 1000, 1000000, 1000000000, 2147483647, -1, -7, -15, -1000000000,
        1000000000000LL, 1000000000000000LL, 1000000000000000000LL, 4611686018427387904LL,
        INT64_MAX, INT64_MIN + 1, INT64_MIN,
    };
    for (unsigned i = 0; i < sizeof edge / sizeof edge[0]; i++) line(edge[i]);
    uint64_t x = 0x9E3779B97F4A7C15ull;               /* splitmix64: 4000 enteros de 64 bits */
    for (int i = 0; i < 4000; i++) {
        uint64_t z = (x += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        line((int64_t)(z >> (i % 64)) * ((i & 1) ? -1 : 1));  /* todos los tamaños, ambos signos */
    }
    return 0;
}
