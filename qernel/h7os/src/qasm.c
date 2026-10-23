/*
 * qasm.c — intérprete de OpenQASM 2.0 (subconjunto, ver qasm.h)
 *
 * Dos pasadas: la primera reúne qreg/creg para saber cuántos qubits hay; la
 * segunda ejecuta. Cada sentencia termina en ';'.
 */
#include "qasm.h"

#define STMT_MAX 192
#define PI 0x1.921fb54442d18p+1
#define INV_SQRT2 0x1.6a09e667f3bcdp-1

static int is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_ident(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_'; }

static const char *skip(const char *p) { while (is_space(*p)) p++; return p; }

static int word(const char **pp, char *out, int max) {
    const char *p = skip(*pp);
    int n = 0;
    while (is_ident(*p) && n < max - 1) out[n++] = *p++;
    out[n] = '\0';
    *pp = p;
    return n;
}

static int number(const char **pp, int *out) {
    const char *p = skip(*pp);
    int v = 0, any = 0;
    while (is_digit(*p)) { v = v * 10 + (*p++ - '0'); any = 1; }
    *pp = p;
    *out = v;
    return any;
}

static int eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

static int starts(const char *s, const char *w) {
    while (*w) if (*s++ != *w++) return 0;
    return !is_ident(*s);
}

static QReg *lookup(QReg *regs, int n, const char *name) {
    for (int i = 0; i < n; i++) if (eq(regs[i].name, name)) return &regs[i];
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Ángulos: expr := term {± term} · term := factor {× o ÷ factor}            */
/*          factor := −factor | ( expr ) | pi | número                       */
/* Los enteros (hasta 18 cifras) son exactos, así que «-3/8» o «pi/8» dan el  */
/* mismo double que en cualquier otro intérprete.                             */
/* ------------------------------------------------------------------------ */

static double expr(QasmRun *r, const char **pp);

static double literal(QasmRun *r, const char **pp) {
    const char *p = *pp;
    int64_t m = 0;
    int digits = 0, scale = 0, any = 0;
    for (; is_digit(*p); p++, any = 1)
        if (digits < 18) { m = m * 10 + (*p - '0'); if (m) digits++; } else scale++;
    if (*p == '.')
        for (p++; is_digit(*p); p++, any = 1)
            if (digits < 18) { m = m * 10 + (*p - '0'); if (m) digits++; scale--; }
    if (!any) { r->err = "ángulo ilegible"; return 0; }
    if (*p == 'e' || *p == 'E') {
        int neg = 0, e = 0;
        p++;
        if (*p == '+' || *p == '-') neg = *p++ == '-';
        if (!is_digit(*p)) { r->err = "exponente ilegible"; return 0; }
        while (is_digit(*p)) { if (e < 1000) e = e * 10 + (*p - '0'); p++; }
        scale += neg ? -e : e;
    }
    *pp = p;
    double v = (double)m, ten = 1;
    for (int i = 0; i < (scale < 0 ? -scale : scale) && i < 400; i++) ten *= 10;
    return scale < 0 ? v / ten : v * ten;
}

static double factor(QasmRun *r, const char **pp) {
    const char *p = skip(*pp);
    double v;
    if (*p == '-') { *pp = p + 1; return -factor(r, pp); }
    if (*p == '+') { *pp = p + 1; return factor(r, pp); }
    if (*p == '(') {
        *pp = p + 1;
        v = expr(r, pp);
        p = skip(*pp);
        if (*p != ')') { if (!r->err) r->err = "falta ) en el ángulo"; return 0; }
        *pp = p + 1;
        return v;
    }
    if (p[0] == 'p' && p[1] == 'i' && !is_ident(p[2])) { *pp = p + 2; return PI; }
    *pp = p;
    return literal(r, pp);
}

static double term(QasmRun *r, const char **pp) {
    double v = factor(r, pp);
    for (;;) {
        const char *p = skip(*pp);
        if (*p == '*') { *pp = p + 1; v *= factor(r, pp); }
        else if (*p == '/') { *pp = p + 1; v /= factor(r, pp); }
        else return v;
    }
}

static double expr(QasmRun *r, const char **pp) {
    double v = term(r, pp);
    for (;;) {
        const char *p = skip(*pp);
        if (*p == '+') { *pp = p + 1; v += term(r, pp); }
        else if (*p == '-') { *pp = p + 1; v -= term(r, pp); }
        else return v;
    }
}

/* ------------------------------------------------------------------------ */

/* nombre[i] o nombre: devuelve el registro y el índice (-1 = todo el registro) */
static QReg *operand(QasmRun *r, const char **pp, int quantum, int *idx) {
    char name[12];
    if (!word(pp, name, sizeof name)) { r->err = "falta un operando"; return 0; }
    QReg *g = quantum ? lookup(r->q, r->nq, name) : lookup(r->c, r->nc, name);
    if (!g) { r->err = quantum ? "qreg desconocido" : "creg desconocido"; return 0; }
    const char *p = skip(*pp);
    *idx = -1;
    if (*p == '[') {
        p++;
        if (!number(&p, idx)) { r->err = "índice inválido"; return 0; }
        p = skip(p);
        if (*p != ']') { r->err = "falta ]"; return 0; }
        p++;
        if (*idx >= g->size) { r->err = "índice fuera del registro"; return 0; }
    }
    *pp = p;
    return g;
}

static void declare(QasmRun *r, const char *s, int quantum) {
    char name[12];
    int size;
    const char *p = s;
    if (!word(&p, name, sizeof name)) { r->err = "declaración sin nombre"; return; }
    p = skip(p);
    if (*p++ != '[' || !number(&p, &size) || *skip(p) != ']' || size < 1) { r->err = "declaración inválida"; return; }
    if (quantum) {
        if (r->nq == QASM_MAXREG || r->nqubits + size > QMAX) { r->err = "demasiados qubits (máx 32)"; return; }
        QReg *g = &r->q[r->nq++];
        for (int i = 0; i < 12; i++) g->name[i] = name[i];
        g->base = r->nqubits; g->size = size; r->nqubits += size;
    } else {
        if (r->nc == QASM_MAXREG || r->ncbits + size > 32) { r->err = "demasiados bits clásicos (máx 32)"; return; }
        QReg *g = &r->c[r->nc++];
        for (int i = 0; i < 12; i++) g->name[i] = name[i];
        g->base = r->ncbits; g->size = size; r->ncbits += size;
    }
}

static const Cx C0 = { 0, 0 }, C1 = { 1, 0 }, CI = { 0, 1 }, CM1 = { -1, 0 }, CMI = { 0, -1 };

static void gate1_tab(QasmRun *r, const char *g, int q) {
    Tableau *t = &r->t;
    if      (eq(g, "h"))   tab_h(t, q);
    else if (eq(g, "s"))   tab_s(t, q);
    else if (eq(g, "sdg")) { tab_s(t, q); tab_s(t, q); tab_s(t, q); }
    else if (eq(g, "x"))   tab_x(t, q);
    else if (eq(g, "y"))   tab_y(t, q);
    else if (eq(g, "z"))   tab_z(t, q);
    else if (eq(g, "id"))  { }
    else if (eq(g, "reset")) tab_reset(t, q);
    else { r->err = "el tablero solo hace Clifford (h s sdg x y z id cx cz); para ángulos usa el vector de estado"; return; }
    r->ops++;
}

static void gate1_sv(QasmRun *r, const char *g, int q, const double *a, int na) {
    QState *s = &r->sv;
    int want = 0;
    if      (eq(g, "h"))   sv_h(s, q);
    else if (eq(g, "x"))   sv_gate(s, q, C0, C1, C1, C0);
    else if (eq(g, "y"))   sv_gate(s, q, C0, CMI, CI, C0);
    else if (eq(g, "z"))   sv_gate(s, q, C1, C0, C0, CM1);
    else if (eq(g, "s"))   sv_gate(s, q, C1, C0, C0, CI);
    else if (eq(g, "sdg")) sv_gate(s, q, C1, C0, C0, CMI);
    else if (eq(g, "t") || eq(g, "tdg")) {
        Cx e = { INV_SQRT2, g[1] ? -INV_SQRT2 : INV_SQRT2 };
        sv_gate(s, q, C1, C0, C0, e);
    }
    else if (eq(g, "id"))  { }
    else if (eq(g, "reset")) { if (sv_measure(s, q)) sv_gate(s, q, C0, C1, C1, C0); }
    else if (eq(g, "rz"))  { want = 1; if (na == 1) sv_rz(s, q, a[0]); }
    else if (eq(g, "u1") || eq(g, "p")) { want = 1; if (na == 1) sv_phase(s, q, a[0]); }
    else if (eq(g, "rx") || eq(g, "ry")) {
        want = 1;
        if (na == 1) {
            double sn, cs;
            q_sincos(a[0] / 2, &sn, &cs);
            Cx c = { cs, 0 };
            if (g[1] == 'x') { Cx m = { 0, -sn }; sv_gate(s, q, c, m, m, c); }
            else { Cx m = { -sn, 0 }, p = { sn, 0 }; sv_gate(s, q, c, m, p, c); }
        }
    }
    else { r->err = "puerta no soportada"; return; }
    if (na != want) { r->err = want ? "a esta puerta le falta su ángulo" : "esta puerta no lleva ángulo"; return; }
    r->ops++;
}

static void measure_one(QasmRun *r, int q, int c) {
    int rnd, v;
    if (r->backend == QASM_SV) {
        double p1 = sv_prob1(&r->sv, q);
        rnd = p1 > 1e-12 && p1 < 1 - 1e-12;
        v = sv_measure(&r->sv, q);
    } else v = tab_measure(&r->t, q, &rnd);
    r->random_meas += rnd;
    if (v) r->cbits |= 1u << c; else r->cbits &= ~(1u << c);
    r->ops++;
}

static void execute(QasmRun *r, const char *s) {
    char gname[12];
    const char *p = skip(s);
    if (!*p) return;
    if (starts(p, "OPENQASM") || starts(p, "include") || starts(p, "qreg") ||
        starts(p, "creg") || starts(p, "barrier")) return;

    if (starts(p, "if")) {                              /* if(creg==valor) puerta */
        p = skip(p + 2);
        if (*p++ != '(') { r->err = "if sin ("; return; }
        char cname[12];
        int val;
        if (!word(&p, cname, sizeof cname)) { r->err = "if sin registro"; return; }
        p = skip(p);
        if (p[0] != '=' || p[1] != '=') { r->err = "if sin =="; return; }
        p += 2;
        if (!number(&p, &val)) { r->err = "if sin valor"; return; }
        p = skip(p);
        if (*p++ != ')') { r->err = "if sin )"; return; }
        int cur = qasm_creg(r, cname);
        if (cur < 0) { r->err = "creg desconocido"; return; }
        if (cur == val) execute(r, p);
        return;
    }

    if (starts(p, "measure")) {
        if (r->stop_at_measure) { r->stopped = 1; return; }
        p += 7;
        int qi, ci;
        QReg *qg = operand(r, &p, 1, &qi);
        if (!qg) return;
        p = skip(p);
        if (p[0] != '-' || p[1] != '>') { r->err = "measure sin ->"; return; }
        p += 2;
        QReg *cg = operand(r, &p, 0, &ci);
        if (!cg) return;
        if ((qi < 0) != (ci < 0) || (qi < 0 && qg->size != cg->size)) { r->err = "measure con tamaños distintos"; return; }
        if (qi >= 0) measure_one(r, qg->base + qi, cg->base + ci);
        else for (int k = 0; k < qg->size; k++) measure_one(r, qg->base + k, cg->base + k);
        return;
    }

    if (!word(&p, gname, sizeof gname)) { r->err = "sentencia ilegible"; return; }

    double ang[3];
    int na = 0;
    p = skip(p);
    if (*p == '(') {
        if (r->backend != QASM_SV) { r->err = "el tablero no admite ángulos; usa el vector de estado"; return; }
        p++;
        for (;;) {
            if (na == 3) { r->err = "demasiados ángulos"; return; }
            ang[na++] = expr(r, &p);
            if (r->err) return;
            p = skip(p);
            if (*p == ',') { p++; continue; }
            if (*p == ')') { p++; break; }
            r->err = "falta ) tras los ángulos";
            return;
        }
    }

    if (eq(gname, "cx") || eq(gname, "cz") || eq(gname, "cu1") || eq(gname, "cp")) {
        int a, b;
        QReg *ga = operand(r, &p, 1, &a);
        if (!ga) return;
        p = skip(p);
        if (*p++ != ',') { r->err = "falta , entre operandos"; return; }
        QReg *gb = operand(r, &p, 1, &b);
        if (!gb) return;
        if (a < 0 || b < 0) { r->err = "las puertas de 2 qubits necesitan índices"; return; }
        int c = ga->base + a, t = gb->base + b;
        if (c == t) { r->err = "control y objetivo iguales"; return; }
        int phase = gname[1] == 'u' || gname[1] == 'p';
        if (na != phase) { r->err = phase ? "a esta puerta le falta su ángulo" : "esta puerta no lleva ángulo"; return; }
        if (r->backend == QASM_SV) {
            if (gname[1] == 'x') sv_cx(&r->sv, c, t);
            else if (gname[1] == 'z') sv_cz(&r->sv, c, t);
            else sv_cphase(&r->sv, c, t, ang[0]);
        } else if (gname[1] == 'x') tab_cx(&r->t, c, t);
        else { tab_h(&r->t, t); tab_cx(&r->t, c, t); tab_h(&r->t, t); }
        r->ops++;
        return;
    }

    int i;
    QReg *g = operand(r, &p, 1, &i);
    if (!g) return;
    int k0 = i >= 0 ? i : 0, k1 = i >= 0 ? i + 1 : g->size;
    for (int k = k0; k < k1 && !r->err; k++) {
        if (r->backend == QASM_SV) gate1_sv(r, gname, g->base + k, ang, na);
        else gate1_tab(r, gname, g->base + k);
    }
}

/* Recorre el texto sentencia por sentencia; pass 0 declara, pass 1 ejecuta. */
static int walk(const char *src, QasmRun *r, int pass) {
    char stmt[STMT_MAX];
    int n = 0;
    r->stmt = 0;
    for (const char *p = src; ; p++) {
        if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; }
        if (!*p || *p == ';') {
            stmt[n] = '\0';
            r->stmt++;
            const char *s = skip(stmt);
            if (pass == 0) {
                if (starts(s, "qreg")) declare(r, s + 4, 1);
                else if (starts(s, "creg")) declare(r, s + 4, 0);
            } else execute(r, s);
            if (r->err) return -1;
            if (r->stopped) return 0;
            n = 0;
            if (!*p) break;
            continue;
        }
        if (n < STMT_MAX - 1) stmt[n++] = *p;
        else { r->err = "sentencia demasiado larga"; return -1; }
    }
    return 0;
}

int qasm_run(const char *src, QasmRun *r) {
    r->nq = r->nc = r->nqubits = r->ncbits = 0;
    r->cbits = 0; r->err = 0; r->random_meas = 0; r->ops = 0; r->stopped = 0;
    if (walk(src, r, 0) < 0) return -1;
    if (!r->nqubits) { r->err = "no hay qreg"; return -1; }
    if (r->backend == QASM_SV) {
        if (r->nqubits > QSV_MAX) { r->err = "el vector de estado admite hasta 10 qubits"; return -1; }
        sv_init(&r->sv, r->nqubits);
    } else tab_init(&r->t, r->nqubits);
    return walk(src, r, 1);
}

int qasm_creg(const QasmRun *r, const char *name) {
    for (int i = 0; i < r->nc; i++)
        if (eq(r->c[i].name, name))
            return (int)((r->cbits >> r->c[i].base) & ((1u << r->c[i].size) - 1u));
    return -1;
}

const QReg *qasm_qreg(const QasmRun *r, const char *name) {
    for (int i = 0; i < r->nq; i++) if (eq(r->q[i].name, name)) return &r->q[i];
    return 0;
}
