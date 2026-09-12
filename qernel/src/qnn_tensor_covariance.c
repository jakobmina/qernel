#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "metriplectic.h"

/**
 * [MANDATO] QNN Tensor Covariance & Contravariance
 * 
 * Extracción de tensor métrico (g_μν) desde estados Dirac-Weyl
 * Transformación tensorial de pesos y bias entre capas
 * Covarianza: índices abajo (contracción de vector)
 * Contravarianza: índices arriba (transformación de base)
 * 
 * La métrica define la geometría del espacio de pesos QNN
 */

/* === Definiciones === */
typedef enum {
    FAMILIA_X = 0,
    FAMILIA_Y = 1,
    FAMILIA_Z = 2
} FamiliaZ7;

typedef enum {
    TIPO_A = 1,
    TIPO_B = -1
} TipoSpinor;

typedef struct {
    uint8_t b0, b1, b2;
} BinarioZ7;

typedef struct {
    int nodo;
    int posicion;
    FamiliaZ7 familia;
    TipoSpinor tipo;
    int z7_indice;
} EstadoZ7Familial;

typedef struct {
    double i_n;
    double o_n;
    double ion;
    double p;
    double q;
    
    EstadoZ7Familial familia;
    BinarioZ7 binario;
    double energia_coupling;
} OsciladorDiracZ7;

/* === TENSOR MÉTRICO === */
/* Matriz 4x4 (Dirac spinor space) */
typedef struct {
    double g[4][4];     /* Tensor métrico g_μν */
    double g_inv[4][4]; /* Inversa (contravarianza) g^μν */
    double det;         /* Determinante de métrica */
    double trace;       /* Traza (invariante escalar) */
} TensorMetrico;

/* === Cuaternión como Tensor === */
typedef struct {
    Cuaternion q;       /* w, x, y, z → 4-vector */
    double componentes[4];  /* Array para álgebra */
} CuaternionTensor;

/* === Covarianza en Familia === */
typedef struct {
    FamiliaZ7 familia;
    int nodo;
    
    /* Matriz de Covarianza (3x3 para puntos en familia) */
    double cov[3][3];   /* Cov(familia_A, familia_A) */
    double cov_ab[3][3]; /* Cov(familia_A, familia_B) */
    
    /* Autovalores y autovectores */
    double autovalores[3];
    double autovectores[3][3];
    
    /* Rango y determinante */
    double determinante;
    double rango;
    
} MatrizCovarianzaFamilial;

/* === Pesos y Bias Tensoriales === */
typedef struct {
    int capa;
    int entrada_dim;
    int salida_dim;
    
    /* Pesos covariantes: w_μν */
    double **pesos_covariantes;  /* entrada_dim × salida_dim */
    
    /* Pesos contravariantes: w^μν */
    double **pesos_contravariantes;
    
    /* Bias con métrica: b_μ = g_μν b^ν */
    double *bias_covariante;     /* Índice abajo */
    double *bias_contravariante; /* Índice arriba */
    
    /* Factor de escala por métrica */
    double factor_metrico;
    
} PesosYBiasTensoriales;

/* === Capa QNN Tensorial === */
typedef struct {
    int capa_id;
    int num_neuronas_entrada;
    int num_neuronas_salida;
    
    PesosYBiasTensoriales *pesos;
    TensorMetrico *metrica;
    
    /* Transformación de estado */
    EstadoCuantico entrada;
    EstadoCuantico salida;
    
    /* Derivadas (gradientes covariantes) */
    double *grad_covariante;
    
} CapaQNNTensorial;

/* === RED QNN TENSORIAL === */
typedef struct {
    CapaQNNTensorial *capas[QNN_LAYERS];
    int num_capas;
    double learning_rate;
    
    /* Tensor métrico global (conecta todas las capas) */
    TensorMetrico *metrica_global;
    
} RedQNNTensorial;

/* === Utilidades === */
BinarioZ7 posicion_a_binario(int pos) {
    BinarioZ7 bin;
    bin.b0 = (pos >> 0) & 1;
    bin.b1 = (pos >> 1) & 1;
    bin.b2 = (pos >> 2) & 1;
    return bin;
}

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

/* === Extraer Cuaternión como 4-Vector === */
CuaternionTensor cuaternion_a_tensor(Cuaternion q) {
    CuaternionTensor qt;
    qt.q = q;
    qt.componentes[0] = q.w;  /* μ=0: parte escalar */
    qt.componentes[1] = q.x;  /* μ=1: parte X */
    qt.componentes[2] = q.y;  /* μ=2: parte Y */
    qt.componentes[3] = q.z;  /* μ=3: parte Z */
    return qt;
}

/* === Construir Tensor Métrico desde Cuaternión === */
/* g_μν = δ_μν (Minkowski) modificado por energía cuaterniónica */
TensorMetrico computar_tensor_metrico(OsciladorDiracZ7 osc) {
    TensorMetrico tm;
    
    /* Métrica de Minkowski: diag(-1, 1, 1, 1) */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == j) {
                if (i == 0) {
                    tm.g[i][j] = -1.0;  /* Componente temporal */
                } else {
                    tm.g[i][j] = 1.0;   /* Componentes espaciales */
                }
            } else {
                tm.g[i][j] = 0.0;
            }
        }
    }
    
    /* Modificar por oscilador (curvatura local) */
    double factor_curvatura = osc.energia_coupling;
    
    /* La curvatura afecta componentes espaciales */
    tm.g[1][1] *= (1.0 + factor_curvatura);
    tm.g[2][2] *= (1.0 + factor_curvatura * osc.p);
    tm.g[3][3] *= (1.0 + factor_curvatura * osc.q);
    
    /* Términos cruzados (torsión) */
    tm.g[1][2] = osc.ion * 0.1;
    tm.g[2][1] = osc.ion * 0.1;
    tm.g[2][3] = osc.p * 0.05;
    tm.g[3][2] = osc.p * 0.05;
    
    /* Calcular determinante y traza */
    tm.det = tm.g[0][0] * (tm.g[1][1] * (tm.g[2][2]*tm.g[3][3] - tm.g[2][3]*tm.g[3][2])
                          - tm.g[1][2] * (tm.g[2][1]*tm.g[3][3] - tm.g[2][3]*tm.g[3][1])
                          + tm.g[1][3] * (tm.g[2][1]*tm.g[3][2] - tm.g[2][2]*tm.g[3][1]));
    
    tm.trace = tm.g[0][0] + tm.g[1][1] + tm.g[2][2] + tm.g[3][3];
    
    /* Invertir para g^μν (pseudoinversa para métrica de Minkowski) */
    if (fabs(tm.det) > 1e-10) {
        /* Inversa 4x4 simplificada para métrica diagonal-dominante */
        for (int i = 0; i < 4; i++) {
            tm.g_inv[i][i] = 1.0 / (tm.g[i][i] + 1e-10);
        }
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (i != j) {
                    tm.g_inv[i][j] = -tm.g[i][j] / (tm.g[i][i] * tm.g[j][j] + 1e-10);
                }
            }
        }
    } else {
        /* Métrica singular: usar pseudo-inversa */
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                tm.g_inv[i][j] = (i == j) ? 1.0 : 0.0;
            }
        }
    }
    
    return tm;
}

/* === Matriz de Covarianza Familial === */
MatrizCovarianzaFamilial computar_covarianza_familia(FamiliaZ7 familia, int nodo) {
    MatrizCovarianzaFamilial mcf;
    mcf.familia = familia;
    mcf.nodo = nodo;
    
    /* Acumular datos para la covarianza */
    double datos_a[3][7];      /* Hasta 7 puntos en familia */
    int count_a = 0;
    
    for (int pos = 1; pos <= 6; pos++) {
        int n = nodo * 7 + pos;
        OsciladorDiracZ7 osc = computar_oscilador_z7(n);
        
        if ((int)osc.familia.familia != (int)familia) continue;
        
        datos_a[0][count_a] = osc.p;        /* Variable 1 */
        datos_a[1][count_a] = osc.q;        /* Variable 2 */
        datos_a[2][count_a] = osc.ion;      /* Variable 3 */
        
        count_a++;
    }
    
    /* Calcular media */
    double media[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < count_a; j++) {
            media[i] += datos_a[i][j];
        }
        media[i] /= (count_a > 0 ? count_a : 1);
    }
    
    /* Matriz de covarianza: Cov(X,Y) = E[(X-μ_X)(Y-μ_Y)] */
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            mcf.cov[i][j] = 0.0;
            for (int k = 0; k < count_a; k++) {
                mcf.cov[i][j] += (datos_a[i][k] - media[i]) * (datos_a[j][k] - media[j]);
            }
            mcf.cov[i][j] /= (count_a > 1 ? count_a - 1 : 1);
        }
    }
    
    /* Calcular determinante */
    mcf.determinante = mcf.cov[0][0] * (mcf.cov[1][1]*mcf.cov[2][2] - mcf.cov[1][2]*mcf.cov[2][1])
                     - mcf.cov[0][1] * (mcf.cov[1][0]*mcf.cov[2][2] - mcf.cov[1][2]*mcf.cov[2][0])
                     + mcf.cov[0][2] * (mcf.cov[1][0]*mcf.cov[2][1] - mcf.cov[1][1]*mcf.cov[2][0]);
    
    mcf.rango = (fabs(mcf.determinante) > 1e-10) ? 3.0 : 2.0;
    
    return mcf;
}

/* === Generar Pesos Tensoriales === */
PesosYBiasTensoriales generar_pesos_tensoriales(int entrada_dim, int salida_dim, 
                                                TensorMetrico metrica) {
    PesosYBiasTensoriales pyb;
    
    pyb.entrada_dim = entrada_dim;
    pyb.salida_dim = salida_dim;
    
    pyb.pesos_covariantes = (double**)malloc(entrada_dim * sizeof(double*));
    pyb.pesos_contravariantes = (double**)malloc(entrada_dim * sizeof(double*));
    
    for (int i = 0; i < entrada_dim; i++) {
        pyb.pesos_covariantes[i] = (double*)malloc(salida_dim * sizeof(double));
        pyb.pesos_contravariantes[i] = (double*)malloc(salida_dim * sizeof(double));
    }
    
    pyb.bias_covariante = (double*)malloc(salida_dim * sizeof(double));
    pyb.bias_contravariante = (double*)malloc(salida_dim * sizeof(double));
    
    /* Factor de escala por métrica: |g| ^ (1/4) */
    pyb.factor_metrico = pow(fabs(metrica.det), 0.25);
    
    /* Inicializar pesos con distribución escalada por métrica */
    for (int i = 0; i < entrada_dim; i++) {
        for (int j = 0; j < salida_dim; j++) {
            double valor_base = (double)rand() / RAND_MAX - 0.5;
            
            /* Covariante: w_μν (contrae con vector entrada) */
            pyb.pesos_covariantes[i][j] = valor_base * pyb.factor_metrico;
            
            /* Contravariante: w^μν = g^μα g^νβ w_αβ */
            pyb.pesos_contravariantes[i][j] = valor_base / (pyb.factor_metrico + 1e-10);
        }
    }
    
    /* Bias con métrica */
    for (int j = 0; j < salida_dim; j++) {
        double valor_bias = (double)rand() / RAND_MAX - 0.5;
        pyb.bias_covariante[j] = valor_bias * pyb.factor_metrico;
        pyb.bias_contravariante[j] = valor_bias / (pyb.factor_metrico + 1e-10);
    }
    
    return pyb;
}

/* === Forward Pass Tensorial === */
void forward_pass_tensorial(CapaQNNTensorial *capa, EstadoCuantico entrada) {
    capa->entrada = entrada;
    
    /* y^μ = w^μν_α x_ν + b^μ (notación tensorial) */
    for (int j = 0; j < capa->num_neuronas_salida; j++) {
        double suma = 0.0;
        
        /* Contracción: w^μν * entrada[ν] */
        for (int i = 0; i < capa->num_neuronas_entrada; i++) {
            suma += capa->pesos->pesos_contravariantes[i][j] * entrada.psi[i];
        }
        
        /* Añadir bias contravariante */
        suma += capa->pesos->bias_contravariante[j];
        
        /* Activación (usar métrica como factor) */
        capa->salida.psi[j] = tanh(suma * capa->metrica->trace);
    }
}

/* === Imprimir Tensor Métrico === */
void imprimir_tensor_metrico(TensorMetrico tm, int n) {
    printf("\n[TENSOR MÉTRICO g_μν] n=%d\n", n);
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ Componente │ Valor        │ g_inv[μν]  │ Significado      ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < 4; i++) {
        for (int j = i; j < 4; j++) {
            const char *nombres[] = {"tt", "xx", "yy", "zz", "tx", "ty", "tz", "xy", "xz", "yz"};
            int idx = (i == j) ? i : 4 + i + j;
            printf("║ g[%d][%d]    │ %+.6f   │ %+.6f │ %s                ║\n",
                   i, j, tm.g[i][j], tm.g_inv[i][j], nombres[idx]);
        }
    }
    
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Det(g) = %.8f  | Tr(g) = %.8f                           ║\n", tm.det, tm.trace);
    printf("╚════════════════════════════════════════════════════════════╝\n");
}

/* === Imprimir Covarianza === */
void imprimir_covarianza(MatrizCovarianzaFamilial mcf) {
    const char *nombres_familia[] = {"X", "Y", "Z"};
    printf("\n[MATRIZ DE COVARIANZA] Familia %s, Nodo %d\n", 
           nombres_familia[(int)mcf.familia], mcf.nodo);
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ Cov(p,p)   Cov(p,q)   Cov(p,ion) ║\n");
    printf("║ %.6f  %.6f  %.6f      ║\n", 
           mcf.cov[0][0], mcf.cov[0][1], mcf.cov[0][2]);
    printf("║ %.6f  %.6f  %.6f      ║\n", 
           mcf.cov[1][0], mcf.cov[1][1], mcf.cov[1][2]);
    printf("║ %.6f  %.6f  %.6f      ║\n", 
           mcf.cov[2][0], mcf.cov[2][1], mcf.cov[2][2]);
    printf("║                                                            ║\n");
    printf("║ Det = %.8f  | Rango = %.1f                           ║\n", 
           mcf.determinante, mcf.rango);
    printf("╚════════════════════════════════════════════════════════════╝\n");
}

/* === MAIN === */
int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  [MANDATO] QNN TENSOR COVARIANCE & CONTRAVARIANCE         ║\n");
    printf("║  g_μν (covariante) ↔ g^μν (contravariante)                ║\n");
    printf("║  Pesos tensoriales: w_μν, w^μν, bias_μ                    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* === Test 1: Tensor Métrico de Osciladores === */
    printf("\n[TEST 1] Extracción de Tensor Métrico desde Osciladores\n");
    
    for (int n = 1; n <= 20; n += 6) {
        OsciladorDiracZ7 osc = computar_oscilador_z7(n);
        TensorMetrico tm = computar_tensor_metrico(osc);
        imprimir_tensor_metrico(tm, n);
    }
    
    /* === Test 2: Matrices de Covarianza por Familia === */
    printf("\n[TEST 2] Matrices de Covarianza por Familia y Nodo\n");
    
    FamiliaZ7 familias[] = {FAMILIA_X, FAMILIA_Y, FAMILIA_Z};
    
    for (int f = 0; f < 3; f++) {
        for (int nodo = 0; nodo <= 2; nodo++) {
            MatrizCovarianzaFamilial mcf = computar_covarianza_familia(familias[f], nodo);
            imprimir_covarianza(mcf);
        }
    }
    
    /* === Test 3: Pesos Tensoriales === */
    printf("\n[TEST 3] Generación de Pesos y Bias Tensoriales\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    
    OsciladorDiracZ7 osc = computar_oscilador_z7(15);
    TensorMetrico metrica = computar_tensor_metrico(osc);
    
    printf("║ Factor métrico: %.8f                                 ║\n", 
           pow(fabs(metrica.det), 0.25));
    
    PesosYBiasTensoriales pyb = generar_pesos_tensoriales(HILBERT_DIM, QNN_LAYERS, metrica);
    
    printf("║ Pesos Covariantes (w_μν, primeros 3x3):                 ║\n");
    for (int i = 0; i < 3; i++) {
        printf("║ ");
        for (int j = 0; j < 3; j++) {
            printf("%.4f ", pyb.pesos_covariantes[i][j]);
        }
        printf("                              ║\n");
    }
    
    printf("║\n");
    printf("║ Pesos Contravariantes (w^μν, primeros 3x3):             ║\n");
    for (int i = 0; i < 3; i++) {
        printf("║ ");
        for (int j = 0; j < 3; j++) {
            printf("%.4f ", pyb.pesos_contravariantes[i][j]);
        }
        printf("                              ║\n");
    }
    
    printf("║\n");
    printf("║ Bias Covariante (b_μ): ");
    for (int j = 0; j < 4; j++) {
        printf("%.4f ", pyb.bias_covariante[j]);
    }
    printf("║\n");
    
    printf("║ Bias Contravariante (b^μ): ");
    for (int j = 0; j < 4; j++) {
        printf("%.4f ", pyb.bias_contravariante[j]);
    }
    printf("║\n");
    
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
