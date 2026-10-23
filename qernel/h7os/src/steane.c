/*
 * steane.c — código de Steane [[7,1,3]] en OpenQASM 2.0 (ver steane.h)
 *
 * Todo sale de la matriz de Hamming: la fila k agrupa las posiciones 1..7 que
 * tienen encendido el bit k. Esas tres filas dan
 *   - la codificación: pivote en la posición 2^k, CNOT a las demás de su fila;
 *   - las paridades Z (ven errores X) y las paridades X (ven errores Z);
 *   - la corrección: el síndrome leído en binario es la posición dañada.
 * No usa nada del kernel: compila igual en Linux para generar qasm/steane.qasm.
 */
#include "steane.h"
#include "qasm.h"

typedef struct { char *p; int left; int len; int full; } Out;

static void put(Out *o, const char *s) {
    for (; *s; s++) {
        if (o->left <= 1) { o->full = 1; return; }
        *o->p++ = *s; o->left--; o->len++;
    }
    *o->p = '\0';
}

static void puti(Out *o, int v) {
    char t[12];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    char s[12];
    for (int i = 0; i < n; i++) s[i] = t[n - 1 - i];
    s[n] = '\0';
    put(o, s);
}

static void reg(Out *o, const char *name, int i) { put(o, name); put(o, "["); puti(o, i); put(o, "]"); }

static void cx(Out *o, const char *ra, int a, const char *rb, int b) {
    put(o, "cx "); reg(o, ra, a); put(o, ","); reg(o, rb, b); put(o, ";\n");
}

int steane_qasm(char *buf, int size, int plus, const QErr *e, int ne) {
    Out o = { buf, size, 0, 0 };
    buf[0] = '\0';

    put(&o, "OPENQASM 2.0;\n");
    put(&o, "include \"qelib1.inc\";\n");
    put(&o, "// H7-OS - codigo de Steane [[7,1,3]] sobre Hamming [7,4,3]\n");
    put(&o, "// q[j-1] es la posicion j = 1..7; un sindrome leido en binario es la posicion\n");
    put(&o, "qreg q[7];\nqreg a[6];\ncreg sz[3];\ncreg sx[3];\ncreg m[7];\n");

    put(&o, "// codificar |0_L>: pivotes en las posiciones 1, 2 y 4\n");
    for (int k = 0; k < 3; k++) { put(&o, "h "); reg(&o, "q", (1 << k) - 1); put(&o, ";\n"); }
    for (int k = 0; k < 3; k++)
        for (int pos = 1; pos <= 7; pos++)
            if (((pos >> k) & 1) && pos != (1 << k)) cx(&o, "q", (1 << k) - 1, "q", pos - 1);
    if (plus) put(&o, "// |+_L>: H transversal\nh q;\n");

    put(&o, "barrier q;\n// errores inyectados\n");
    for (int i = 0; i < ne; i++) {
        char g[3] = { e[i].pauli, ' ', '\0' };
        put(&o, g); reg(&o, "q", e[i].pos - 1); put(&o, ";\n");
    }
    put(&o, "barrier q;\n");

    put(&o, "// paridades Z: sz[k] = posiciones con el bit k encendido (ven errores X)\n");
    for (int k = 0; k < 3; k++)
        for (int pos = 1; pos <= 7; pos++)
            if ((pos >> k) & 1) cx(&o, "q", pos - 1, "a", k);
    for (int k = 0; k < 3; k++) { put(&o, "measure "); reg(&o, "a", k); put(&o, " -> "); reg(&o, "sz", k); put(&o, ";\n"); }

    put(&o, "// paridades X: las mismas posiciones (ven errores Z)\n");
    for (int k = 0; k < 3; k++) { put(&o, "h "); reg(&o, "a", 3 + k); put(&o, ";\n"); }
    for (int k = 0; k < 3; k++)
        for (int pos = 1; pos <= 7; pos++)
            if ((pos >> k) & 1) cx(&o, "a", 3 + k, "q", pos - 1);
    for (int k = 0; k < 3; k++) { put(&o, "h "); reg(&o, "a", 3 + k); put(&o, ";\n"); }
    for (int k = 0; k < 3; k++) { put(&o, "measure "); reg(&o, "a", 3 + k); put(&o, " -> "); reg(&o, "sx", k); put(&o, ";\n"); }

    put(&o, "// correccion: el sindrome es la posicion danada\n");
    for (int pos = 1; pos <= 7; pos++) { put(&o, "if(sz=="); puti(&o, pos); put(&o, ") x "); reg(&o, "q", pos - 1); put(&o, ";\n"); }
    for (int pos = 1; pos <= 7; pos++) { put(&o, "if(sx=="); puti(&o, pos); put(&o, ") z "); reg(&o, "q", pos - 1); put(&o, ";\n"); }
    if (plus) put(&o, "h q;\n");

    put(&o, "// lectura: m es una palabra de Hamming; su paridad es el valor logico\n");
    put(&o, "measure q -> m;\n");
    return o.full ? -1 : o.len;
}

static char qasm_buf[4096];

int steane_run(int plus, const QErr *e, int ne, SteaneResult *res, const char **err) {
    static QasmRun run;
    if (steane_qasm(qasm_buf, sizeof qasm_buf, plus, e, ne) < 0) { *err = "el circuito no cupo"; return -1; }
    if (qasm_run(qasm_buf, &run) < 0) { *err = run.err; return -1; }
    res->sz = qasm_creg(&run, "sz");
    res->sx = qasm_creg(&run, "sx");
    res->m  = qasm_creg(&run, "m");
    res->msyn = 0;
    res->logical = 0;
    for (int k = 0; k < 7; k++)
        if ((res->m >> k) & 1) { res->msyn ^= k + 1; res->logical ^= 1; }
    res->random = run.random_meas;
    res->ops = run.ops;
    return 0;
}
