/*
 * shell.c — shell de H7-OS
 *
 * Una línea, hasta 4 palabras. Los enteros aceptan decimal, 0b binario y
 * 0x hexadecimal (32 bits con signo). La entrada llega por teclado PS/2 o por
 * COM1; la salida va a las dos consolas.
 */
#include <stdint.h>
#include "shell.h"
#include "con.h"
#include "h7.h"
#include "rs.h"
#include "qsim.h"
#include "steane.h"
#include "echo.h"
#include "unital.h"
#include "ghz.h"

#define LINE_MAX 72                          /* "h7> " + 72 caracteres < 80 columnas */
#define KEY_UP   0x10                        /* flecha arriba (teclado o ESC [ A) */

bool shell_vgadump;

static const int64_t TEST_NS[] = {1, 8, 15, 22, 29, 0, 7, 14, 21, 3, 10, 17, 19, 20};
#define NTEST ((int)(sizeof TEST_NS / sizeof TEST_NS[0]))

/* ------------------------------------------------------------------------ */
/* Utilidades                                                               */
/* ------------------------------------------------------------------------ */

static const char *bin(uint32_t v, int bits) {
    static char out[4][36];                  /* hasta 4 usos en un mismo kprintf */
    static int k;
    char *s = out[k = (k + 1) & 3];
    for (int i = 0; i < bits; i++) s[i] = (char)('0' + ((v >> (bits - 1 - i)) & 1));
    s[bits] = '\0';
    return s;
}

static void hex23(const uint8_t *r) {
    for (int i = 0; i < H7_REC_SIZE; i++) kprintf("%02X", r[i]);
}

void label(const char *s) {
    con_attr(ATTR_BRIGHT);
    kprintf("%-8s", s);
    con_attr(ATTR_TEXT);
}

static bool streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static bool parse_i32(const char *s, int32_t *out) {
    bool neg = false;
    if (*s == '-' || *s == '+') neg = (*s++ == '-');
    unsigned base = 10;
    if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) { base = 2;  s += 2; }
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
    if (!*s) return false;
    uint64_t v = 0;
    for (; *s; s++) {
        unsigned d;
        if (*s >= '0' && *s <= '9') d = (unsigned)(*s - '0');
        else if (*s >= 'a' && *s <= 'f') d = (unsigned)(*s - 'a' + 10);
        else if (*s >= 'A' && *s <= 'F') d = (unsigned)(*s - 'A' + 10);
        else return false;
        if (d >= base) return false;
        v = v * base + d;
        if (v > 0x80000000u) return false;
    }
    if (!neg && v == 0x80000000u) return false;
    *out = neg ? (int32_t)(0u - (uint32_t)v) : (int32_t)v;
    return true;
}

/* dígitos / 10^k: una sola división redondeada, exacta hasta 15 dígitos */
static bool parse_f64(const char *s, double *out) {
    bool neg = false, dot = false, any = false;
    if (*s == '-' || *s == '+') neg = (*s++ == '-');
    double digits = 0, scale = 1;
    for (; *s; s++) {
        if (*s == '.' && !dot) { dot = true; continue; }
        if (*s < '0' || *s > '9') return false;
        any = true;
        digits = digits * 10 + (*s - '0');
        if (dot) scale *= 10;
    }
    if (!any) return false;
    *out = (neg ? -digits : digits) / scale;
    return true;
}

static const char *field_of(int byte) {
    return byte < 8 ? "q · n" : byte < 10 ? "H · topología" : byte < 18 ? "d · valor"
         : byte < 19 ? "B · origin" : "i · updates";
}

static void mask_line(uint8_t m) {
    kprintf("máscara 0b%s", bin(m, 7));
    if (!m) { kprintf(" → 7/7 cubos sanos\n"); return; }
    kprintf(" → roto:");
    for (int b = 0; b < RS_BUCKETS; b++) if ((m >> b) & 1) kprintf(" %d", b);
    kprintf("\n");
}

/* Por qué está roto cada cubo marcado: la suma no cierra, o un registro rompe
 * su invariante (eso último ni seal lo acepta). */
static void explain(uint8_t m) {
    for (int b = 0; b < RS_BUCKETS; b++) {
        if (!((m >> b) & 1)) continue;
        RsBucketInfo info = rs_bucket(b);
        if (h7_fold7((uint64_t)(info.sum + info.seal)) != 0)
            kprintf("  cubo %d: suma %s + sello %s no da 111\n", b, bin((uint32_t)info.sum, 3),
                    bin((uint32_t)info.seal, 3));
        for (int i = 0; i < info.count; i++) {
            int why = rs_check(b, i);
            const uint8_t *r = rs_slot(b, i);
            int k = (int)rs_key(b, i);
            TopoUnpacked t = unpack_topology(rec_packed(r));
            if (why & RS_BAD_KEY) {
                uint64_t f = (uint64_t)rec_n(r);
                kprintf("  cubo %d · n=%d: el campo n del registro vale 0x%08X%08X\n", b, k,
                        (unsigned)(f >> 32), (unsigned)f);
            }
            if (why & RS_BAD_INDEX) kprintf("  cubo %d · n=%d: index %d no es el cubo %d\n", b, k, t.index, b);
            if (why & RS_BAD_PAIR)  kprintf("  cubo %d · n=%d: index %d + pair %d no cierra a 111\n", b, k, t.index, t.pair);
            if (why & RS_BAD_ECC) {
                RsEcc e = rs_ecc(b, i);
                if (e.kind == RS_ECC_DATA)
                    kprintf("  cubo %d · n=%d: Hamming señala el bit %d (síndrome %s) · fix lo corrige\n",
                            b, k, e.bit, bin(e.syndrome, 8));
                else if (e.kind == RS_ECC_DOUBLE)
                    kprintf("  cubo %d · n=%d: Hamming ve 2 bits dañados · no se puede corregir: put o del\n", b, k);
                else
                    kprintf("  cubo %d · n=%d: el daño está en los bits de Hamming, no en el registro\n", b, k);
            }
        }
    }
}

static void status_bar(void) {
    char bar[128];
    ksprintf(bar, sizeof bar, "H7-OS v0.7 │ rs %d regs · %d B │ máscara 0b%s │ teclado + COM1",
             rs_total(), rs_total() * H7_REC_SIZE, bin(rs_verify(), 7));
    con_status(bar);
}

/* ------------------------------------------------------------------------ */
/* Bloques reutilizados de la v0.1                                          */
/* ------------------------------------------------------------------------ */

static void reference_block(void) {
    kprintf("=== H7 Hadronic Vectors - pipeline cos -> n%%7 -> quark (C-Native) ===\n");
    kprintf("0=u, 1=d, 0&7 superposicion en multiplos de 7\n\n");
    kprintf("%4s %2s %9s -> %4s %9s %8s %6s %4s %6s %s\n",
            "n", "z", "quarks", "comp", "comp_q", "o_n", "packed", "BE", "m_eff", "fam");
    kprintf("------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < NTEST; i++) {
        const uint8_t *r = rs_get(TEST_NS[i]);
        int64_t  n      = rec_n(r);
        double   cur    = rec_value(r);
        double   o_n    = ((n & 1) ? -1.0 : 1.0) * cur;          /* exacto: (±1)² = 1 */
        uint16_t packed = rec_packed(r);
        int      z      = n_z7(n);
        bool     vac    = (z == 0);
        int64_t  comp   = complement_block(n);
        int      frag   = unpack_topology(packed).discrete_phase_fragment;
        double   prob   = cur < 0 ? -cur : cur;
        double   m_eff  = prob * (1.0 + 0.2 * frag);
        bool     toroid = (z == 1 || z == 3 || z == 5);

        kprintf("%4d %2d %9s -> %4d %9s %8.4f 0x%04X %04X %6.3f %s\n",
                (int)n, z, quark_triplet(z), (int)comp, quark_triplet(n_z7(comp)),
                o_n, packed, __builtin_bswap16(packed), m_eff,
                toroid ? "TOROID" : (vac ? "VACUUM" : "TRANSIT"));
    }

    kprintf("\n=== 23-byte record hex (big endian q H d B i) ===\n");
    for (int i = 0; i < 5; i++) {
        const uint8_t *r = rs_get(TEST_NS[i]);
        kprintf("n=%2d record=", (int)TEST_NS[i]);
        hex23(r);
        kprintf(" (23 bytes) quarks=%s\n", quark_triplet(n_z7(TEST_NS[i])));
    }
}

static void bucket_table(void) {
    kprintf(" z  bin  quarks   regs  suma  sello  n\n");
    for (int b = 0; b < RS_BUCKETS; b++) {
        RsBucketInfo info = rs_bucket(b);
        kprintf(" %d  %s  %-7s  %4d   %s   %s  ", b, bin((uint32_t)b, 3), quark_triplet(b),
                info.count, bin((uint32_t)info.sum, 3), bin((uint32_t)info.seal, 3));
        for (int i = 0; i < info.count; i++)
            kprintf("%s%d%s", i ? " " : "", (int)rs_key(b, i), rs_check(b, i) ? "!" : "");
        kprintf(info.count ? "\n" : "·\n");
    }
}

/* Todo flip de 1 bit en todo registro debe marcar exactamente su cubo, y
 * Hamming debe señalar exactamente ese bit. */
static void single_bit_sweep(int *detected, int *corrected, int *total) {
    *detected = *corrected = *total = 0;
    for (int b = 0; b < RS_BUCKETS; b++)
        for (int i = 0; i < rs_bucket(b).count; i++)
            for (int bit = 0; bit < RS_DATA_BITS; bit++) {
                uint8_t *r = rs_slot(b, i);
                rs_flip(r, bit);
                if (rs_verify() == (1u << b)) (*detected)++;
                RsEcc e = rs_ecc(b, i);
                if (e.kind == RS_ECC_DATA && e.bit == bit) (*corrected)++;
                rs_flip(r, bit);
                (*total)++;
            }
}

/* Todos los pares de bits de un registro: cuántos ve el sello Z7 solo y
 * cuántos ven las tres capas juntas. */
static void double_bit_sweep(uint8_t *r, int *det_z7, int *det_all, int *total) {
    *det_z7 = *det_all = *total = 0;
    for (int a = 0; a < RS_DATA_BITS; a++)
        for (int c = a + 1; c < RS_DATA_BITS; c++) {
            rs_flip(r, a); rs_flip(r, c);
            if (rs_verify_z7()) (*det_z7)++;
            if (rs_verify()) (*det_all)++;
            rs_flip(r, c); rs_flip(r, a);
            (*total)++;
        }
}

/* ------------------------------------------------------------------------ */
/* Comandos                                                                 */
/* ------------------------------------------------------------------------ */

static void cmd_help(void) {
    kprintf("n: entero de 32 bits en decimal, 0b binario o 0x hex · bit: 0..183\n");
    kprintf("  put n [v]    guarda n; v por defecto (-1)^n·cos(πφn)\n");
    kprintf("  get n        registro decodificado\n");
    kprintf("  del n        borra el registro\n");
    kprintf("  ls           cubos: registros, suma Z7 y sello\n");
    kprintf("  verify       máscara de 7 bits y por qué está roto cada cubo\n");
    kprintf("  fix          corrige con Hamming los registros con 1 bit dañado\n");
    kprintf("  flip n bit   invierte un bit (número 0..183) · ej. flip 12 100\n");
    kprintf("  seal         re-sella los cubos cuyos registros están íntegros\n");
    kprintf("  sweep [n]    todos los flips de 1 bit; con n, todos los pares en n\n");
    kprintf("  ref          bloque de referencia de h7_hexa_dec.c\n");
    kprintf("  phase n      nodo, posición, etiqueta y fase áurea exacta vs rotate_pow\n");
    kprintf("  q            código de Steane en OpenQASM: q run, q qasm, q sweep (q help)\n");
    kprintf("  clear · exit · halt\n");
    kprintf("flecha arriba repite la última línea\n");
}

static void cmd_put(int32_t n, bool has_v, double v) {
    if (!has_v) v = h7_current(n);
    int upd = rs_put(n, v);
    int z = n_z7(n);
    if (upd < 0) { kprintf("cubo %d (%s) lleno: %d ranuras\n", z, bin((uint32_t)z, 3), RS_SLOTS); return; }
    const uint8_t *r = rs_get(n);
    kprintf("n=%d → cubo %d (%s) · updates %d · clase Z7 %s\n", (int)n, z, bin((uint32_t)z, 3), upd,
            bin((uint32_t)h7_rec_z7(r), 3));
    kprintf("record ");
    hex23(r);
    kprintf("\n");
}

static void cmd_get(int32_t n) {
    const uint8_t *r = rs_get(n);
    if (!r) { kprintf("no existe n=%d\n", (int)n); return; }
    int z = n_z7(n);
    uint16_t h = rec_packed(r);
    TopoUnpacked t = unpack_topology(h);
    kprintf("n=%d · z %d (%s) · %s · clase Z7 %s\n", (int)n, z, bin((uint32_t)z, 3), quark_triplet(z),
            bin((uint32_t)h7_rec_z7(r), 3));
    kprintf("record ");
    hex23(r);
    kprintf("\n");
    if (rec_n(r) != n) {
        uint64_t f = (uint64_t)rec_n(r);
        kprintf("ojo: el campo n vale 0x%08X%08X; el índice lo encontró por su clave %d\n",
                (unsigned)(f >> 32), (unsigned)f, (int)n);
    }
    kprintf("H 0x%04X · index %d · pair %d · winding %d · mapping %d · peso %d · frag %d\n",
            h, t.index, t.pair, t.winding, t.mapping, t.ternary_weight, t.discrete_phase_fragment);
    kprintf("d %.9f · origin %d · updates %d\n", rec_value(r), r[18], (int)rec_updates(r));
    int b, i;
    rs_find(n, &b, &i);
    uint16_t w = rs_ecc_word(b, i);
    RsEcc e = rs_ecc(b, i);
    kprintf("hamming P %s · paridad %d · síndrome %s%s\n", bin(w & 0xFF, 8), (w >> 8) & 1, bin(e.syndrome, 8),
            e.kind == RS_ECC_OK ? " · íntegro" : e.kind == RS_ECC_DATA ? " · 1 bit dañado" : " · dañado");
}

static void cmd_flip(int32_t n, int32_t bit) {
    uint8_t *r = rs_get(n);
    if (!r) { kprintf("no existe n=%d\n", (int)n); return; }
    if (bit < 0 || bit >= H7_REC_SIZE * 8) { kprintf("bit fuera de 0..183\n"); return; }
    rs_flip(r, bit);
    kprintf("n=%d bit %d (byte %d, %s) · ", (int)n, (int)bit, (int)(bit >> 3), field_of(bit >> 3));
    mask_line(rs_verify());
}

static void cmd_sweep(bool has_n, int32_t n) {
    uint8_t m = rs_verify();
    if (m) { kprintf("el almacén no está sano: "); mask_line(m); return; }
    int det, cor, tot;
    single_bit_sweep(&det, &cor, &tot);
    kprintf("1 bit:  %d/%d detectados en su cubo · %d/%d corregidos por Hamming\n", det, tot, cor, tot);
    if (has_n) {
        uint8_t *r = rs_get(n);
        if (!r) { kprintf("no existe n=%d\n", (int)n); return; }
        int z7;
        double_bit_sweep(r, &z7, &det, &tot);
        kprintf("2 bits en n=%d: sin Hamming %d/%d · con Hamming %d/%d\n", (int)n, z7, tot, det, tot);
    }
}

/* |v| en notación científica con 2 cifras: 2.9e-07 (kprintf no tiene %e) */
static const char *sci(double v) {
    static char s[16];
    if (v < 0) v = -v;
    if (v == 0) return "0";
    int e = 0;
    while (v < 1) { v *= 10; e--; }
    while (v >= 10) { v /= 10; e++; }
    if (v >= 9.95) { v /= 10; e++; }
    ksprintf(s, sizeof s, "%.1fe%c%02d", v, e < 0 ? '-' : '+', e < 0 ? -e : e);
    return s;
}

/* phase n: nodo, posición y etiqueta, más la fase áurea exacta (h7.h) */
static void cmd_phase(int32_t n) {
    int r = n_z7(n);
    int32_t node = (int32_t)(((int64_t)n - r) / 7);
    uint64_t lab = h7_label(n);
    double g = h7_golden(n), rp = rotate_pow(n);
    kprintf("n=%d · nodo %d · posición %d (%s) · etiqueta %c\n", (int)n, (int)node, r,
            bin((uint32_t)r, 3), (n & 1) ? 'q' : 'p');
    kprintf("frac(nφ)    %.9f · 0x%08X%08X\n", (double)(lab >> 11) * 0x1p-53,
            (unsigned)(lab >> 32), (unsigned)lab);
    kprintf("O_n exacta  %.9f  (fase entera de 128 bits)\n", g);
    kprintf("rotate_pow  %.9f  · difiere en %s\n", rp, sci(g - rp));
}

static void cmd_fix(void) {
    int fixed = 0, stuck = 0;
    for (int b = 0; b < RS_BUCKETS; b++)
        for (int i = 0; i < rs_bucket(b).count; i++) {
            RsEcc e = rs_ecc(b, i);
            if (e.kind == RS_ECC_DATA) {
                rs_fix_slot(b, i);
                kprintf("n=%d: bit %d corregido (síndrome %s = posición %d, byte %d, %s)\n", (int)rs_key(b, i),
                        e.bit, bin(e.syndrome, 8), e.syndrome, e.bit >> 3, field_of(e.bit >> 3));
                fixed++;
            } else if (e.kind != RS_ECC_OK) {
                kprintf("n=%d: no se puede corregir (2 o más bits)\n", (int)rs_key(b, i));
                stuck++;
            }
        }
    if (!fixed && !stuck) kprintf("nada que corregir · ");
    uint8_t m = rs_verify();
    mask_line(m);
    explain(m);
}

/* ------------------------------------------------------------------------ */
/* q: código de Steane [[7,1,3]] escrito en OpenQASM 2.0                    */
/* ------------------------------------------------------------------------ */

static void q_help(void) {
    kprintf("q run [0|+] [x|y|z pos]...   ciclo de Steane con errores en las posiciones 1..7\n");
    kprintf("q qasm [0|+] [x|y|z pos]...  imprime ese mismo circuito en OpenQASM 2.0\n");
    kprintf("q sweep                      todos los errores de 1 y 2 qubits, |0_L> y |+_L>\n");
    kprintf("q seed n                     semilla de las mediciones al azar\n");
    kprintf("ej. q run x 3 · q run + z 5 · q run x 1 x 2 · q qasm y 7\n");
    kprintf("q echo [A..G]                experimento del eco: r libre y con eco, 7 modelos\n");
    kprintf("q echo qasm A [eco] [yxx] [i] circuito de la realización i del modelo A\n");
    kprintf("q unital [A..F]              prueba de unitalidad: z0, z1 y |t| en 6 modelos\n");
    kprintf("q unital qasm E [x|y|z] [i]  circuito de una realización\n");
    kprintf("q ghz [n]                    n mod 14 en la base GHZ: lectura sin azar (P = 1)\n");
    kprintf("q ghz qasm n [enc]           ese circuito; con enc, solo la codificación\n");
}

static int q_args(int argc, char **argv, int *plus, QErr *e, int *ne) {
    int i = 2;
    *plus = 0;
    *ne = 0;
    if (i < argc && (streq(argv[i], "+") || streq(argv[i], "0"))) { *plus = argv[i][0] == '+'; i++; }
    while (i < argc) {
        char p = argv[i][0];
        int32_t pos;
        if (p >= 'X' && p <= 'Z') p = (char)(p + 32);
        if (*ne == STEANE_MAX_ERR || argv[i][1] || (p != 'x' && p != 'y' && p != 'z') ||
            i + 1 >= argc || !parse_i32(argv[i + 1], &pos) || pos < 1 || pos > 7) return -1;
        e[*ne].pauli = p;
        e[*ne].pos = pos;
        (*ne)++;
        i += 2;
    }
    return 0;
}

static void q_errs(const QErr *e, int ne) {
    if (!ne) { kprintf("sin errores"); return; }
    kprintf("error%s", ne > 1 ? "es" : "");
    for (int i = 0; i < ne; i++) kprintf("%s %c en %d", i ? "," : "", e[i].pauli - 32, e[i].pos);
}

static void q_run(int plus, const QErr *e, int ne) {
    SteaneResult r;
    const char *err;
    if (steane_run(plus, e, ne, &r, &err) < 0) { kprintf("qasm: %s\n", err); return; }
    char m[8];
    for (int k = 0; k < 7; k++) m[k] = (char)('0' + ((r.m >> k) & 1));
    m[7] = '\0';
    kprintf("estado %s · ", plus ? "|+_L>" : "|0_L>");
    q_errs(e, ne);
    kprintf("\n");
    kprintf("paridad Z  sz %s", bin((uint32_t)r.sz, 3));
    if (r.sz) kprintf(" → x en la posición %d\n", r.sz); else kprintf(" → sin error de bit\n");
    kprintf("paridad X  sx %s", bin((uint32_t)r.sx, 3));
    if (r.sx) kprintf(" → z en la posición %d\n", r.sx); else kprintf(" → sin error de fase\n");
    kprintf("lectura    m %s (posiciones 1..7) · síndrome clásico %s · paridad %d\n", m,
            bin((uint32_t)r.msyn, 3), r.logical);
    if (!r.logical) kprintf("resultado  lógico 0 conservado\n");
    else kprintf("resultado  lógico 1: error lógico (d = 3 solo garantiza corregir 1 error)\n");
}

static void print_qasm(const char *buf) {
    kprintf("--8<-- qasm\n");
    char line[160];
    int n = 0;
    for (const char *p = buf; *p; p++) {
        if (*p == '\n' || n == (int)sizeof line - 1) { line[n] = '\0'; kprintf("%s\n", line); n = 0; continue; }
        line[n++] = *p;
    }
    kprintf("-->8-- qasm\n");
}

static void q_qasm(int plus, const QErr *e, int ne) {
    static char buf[4096];
    if (steane_qasm(buf, sizeof buf, plus, e, ne) < 0) { kprintf("qasm: el circuito no cupo\n"); return; }
    print_qasm(buf);
}

/* q echo: experimento del eco (exp/eco_prerregistro.md) */
static void emit_line(const char *s) { kprintf("%s\n", s); }

static void q_echo(int argc, char **argv) {
    const char *err;
    if (argc >= 3 && streq(argv[2], "qasm")) {
        static char buf[4096];
        int m = argc >= 4 && !argv[3][1] ? echo_find(argv[3][0]) : -1, eco = 0, yxx = 0, i = 0, bad = m < 0;
        for (int k = 4; k < argc && !bad; k++) {
            int32_t v;
            if (streq(argv[k], "eco")) eco = 1;
            else if (streq(argv[k], "libre")) eco = 0;
            else if (streq(argv[k], "yxx")) yxx = 1;
            else if (streq(argv[k], "xxx")) yxx = 0;
            else if (parse_i32(argv[k], &v) && v >= 0 && v < echo_model[m].M) i = v;
            else bad = 1;
        }
        if (bad) { kprintf("uso: q echo qasm A..G [eco] [yxx] [i] · ej. q echo qasm F eco\n"); return; }
        echo_qasm(buf, sizeof buf, m, eco, yxx, i);
        print_qasm(buf);
        return;
    }
    int m = -1;
    if (argc > 3 || (argc == 3 && (argv[2][1] || (m = echo_find(argv[2][0])) < 0))) {
        kprintf("uso: q echo [A..G] · ej. q echo · q echo F\n");
        return;
    }
    if (m < 0) kprintf("--8<-- echo\n");
    if (echo_report(m, emit_line, &err) < 0) kprintf("echo: %s\n", err);
    if (m < 0) kprintf("-->8-- echo\n");
}

/* q unital: prueba de unitalidad (exp/unital_prerregistro.md) */
static void q_unital(int argc, char **argv) {
    const char *err;
    if (argc >= 3 && streq(argv[2], "qasm")) {
        static char buf[4096];
        int m = argc >= 4 && !argv[3][1] ? uni_find(argv[3][0]) : -1, basis = 2, i = 0, bad = m < 0;
        for (int k = 4; k < argc && !bad; k++) {
            int32_t v;
            if (streq(argv[k], "x")) basis = 0;
            else if (streq(argv[k], "y")) basis = 1;
            else if (streq(argv[k], "z")) basis = 2;
            else if (parse_i32(argv[k], &v) && v >= 0 && v < uni_model[m].M) i = v;
            else bad = 1;
        }
        if (bad) { kprintf("uso: q unital qasm A..F [x|y|z] [i] · ej. q unital qasm E z\n"); return; }
        uni_qasm(buf, sizeof buf, m, basis, i);
        print_qasm(buf);
        return;
    }
    int m = -1;
    if (argc > 3 || (argc == 3 && (argv[2][1] || (m = uni_find(argv[2][0])) < 0))) {
        kprintf("uso: q unital [A..F] · ej. q unital · q unital E\n");
        return;
    }
    if (m < 0) kprintf("--8<-- unital\n");
    if (uni_report(m, emit_line, &err) < 0) kprintf("unital: %s\n", err);
    if (m < 0) kprintf("-->8-- unital\n");
}

/* q ghz: n mod 14 en la base GHZ, con lectura determinista (src/ghz.h) */
static void q_ghz(int argc, char **argv) {
    const char *err;
    int32_t n = 0;
    if (argc >= 3 && streq(argv[2], "qasm")) {
        static char buf[1024];
        int enc = argc == 5 && streq(argv[4], "enc");
        if (argc < 4 || argc > 5 || !parse_i32(argv[3], &n) || (argc == 5 && !enc)) {
            kprintf("uso: q ghz qasm n [enc] · ej. q ghz qasm 15\n");
            return;
        }
        ghz_qasm(buf, sizeof buf, n, !enc);
        print_qasm(buf);
        return;
    }
    if (argc > 3 || (argc == 3 && !parse_i32(argv[2], &n))) { kprintf("uso: q ghz [n] · ej. q ghz · q ghz 15\n"); return; }
    if (argc == 2) kprintf("--8<-- ghz\n");
    if (ghz_report(n, argc == 2, emit_line, &err) < 0) kprintf("ghz: %s\n", err);
    if (argc == 2) kprintf("-->8-- ghz\n");
}

static void q_sweep(void) {
    static const char P[3] = { 'x', 'y', 'z' };
    SteaneResult r;
    const char *err;
    int ok1[2] = { 0, 0 }, base_ok = 1, runs = 0;
    for (int plus = 0; plus < 2; plus++) {
        if (steane_run(plus, 0, 0, &r, &err) < 0) { kprintf("qasm: %s\n", err); return; }
        base_ok &= (r.sz == 0 && r.sx == 0 && r.logical == 0 && r.msyn == 0);
        runs++;
        for (int p = 0; p < 3; p++)
            for (int pos = 1; pos <= 7; pos++) {
                QErr e = { P[p], pos };
                steane_run(plus, &e, 1, &r, &err);
                int want_sz = P[p] != 'z' ? pos : 0, want_sx = P[p] != 'x' ? pos : 0;
                ok1[plus] += (r.sz == want_sz && r.sx == want_sx && r.logical == 0 && r.msyn == 0);
                runs++;
            }
    }
    int ok2 = 0, tot2 = 0;
    for (int i = 1; i <= 7; i++)
        for (int j = i + 1; j <= 7; j++)
            for (int p = 0; p < 3; p++)
                for (int q = 0; q < 3; q++) {
                    QErr e[2] = { { P[p], i }, { P[q], j } };
                    SteaneResult a, b;
                    steane_run(0, e, 2, &a, &err);
                    steane_run(1, e, 2, &b, &err);
                    ok2 += (a.logical == 0 && b.logical == 0);
                    tot2++;
                    runs += 2;
                }
    kprintf("sin error: síndromes 000/000 y lógico 0 en ambas bases: %s\n", base_ok ? "sí" : "NO");
    kprintf("1 error:   %d/21 corregidos en |0_L> · %d/21 en |+_L>\n", ok1[0], ok1[1]);
    kprintf("2 errores: %d/%d sin error lógico · %d/%d con error lógico (d = 3)\n", ok2, tot2, tot2 - ok2, tot2);
    kprintf("%d circuitos OpenQASM ejecutados\n", runs);
}

static void cmd_q(int argc, char **argv) {
    int plus, ne;
    QErr e[STEANE_MAX_ERR];
    if (argc < 2 || streq(argv[1], "help")) { q_help(); return; }
    if (streq(argv[1], "seed")) {
        int32_t s;
        if (argc != 3 || !parse_i32(argv[2], &s)) { kprintf("uso: q seed n\n"); return; }
        q_seed((uint32_t)s);
        kprintf("semilla %d\n", (int)s);
        return;
    }
    if (streq(argv[1], "sweep")) { q_sweep(); return; }
    if (streq(argv[1], "echo"))  { q_echo(argc, argv); return; }
    if (streq(argv[1], "unital")) { q_unital(argc, argv); return; }
    if (streq(argv[1], "ghz"))   { q_ghz(argc, argv); return; }
    if (!streq(argv[1], "run") && !streq(argv[1], "qasm")) { q_help(); return; }
    if (q_args(argc, argv, &plus, e, &ne) < 0) {
        kprintf("uso: q %s [0|+] [x|y|z pos]... · pos 1..7, hasta %d errores\n", argv[1], STEANE_MAX_ERR);
        return;
    }
    if (argv[1][0] == 'r') q_run(plus, e, ne); else q_qasm(plus, e, ne);
}

static void cmd_ref(void) {
    uint8_t m = rs_verify();
    if (m) { kprintf("el almacén no está sano: "); mask_line(m); explain(m); return; }
    for (int i = 0; i < NTEST; i++)
        if (!rs_get(TEST_NS[i])) { kprintf("falta n=%d en el almacén\n", (int)TEST_NS[i]); return; }
    kprintf("--8<-- ref\n");
    reference_block();
    kprintf("-->8-- ref\n");
}

static void cmd_exit(void) {
    uint8_t m = rs_verify();
    if (shell_vgadump) con_dump_vga();
    outb(0xF4, m ? 1 : 0);                           /* QEMU sale con (v << 1) | 1 */
    kprintf("sin isa-debug-exit: cierra la ventana de QEMU o usa halt\n");
}

void shell_exec(char *line) {
    char *argv[12];
    int argc = 0;
    for (char *p = line; *p && argc < 12; ) {
        while (*p == ' ') *p++ = '\0';
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
    }
    if (!argc) return;

    const char *c = argv[0];
    int32_t a = 0, b = 0;
    double v = 0;
    bool a_ok = argc > 1 && parse_i32(argv[1], &a);
    bool b_ok = argc > 2 && parse_i32(argv[2], &b);

    if (streq(c, "help") || streq(c, "ayuda")) cmd_help();
    else if (streq(c, "put")) {
        if (!a_ok || argc > 3 || (argc == 3 && !parse_f64(argv[2], &v))) kprintf("uso: put n [v] · ej. put 12 o put 12 0.5\n");
        else cmd_put(a, argc == 3, v);
    }
    else if (streq(c, "get"))   { if (a_ok && argc == 2) cmd_get(a); else kprintf("uso: get n · ej. get 12\n"); }
    else if (streq(c, "del"))   {
        if (!a_ok || argc != 2) kprintf("uso: del n · ej. del 12\n");
        else if (rs_del(a) < 0) kprintf("no existe n=%d\n", (int)a);
        else { kprintf("n=%d borrado · ", (int)a); mask_line(rs_verify()); }
    }
    else if (streq(c, "ls"))     {
        bucket_table();
        kprintf("%d registros · %d B de %d B\n", rs_total(), rs_total() * H7_REC_SIZE,
                RS_BUCKETS * RS_SLOTS * H7_REC_SIZE);
    }
    else if (streq(c, "fix"))    cmd_fix();
    else if (streq(c, "q"))      cmd_q(argc, argv);
    else if (streq(c, "verify")) { uint8_t m = rs_verify(); mask_line(m); explain(m); }
    else if (streq(c, "flip"))   { if (a_ok && b_ok && argc == 3) cmd_flip(a, b); else kprintf("uso: flip n bit · bit es un número 0..183, ej. flip 12 100\n"); }
    else if (streq(c, "seal"))   {
        uint8_t refused = rs_seal();
        if (refused) {
            kprintf("no se re-sellan los cubos con registros dañados:");
            for (int k = 0; k < RS_BUCKETS; k++) if ((refused >> k) & 1) kprintf(" %d", k);
            kprintf("\n");
        }
        uint8_t m = rs_verify();
        kprintf("sellos recalculados · ");
        mask_line(m);
        explain(m);
    }
    else if (streq(c, "sweep"))  {
        if (argc > 2 || (argc == 2 && !a_ok)) kprintf("uso: sweep [n] · ej. sweep 12\n");
        else cmd_sweep(argc == 2, a);
    }
    else if (streq(c, "ref"))    cmd_ref();
    else if (streq(c, "phase"))  { if (a_ok && argc == 2) cmd_phase(a); else kprintf("uso: phase n · ej. phase 1000000000\n"); }
    else if (streq(c, "clear"))  con_clear();
    else if (streq(c, "exit"))   cmd_exit();
    else if (streq(c, "halt"))   { kprintf("sistema detenido\n"); for (;;) __asm__ volatile ("cli; hlt"); }
    else kprintf("comando desconocido: %s · escribe help\n", c);

    status_bar();
}

/* ------------------------------------------------------------------------ */
/* Arranque, script y bucle interactivo                                     */
/* ------------------------------------------------------------------------ */

void shell_seed(void) {
    rs_init();
    for (int i = 0; i < NTEST; i++) rs_put(TEST_NS[i], h7_current(TEST_NS[i]));
    int det, cor, tot;
    single_bit_sweep(&det, &cor, &tot);
    label("rs");
    kprintf("%d registros sembrados · %d B de %d B · SECDED 9 bits por registro\n", rs_total(),
            rs_total() * H7_REC_SIZE, RS_BUCKETS * RS_SLOTS * H7_REC_SIZE);
    label("test");
    kprintf("1 bit: %d/%d detectados en su cubo · %d/%d corregidos\n", det, tot, cor, tot);
    label("verify");
    mask_line(rs_verify());
    status_bar();
}

static void prompt(void) {
    con_attr(ATTR_BRIGHT);
    kprintf("h7> ");
    con_attr(ATTR_TEXT);
}

void shell_run_script(const char *s) {
    char line[LINE_MAX + 1];
    while (*s) {
        int n = 0;
        while (*s == ';') s++;
        while (*s && *s != ';' && n < LINE_MAX) line[n++] = *s++;
        while (*s && *s != ';') s++;
        line[n] = '\0';
        if (!n) continue;
        prompt();
        kprintf("%s\n", line);
        shell_exec(line);
    }
}

static int read_line(char *buf, const char *last) {
    static bool cr;
    int n = 0, esc = 0;
    for (;;) {
        int c = con_getc();
        if (esc == 1) { esc = (c == '[') ? 2 : 0; continue; }          /* ESC [ X */
        if (esc == 2) { esc = 0; if (c != 'A') continue; c = KEY_UP; }
        if (c == 27) { esc = 1; continue; }

        if (c == '\n' && cr) { cr = false; continue; }                  /* \r\n cuenta una vez */
        cr = (c == '\r');
        if (c == '\r' || c == '\n') { kprintf("\n"); buf[n] = '\0'; return n; }

        if (c == '\b' || c == 0x7F) { if (n) { n--; con_backspace(); } continue; }
        if (c == KEY_UP) {
            while (n) { n--; con_backspace(); }
            for (; last[n] && n < LINE_MAX; n++) { buf[n] = last[n]; con_echo(last[n]); }
            continue;
        }
        if (c < 0x20 || c > 0x7E || n == LINE_MAX) continue;
        buf[n++] = (char)c;
        con_echo((char)c);
    }
}

void shell_loop(void) {
    static char line[LINE_MAX + 1], last[LINE_MAX + 1];
    for (;;) {
        prompt();
        if (read_line(line, last)) {
            for (int i = 0; i <= LINE_MAX; i++) last[i] = line[i];
            shell_exec(line);
        }
    }
}
