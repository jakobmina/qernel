/*
 * h7.c — núcleo H7 freestanding. Misma lógica que h7_hexa_dec.c; los cambios
 * son solo los necesarios para correr sin libc/libm en i386 (ver h7.h).
 */
#include "h7.h"
#include "qsv.h"          /* q_sincos, para h7_golden */

/* cos(pi*phi) y sin(pi*phi) con los mismos bits que glibc en la referencia.
 * El kernel pone la FPU x87 a 53 bits, así que cada producto y suma redondea
 * igual que un double IEEE: los registros salen idénticos byte a byte. */
static const double H7_COS_THETA =  0x1.7312673726265p-2;
static const double H7_SIN_THETA = -0x1.dd335a962dc91p-1;

/* ------------------------------------------------------------------------ */
/* Aritmética Z7                                                            */
/* ------------------------------------------------------------------------ */

/* 8 ≡ 1 (mod 7): sumar los dígitos octales conserva la clase. Se pliega
 * hasta caber en 3 bits; 000 y 111 son las dos caras del cero. */
int h7_fold7(uint64_t u) {
    while (u > 7) u = (u & 7) + (u >> 3);
    return (u == 7) ? 0 : (int)u;
}

int n_z7(int64_t n) {
    if (n >= 0) return h7_fold7((uint64_t)n);
    int r = h7_fold7((uint64_t)(-(n + 1)) + 1u);   /* |n| sin desbordar INT64_MIN */
    return r ? 7 - r : 0;
}

bool is_vacuum(int64_t n) { return n_z7(n) == 0; }

/* Complemento en 3 bits: 7 - z == z XOR 111, también para z = 0 (vacío -> +7). */
int64_t complement_block(int64_t n) {
    int z = n_z7(n);
    return n - z + (z ^ 7);
}

const char *quark_triplet(int z) {
    static const char *const T[8] = {
        "uuu+ddd", "uud", "udu", "udd", "duu", "dud", "ddu", "ddd"
    };
    return T[z & 7];
}

/* ------------------------------------------------------------------------ */
/* Operador áureo, pesos y fragmentos                                       */
/* ------------------------------------------------------------------------ */

double rotate_pow(int64_t n) {
    double rc = 1.0, rs = 0.0;
    double bc = H7_COS_THETA, bs = H7_SIN_THETA;
    /* |n| sin signo: -n con n = INT64_MIN desborda, y con -O2 el ciclo no
     * terminaba. Para cualquier otro n los bits son los mismos que en v0.6. */
    uint64_t e = (n < 0) ? 0u - (uint64_t)n : (uint64_t)n;

    while (e > 0) {
        if (e & 1) {
            double nc = rc * bc - rs * bs;
            double ns = rc * bs + rs * bc;
            rc = nc;
            rs = ns;
        }
        double nc = bc * bc - bs * bs;
        double ns = 2 * bc * bs;
        bc = nc;
        bs = ns;
        e >>= 1;
    }
    return rc;
}

double h7_current(int64_t n) {
    return ((n & 1) ? -1.0 : 1.0) * rotate_pow(n);
}

/* frac(φ) y φ/2 por 2^128, redondeados; w[0] es la palabra baja.
 * Los 64 bits altos de H7_FRAC_PHI son 0x9E3779B97F4A7C15: la constante del
 * Fibonacci hashing de Knuth. */
static const uint32_t H7_FRAC_PHI[4] = { 0x5CEDC834u, 0xF39CC060u, 0x7F4A7C15u, 0x9E3779B9u };
static const uint32_t H7_HALF_PHI[4] = { 0x2E76E41Au, 0xF9CE6030u, 0xBFA53E0Au, 0xCF1BBCDCu };

/* n · c módulo 2^128 con n extendido con signo: el complemento a dos da
 * frac(n·x) correcto también para n < 0. Devuelve los 64 bits altos.
 * Solo productos 32×32 → 64, que gcc emite en línea en i386. */
static uint64_t frac_mul(int64_t n, const uint32_t c[4]) {
    uint32_t s = n < 0 ? 0xFFFFFFFFu : 0u;
    uint32_t a[4] = { (uint32_t)(uint64_t)n, (uint32_t)((uint64_t)n >> 32), s, s };
    uint32_t r[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; i++) {
        uint64_t carry = 0;
        for (int j = 0; i + j < 4; j++) {
            uint64_t t = (uint64_t)a[i] * c[j] + r[i + j] + carry;
            r[i + j] = (uint32_t)t;
            carry = t >> 32;
        }
    }
    return ((uint64_t)r[3] << 32) | r[2];
}

uint64_t h7_label(int64_t n) { return frac_mul(n, H7_FRAC_PHI); }

/* cos(πφn) = cos(2π·x) con x = frac(n·φ/2), llevado a [-1/2, 1/2): los 53
 * bits altos de x caben exactos en un double y el argumento queda en [-π, π). */
double h7_golden(int64_t n) {
    static const double TWO_PI = 0x1.921fb54442d18p+2;
    int64_t m = (n < 0 && n != INT64_MIN) ? -n : n;     /* coseno par: h7_golden(-n) = h7_golden(n) */
    double x = (double)(frac_mul(m, H7_HALF_PHI) >> 11) * 0x1p-53, sn, cs;
    if (x >= 0.5) x -= 1.0;
    q_sincos(TWO_PI * x, &sn, &cs);
    return cs;
}

int ternary_weight_of(int64_t n, double value) {
    double i_exact = ((n & 1) == 0) ? 1.0 : -1.0;
    double v = value * (i_exact + 1.0);
    if (v < 1e-9 && v > -1e-9) return 0;
    return (v > 0) ? 1 : -1;
}

/* round() de C (mitad lejos de cero) sin libm; x - trunc(x) es exacto. */
static int h7_round(double x) {
    if (x < 0) return -h7_round(-x);
    int t = (int)x;
    return (x - t >= 0.5) ? t + 1 : t;
}

int phase_fragment_of(double value) {
    int frag = h7_round((value + 1.0) * 3.5);
    if (frag < 0) return 0;
    if (frag > 7) return 7;
    return frag;
}

/* ------------------------------------------------------------------------ */
/* Topología de 16 bits                                                     */
/* ------------------------------------------------------------------------ */

uint16_t pack_topology(int index, int pair, int winding, int mapping,
                       int ternary_weight, int discrete_phase_fragment) {
    uint16_t v = 0;
    uint16_t weight_bits = (ternary_weight == -1) ? 0x00 : (ternary_weight == 0) ? 0x01 : 0x02;

    v |= (index == 0) ? 0x07 : (uint16_t)(index - 1);         /* bits 0-2  */
    v |= (uint16_t)((pair - 1) << 3);                          /* bits 3-5  */
    v |= (uint16_t)((winding / 2) << 6);                       /* bit 6     */
    v |= (uint16_t)(mapping << 8);                             /* bit 8     */
    v |= (uint16_t)(weight_bits << 9);                         /* bits 9-10 */
    v |= (uint16_t)((discrete_phase_fragment & 0x07) << 11);   /* bits 11-13 */
    return v;
}

TopoUnpacked unpack_topology(uint16_t value) {
    TopoUnpacked u;
    int raw_index = value & 0x07;
    u.is_vacuum_superposition = (raw_index == 0x07);
    u.index   = u.is_vacuum_superposition ? 0 : raw_index + 1;
    u.pair    = ((value >> 3) & 0x07) + 1;
    u.winding = ((value >> 6) & 0x01) * 2;
    u.mapping = (value >> 8) & 0x01;
    int wb    = (value >> 9) & 0x03;
    u.ternary_weight = (wb == 0) ? -1 : (wb == 1) ? 0 : 1;
    u.discrete_phase_fragment = (value >> 11) & 0x07;
    return u;
}

/* ------------------------------------------------------------------------ */
/* Registro de 23 bytes (big endian)                                        */
/* ------------------------------------------------------------------------ */

static uint64_t double_bits(double d) {
    union { double f; uint64_t i; } c;
    c.f = d;
    return c.i;
}

H7Record23 build_23byte_record(int64_t n, double signal_val) {
    H7Record23 rec;
    int  z   = n_z7(n);
    bool vac = (z == 0);

    uint16_t packed = pack_topology(vac ? 0 : z, vac ? 7 : 7 - z, vac ? 0 : 2, vac ? 0 : 1,
                                    ternary_weight_of(n, signal_val),
                                    phase_fragment_of(signal_val));

    rec.n        = (int64_t)__builtin_bswap64((uint64_t)n);
    rec.packed   = __builtin_bswap16(packed);
    rec.value_be = __builtin_bswap64(double_bits(signal_val));
    rec.origin   = 0;
    rec.updates  = 0;
    return rec;
}

int64_t rec_n(const uint8_t *r) {
    uint64_t v;
    __builtin_memcpy(&v, r + 0, 8);
    return (int64_t)__builtin_bswap64(v);
}

uint16_t rec_packed(const uint8_t *r) {
    uint16_t v;
    __builtin_memcpy(&v, r + 8, 2);
    return __builtin_bswap16(v);
}

double rec_value(const uint8_t *r) {
    union { double f; uint64_t i; } c;
    __builtin_memcpy(&c.i, r + 10, 8);
    c.i = __builtin_bswap64(c.i);
    return c.f;
}

int32_t rec_updates(const uint8_t *r) {
    uint32_t v;
    __builtin_memcpy(&v, r + 19, 4);
    return (int32_t)__builtin_bswap32(v);
}

void rec_set_updates(uint8_t *r, int32_t updates) {
    uint32_t v = __builtin_bswap32((uint32_t)updates);
    __builtin_memcpy(r + 19, &v, 4);
}

/* El registro leído como un entero big endian de 184 bits, módulo 7.
 * 2^24 ≡ 1 (mod 7): con un byte 0 delante quedan 8 palabras de 24 bits;
 * su suma (< 2^27) tiene la misma clase. Es el checksum de Internet
 * (complemento a uno) llevado a palabras múltiplo de 3 bits. */
int h7_rec_z7(const uint8_t *r) {
    uint32_t s = 0;
    for (int i = 0; i < H7_REC_SIZE; i++)
        s += (uint32_t)r[i] << (8 * (2 - (i + 1) % 3));
    return h7_fold7(s);
}
