/*
 * echo.c — experimento del eco (ver echo.h y exp/eco_prerregistro.md)
 *
 * Cada realización i del modelo m tiene su semilla, que depende solo de
 * (m, i): la versión libre, la versión con eco y las dos bases de lectura
 * sortean exactamente el mismo ruido. Los ángulos se escriben como fracciones
 * exactas (−3/8, 12345/33554432) o pi/8, así que cualquier intérprete de
 * OpenQASM reconstruye los mismos doubles.
 */
#include "echo.h"
#include "qasm.h"
#include "qtext.h"

#define ECHO_SEED 0x48370001u

/* predicciones del prerregistro (formas cerradas) */
const EchoModel echo_model[ECHO_MODELS] = {
    { 'A', "campo estático",        1000, 1.0408832349856567e-4, 1.0,                 ECHO_REV },
    { 'B', "patadas Z",             1000, 0.3754132467271021,    0.3754132467271021,  ECHO_IRR },
    { 'C', "estático + patadas",    1000, 3.907613547097745e-5,  0.3754132467271021,  ECHO_MIX },
    { 'D', "telégrafo lento",       1000, 0.0016553206598614912, 0.5605539536296912,  ECHO_MIX },
    { 'E', "telégrafo rápido",      1000, 0.17753273197351435,   0.17753273197351435, ECHO_IRR },
    { 'F', "entorno cuántico fijo", 1,    0.0,                   1.0,                 ECHO_REV },
    { 'G', "entorno que cambia",    1,    0.35355339059327373,   0.35355339059327373, ECHO_IRR },
};

const char *const echo_class_name[3] = { "reversible", "irreversible", "mixto" };

#define P_KICK  335544      /* 0.02 · 2^24: patada Z (B, C) */
#define P_FLIP  838861      /* 0.05 · 2^24: cambio de signo del telégrafo lento (D) */
#define HALF    8388608     /* 2^23: signo del telégrafo rápido (E) */

int echo_find(char id) {
    if (id >= 'a' && id <= 'z') id = (char)(id - 32);
    for (int m = 0; m < ECHO_MODELS; m++) if (echo_model[m].id == id) return m;
    return -1;
}


/* ------------------------------------------------------------------------ */
/* Circuito de una realización                                              */
/* ------------------------------------------------------------------------ */

static const char *const ascii_name[ECHO_MODELS] = {
    "campo estatico", "patadas Z", "estatico + patadas", "telegrafo lento",
    "telegrafo rapido", "entorno cuantico fijo", "entorno que cambia",
};

int echo_qasm(char *buf, int size, int m, int eco, int yxx, int i) {
    Out o = { buf, size, 0, 0 };
    char id = echo_model[m].id;
    int env = id == 'F' || id == 'G';
    int delta[3] = { 0, 0, 0 }, s[3] = { 1, 1, 1 };

    buf[0] = '\0';
    qx_seed(ECHO_SEED, m, i);
    qx_put(&o, "OPENQASM 2.0;\ninclude \"qelib1.inc\";\n");
    qx_put(&o, "// H7-OS - eco de Hahn sobre GHZ de 3 qubits (exp/eco_prerregistro.md)\n");
    qx_put(&o, "// modelo "); { char t[2] = { id, 0 }; qx_put(&o, t); }
    qx_put(&o, " ("); qx_put(&o, ascii_name[m]); qx_put(&o, eco ? ") con eco" : ") sin eco");
    qx_put(&o, yxx ? ", lectura YXX" : ", lectura XXX");
    qx_put(&o, ", realizacion "); qx_puti(&o, i); qx_put(&o, "\n");
    qx_put(&o, "qreg q[3];\n");
    if (env) qx_put(&o, "qreg e[3];\n");
    qx_put(&o, "creg c[3];\n");
    qx_put(&o, "h q[0];\ncx q[0],q[1];\ncx q[0],q[2];\n");
    if (env) qx_put(&o, "h e;\n");

    /* sorteos iniciales: Δ_j = 6u − 3 (A, C) y signo del telégrafo (D) */
    if (id == 'A' || id == 'C')
        for (int j = 0; j < 3; j++) delta[j] = 6 * qx_draw24() - 3 * (1 << 24);   /* Δ_j · 2^24 */
    if (id == 'D')
        for (int j = 0; j < 3; j++) s[j] = qx_draw24() < HALF ? 1 : -1;

    for (int k = 0; k < ECHO_K; k++) {
        qx_put(&o, "// paso "); qx_puti(&o, k + 1); qx_put(&o, "\n");
        for (int j = 0; j < 3; j++) {
            if (id == 'A' || id == 'C') {          /* rz(Δ_j · dt), dt = 1/8 */
                qx_put(&o, "rz("); qx_putfrac(&o, delta[j], 27); qx_put(&o, ") "); qx_qubit(&o, "q", j); qx_put(&o, ";\n");
            }
            if ((id == 'B' || id == 'C') && qx_draw24() < P_KICK) { qx_put(&o, "z "); qx_qubit(&o, "q", j); qx_put(&o, ";\n"); }
            if (id == 'D') {                       /* rz(2 s_j dt) y quizá cambia de signo */
                qx_put(&o, s[j] > 0 ? "rz(1/4) " : "rz(-1/4) "); qx_qubit(&o, "q", j); qx_put(&o, ";\n");
                if (k < ECHO_K - 1 && qx_draw24() < P_FLIP) s[j] = -s[j];
            }
            if (id == 'E') {                       /* rz(±3 dt), signo nuevo cada paso */
                qx_put(&o, qx_draw24() < HALF ? "rz(3/8) " : "rz(-3/8) "); qx_qubit(&o, "q", j); qx_put(&o, ";\n");
            }
            if (env) { qx_put(&o, "cu1(pi/8) "); qx_qubit(&o, "q", j); qx_put(&o, ","); qx_qubit(&o, "e", j); qx_put(&o, ";\n"); }
        }
        if (k == ECHO_K / 2 - 1) {
            if (eco) qx_put(&o, "// eco\nx q;\n");
            if (id == 'G') qx_put(&o, "// el entorno cambia\nh e;\n");
        }
    }
    if (eco) qx_put(&o, "// eco\nx q;\n");
    qx_put(&o, yxx ? "// lectura YXX\nsdg q[0];\nh q;\n" : "// lectura XXX\nh q;\n");
    qx_put(&o, "measure q -> c;\n");
    return o.full ? -1 : o.len;
}

/* ------------------------------------------------------------------------ */
/* Ejecución                                                                */
/* ------------------------------------------------------------------------ */

static char qbuf[4096];
static QasmRun run;

static int parity(int m, int eco, int yxx, int i, double *v, const char **err) {
    if (echo_qasm(qbuf, sizeof qbuf, m, eco, yxx, i) < 0) { *err = "el circuito no cupo"; return -1; }
    run.backend = QASM_SV;
    run.stop_at_measure = 1;
    if (qasm_run(qbuf, &run) < 0) { *err = run.err; return -1; }
    const QReg *q = qasm_qreg(&run, "q");
    if (!run.stopped || !q) { *err = "el circuito no llegó a la medición"; return -1; }
    *v = sv_parity(&run.sv, ((1u << q->size) - 1u) << q->base);
    return 0;
}

int echo_c(int m, int eco, int i, double *re, double *im, const char **err) {
    if (parity(m, eco, 0, i, re, err) < 0) return -1;
    return parity(m, eco, 1, i, im, err);
}

/* e^x para x ≤ 0 sin libm: x = k·ln2 + t con |t| ≤ ln2/2, Taylor hasta t^13
 * (el resto es < 1e-17) y 2^k multiplicando por 1/2. */
static double exp_neg(double x) {
    static const double LN2 = 0x1.62e42fefa39efp-1;
    if (x < -700) return 0;
    int k = (int)(x / LN2 - 0.5);
    double t = x - k * LN2, s = 1, term = 1;
    for (int j = 1; j <= 13; j++) { term *= t / j; s += term; }
    for (; k < 0; k++) s *= 0.5;
    return s;
}

int echo_stat(int m, int eco, EchoStat *st, const char **err) {
    double sr = 0, si = 0, srr = 0, sii = 0, sri = 0, re, im;
    int M = echo_model[m].M;
    for (int i = 0; i < M; i++) {
        if (echo_c(m, eco, i, &re, &im, err) < 0) return -1;
        sr += re;      si += im;
        srr += re * re; sii += im * im; sri += re * im;
    }
    sr /= M;
    si /= M;
    st->r = q_sqrt(sr * sr + si * si);
    st->exact = M == 1;
    st->sd = st->p = 0;
    if (st->exact) return 0;
    double cxx = srr / M - sr * sr, cyy = sii / M - si * si, cxy = sri / M - sr * si;
    double var = st->r > 0 ? (sr * sr * cxx + 2 * sr * si * cxy + si * si * cyy) / (st->r * st->r)
                           : (cxx + cyy) / 2;                 /* varianza de c a lo largo de la media */
    st->sd = q_sqrt((var > 0 ? var : 0) / M);
    st->p = exp_neg(-M * st->r * st->r);
    return 0;
}

int echo_r(int m, int eco, double *r, const char **err) {
    EchoStat s;
    if (echo_stat(m, eco, &s, err) < 0) return -1;
    *r = s.r;
    return 0;
}

int echo_class(double rl, double re) {
    if (re >= 0.9 && rl <= 0.5 * re) return ECHO_REV;
    double d = re - rl;
    if (d <= 0.1 && d >= -0.1) return ECHO_IRR;
    return ECHO_MIX;
}

static int near(double r, double pred, int exact) {
    double d = r - pred;
    if (d < 0) d = -d;
    if (exact) return d <= 1e-9;
    return pred < 0.05 ? r <= 0.15 : d <= 0.15;
}

int echo_within(int m, double rl, double re) {
    const EchoModel *e = &echo_model[m];
    int det = e->M == 1;                           /* F y G no tienen azar */
    return near(rl, e->pred_l, det) && near(re, e->pred_e, det || e->id == 'A');
}

/* ------------------------------------------------------------------------ */
/* Informe                                                                  */
/* ------------------------------------------------------------------------ */

static void row(char *buf, int size, int m, double rl, double re) {
    Out o = { buf, size, 0, 0 };
    char id[3] = { echo_model[m].id, ' ', 0 };
    int c = echo_class(rl, re);
    qx_put(&o, id);
    qx_putpad(&o, echo_model[m].name, 22);
    qx_putfix(&o, rl, 4); qx_put(&o, "   ");
    qx_putfix(&o, re, 4); qx_put(&o, "   ");
    qx_putpad(&o, echo_class_name[c], 13);
    qx_putpad(&o, echo_class_name[echo_model[m].cls], 13);
    qx_put(&o, c == echo_model[m].cls ? "sí    " : "NO    ");
    qx_put(&o, echo_within(m, rl, re) ? "sí" : "NO");
}

/* "0.0339 ± 0.0221    0.317" (24 caracteres) o "0.0000 (exacto)"; pad rellena
 * hasta 24 para alinear la columna siguiente */
static void stat_cell(Out *o, const EchoStat *s, int pad) {
    qx_putfix(o, s->r, 4);
    if (s->exact) { qx_put(o, pad ? " (exacto)         " : " (exacto)"); return; }
    qx_put(o, " ± "); qx_putfix(o, s->sd, 4); qx_put(o, "  ");
    if (s->p < 0.001) qx_put(o, "< 0.001"); else { qx_put(o, "  "); qx_putfix(o, s->p, 3); }
}

int echo_report(int only, void (*emit)(const char *line), const char **err) {
    static char line[160];
    double rl[ECHO_MODELS], re[ECHO_MODELS];
    EchoStat sl[ECHO_MODELS], se[ECHO_MODELS];
    int cls_ok = 0, num_ok = 0, n = 0;
    emit("  modelo                r libre  r eco    clase        predicha     clase núm");
    for (int m = 0; m < ECHO_MODELS; m++) {
        if (only >= 0 && m != only) continue;
        if (echo_stat(m, 0, &sl[m], err) < 0 || echo_stat(m, 1, &se[m], err) < 0) return -1;
        rl[m] = sl[m].r;
        re[m] = se[m].r;
        row(line, sizeof line, m, rl[m], re[m]);
        emit(line);
        cls_ok += echo_class(rl[m], re[m]) == echo_model[m].cls;
        num_ok += echo_within(m, rl[m], re[m]);
        n++;
    }
    Out o = { line, sizeof line, 0, 0 };
    if (only >= 0) {
        qx_put(&o, "fidelidad GHZ (1 + r)/2: libre ");
        qx_putfix(&o, (1 + rl[only]) / 2, 4);
        qx_put(&o, " · eco ");
        qx_putfix(&o, (1 + re[only]) / 2, 4);
        emit(line);
        if (sl[only].exact) return 0;
        /* después del prerregistro: ¿la fidelidad libre > 1/2 es real o es el piso? */
        for (int eco = 0; eco < 2; eco++) {
            const EchoStat *s = eco ? &se[only] : &sl[only];
            Out t = { line, sizeof line, 0, 0 };
            qx_put(&t, eco ? "r eco   " : "r libre ");
            qx_putfix(&t, s->r, 4); qx_put(&t, " ± "); qx_putfix(&t, s->sd, 4);
            if (s->p < 0.001) qx_put(&t, " · p < 0.001: distinto de 0");
            else {
                qx_put(&t, " · p = "); qx_putfix(&t, s->p, 3);
                qx_put(&t, s->p > 0.05 ? ": no se distingue de 0 (piso de muestreo)" : ": distinto de 0");
            }
            emit(line);
        }
        return 0;
    }
    qx_put(&o, "clases como se predijo: "); qx_puti(&o, cls_ok); qx_put(&o, "/"); qx_puti(&o, n);
    qx_put(&o, " · números en tolerancia: "); qx_puti(&o, num_ok); qx_put(&o, "/"); qx_puti(&o, n);
    emit(line);

    /* comprobaciones de implementación del prerregistro */
    const double G = echo_model[6].pred_l;
    int impl = near(re[0], 1, 1) && rl[1] == re[1] && near(rl[5], 0, 1) && near(re[5], 1, 1) &&
               near(rl[6], G, 1) && near(re[6], G, 1);
    emit(impl ? "exactas: A eco = 1 · B libre = eco · F = (0, 1) · G = (0.3536, 0.3536): sí"
              : "exactas: alguna comprobación de implementación FALLA (ver el prerregistro)");

    /* criterio de refutación fijado de antemano */
    int f_like_a = near(rl[5], echo_model[0].pred_l, 0) && near(re[5], echo_model[0].pred_e, 1);
    int g_like_be = echo_class(rl[6], re[6]) == echo_class(rl[1], re[1]) &&
                    echo_class(rl[6], re[6]) == echo_class(rl[4], re[4]);
    emit(f_like_a && g_like_be
         ? "H-ingenua (r separa cuántico de clásico): refutada · F cae con A; G con B y E"
         : "H-ingenua (r separa cuántico de clásico): no refutada por el criterio fijado");
    emit(cls_ok == n
         ? "H-memoria (r + eco separa por memoria del entorno): 7/7 clases como se predijo"
         : "H-memoria (r + eco separa por memoria del entorno): alguna clase no coincide");
    emit("r > 0 equivale a entrelazamiento genuino de los 3 qubits (Gühne–Seevinck);");
    emit("r mide cuánto entrelazamiento queda (fidelidad GHZ (1+r)/2), no por qué se fue");

    /* análisis añadido después del prerregistro: no cambia nada de lo de arriba */
    emit("-- después del prerregistro: ¿r distinto de 0 o solo ruido de muestreo? --");
    emit("   r libre ± σ         p      r eco ± σ           p");
    for (int m = 0; m < ECHO_MODELS; m++) {
        Out t = { line, sizeof line, 0, 0 };
        char id[4] = { echo_model[m].id, ' ', ' ', 0 };
        qx_put(&t, id);
        stat_cell(&t, &sl[m], 1);
        qx_put(&t, "   ");
        stat_cell(&t, &se[m], 0);
        emit(line);
    }
    Out t = { line, sizeof line, 0, 0 };
    qx_put(&t, "p = exp(−M·r²) (Rayleigh) · σ de r por el método delta · M = ");
    qx_puti(&t, echo_model[0].M);
    emit(line);
    emit("con p > 0.05 r no se distingue de 0: ese r no demuestra entrelazamiento genuino");
    return 0;
}
