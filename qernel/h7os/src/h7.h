/*
 * h7.h — núcleo H7 freestanding (port de h7_hexa_dec.c para H7-OS)
 *
 * Sin libc ni libm:
 *   - n mod 7 sin división: plegado octal (8 ≡ 1 mod 7), con 000 y 111 como
 *     las dos representaciones del cero (complemento a uno en 3 bits).
 *   - cos(pi*phi*n) con exponenciación binaria sobre constantes precalculadas.
 *   - big endian con __builtin_bswap*.
 */
#ifndef H7_H
#define H7_H

#include <stdint.h>
#include <stdbool.h>

#define H7_REC_SIZE 23

/* Registro de 23 bytes, formato de cable "q H d B i" en big endian. */
typedef struct __attribute__((packed)) {
    int64_t  n;          /* q: 8 B */
    uint16_t packed;     /* H: 2 B, topología */
    uint64_t value_be;   /* d: 8 B, double crudo */
    uint8_t  origin;     /* B: 1 B */
    int32_t  updates;    /* i: 4 B */
} H7Record23;

_Static_assert(sizeof(H7Record23) == H7_REC_SIZE, "H7Record23 debe medir 23 bytes");

typedef struct {
    int  index;
    int  pair;
    int  winding;
    int  mapping;
    int  ternary_weight;
    int  discrete_phase_fragment;
    bool is_vacuum_superposition;
} TopoUnpacked;

/* Aritmética Z7 */
int          h7_fold7(uint64_t u);             /* u mod 7 por plegado octal */
int          n_z7(int64_t n);
bool         is_vacuum(int64_t n);
int64_t      complement_block(int64_t n);      /* n - z + (z XOR 7) */
const char  *quark_triplet(int z);

/* Operador áureo y pesos */
double       rotate_pow(int64_t n);            /* cos(pi*phi*n) */
double       h7_current(int64_t n);            /* (-1)^n * cos(pi*phi*n) */

/* Fase áurea exacta (v0.7): n·φ y n·φ/2 módulo 1 en punto fijo de 128 bits,
 * solo con enteros. rotate_pow arrastra el redondeo de cos(πφ) multiplicado
 * por n; aquí la constante tiene error ≤ 2^-129, así que |n| < 2^63 deja la
 * fase con error < 2^-66. rotate_pow y los registros no cambian: `ref` sigue
 * idéntico a h7_hexa_dec.c. */
uint64_t     h7_label(int64_t n);              /* frac(n·φ) · 2^64 (64 bits altos de 128) */
double       h7_golden(int64_t n);             /* cos(pi*phi*n) desde la fase exacta */
int          ternary_weight_of(int64_t n, double value);
int          phase_fragment_of(double value);

/* Topología de 16 bits */
uint16_t     pack_topology(int index, int pair, int winding, int mapping,
                           int ternary_weight, int discrete_phase_fragment);
TopoUnpacked unpack_topology(uint16_t value);

/* Registro de 23 bytes */
H7Record23   build_23byte_record(int64_t n, double signal_val);
int64_t      rec_n(const uint8_t *r);
uint16_t     rec_packed(const uint8_t *r);
double       rec_value(const uint8_t *r);
int32_t      rec_updates(const uint8_t *r);
void         rec_set_updates(uint8_t *r, int32_t updates);
int          h7_rec_z7(const uint8_t *r);      /* el registro como entero de 184 bits, mod 7 */

#endif
