/*
 * ghz.c — n mod 14 en la base GHZ (ver ghz.h)
 */
#include "ghz.h"
#include "h7.h"
#include "qasm.h"
#include "qtext.h"

static int mod14(int32_t n) { int m = (int)(n % 14); return m < 0 ? m + 14 : m; }

int ghz_qasm(char *buf, int size, int32_t n, int decode) {
    Out o = { buf, size, 0, 0 };
    int r = n_z7(n), par = (int)(n & 1), rep = r < 4 ? r : 7 - r;
    buf[0] = '\0';
    qx_put(&o, "OPENQASM 2.0;\ninclude \"qelib1.inc\";\n");
    qx_put(&o, "// H7-OS - n mod 14 en la base GHZ: (|r> + (-1)^n |7-r>)/sqrt2\n");
    qx_put(&o, "// n = "); qx_puti(&o, n);
    qx_put(&o, ", r = "); qx_puti(&o, r);
    qx_put(&o, par ? ", paridad impar (-)\n" : ", paridad par (+)\n");
    qx_put(&o, "qreg q[3];\ncreg c[3];\n");
    qx_put(&o, "// q[2] = paridad, q[1] q[0] = bits bajos del miembro del par con q[2] = 0\n");
    if (par)     qx_put(&o, "x q[2];\n");
    if (rep & 2) qx_put(&o, "x q[1];\n");
    if (rep & 1) qx_put(&o, "x q[0];\n");
    qx_put(&o, "// codificar\nh q[2];\ncx q[2],q[1];\ncx q[2],q[0];\n");
    if (decode) qx_put(&o, "// leer en la base GHZ (el inverso)\ncx q[2],q[0];\ncx q[2],q[1];\nh q[2];\n");
    qx_put(&o, "measure q -> c;\n");
    return o.full ? -1 : o.len;
}

static char qbuf[1024];
static QasmRun run;

static int amps(int32_t n, int decode, const char **err) {
    if (ghz_qasm(qbuf, sizeof qbuf, n, decode) < 0) { *err = "el circuito no cupo"; return -1; }
    run.backend = QASM_SV;
    run.stop_at_measure = 1;
    if (qasm_run(qbuf, &run) < 0) { *err = run.err; return -1; }
    if (!run.stopped) { *err = "el circuito no llegó a la medición"; return -1; }
    return 0;
}

static double prob(int i) { return run.sv.a[i].re * run.sv.a[i].re + run.sv.a[i].im * run.sv.a[i].im; }

int ghz_read(int32_t n, int *code, double *p, int *state_ok, const char **err) {
    int r = n_z7(n), rb = 7 - r;
    double s = (n & 1) ? -1 : 1;
    /* 1. el estado codificado es la columna H: a[r̄] = (−1)^n a[r], |a|² = 1/2 */
    if (amps(n, 0, err) < 0) return -1;
    Cx a = run.sv.a[r], b = run.sv.a[rb];
    double d1 = b.re - s * a.re, d2 = b.im - s * a.im;
    *state_ok = d1 * d1 + d2 * d2 < 1e-24 && prob(r) > 0.5 - 1e-12 && prob(r) < 0.5 + 1e-12;
    /* 2. la lectura en la base GHZ */
    if (amps(n, 1, err) < 0) return -1;
    int best = 0;
    for (int i = 1; i < 8; i++) if (prob(i) > prob(best)) best = i;
    *code = best;
    *p = prob(best);
    return 0;
}

int ghz_mod14(int code, int flag) {
    int s = code >> 2, rep = code & 3, r = flag ? 7 - rep : rep;
    return (r & 1) == s ? r : r + 7;                 /* el k en 0..13 con k ≡ r (7) y k ≡ s (2) */
}

static void bits3(Out *o, int v) {
    char t[4] = { (char)('0' + ((v >> 2) & 1)), (char)('0' + ((v >> 1) & 1)), (char)('0' + (v & 1)), 0 };
    qx_put(o, t);
}

static void state(Out *o, int r, int odd) {
    qx_put(o, "|"); bits3(o, r); qx_put(o, odd ? ">−|" : ">+|"); bits3(o, 7 - r); qx_put(o, ">");
}

int ghz_report(int32_t n, int all, void (*emit)(const char *line), const char **err) {
    static char line[160];
    int code, ok;
    double p;
    if (!all) {
        int r = n_z7(n), flag = r >= 4;
        if (ghz_read(n, &code, &p, &ok, err) < 0) return -1;
        Out o = { line, sizeof line, 0, 0 };
        qx_put(&o, "n="); qx_puti(&o, n); qx_put(&o, " · r "); qx_puti(&o, r); qx_put(&o, " (");
        bits3(&o, r); qx_put(&o, ") · 7−r = "); qx_puti(&o, 7 - r); qx_put(&o, " (");
        bits3(&o, 7 - r); qx_put(&o, (n & 1) ? ") · paridad − (q)" : ") · paridad + (p)");
        emit(line);
        Out e = { line, sizeof line, 0, 0 };
        qx_put(&e, "estado     ("); state(&e, r, (int)(n & 1)); qx_put(&e, ")/√2");
        qx_put(&e, ok ? " · comprobado en el vector de estado" : " · NO coincide con el vector de estado");
        emit(line);
        Out l = { line, sizeof line, 0, 0 };
        qx_put(&l, "lectura    "); bits3(&l, code); qx_put(&l, " (P = "); qx_putfix(&l, p, 3);
        qx_put(&l, ") · paridad "); qx_put(&l, (code >> 2) ? "−" : "+");
        qx_put(&l, " · par {"); bits3(&l, code & 3); qx_put(&l, ", "); bits3(&l, 7 - (code & 3)); qx_put(&l, "}");
        emit(line);
        Out m = { line, sizeof line, 0, 0 };
        int k = ghz_mod14(code, flag);
        qx_put(&m, "bit r ≥ 4  "); qx_puti(&m, flag); qx_put(&m, " → n mod 14 = "); qx_puti(&m, k);
        qx_put(&m, k == mod14(n) ? " (correcto)" : " (NO coincide)");
        emit(line);
        return 0;
    }
    int same_state = 0, det = 0, mirror = 0, back = 0, seen = 0, codes[14];
    emit(" n  r  paridad  estado de 3 qubits  lectura  P      r≥4  n mod 14");
    for (int k = 0; k < 14; k++) {
        if (ghz_read(k, &code, &p, &ok, err) < 0) return -1;
        int r = n_z7(k), flag = r >= 4, got = ghz_mod14(code, flag);
        codes[k] = code;
        same_state += ok;
        det += p > 1 - 1e-12;
        back += got == k;
        seen |= 1 << code;
        Out o = { line, sizeof line, 0, 0 };
        if (k < 10) qx_put(&o, " ");
        qx_puti(&o, k); qx_put(&o, "  "); qx_puti(&o, r);
        qx_put(&o, (k & 1) ? "  −        " : "  +        ");
        state(&o, r, k & 1); qx_put(&o, "         ");
        bits3(&o, code); qx_put(&o, "      "); qx_putfix(&o, p, 3); qx_put(&o, "  ");
        qx_puti(&o, flag); qx_put(&o, "    "); qx_puti(&o, got);
        emit(line);
    }
    for (int k = 0; k < 14; k++) mirror += codes[k] == codes[(14 - k) % 14];
    int distinct = 0;
    for (int c = 0; c < 8; c++) distinct += (seen >> c) & 1;
    Out o = { line, sizeof line, 0, 0 };
    qx_put(&o, "estado = (|r> + (−1)^n |7−r>)/√2 en "); qx_puti(&o, same_state);
    qx_put(&o, "/14 · lectura determinista (P = 1) en "); qx_puti(&o, det); qx_put(&o, "/14");
    emit(line);
    Out t = { line, sizeof line, 0, 0 };
    qx_put(&t, "lectura sola: "); qx_puti(&t, distinct);
    qx_put(&t, " resultados distintos · n y 14 − n comparten lectura: "); qx_puti(&t, mirror); qx_put(&t, "/14");
    emit(line);
    Out u = { line, sizeof line, 0, 0 };
    qx_put(&u, "con el bit r ≥ 4: n mod 14 recuperado en "); qx_puti(&u, back); qx_put(&u, "/14");
    emit(line);
    return 0;
}
