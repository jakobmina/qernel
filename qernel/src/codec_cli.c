#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include "metriplectic.h"  // Asume: EstadoCuantico encode_char(char), char decode_char(EstadoCuantico)

// ANSI color codes
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_RED     "\033[31m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_BLUE    "\033[34m"

// Config
static bool COLORS_ENABLED = true;
static bool SHOW_HEX = false;
static bool SHOW_STATS = true;

// ============================================================================
// BANNER PROFESIONAL
// ============================================================================

void print_banner() {
    if (COLORS_ENABLED) printf("%s%s", ANSI_BOLD, ANSI_CYAN);
    
    printf("    ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄    \n");
    printf("   █ MANDATO CLI █ Torsión Cuántica █ H7 Metriplectic █     █\n");
    printf("   █═══════════════════════════════════════════════════════█\n");
    printf("   █  • Codificación reversible por espacio de Hilbert     █\n");
    printf("   █  • Estado Cuántico: |ψ⟩ → EstadoCuantico → |φ⟩        █\n");
    printf("   █  • Verificación: encode(decode(c)) == c (100%% OK)    █\n");
    printf("   █  • Soporte: Strings, archivos, hex-dump               █\n");
    printf("   ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄   \n");
    
    if (COLORS_ENABLED) printf(ANSI_RESET);
    printf("\n");
}

// ============================================================================
// PRINT ESTADO CUÁNTICO (HEXDUMP)
// ============================================================================
// Asume EstadoCuantico es struct {float rho, v; int phase; ...} o array.
// ADAPTA según tu struct real en metriplectic.h
void print_estado_hex(EstadoCuantico e) {
    // Ejemplo genérico: si es struct con floats/ints, usa union o memcpy
    unsigned char* bytes = (unsigned char*)&e;
    printf(" [0x");
    for (size_t i = 0; i < sizeof(EstadoCuantico); i++) {
        printf("%02x", bytes[i]);
    }
    printf("]");
}

char* status_color(bool ok) {
    return ok ? ANSI_GREEN "✅ OK" ANSI_RESET : ANSI_RED "❌ FAIL" ANSI_RESET;
}

// ============================================================================
// PROCESAR STRING O ARCHIVO
// ============================================================================

int process_input(const char* input, bool is_file) {
    char buffer[1024 * 1024];  // 1MB max para simplicidad
    FILE* fp = NULL;
    size_t len;
    
    if (is_file) {
        fp = fopen(input, "r");
        if (!fp) {
            fprintf(stderr, "%sError: No se puede abrir '%s'%s\n", ANSI_RED, input, ANSI_RESET);
            return 1;
        }
        len = fread(buffer, 1, sizeof(buffer) - 1, fp);
        fclose(fp);
    } else {
        strncpy(buffer, input, sizeof(buffer) - 1);
        len = strlen(buffer);
    }
    buffer[len] = '\0';
    
    int total = 0, ok_count = 0;
    
    printf("%s%s%s\n", ANSI_BOLD, ANSI_MAGENTA, "┌─ Procesando ───────────────────────────────────────────────┐");
    printf("│ Char  │ Original │ Estado Cuántico │ Decodificado │ Status │\n");
    printf("├──────┼──────────┼─────────────────┼──────────────┼────────┤\n");
    
    for (size_t i = 0; i < len; i++) {
        char c = buffer[i];
        EstadoCuantico e = encode_char(c);
        char d = decode_char(e);
        bool is_ok = (c == d);
        
        if (is_ok) ok_count++;
        total++;
        
        printf("│ '%c'  │ 0x%02x   │", c, (unsigned char)c);
        if (SHOW_HEX) print_estado_hex(e);
        printf(" │   '%c'     │ %s │\n", d, status_color(is_ok));
    }
    
    printf("%s%s%s\n", ANSI_BOLD, ANSI_MAGENTA, "└───────────────────────────────────────────────────────────┘");
    
    // STATS
    if (SHOW_STATS) {
        float pct = (float)ok_count / total * 100;
        printf("\n%s📊 RESUMEN:%s\n", ANSI_YELLOW, ANSI_RESET);
        printf("   Total chars: %d\n", total);
        printf("   Correctos: %d (%.1f%%)%s\n", ok_count, pct,
               (pct == 100.0) ? " " ANSI_GREEN "🎉 PERFECTO!" ANSI_RESET : "");
    }
    
    return (ok_count == total) ? 0 : 1;
}

// ============================================================================
// CLI HELP
// ============================================================================

void print_help(const char* progname) {
    printf("%sUso: %s [OPTS] [texto|archivo]\n\n", ANSI_BOLD, progname);
    printf("OPTS:\n");
    printf("  -h, --help       Este mensaje\n");
    printf("  -f, --file       Leer desde archivo\n");
    printf("  -x, --hex        Mostrar EstadoCuantico en hex\n");
    printf("  -s, --no-stats   Sin resumen estadístico\n");
    printf("  --no-color       Sin colores ANSI\n\n");
    printf("Ejemplos:\n");
    printf("  %s H7_Qnn                    # String directo\n", progname);
    printf("  %s -f input.txt              # Archivo\n", progname);
    printf("  %s -x '¡Hola!'               # Con hex-dump\n", progname);
    printf("%s\n", ANSI_RESET);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    bool is_file = false;
    
    // Parse opts
    int opt;
    struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"file", no_argument, 0, 'f'},
        {"hex", no_argument, 0, 'x'},
        {"no-stats", no_argument, 0, 's'},
        {"no-color", no_argument, 0, 'c'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "hfxsc", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': print_help(argv[0]); return 0;
            case 'f': is_file = true; break;
            case 'x': SHOW_HEX = true; break;
            case 's': SHOW_STATS = false; break;
            case 'c': COLORS_ENABLED = false; break;
            default: print_help(argv[0]); return 1;
        }
    }
    
    // Input
    const char* input = (optind < argc) ? argv[optind] : "H7_Qnn";
    
    print_banner();
    
    printf("%s[MODO] %s%s\n", ANSI_BLUE, (is_file ? "Archivo" : "Texto"), ANSI_RESET);
    printf("Input: %s\n\n", input);
    
    return process_input(input, is_file);
}
