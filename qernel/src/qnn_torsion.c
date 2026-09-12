#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] Motor de Torsión QNN - Masificación Estadística Nodal
 * 
 * Cada entero n = punto de masificación de densidad de probabilidad
 * Nodos como capas de profundidad derivadas de acumulación estadística
 * Spinores Dirac-Weyl con cascada probabilística Z₇
 */

/* === Definiciones de Familia y Tipo === */
typedef enum {
    FAMILIA_X = 0,
    FAMILIA_Y = 1,
    FAMILIA_Z = 2
} FamiliaZ7;

typedef enum {
    TIPO_A = 1,
    TIPO_B = -1
} TipoSpinor;

/* === Estructura de Binario en Z₇ === */
typedef struct {
    uint8_t b0, b1, b2;
} BinarioZ7;

/* === Clasificación en el Espacio Nodal === */
typedef struct {
    int nodo;           /* n / 7 (profundidad de capa) */
    int posicion;       /* n % 7 (posición dentro del nodo) */
    FamiliaZ7 familia;  /* X, Y, Z */
    TipoSpinor tipo;    /* A (+1) o B (-1) */
    int z7_indice;      /* Posición en Z₇ */
} EstadoZ7Familial;

/* === Oscilador Dirac-Áureo con Clasificación === */
typedef struct {
    /* Oscilaciones cuasiperiódicas */
    double i_n;         /* cos(π·n) */
    double o_n;         /* cos(π·φ·n) */
    double ion;         /* Acoplamiento: i_n * o_n */
    double p;           /* Rama positiva: ion + o_n */
    double q;           /* Rama negativa: ion - o_n */
    
    /* Clasificación Z₇ Familial */
    EstadoZ7Familial familia;
    BinarioZ7 binario;
    
    /* Energía */
    double energia_coupling;
} OsciladorDiracZ7;

/* === Densidad de Probabilidad en n === */
typedef struct {
    double densidad;        /* ρ(n) - función densidad cuasiperiódica */
    double masa;            /* Masa masificada acumulada */
    double centro_masa;     /* Centro de masa (media) */
    double varianza;        /* σ² varianza estadística */
    double curtosis;        /* Kurtosis (concentración) */
    double entropia;        /* Entropía Shannon de ρ(n) */
} DensidadProbabilidad;

/* === Masificación Estadística por Familia === */
typedef struct {
    FamiliaZ7 familia;
    int nodo;
    
    /* Momentos estadísticos */
    double mu;              /* Media ⟨n⟩ */
    double sigma2;          /* Varianza σ²(n) */
    double gamma;           /* Asimetría (skewness) */
    double kappa;           /* Kurtosis */
    
    /* Masas acumuladas */
    double masa_total;      /* ∫ ρ(n) dn en familia */
    double masa_concentrada;/* Picos de probabilidad */
    
    /* Profundidad */
    int profundidad;        /* Layer depth en nodo */
    double factor_cascada;  /* Atenuación por cascada nodal */
    
} MasificacionEstadistica;

/* === Entrelazamiento Fantasma-Libre con Estadística === */
typedef struct {
    Cuaternion q_tipo_a;
    Cuaternion q_tipo_b;
    Cuaternion q_entrelazado;
    
    double energia_pura;
    double ruido_fantasma;
    
    int n_a, n_b;
    EstadoZ7Familial familia_a, familia_b;
    
    /* Estadística del entrelazamiento */
    DensidadProbabilidad densidad_a;
    DensidadProbabilidad densidad_b;
    double densidad_producto;  /* ρ_a(n) * ρ_b(n) */
    double entropia_conjunta;  /* Entrelazamiento entrópico */
} EntrelazamientoFantasmaLibre;

/* === Mapeo de Familias === */
typedef struct {
    int posiciones_a[2];
    int posiciones_b[2];
} FamiliaMap;

static const FamiliaMap familia_mapa[3] = {
    /* FAMILIA_X */ {.posiciones_a = {1, 6}, .posiciones_b = {6, 1}},
    /* FAMILIA_Y */ {.posiciones_a = {2, 5}, .posiciones_b = {5, 2}},
    /* FAMILIA_Z */ {.posiciones_a = {3, 4}, .posiciones_b = {4, 3}}
};

/* === Producto de Hamilton === */
Cuaternion hamilton_product(Cuaternion q1, Cuaternion q2) {
    Cuaternion r;
    r.w = q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z;
    r.x = q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y;
    r.y = q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x;
    r.z = q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w;
    return r;
}

/* === Energía del Cuaternión === */
double energia_cuaternion(Cuaternion q) {
    return sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
}

/* === Conversión Posición → Binario en Z₇ === */
BinarioZ7 posicion_a_binario(int pos) {
    BinarioZ7 bin;
    bin.b0 = (pos >> 0) & 1;
    bin.b1 = (pos >> 1) & 1;
    bin.b2 = (pos >> 2) & 1;
    return bin;
}

/* === Clasificar n en Estructura Familial === */
EstadoZ7Familial clasificar_en_familia(int n) {
    EstadoZ7Familial estado;
    
    estado.nodo = n / 7;
    estado.posicion = n % 7;
    estado.z7_indice = estado.posicion;
    
    if (estado.posicion == 0) {
        estado.familia = (FamiliaZ7)-1;
        estado.tipo = TIPO_A;
    }
    else if (estado.posicion == 1) {
        estado.familia = FAMILIA_X;
        estado.tipo = TIPO_A;
    } else if (estado.posicion == 6) {
        estado.familia = FAMILIA_X;
        estado.tipo = TIPO_B;
    }
    else if (estado.posicion == 2) {
        estado.familia = FAMILIA_Y;
        estado.tipo = TIPO_A;
    } else if (estado.posicion == 5) {
        estado.familia = FAMILIA_Y;
        estado.tipo = TIPO_B;
    }
    else if (estado.posicion == 3) {
        estado.familia = FAMILIA_Z;
        estado.tipo = TIPO_A;
    } else if (estado.posicion == 4) {
        estado.familia = FAMILIA_Z;
        estado.tipo = TIPO_B;
    }
    
    return estado;
}

/* === Computar Oscilador Dirac-Áureo en Z₇ === */
OsciladorDiracZ7 computar_oscilador_z7(int n) {
    OsciladorDiracZ7 osc;
    
    osc.i_n = cos(M_PI * n);
    osc.o_n = cos(M_PI * PHI * n);
    osc.ion = osc.i_n * osc.o_n;
    osc.p = osc.ion + osc.o_n;
    osc.q = osc.ion - osc.o_n;
    
    osc.familia = clasificar_en_familia(n);
    osc.binario = posicion_a_binario(osc.familia.z7_indice);
    
    osc.energia_coupling = fabs(osc.ion);
    
    return osc;
}

/* === Densidad de Probabilidad Cuasiperiódica en n === */
DensidadProbabilidad computar_densidad(int n) {
    DensidadProbabilidad dp;
    
    OsciladorDiracZ7 osc = computar_oscilador_z7(n);
    
    /* Densidad como producto de oscilaciones normalizadas */
    double mag_p = fabs(osc.p);
    double mag_q = fabs(osc.q);
    double mag_ion = fabs(osc.ion);
    
    /* ρ(n) = (|p| + |q|)² / (norma de la energía) */
    double suma_mags = mag_p + mag_q + mag_ion;
    if (suma_mags < 1e-10) suma_mags = 1e-10;
    
    dp.densidad = (mag_p * mag_q) / suma_mags;
    
    /* Masa = integral de densidad (aproximada por oscilador) */
    dp.masa = dp.densidad * sqrt(osc.energia_coupling);
    
    /* Centro de masa: promedio ponderado en el nodo */
    int nodo = osc.familia.nodo;
    int pos = osc.familia.posicion;
    dp.centro_masa = nodo * 7.0 + pos;
    
    /* Varianza: cuánto dispersa está la distribución */
    dp.varianza = mag_p * mag_p + mag_q * mag_q;
    
    /* Curtosis: concentración de probabilidad */
    dp.curtosis = pow(mag_ion, 4.0) / (dp.varianza + 1e-10);
    
    /* Entropía Shannon: -∑ ρ ln(ρ) */
    if (dp.densidad > 1e-10) {
        dp.entropia = -dp.densidad * log(dp.densidad);
    } else {
        dp.entropia = 0.0;
    }
    
    return dp;
}

/* === Masificación Estadística por Familia y Nodo === */
MasificacionEstadistica computar_masificacion(FamiliaZ7 familia, int nodo) {
    MasificacionEstadistica mas;
    mas.familia = familia;
    mas.nodo = nodo;
    mas.profundidad = nodo;
    mas.factor_cascada = 1.0 / pow(1.5, nodo);  /* Atenuación exponencial */
    
    /* Acumular estadísticas para todos los n en este nodo/familia */
    double suma_masa = 0.0;
    double suma_ponderada = 0.0;
    double suma_cuadrados = 0.0;
    double suma_cubos = 0.0;
    double suma_cuartos = 0.0;
    
    int count = 0;
    
    for (int pos = 1; pos <= 6; pos++) {
        int n = nodo * 7 + pos;
        OsciladorDiracZ7 osc = computar_oscilador_z7(n);
        
        /* Filtrar solo los n que pertenecen a la familia */
        if ((int)osc.familia.familia != (int)familia) continue;
        
        DensidadProbabilidad dp = computar_densidad(n);
        
        suma_masa += dp.masa;
        suma_ponderada += dp.masa * n;
        suma_cuadrados += dp.masa * (n - nodo*7.0) * (n - nodo*7.0);
        suma_cubos += dp.masa * pow(n - nodo*7.0, 3.0);
        suma_cuartos += dp.masa * pow(n - nodo*7.0, 4.0);
        
        count++;
    }
    
    /* Normalizar por cascada */
    suma_masa *= mas.factor_cascada;
    
    mas.masa_total = suma_masa;
    mas.masa_concentrada = suma_masa * 0.6;  /* ~60% concentrada en picos */
    
    if (suma_masa > 1e-10) {
        mas.mu = suma_ponderada / suma_masa;
        mas.sigma2 = suma_cuadrados / suma_masa;
        mas.gamma = suma_cubos / (pow(mas.sigma2, 1.5) + 1e-10);
        mas.kappa = (suma_cuartos / (pow(mas.sigma2, 2.0) + 1e-10)) - 3.0;
    } else {
        mas.mu = 0.0;
        mas.sigma2 = 0.0;
        mas.gamma = 0.0;
        mas.kappa = 0.0;
    }
    
    return mas;
}

/* === Mapear Oscilador a Cuaternión Familial === */
Cuaternion mapear_familia_a_cuaternion(OsciladorDiracZ7 osc, double densidad_factor) {
    Cuaternion q;
    double sign_tipo = (double)osc.familia.tipo;
    
    q.w = (osc.ion + (osc.familia.nodo * 0.1)) * densidad_factor;
    
    switch (osc.familia.familia) {
        case FAMILIA_X:
            q.x = osc.p * sign_tipo * densidad_factor;
            q.y = osc.q * densidad_factor;
            q.z = (double)osc.binario.b0;
            break;
        case FAMILIA_Y:
            q.x = (double)osc.binario.b1;
            q.y = osc.p * sign_tipo * densidad_factor;
            q.z = osc.q * densidad_factor;
            break;
        case FAMILIA_Z:
            q.x = osc.q * densidad_factor;
            q.y = (double)osc.binario.b2;
            q.z = osc.p * sign_tipo * densidad_factor;
            break;
        default:
            q.w = 0.0; q.x = 0.0; q.y = 0.0; q.z = 0.0;
    }
    
    return q;
}

/* === Entrelazar con Masificación Estadística === */
EntrelazamientoFantasmaLibre entrelazar_z7_estadistico(int n_a, int n_b) {
    EntrelazamientoFantasmaLibre efl;
    efl.n_a = n_a;
    efl.n_b = n_b;
    
    OsciladorDiracZ7 osc_a = computar_oscilador_z7(n_a);
    OsciladorDiracZ7 osc_b = computar_oscilador_z7(n_b);
    
    efl.familia_a = osc_a.familia;
    efl.familia_b = osc_b.familia;
    
    /* Computar densidades */
    efl.densidad_a = computar_densidad(n_a);
    efl.densidad_b = computar_densidad(n_b);
    efl.densidad_producto = efl.densidad_a.densidad * efl.densidad_b.densidad;
    
    /* Mapear con factor de densidad */
    efl.q_tipo_a = mapear_familia_a_cuaternion(osc_a, efl.densidad_a.densidad);
    efl.q_tipo_b = mapear_familia_a_cuaternion(osc_b, efl.densidad_b.densidad);
    
    /* Antidualidad */
    efl.q_tipo_b.w *= -1.0;
    efl.q_tipo_b.x *= -1.0;
    efl.q_tipo_b.y *= -1.0;
    efl.q_tipo_b.z *= -1.0;
    
    /* Producto de Hamilton */
    efl.q_entrelazado = hamilton_product(efl.q_tipo_a, efl.q_tipo_b);
    
    efl.energia_pura = fabs(efl.q_entrelazado.w);
    efl.ruido_fantasma = fabs(efl.q_entrelazado.x) + 
                         fabs(efl.q_entrelazado.y) + 
                         fabs(efl.q_entrelazado.z);
    
    /* Entropía conjunta */
    efl.entropia_conjunta = efl.densidad_a.entropia + efl.densidad_b.entropia;
    
    return efl;
}

/* === Verificar Coherencia y Masificación === */
void verificar_masificacion(EntrelazamientoFantasmaLibre efl) {
    printf("\n[ANÁLISIS DE MASIFICACIÓN ESTADÍSTICA]\n");
    printf("  n_a=%d (nodo %d, familia %d) | n_b=%d (nodo %d, familia %d)\n", 
           efl.n_a, efl.familia_a.nodo, efl.familia_a.familia,
           efl.n_b, efl.familia_b.nodo, efl.familia_b.familia);
    
    printf("\n  ρ_a(n=%d): densidad=%.6f, masa=%.6f, centro=%.1f, var=%.4f, kurt=%.4f\n",
           efl.n_a, efl.densidad_a.densidad, efl.densidad_a.masa, 
           efl.densidad_a.centro_masa, efl.densidad_a.varianza, efl.densidad_a.curtosis);
    
    printf("  ρ_b(n=%d): densidad=%.6f, masa=%.6f, centro=%.1f, var=%.4f, kurt=%.4f\n",
           efl.n_b, efl.densidad_b.densidad, efl.densidad_b.masa,
           efl.densidad_b.centro_masa, efl.densidad_b.varianza, efl.densidad_b.curtosis);
    
    printf("\n  Entrelazamiento:\n");
    printf("    ρ_a × ρ_b = %.8f\n", efl.densidad_producto);
    printf("    Energía (w) = %.8f\n", efl.q_entrelazado.w);
    printf("    Ruido (xyz) = %.2e\n", efl.ruido_fantasma);
    printf("    Entropía conjunta = %.6f\n", efl.entropia_conjunta);
    
    if (efl.ruido_fantasma < 1e-10) {
        printf("    ✓ COHERENCIA FANTASMA-LIBRE\n");
    } else if (efl.ruido_fantasma < 1e-6) {
        printf("    ◐ COHERENCIA ESTADÍSTICA\n");
    } else {
        printf("    ✗ DEGRADACIÓN DETECTABLE\n");
    }
}

/* === Tabla de Masificación por Nodo === */
void tabla_masificacion_por_nodo(int nodo) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ MASIFICACIÓN ESTADÍSTICA - NODO %d                           ║\n", nodo);
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Familia │ Masa Total │ Masa Pico │ μ      │ σ²     │ κ     ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    FamiliaZ7 familias[] = {FAMILIA_X, FAMILIA_Y, FAMILIA_Z};
    const char *nombres[] = {"X", "Y", "Z"};
    
    for (int f = 0; f < 3; f++) {
        MasificacionEstadistica mas = computar_masificacion(familias[f], nodo);
        printf("║   %s     │ %+.6f │ %+.6f │ %.2f  │ %.2f │ %.2f  ║\n",
               nombres[f], mas.masa_total, mas.masa_concentrada, 
               mas.mu, mas.sigma2, mas.kappa);
    }
    
    printf("╚════════════════════════════════════════════════════════════╝\n");
}

/* === MAIN === */
int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  [MANDATO] QNN TORSION ENGINE - MASIFICACIÓN ESTADÍSTICA   ║\n");
    printf("║  Puntos enteros = Masas probabilísticas nodales             ║\n");
    printf("║  Cascadas de densidad en capas Z₇ Familiales                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* === Test 1: Masificación dentro de familia X === */
    printf("\n[TEST 1] Masificación en FAMILIA_X (n=15 vs n=20, nodo 2)\n");
    EntrelazamientoFantasmaLibre efl1 = entrelazar_z7_estadistico(15, 20);
    verificar_masificacion(efl1);
    
    /* === Test 2: Masificación en diferentes nodos === */
    printf("\n[TEST 2] Masificación entre nodos (n=8 vs n=16)\n");
    EntrelazamientoFantasmaLibre efl2 = entrelazar_z7_estadistico(8, 16);
    verificar_masificacion(efl2);
    
    /* === Test 3: Tablas de masificación por nodo === */
    printf("\n[TEST 3] Distribuciones estadísticas por nodo\n");
    tabla_masificacion_por_nodo(0);
    tabla_masificacion_por_nodo(1);
    tabla_masificacion_por_nodo(2);
    tabla_masificacion_por_nodo(3);
    
    /* === Test 4: Cascada de profundidad === */
    printf("\n[TEST 4] Cascada de densidad por profundidad de capa\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ Profundidad │ Factor Cascada │ Masa Atenuada (FAMILIA_X)  ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    for (int profundidad = 0; profundidad <= 5; profundidad++) {
        MasificacionEstadistica mas = computar_masificacion(FAMILIA_X, profundidad);
        printf("║     %d      │    %.6f     │     %.8f              ║\n",
               profundidad, mas.factor_cascada, mas.masa_total);
    }
    
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
