/*
 * echo.h — experimento del eco (exp/eco_prerregistro.md)
 *
 * GHZ de 3 qubits bajo siete modelos de desfase, con y sin eco de Hahn. Cada
 * realización es un circuito OpenQASM 2.0 que corre en el vector de estado;
 * de la lectura sale c = <XXX> + i<YXX> = e^{iθ} y de M realizaciones
 * r = |media de c|, el parámetro de orden de Kuramoto de las fases θ.
 * No usa nada del kernel: compila igual en Linux (tools/echo_exp.c).
 */
#ifndef ECHO_H
#define ECHO_H

#define ECHO_MODELS 7
#define ECHO_K      8                    /* pasos; el eco va después del paso 4 */

enum { ECHO_REV, ECHO_IRR, ECHO_MIX };

typedef struct {
    char        id;                      /* 'A'..'G' */
    const char *name;
    int         M;                       /* realizaciones */
    double      pred_l, pred_e;          /* predicción en forma cerrada */
    int         cls;                     /* clase predicha */
} EchoModel;

extern const EchoModel echo_model[ECHO_MODELS];
extern const char *const echo_class_name[3];

/* Análisis añadido después del prerregistro (v0.7): no cambia clases, tolerancias
 * ni criterio. sd es la desviación típica de r (método delta sobre la covarianza
 * de las c_i) y p = exp(−M·r²) la prueba de Rayleigh: la probabilidad de un r así
 * de grande si las fases fueran uniformes, es decir, si el r verdadero fuera 0. */
typedef struct {
    double r, sd, p;
    int    exact;                        /* M = 1 (F, G): sin azar, sin sd ni p */
} EchoStat;

int echo_find(char id);                                           /* 'A'..'G' (o minúscula) -> índice, -1 si no */
int echo_stat(int m, int eco, EchoStat *s, const char **err);
int echo_qasm(char *buf, int size, int m, int eco, int yxx, int i); /* longitud, -1 si no cupo */
int echo_c(int m, int eco, int i, double *re, double *im, const char **err);
int echo_r(int m, int eco, double *r, const char **err);
int echo_class(double rl, double re);
int echo_within(int m, double rl, double re);                      /* números dentro de la tolerancia prerregistrada */

/* Corre todo (m < 0) o un modelo y escribe la tabla línea a línea. */
int echo_report(int m, void (*emit)(const char *line), const char **err);

#endif
