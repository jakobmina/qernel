/**
 * ============================================================================
 * Módulo H7 Hexa Dec - Metriplectic Daemon C-Native
 * ============================================================================
 * Descripción: Implementación bare-metal de la codificación topológica Z7, 
 * operadores cuasiperiódicos y serialización Big Endian a 23 bytes.
 * 
 * Compilación recomendada (Linux/GCC):
 * gcc -O3 -Wall -Wextra h7_hexa_dec.c -o h7_hexa_dec -lm
 * ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

// Cabecera POSIX para macros de endianness (htobe64, htobe16, etc.) en Linux Mint
#include <endian.h>

// ============================================================================
// 1. CONSTANTES FÍSICAS Y MATEMÁTICAS H7
// ============================================================================
#define PHI 1.618033988749895
#define PI 3.141592653589793
#define THETA (PI * PHI)

// ============================================================================
// 2. ESTRUCTURAS DE DATOS (PACKING A 23 BYTES)
// ============================================================================

// Forzamos alineación de 1 byte para evitar padding del compilador
#pragma pack(push, 1)
typedef struct {
    int64_t n;          // 8 bytes (q)
    uint16_t packed;    // 2 bytes (H)
    uint64_t value_be;  // 8 bytes (d) - Guardado como entero para forzar Big Endian
    uint8_t origin;     // 1 byte  (B)
    int32_t updates;    // 4 bytes (i)
} __attribute__((packed)) H7Record23;
#pragma pack(pop)

// Estructura auxiliar para desempaquetar la topología
typedef struct {
    int index;
    int pair;
    int winding;
    int mapping;
    int ternary_weight;
    int discrete_phase_fragment;
    bool is_vacuum_superposition;
} TopoUnpacked;

// ============================================================================
// 3. LÓGICA QUARK Y OPERADOR ÁUREO
// ============================================================================

// cos(pi*phi*n) estable con exponenciación binaria
double rotate_pow(int64_t n) {
    double rc = 1.0, rs = 0.0;
    double bc = cos(THETA), bs = sin(THETA);
    int64_t e = (n < 0) ? -n : n;

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

int n_z7(int64_t n) {
    int mod = n % 7;
    return (mod < 0) ? mod + 7 : mod; // Soporte para n negativo
}

bool is_vacuum(int64_t n) {
    return n_z7(n) == 0;
}

int complement_block(int64_t n) {
    int z = n_z7(n);
    int64_t base = n - z;
    return (z == 0) ? base + 7 : base + (7 - z);
}

// Devuelve el triplete de quarks en formato string constante
const char* quark_triplet(int z) {
    if (z == 0) return "uuu+ddd";
    int b = z % 8; // 1..7 -> 001..111
    // Mapeo: bit = 0 -> u, bit = 1 -> d. (bit2, bit1, bit0) invertido lógicamente
    switch(b) {
        case 1: return "uud";
        case 2: return "udu";
        case 3: return "udd";
        case 4: return "duu";
        case 5: return "dud";
        case 6: return "ddu";
        case 7: return "ddd";
        default: return "???";
    }
}

// ============================================================================
// 4. PESOS Y FRACTALES DE FASE
// ============================================================================

int ternary_weight_of(int64_t n, double value) {
    double i_exact = (n % 2 == 0) ? 1.0 : -1.0;
    double v = value * (i_exact + 1.0);
    if (fabs(v) < 1e-9) return 0;
    return (v > 0) ? 1 : -1;
}

int phase_fragment_of(double value) {
    int frag = (int)round((value + 1.0) * 3.5);
    if (frag < 0) return 0;
    if (frag > 7) return 7;
    return frag;
}

// ============================================================================
// 5. EMPAQUETADOR TOPOLÓGICO (BITWISE)
// ============================================================================

uint16_t pack_topology(int index, int pair, int winding, int mapping, int ternary_weight, int discrete_phase_fragment) {
    uint16_t v = 0;
    
    // TERNARY_TO_BITS: -1 -> 0b00, 0 -> 0b01, 1 -> 0b10
    uint16_t weight_bits = (ternary_weight == -1) ? 0x00 : (ternary_weight == 0) ? 0x01 : 0x02;

    // Bit 0-2: Index o flag de vacío (111)
    if (index == 0) {
        v |= (0x07 << 0);
    } else {
        v |= ((index - 1) << 0);
    }

    // Bit 3-5: Pair
    v |= ((pair - 1) << 3);
    // Bit 6: Winding
    v |= ((winding / 2) << 6);
    // Bit 8: Mapping
    v |= (mapping << 8);
    // Bit 9-10: Ternary weight
    v |= (weight_bits << 9);
    // Bit 11-13: Fragment
    v |= ((discrete_phase_fragment & 0x07) << 11);

    return v;
}

TopoUnpacked unpack_topology(uint16_t value) {
    TopoUnpacked u;
    int raw_index = (value >> 0) & 0x07;
    
    u.is_vacuum_superposition = (raw_index == 0x07);
    u.index = u.is_vacuum_superposition ? 0 : raw_index + 1;
    
    u.pair = ((value >> 3) & 0x07) + 1;
    u.winding = ((value >> 6) & 0x01) * 2;
    u.mapping = (value >> 8) & 0x01;
    
    int weight_bits = (value >> 9) & 0x03;
    u.ternary_weight = (weight_bits == 0) ? -1 : (weight_bits == 1) ? 0 : 1;
    u.discrete_phase_fragment = (value >> 11) & 0x07;
    
    return u;
}

// ============================================================================
// 6. SERIALIZACIÓN DE 23 BYTES (BIG ENDIAN)
// ============================================================================

// Convierte un double a su representación uint64_t cruda para hacer swap
uint64_t double_to_uint64(double d) {
    union { double f; uint64_t i; } conv;
    conv.f = d;
    return conv.i;
}

// Construye el registro forzando el orden de bytes nativo a Big Endian
H7Record23 build_23byte_record(int64_t n, double signal_val) {
    H7Record23 rec;
    
    int z = n_z7(n);
    bool vac = is_vacuum(n);
    
    int idx_for_pack = vac ? 0 : z;
    int pair = vac ? 7 : (7 - z);
    if (!vac && z == 0) pair = 7; // Backup lógico
    
    int winding = vac ? 0 : 2;
    int mapping = vac ? 0 : 1;
    int tw = ternary_weight_of(n, signal_val);
    int frag = phase_fragment_of(signal_val);
    
    uint16_t packed = pack_topology(idx_for_pack, pair, winding, mapping, tw, frag);

    // Convertir todo a Big Endian para red / escritura binaria estricta
    rec.n = htobe64(n);
    rec.packed = htobe16(packed);
    rec.value_be = htobe64(double_to_uint64(signal_val));
    rec.origin = 0; // 1 byte no ocupa swap
    rec.updates = htobe32(0);

    return rec;
}

// ============================================================================
// 7. UTILIDAD DE PRUEBA (EQUIVALENTE A __main__)
// ============================================================================

void print_hex_buffer(const uint8_t* buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X", buffer[i]);
    }
}

int main() {
    printf("=== H7 Hadronic Vectors - pipeline cos -> n%%7 -> quark (C-Native) ===\n");
    printf("0=u, 1=d, 0&7 superposicion en multiplos de 7\n\n");

    int64_t test_ns[] = {1, 8, 15, 22, 29, 0, 7, 14, 21, 3, 10, 17, 19, 20};
    int num_tests = sizeof(test_ns) / sizeof(test_ns[0]);

    printf("%4s %2s %9s -> %4s %9s %8s %6s %4s %6s %s\n", 
           "n", "z", "quarks", "comp", "comp_q", "o_n", "packed", "BE", "m_eff", "fam");
    printf("------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < num_tests; i++) {
        int64_t n = test_ns[i];
        
        double o_n = rotate_pow(n);
        double i_n = (n % 2 != 0) ? -1.0 : 1.0;
        double double_current = i_n * o_n; // Mode double por defecto
        
        int z = n_z7(n);
        bool vac = is_vacuum(n);
        
        const char* quarks = vac ? "uuu+ddd" : quark_triplet(z);
        
        int comp_n = complement_block(n);
        int comp_z = n_z7(comp_n);
        const char* comp_quarks = is_vacuum(comp_n) ? "uuu+ddd" : quark_triplet(comp_z);
        
        int tw = ternary_weight_of(n, double_current);
        int frag = phase_fragment_of(double_current);
        
        uint16_t packed = pack_topology(vac ? 0 : z, vac ? 7 : (7-z), vac ? 0 : 2, vac ? 0 : 1, tw, frag);
        
        // Simulación de swap BE para visualización (uint16)
        uint16_t packed_be = htobe16(packed);
        
        double prob = fabs(double_current);
        double m_eff = prob * (1.0 + 0.2 * frag);
        
        bool is_toroid = (z == 1 || z == 3 || z == 5);
        const char* fam = is_toroid ? "TOROID" : (vac ? "VACUUM" : "TRANSIT");

        printf("%4ld %2d %9s -> %4d %9s %8.4f 0x%04X %04X %6.3f %s\n",
               n, z, quarks, comp_n, comp_quarks, o_n, packed, packed_be, m_eff, fam);
    }

    printf("\n=== 23-byte record hex (big endian q H d B i) ===\n");
    for (int i = 0; i < 5 && i < num_tests; i++) {
        int64_t n = test_ns[i];
        double double_current = ((n % 2 != 0) ? -1.0 : 1.0) * rotate_pow(n);
        
        H7Record23 record = build_23byte_record(n, double_current);
        
        printf("n=%2ld record=", n);
        print_hex_buffer((const uint8_t*)&record, sizeof(H7Record23));
        printf(" (23 bytes) quarks=%s\n", is_vacuum(n) ? "uuu+ddd" : quark_triplet(n_z7(n)));
    }

    return 0;
}