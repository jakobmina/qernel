/*
 * rs.h — almacén Z7 de registros de 23 bytes
 *
 * 7 cubos indexados por n mod 7. Cada cubo guarda registros crudos (el formato
 * de cable, big endian) y un sello Z7. Cada ranura lleva aparte su clave n y
 * 9 bits de Hamming extendido (SECDED). Ver rs.c para las tres capas.
 */
#ifndef RS_H
#define RS_H

#include <stdint.h>
#include "h7.h"

#define RS_BUCKETS   7
#define RS_SLOTS     32
#define RS_DATA_BITS (H7_REC_SIZE * 8)             /* 184 = 192 - 8 posiciones potencia de 2 */

#define RS_BAD_KEY   1    /* el campo n del registro no coincide con su clave en el índice */
#define RS_BAD_INDEX 2    /* index de la topología no es el cubo */
#define RS_BAD_PAIR  4    /* index + pair no cierra a 111 */
#define RS_BAD_ECC   8    /* el síndrome Hamming o la paridad global no dan 0 */

#define RS_ECC_OK     0
#define RS_ECC_DATA   1   /* un bit de datos dañado: localizado, corregible */
#define RS_ECC_META   2   /* un bit del propio ECC dañado */
#define RS_ECC_DOUBLE 3   /* número par de errores (o posición inválida): detectado, no corregible */

typedef struct {
    int count;
    int sum;    /* suma Z7 recalculada ahora */
    int seal;   /* sello guardado, 1..7 */
} RsBucketInfo;

typedef struct {
    int     kind;       /* RS_ECC_* */
    int     bit;        /* bit de datos dañado (0..183) si kind == RS_ECC_DATA */
    uint8_t syndrome;   /* posición Hamming señalada (1..192) o 0 */
} RsEcc;

void            rs_init(void);
int             rs_put(int64_t n, double value);     /* devuelve updates tras escribir, -1 si lleno */
int             rs_del(int64_t n);                   /* 0, o -1 si no existe */
uint8_t        *rs_get(int64_t n);                   /* ranura del registro con clave n, o 0 */
int             rs_find(int64_t n, int *bucket, int *slot);
uint8_t        *rs_slot(int bucket, int slot);
int64_t         rs_key(int bucket, int slot);
uint16_t        rs_ecc_word(int bucket, int slot);
RsBucketInfo    rs_bucket(int bucket);
RsEcc           rs_ecc(int bucket, int slot);
int             rs_fix_slot(int bucket, int slot);   /* corrige un bit de datos; devuelve el bit o -1 */
int             rs_check(int bucket, int slot);      /* 0 o combinación de RS_BAD_* */
uint8_t         rs_verify(void);                     /* máscara de 7 bits con las tres capas */
uint8_t         rs_verify_z7(void);                  /* solo sello Z7 + invariantes, sin Hamming */
uint8_t         rs_seal(void);                       /* re-sella los cubos íntegros; devuelve los que no */
void            rs_flip(uint8_t *rec, int bit);      /* inyección de fallo: invierte 1 bit (0..183) */
int             rs_hamming_pos(int bit);             /* posición Hamming (1..192) del bit de datos */
int             rs_total(void);
const void     *rs_arena(void);

#endif
