/*
 * unital.c — prueba de unitalidad (ver unital.h y exp/unital_prerregistro.md)
 */
#include "unital.h"
#include "qasm.h"
#include "qtext.h"

#define UNI_SEED  0x55A10001u
#define P_XKICK   1228483        /* sin²(π/8)/2 · 2^24: patada X (A) */
#define HALF      8388608        /* 2^23: centro de los ángulos de B */
#define EXACT     1e-9

/* predicciones del prerregistro (formas cerradas) */
#define C16 0.28173806968950743                  /* cos^16(π/8) */
#define S16 0.5105268265463945                   /* (2 sin ½)^16 */
const UniModel uni_model[UNI_MODELS] = {
    { 'A', "patadas X",        "clásico",  1000, C16,  -C16, 0,       UNI_UNITAL },
    { 'B', "campo 3D al azar", "clásico",  1000, S16,  -S16, 0,       UNI_UNITAL },
    { 'C', "desfase cuántico", "cuántico", 1,    1,    -1,   0,       UNI_UNITAL },
    { 'D', "entorno caliente", "cuántico", 256,  C16,  -C16, 0,       UNI_UNITAL },
    { 'E', "entorno frío",     "cuántico", 1,    1, 1 - 2 * C16, 1 - C16, UNI_NONUNITAL },
    { 'F', "demonio que mide", "registro", 1,    1, 1 - 2 * C16, 1 - C16, UNI_NONUNITAL },
};

const char *const uni_class_name[2] = { "unital", "no unital" };

static const char *const ascii_name[UNI_MODELS] = {
    "patadas X", "campo 3D al azar", "desfase cuantico", "entorno caliente", "entorno frio",
    "demonio que mide y corrige",
};

int uni_find(char id) {
    if (id >= 'a' && id <= 'z') id = (char)(id - 32);
    for (int m = 0; m < UNI_MODELS; m++) if (uni_model[m].id == id) return m;
    return -1;
}

/* ------------------------------------------------------------------------ */
/* Circuito de una realización                                              */
/* ------------------------------------------------------------------------ */

static void line2(Out *o, const char *g, const char *ra, int a, const char *rb, int b) {
    qx_put(o, g); qx_put(o, " "); qx_qubit(o, ra, a); qx_put(o, ","); qx_qubit(o, rb, b); qx_put(o, ";\n");
}

static void line1(Out *o, const char *g, const char *reg, int j) {
    qx_put(o, g); qx_put(o, " "); qx_qubit(o, reg, j); qx_put(o, ";\n");
}

static void angle(Out *o, const char *g, int num, int e) {        /* g(num/2^e) q[0]; */
    qx_put(o, g); qx_put(o, "("); qx_putfrac(o, num, e); qx_put(o, ") q[0];\n");
}

int uni_qasm(char *buf, int size, int m, int basis, int i) {
    Out o = { buf, size, 0, 0 };
    char id = uni_model[m].id;
    const char *env = id == 'F' ? "m" : "e";
    int nenv = id == 'C' ? 1 : (id >= 'D' ? UNI_K : 0);

    buf[0] = '\0';
    qx_seed(UNI_SEED, m, i);
    qx_put(&o, "OPENQASM 2.0;\ninclude \"qelib1.inc\";\n");
    qx_put(&o, "// H7-OS - prueba de unitalidad (exp/unital_prerregistro.md)\n");
    qx_put(&o, "// modelo "); { char t[2] = { id, 0 }; qx_put(&o, t); }
    qx_put(&o, " ("); qx_put(&o, ascii_name[m]); qx_put(&o, "), lectura ");
    qx_put(&o, basis == 0 ? "X" : basis == 1 ? "Y" : "Z");
    qx_put(&o, ", realizacion "); qx_puti(&o, i); qx_put(&o, "\n");
    qx_put(&o, "qreg q[1];\nqreg r[1];\n");
    if (nenv) { qx_put(&o, "qreg "); qx_put(&o, env); qx_put(&o, "["); qx_puti(&o, nenv); qx_put(&o, "];\n"); }
    qx_put(&o, "creg c[2];\n");
    qx_put(&o, "// q empieza en I/2: par de Bell con la referencia r\nh q[0];\ncx q[0],r[0];\n");
    if (id == 'C') qx_put(&o, "h e[0];\n");
    if (id == 'D')                                     /* configuración i del entorno caliente */
        for (int k = 0; k < UNI_K; k++) if ((i >> k) & 1) line1(&o, "x", "e", k);

    for (int k = 0; k < UNI_K; k++) {
        qx_put(&o, "// paso "); qx_puti(&o, k + 1); qx_put(&o, "\n");
        switch (id) {
        case 'A':
            if (qx_draw24() < P_XKICK) qx_put(&o, "x q[0];\n");
            break;
        case 'B':                                      /* ángulos (u − 2^23)/2^24 ∈ [−1/2, 1/2) */
            angle(&o, "rx", qx_draw24() - HALF, 24);
            angle(&o, "ry", qx_draw24() - HALF, 24);
            angle(&o, "rz", qx_draw24() - HALF, 24);
            break;
        case 'C':
            qx_put(&o, "cu1(pi/8) q[0],e[0];\n");
            break;
        case 'D': case 'E':                            /* intercambio parcial exp(−iπ/8 (XX+YY)/2) */
            line2(&o, "cx", "q", 0, "e", k);
            qx_put(&o, "h q[0];\nrz(pi/8) q[0];\n");
            line2(&o, "cx", "e", k, "q", 0);
            qx_put(&o, "rz(-pi/8) q[0];\n");
            line2(&o, "cx", "e", k, "q", 0);
            qx_put(&o, "h q[0];\n");
            line2(&o, "cx", "q", 0, "e", k);
            break;
        case 'F':                                      /* medición débil cry(π/4) y corrección */
            line1(&o, "ry(pi/8)", "m", k);
            line2(&o, "cx", "q", 0, "m", k);
            line1(&o, "ry(-pi/8)", "m", k);
            line2(&o, "cx", "q", 0, "m", k);
            line2(&o, "cx", "m", k, "q", 0);
            break;
        }
    }
    qx_put(&o, basis == 0 ? "// lectura X\nh q[0];\n" : basis == 1 ? "// lectura Y\nsdg q[0];\nh q[0];\n" : "// lectura Z\n");
    qx_put(&o, "measure q[0] -> c[0];\nmeasure r[0] -> c[1];\n");
    return o.full ? -1 : o.len;
}

/* ------------------------------------------------------------------------ */
/* Ejecución                                                                */
/* ------------------------------------------------------------------------ */

static char qbuf[4096];
static QasmRun run;

static double fabs_(double x) { return x < 0 ? -x : x; }

int uni_measure(int m, UniResult *res, const char **err) {
    int M = uni_model[m].M;
    double sum[3] = { 0, 0, 0 }, sw = 0;
    res->tmax = 0;
    for (int i = 0; i < M; i++) {
        double ti[3], wi = 0;
        for (int b = 0; b < 3; b++) {
            if (uni_qasm(qbuf, sizeof qbuf, m, b, i) < 0) { *err = "el circuito no cupo"; return -1; }
            run.backend = QASM_SV;
            run.stop_at_measure = 1;
            if (qasm_run(qbuf, &run) < 0) { *err = run.err; return -1; }
            const QReg *q = qasm_qreg(&run, "q"), *r = qasm_qreg(&run, "r");
            if (!run.stopped || !q || !r) { *err = "el circuito no llegó a la medición"; return -1; }
            uint32_t mq = 1u << q->base, mr = 1u << r->base;
            ti[b] = sv_parity(&run.sv, mq);
            if (b == 2) wi = sv_parity(&run.sv, mq | mr);
        }
        double n = q_sqrt(ti[0] * ti[0] + ti[1] * ti[1] + ti[2] * ti[2]);
        if (n > res->tmax) res->tmax = n;
        for (int b = 0; b < 3; b++) sum[b] += ti[b];
        sw += wi;
    }
    for (int b = 0; b < 3; b++) res->t[b] = sum[b] / M;
    res->w = sw / M;
    res->z0 = res->t[2] + res->w;
    res->z1 = res->t[2] - res->w;
    res->tn = q_sqrt(res->t[0] * res->t[0] + res->t[1] * res->t[1] + res->t[2] * res->t[2]);
    return 0;
}

int uni_class(const UniResult *res) { return res->tn > EXACT ? UNI_NONUNITAL : UNI_UNITAL; }

static int near(double v, double pred, double tol) { return fabs_(v - pred) <= tol; }

int uni_within(int m, const UniResult *res) {
    const UniModel *u = &uni_model[m];
    double tol = (u->id == 'A' || u->id == 'B') ? 0.15 : EXACT;
    return near(res->z0, u->z0, tol) && near(res->z1, u->z1, tol) && near(res->tn, u->t, EXACT);
}

/* ------------------------------------------------------------------------ */
/* Informe                                                                  */
/* ------------------------------------------------------------------------ */

static void row(char *buf, int size, int m, const UniResult *res) {
    Out o = { buf, size, 0, 0 };
    char id[3] = { uni_model[m].id, ' ', 0 };
    int c = uni_class(res);
    qx_put(&o, id);
    qx_putpad(&o, uni_model[m].name, 18);
    qx_putpad(&o, uni_model[m].kind, 10);
    qx_putsfix(&o, res->z0, 4); qx_put(&o, "  ");
    qx_putsfix(&o, res->z1, 4); qx_put(&o, "  ");
    qx_putsfix(&o, res->tn, 4); qx_put(&o, "   ");
    qx_putpad(&o, uni_class_name[c], 11);
    qx_putpad(&o, c == uni_model[m].cls ? "sí" : "NO", 7);
    qx_put(&o, uni_within(m, res) ? "sí" : "NO");
}

int uni_report(int only, void (*emit)(const char *line), const char **err) {
    static char line[160];
    UniResult r[UNI_MODELS];
    int cls_ok = 0, num_ok = 0, n = 0;
    emit("  modelo            tipo       z de 0   z de 1   |t|     clase      pred   núm");
    for (int m = 0; m < UNI_MODELS; m++) {
        if (only >= 0 && m != only) continue;
        if (uni_measure(m, &r[m], err) < 0) return -1;
        row(line, sizeof line, m, &r[m]);
        emit(line);
        cls_ok += uni_class(&r[m]) == uni_model[m].cls;
        num_ok += uni_within(m, &r[m]);
        n++;
    }
    Out o = { line, sizeof line, 0, 0 };
    if (only >= 0) {
        const UniResult *u = &r[only];
        qx_put(&o, "t = (");
        for (int b = 0; b < 3; b++) { qx_putsfix(&o, u->t[b], 4); qx_put(&o, b < 2 ? "," : ")"); }
        qx_put(&o, " · desde I/2 sale p(0) = ");
        qx_putfix(&o, (1 + u->t[2]) / 2, 4);
        emit(line);
        Out o2 = { line, sizeof line, 0, 0 };
        qx_put(&o2, "máx |t| de una sola realización: ");
        qx_putfix(&o2, u->tmax, 4);
        emit(line);
        return 0;
    }
    qx_put(&o, "clases como se predijo: "); qx_puti(&o, cls_ok); qx_put(&o, "/"); qx_puti(&o, n);
    qx_put(&o, " · números en tolerancia: "); qx_puti(&o, num_ok); qx_put(&o, "/"); qx_puti(&o, n);
    emit(line);

    /* comprobaciones de implementación del prerregistro */
    int txy = 1;
    for (int m = 0; m < UNI_MODELS; m++) txy &= fabs_(r[m].t[0]) <= EXACT && fabs_(r[m].t[1]) <= EXACT;
    int each = r[0].tmax <= EXACT && r[1].tmax <= EXACT && r[2].tmax <= EXACT;
    int ef = near(r[4].z0, r[5].z0, EXACT) && near(r[4].z1, r[5].z1, EXACT) && near(r[4].tn, r[5].tn, EXACT) &&
             near(r[4].z0, 1, EXACT);
    emit(txy && each && ef ? "exactas: t_x = t_y = 0 · |t| = 0 en cada realización de A, B, C · E = F: sí"
                           : "exactas: alguna comprobación de implementación FALLA (ver el prerregistro)");

    /* criterio de refutación fijado de antemano */
    int refuted = uni_class(&r[3]) == UNI_UNITAL && uni_class(&r[5]) == UNI_NONUNITAL &&
                  near(r[5].tn, r[4].tn, EXACT);
    emit(refuted ? "H-ingenua-2 (no unital = entorno cuántico): refutada · D es unital y F = E"
                 : "H-ingenua-2 (no unital = entorno cuántico): no refutada por el criterio fijado");
    emit(cls_ok == n
         ? "H-sumidero (baja entropía + acoplamiento que mueve poblaciones): 6/6"
         : "H-sumidero (baja entropía + acoplamiento que mueve poblaciones): falla");
    emit("con |t| > 0, ningún campo clásico (mezcla de unitarios) explica el canal");
    return 0;
}
