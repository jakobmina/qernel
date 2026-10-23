/*
 * qsv.c — vector de estado (ver qsv.h)
 */
#include "qsv.h"
#include "qsim.h"

#define INV_SQRT2 0x1.6a09e667f3bcdp-1        /* 1/√2 redondeado una sola vez */

/* Reducción a |r| ≤ π/4 (Cody–Waite: π/2 en dos partes, la alta con 33 bits,
 * así k·alta es exacto para |k| < 2^20) y polinomios de Taylor hasta r^15 y
 * r^16: el primer término omitido vale < 5e-17. Solo + − ×, como en x87. */
void q_sincos(double x, double *sn, double *cs) {
    static const double PIO2_HI = 0x1.921fb544p+0, PIO2_LO = 0x1.0b4611a626331p-34;
    double t = x * 0x1.45f306dc9c883p-1;                  /* x · 2/π */
    int k = (int)(t < 0 ? t - 0.5 : t + 0.5);
    double r = (x - k * PIO2_HI) - k * PIO2_LO, r2 = r * r;
    double s = r + r * r2 * (-1.0 / 6 + r2 * (1.0 / 120 + r2 * (-1.0 / 5040 + r2 * (1.0 / 362880 +
               r2 * (-1.0 / 39916800 + r2 * (1.0 / 6227020800.0 + r2 * (-1.0 / 1307674368000.0)))))));
    double c = 1 + r2 * (-0.5 + r2 * (1.0 / 24 + r2 * (-1.0 / 720 + r2 * (1.0 / 40320 +
               r2 * (-1.0 / 3628800 + r2 * (1.0 / 479001600 + r2 * (-1.0 / 87178291200.0 +
               r2 * (1.0 / 20922789888000.0))))))));
    switch (k & 3) {
    case 0:  *sn = s;  *cs = c;  break;
    case 1:  *sn = c;  *cs = -s; break;
    case 2:  *sn = -s; *cs = -c; break;
    default: *sn = -c; *cs = s;  break;
    }
}

/* fsqrt con el x87 a 53 bits y sqrtsd en SSE2 redondean igual (IEEE exacto) */
double q_sqrt(double x) {
#if __STDC_HOSTED__
    return __builtin_sqrt(x);
#else
    __asm__ ("fsqrt" : "+t"(x));
    return x;
#endif
}

static Cx cmul(Cx a, Cx b) { Cx r = { a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re }; return r; }
static Cx cadd(Cx a, Cx b) { Cx r = { a.re + b.re, a.im + b.im }; return r; }
static Cx cis(double t) { Cx r; q_sincos(t, &r.im, &r.re); return r; }

void sv_init(QState *s, int n) {
    s->n = n;
    for (int i = 0; i < (1 << n); i++) s->a[i].re = s->a[i].im = 0;
    s->a[0].re = 1;
}

void sv_gate(QState *s, int q, Cx u00, Cx u01, Cx u10, Cx u11) {
    int bit = 1 << q;
    for (int i = 0; i < (1 << s->n); i++) {
        if (i & bit) continue;
        Cx a0 = s->a[i], a1 = s->a[i | bit];
        s->a[i]       = cadd(cmul(u00, a0), cmul(u01, a1));
        s->a[i | bit] = cadd(cmul(u10, a0), cmul(u11, a1));
    }
}

void sv_h(QState *s, int q) {
    int bit = 1 << q;
    for (int i = 0; i < (1 << s->n); i++) {
        if (i & bit) continue;
        Cx a0 = s->a[i], a1 = s->a[i | bit];
        s->a[i].re       = (a0.re + a1.re) * INV_SQRT2; s->a[i].im       = (a0.im + a1.im) * INV_SQRT2;
        s->a[i | bit].re = (a0.re - a1.re) * INV_SQRT2; s->a[i | bit].im = (a0.im - a1.im) * INV_SQRT2;
    }
}

void sv_phase(QState *s, int q, double lambda) {
    Cx e = cis(lambda);
    for (int i = 0; i < (1 << s->n); i++) if (i & (1 << q)) s->a[i] = cmul(e, s->a[i]);
}

void sv_rz(QState *s, int q, double lambda) {
    Cx p = cis(lambda / 2), m = { p.re, -p.im };
    for (int i = 0; i < (1 << s->n); i++) s->a[i] = cmul((i & (1 << q)) ? p : m, s->a[i]);
}

/* mismas matrices que RX/RY de Qiskit (y las que ya usaba qasm.c inline) */
void sv_rx(QState *s, int q, double theta) {
    double sn, cs; q_sincos(theta / 2, &sn, &cs);
    Cx c = { cs, 0 }, m = { 0, -sn };
    sv_gate(s, q, c, m, m, c);
}

void sv_ry(QState *s, int q, double theta) {
    double sn, cs; q_sincos(theta / 2, &sn, &cs);
    Cx c = { cs, 0 }, m = { -sn, 0 }, p = { sn, 0 };
    sv_gate(s, q, c, m, p, c);
}

void sv_cx(QState *s, int c, int t) {
    for (int i = 0; i < (1 << s->n); i++)
        if ((i & (1 << c)) && !(i & (1 << t))) {
            Cx tmp = s->a[i];
            s->a[i] = s->a[i | (1 << t)];
            s->a[i | (1 << t)] = tmp;
        }
}

void sv_cphase(QState *s, int c, int t, double lambda) {
    Cx e = cis(lambda);
    int both = (1 << c) | (1 << t);
    for (int i = 0; i < (1 << s->n); i++) if ((i & both) == both) s->a[i] = cmul(e, s->a[i]);
}

void sv_cz(QState *s, int c, int t) {
    int both = (1 << c) | (1 << t);
    for (int i = 0; i < (1 << s->n); i++)
        if ((i & both) == both) { s->a[i].re = -s->a[i].re; s->a[i].im = -s->a[i].im; }
}

void sv_ccx(QState *s, int c1, int c2, int t) {
    int cbits = (1 << c1) | (1 << c2), tbit = 1 << t;
    for (int i = 0; i < (1 << s->n); i++)
        if ((i & cbits) == cbits && !(i & tbit)) {
            Cx tmp = s->a[i];
            s->a[i] = s->a[i | tbit];
            s->a[i | tbit] = tmp;
        }
}

/* Intercambia los qubits t1,t2 cuando c vale 1: para cada i con c=1 y t1≠t2,
 * voltear ambos bits da el índice j con t1,t2 cambiados entre sí; i<j evita
 * deshacer el canje al volver a encontrar el par en la segunda mitad. */
void sv_cswap(QState *s, int c, int t1, int t2) {
    int cbit = 1 << c, b1 = 1 << t1, b2 = 1 << t2;
    for (int i = 0; i < (1 << s->n); i++) {
        if (!(i & cbit)) continue;
        int v1 = (i & b1) != 0, v2 = (i & b2) != 0;
        if (v1 == v2) continue;
        int j = i ^ b1 ^ b2;
        if (i < j) { Cx tmp = s->a[i]; s->a[i] = s->a[j]; s->a[j] = tmp; }
    }
}

double sv_prob1(const QState *s, int q) {
    double p = 0;
    for (int i = 0; i < (1 << s->n); i++)
        if (i & (1 << q)) p += s->a[i].re * s->a[i].re + s->a[i].im * s->a[i].im;
    return p;
}

double sv_prob_all1(const QState *s, uint32_t mask) {
    double p = 0;
    for (int i = 0; i < (1 << s->n); i++)
        if (((uint32_t)i & mask) == mask) p += s->a[i].re * s->a[i].re + s->a[i].im * s->a[i].im;
    return p;
}

int sv_measure(QState *s, int q) {
    double p1 = sv_prob1(s, q);
    double u = (double)(int)(q_rand() >> 8) * 0x1p-24;
    int out = u < p1;
    double norm = 1 / q_sqrt(out ? p1 : 1 - p1);
    for (int i = 0; i < (1 << s->n); i++) {
        if (((i >> q) & 1) == out) { s->a[i].re *= norm; s->a[i].im *= norm; }
        else s->a[i].re = s->a[i].im = 0;
    }
    return out;
}

double sv_parity(const QState *s, uint32_t mask) {
    double v = 0;
    for (int i = 0; i < (1 << s->n); i++) {
        double p = s->a[i].re * s->a[i].re + s->a[i].im * s->a[i].im;
        uint32_t m = (uint32_t)i & mask;
        int odd = 0;
        while (m) { odd ^= 1; m &= m - 1; }
        v += odd ? -p : p;
    }
    return v;
}
