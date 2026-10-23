/*
 * qsim.c — tablero de estabilizadores (ver qsim.h)
 *
 * Reglas de actualización de Aaronson y Gottesman, "Improved simulation of
 * stabilizer circuits", Phys. Rev. A 70, 052328 (2004), sección III.
 */
#include "qsim.h"

static uint32_t rng_state = 0x48370001u;

void q_seed(uint32_t s) { rng_state = s ? s : 0x48370001u; }

uint32_t q_rand(void) {                           /* xorshift32 */
    uint32_t v = rng_state;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    return rng_state = v;
}

#define BIT(m, j) (((m) >> (j)) & 1u)

void tab_init(Tableau *t, int n) {
    t->n = n;
    for (int i = 0; i < 2 * n + 1; i++) { t->x[i] = 0; t->z[i] = 0; t->r[i] = 0; }
    for (int i = 0; i < n; i++) {
        t->x[i] = 1u << i;                         /* desestabilizador X_i */
        t->z[i + n] = 1u << i;                     /* estabilizador Z_i: el estado es |0> */
    }
}

void tab_h(Tableau *t, int a) {
    uint32_t m = 1u << a;
    for (int i = 0; i < 2 * t->n; i++) {
        uint32_t xa = t->x[i] & m, za = t->z[i] & m;
        if (xa && za) t->r[i] ^= 1;
        t->x[i] = (t->x[i] & ~m) | za;
        t->z[i] = (t->z[i] & ~m) | xa;
    }
}

void tab_s(Tableau *t, int a) {
    uint32_t m = 1u << a;
    for (int i = 0; i < 2 * t->n; i++) {
        if ((t->x[i] & m) && (t->z[i] & m)) t->r[i] ^= 1;
        t->z[i] ^= t->x[i] & m;
    }
}

void tab_cx(Tableau *t, int c, int tg) {
    for (int i = 0; i < 2 * t->n; i++) {
        uint32_t xa = BIT(t->x[i], c), za = BIT(t->z[i], c);
        uint32_t xb = BIT(t->x[i], tg), zb = BIT(t->z[i], tg);
        t->r[i] ^= (uint8_t)(xa & zb & (xb ^ za ^ 1u));
        if (xa) t->x[i] ^= 1u << tg;
        if (zb) t->z[i] ^= 1u << c;
    }
}

/* Un Pauli conjugado cambia el signo de las filas con las que anticonmuta. */
void tab_x(Tableau *t, int a) { for (int i = 0; i < 2 * t->n; i++) t->r[i] ^= (uint8_t)BIT(t->z[i], a); }
void tab_z(Tableau *t, int a) { for (int i = 0; i < 2 * t->n; i++) t->r[i] ^= (uint8_t)BIT(t->x[i], a); }
void tab_y(Tableau *t, int a) { for (int i = 0; i < 2 * t->n; i++) t->r[i] ^= (uint8_t)BIT(t->x[i] ^ t->z[i], a); }

/* Exponente de i al multiplicar dos Paulis de un qubit (−1, 0 o 1). */
static int g(uint32_t x1, uint32_t z1, uint32_t x2, uint32_t z2) {
    if (!x1 && !z1) return 0;
    if (x1 && z1)   return (int)z2 - (int)x2;
    if (x1)         return (int)z2 * (2 * (int)x2 - 1);
    return (int)x2 * (1 - 2 * (int)z2);
}

/* fila h <- fila i · fila h, con el signo correcto */
static void rowsum(Tableau *t, int h, int i) {
    int s = 2 * t->r[h] + 2 * t->r[i];
    for (int j = 0; j < t->n; j++)
        s += g(BIT(t->x[i], j), BIT(t->z[i], j), BIT(t->x[h], j), BIT(t->z[h], j));
    t->r[h] = (uint8_t)((s & 3) == 2);           /* s mod 4 es 0 o 2 */
    t->x[h] ^= t->x[i];
    t->z[h] ^= t->z[i];
}

int tab_measure(Tableau *t, int a, int *random) {
    int n = t->n, p = -1;
    uint32_t m = 1u << a;
    for (int i = n; i < 2 * n; i++) if (t->x[i] & m) { p = i; break; }

    if (p >= 0) {                                  /* algún estabilizador anticonmuta con Z_a: al azar */
        for (int i = 0; i < 2 * n; i++)
            if (i != p && (t->x[i] & m)) rowsum(t, i, p);
        t->x[p - n] = t->x[p]; t->z[p - n] = t->z[p]; t->r[p - n] = t->r[p];
        t->x[p] = 0;
        t->z[p] = m;
        t->r[p] = (uint8_t)(q_rand() & 1);
        if (random) *random = 1;
        return t->r[p];
    }

    int s = 2 * n;                                 /* determinista: se reconstruye con la fila auxiliar */
    t->x[s] = 0; t->z[s] = 0; t->r[s] = 0;
    for (int i = 0; i < n; i++)
        if (t->x[i] & m) rowsum(t, s, i + n);
    if (random) *random = 0;
    return t->r[s];
}

void tab_reset(Tableau *t, int a) {
    if (tab_measure(t, a, 0)) tab_x(t, a);
}
