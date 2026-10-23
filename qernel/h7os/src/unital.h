/*
 * unital.h — prueba de unitalidad (exp/unital_prerregistro.md)
 *
 * Un qubit q en par de Bell con una referencia r: q empieza exactamente en
 * I/2. Tras 8 pasos de uno de seis modelos se leen <X_q>, <Y_q>, <Z_q> y
 * <Z_q Z_r>. Promediados sobre las realizaciones dan
 *   t  = vector de Bloch de Λ(I/2): |t| > 0 ⟺ el canal no es unital;
 *   z0 = t_z + w, z1 = t_z − w: <Z> de salida desde |0> y desde |1>.
 * No usa nada del kernel: compila igual en Linux (tools/unital_exp.c).
 */
#ifndef UNITAL_H
#define UNITAL_H

#define UNI_MODELS 6
#define UNI_K      8

enum { UNI_UNITAL, UNI_NONUNITAL };

typedef struct {
    char        id;                      /* 'A'..'F' */
    const char *name, *kind;
    int         M;                       /* realizaciones (D: las 256 configuraciones) */
    double      z0, z1, t;               /* predicción en forma cerrada */
    int         cls;
} UniModel;

typedef struct {
    double t[3], w;                      /* promedios */
    double z0, z1, tn;                   /* derivados: z0, z1 y |t| */
    double tmax;                         /* máx |t| de una sola realización */
} UniResult;

extern const UniModel uni_model[UNI_MODELS];
extern const char *const uni_class_name[2];

int uni_find(char id);
int uni_qasm(char *buf, int size, int m, int basis, int i);       /* basis 0 X, 1 Y, 2 Z */
int uni_measure(int m, UniResult *res, const char **err);
int uni_class(const UniResult *res);
int uni_within(int m, const UniResult *res);
int uni_report(int only, void (*emit)(const char *line), const char **err);

#endif
