#include <stdio.h>
#include <string.h>
#include "metriplectic.h"

int main(int argc, char** argv) {
    const char* texto = (argc > 1) ? argv[1] : "H7_Qnn";
    int len = strlen(texto);
    printf("[MANDATO] CLI de Torsión Cuántica\n");
    
    for (int i = 0; i < len; i++) {
        EstadoCuantico e = encode_char(texto[i]);
        char d = decode_char(e);
        printf(" '%c' -> [Hilbert] -> '%c' (%s)\n", texto[i], d, (texto[i] == d) ? "OK" : "FAIL");
    }
    return 0;
}
