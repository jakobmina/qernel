/*
 * rs.c — almacén Z7 de registros de 23 bytes (ver rs.h)
 *
 * Tres capas de integridad, de la más barata a la más fuerte:
 *
 *  1. Sello Z7 por cubo: suma de las clases mod 7 de sus registros, XOR 111.
 *     Detecta todo flip de 1 bit; deja pasar algunos pares.
 *  2. Índice aparte (clave n de cada ranura) e invariantes del registro.
 *  3. Hamming extendido (SECDED) por registro. Los 184 bits de datos ocupan
 *     las posiciones 1..192 que no son potencia de 2 (son exactamente 184).
 *     El síndrome es el XOR de las posiciones de los bits encendidos, así que
 *     un bit dañado en la posición p da síndrome p: se localiza y se corrige.
 *     La paridad global separa 1 error (impar) de 2 (par): detecta todos los
 *     pares.
 *
 * El índice y los 9 bits de ECC viven fuera del registro, como metadatos.
 */
#include "rs.h"

static uint8_t  arena[RS_BUCKETS][RS_SLOTS][H7_REC_SIZE];
static int64_t  key[RS_BUCKETS][RS_SLOTS];
static uint16_t ecc[RS_BUCKETS][RS_SLOTS];        /* bits 0-7: paridad Hamming P, bit 8: paridad global */
static int      count[RS_BUCKETS];
static uint8_t  seal[RS_BUCKETS];

static uint8_t pos_of[RS_DATA_BITS];               /* posición Hamming de cada bit de datos */
static int16_t bit_at[256];                        /* inversa: bit de datos en esa posición, o -1 */

static int pop8(uint32_t x) { int c = 0; while (x) { c += (int)(x & 1); x >>= 1; } return c; }

static void ecc_init(void) {
    int p = 1;
    for (int i = 0; i < 256; i++) bit_at[i] = -1;
    for (int i = 0; i < RS_DATA_BITS; i++) {
        while ((p & (p - 1)) == 0) p++;            /* salta 1, 2, 4, ..., 128 */
        pos_of[i] = (uint8_t)p;
        bit_at[p] = (int16_t)i;
        p++;
    }
}

/* XOR de las posiciones de los bits encendidos y cuántos hay. */
static uint8_t syndrome_of(const uint8_t *r, int *ones) {
    uint8_t s = 0;
    int n = 0;
    for (int i = 0; i < RS_DATA_BITS; i++)
        if (r[i >> 3] & (0x80u >> (i & 7))) { s ^= pos_of[i]; n++; }
    *ones = n;
    return s;
}

static uint16_t ecc_compute(const uint8_t *r) {
    int ones;
    uint8_t P = syndrome_of(r, &ones);             /* con P, el XOR total de posiciones queda en 0 */
    int q = (ones + pop8(P)) & 1;                  /* paridad global par */
    return (uint16_t)(P | (q << 8));
}

static int bucket_sum(int b) {
    uint32_t s = 0;
    for (int i = 0; i < count[b]; i++) s += (uint32_t)h7_rec_z7(arena[b][i]);
    return h7_fold7(s);
}

/* sello = suma XOR 111; suma 0 da sello 111: el vacío en su otra cara */
static void seal_add(int b, int delta) {           /* delta en -6..6 */
    int s = seal[b] ^ 7;
    seal[b] = (uint8_t)(h7_fold7((uint64_t)(s + 7 + delta)) ^ 7);
}

static int find(int b, int64_t n) {
    for (int i = 0; i < count[b]; i++)
        if (key[b][i] == n) return i;
    return -1;
}

void rs_init(void) {
    ecc_init();
    for (int b = 0; b < RS_BUCKETS; b++) { count[b] = 0; seal[b] = 7; }
}

int rs_put(int64_t n, double value) {
    int b = n_z7(n);
    int i = find(b, n);
    int32_t updates = 0;

    if (i >= 0) {
        updates = rec_updates(arena[b][i]) + 1;
        seal_add(b, -h7_rec_z7(arena[b][i]));
    } else {
        if (count[b] == RS_SLOTS) return -1;
        i = count[b]++;
        key[b][i] = n;
    }
    H7Record23 rec = build_23byte_record(n, value);
    __builtin_memcpy(arena[b][i], &rec, H7_REC_SIZE);
    rec_set_updates(arena[b][i], updates);
    seal_add(b, h7_rec_z7(arena[b][i]));
    ecc[b][i] = ecc_compute(arena[b][i]);
    return updates;
}

int rs_del(int64_t n) {
    int b = n_z7(n);
    int i = find(b, n);
    if (i < 0) return -1;
    seal_add(b, -h7_rec_z7(arena[b][i]));
    if (i != --count[b]) {
        __builtin_memcpy(arena[b][i], arena[b][count[b]], H7_REC_SIZE);
        key[b][i] = key[b][count[b]];
        ecc[b][i] = ecc[b][count[b]];
    }
    return 0;
}

uint8_t *rs_get(int64_t n) {
    int b = n_z7(n);
    int i = find(b, n);
    return (i < 0) ? 0 : arena[b][i];
}

int rs_find(int64_t n, int *bucket, int *slot) {
    int b = n_z7(n);
    int i = find(b, n);
    if (i < 0) return -1;
    *bucket = b;
    *slot = i;
    return 0;
}

uint8_t *rs_slot(int bucket, int slot) { return arena[bucket][slot]; }
int64_t  rs_key(int bucket, int slot)  { return key[bucket][slot]; }
uint16_t rs_ecc_word(int bucket, int slot) { return ecc[bucket][slot]; }

RsBucketInfo rs_bucket(int b) {
    RsBucketInfo info = { count[b], bucket_sum(b), seal[b] };
    return info;
}

RsEcc rs_ecc(int b, int i) {
    RsEcc e = { RS_ECC_OK, -1, 0 };
    int ones;
    uint8_t P = (uint8_t)ecc[b][i];
    int q = (ecc[b][i] >> 8) & 1;
    uint8_t s = (uint8_t)(P ^ syndrome_of(arena[b][i], &ones));
    int odd = (ones + pop8(P) + q) & 1;
    e.syndrome = s;
    if (!s && !odd) return e;
    if (!odd)                     e.kind = RS_ECC_DOUBLE;                 /* par de errores */
    else if (!s)                  e.kind = RS_ECC_META;                   /* el bit de paridad global */
    else if (bit_at[s] >= 0)    { e.kind = RS_ECC_DATA; e.bit = bit_at[s]; }
    else if ((s & (s - 1)) == 0)  e.kind = RS_ECC_META;                   /* un bit de P */
    else                          e.kind = RS_ECC_DOUBLE;                 /* 3 o más: posición inválida */
    return e;
}

int rs_fix_slot(int b, int i) {
    RsEcc e = rs_ecc(b, i);
    if (e.kind != RS_ECC_DATA) return -1;
    rs_flip(arena[b][i], e.bit);
    return e.bit;
}

/* Invariantes propios del registro, sin el ECC. */
static int check_base(int b, int i) {
    const uint8_t *r = arena[b][i];
    TopoUnpacked t = unpack_topology(rec_packed(r));
    int why = 0;
    if (rec_n(r) != key[b][i]) why |= RS_BAD_KEY;
    if (t.index != b) why |= RS_BAD_INDEX;
    if (h7_fold7((uint64_t)(t.index + t.pair)) != 0) why |= RS_BAD_PAIR;
    return why;
}

int rs_check(int b, int i) {
    return check_base(b, i) | (rs_ecc(b, i).kind != RS_ECC_OK ? RS_BAD_ECC : 0);
}

static uint8_t verify_with(int use_ecc) {
    uint8_t mask = 0;
    for (int b = 0; b < RS_BUCKETS; b++) {
        int ok = h7_fold7((uint64_t)(bucket_sum(b) + seal[b])) == 0;
        for (int i = 0; ok && i < count[b]; i++) ok = ((use_ecc ? rs_check(b, i) : check_base(b, i)) == 0);
        if (!ok) mask |= (uint8_t)(1u << b);
    }
    return mask;
}

uint8_t rs_verify(void)    { return verify_with(1); }
uint8_t rs_verify_z7(void) { return verify_with(0); }

uint8_t rs_seal(void) {
    uint8_t refused = 0;
    for (int b = 0; b < RS_BUCKETS; b++) {
        int bad = 0;
        for (int i = 0; i < count[b]; i++) bad |= rs_check(b, i);
        if (bad) refused |= (uint8_t)(1u << b);
        else seal[b] = (uint8_t)(bucket_sum(b) ^ 7);
    }
    return refused;
}

/* Se opera sobre la ranura física. */
void rs_flip(uint8_t *r, int bit) {
    r[bit >> 3] ^= (uint8_t)(0x80u >> (bit & 7));
}

int rs_hamming_pos(int bit) { return pos_of[bit]; }

int rs_total(void) {
    int t = 0;
    for (int b = 0; b < RS_BUCKETS; b++) t += count[b];
    return t;
}

const void *rs_arena(void) { return arena; }
