/*
 * core.c — comando "core"
 *
 * Motor metripléctico (qernel/baremetal/src/qcore_metriplectic.c) portado a
 * H7-OS: mismo operador áureo, mismo campo toroidal Φ sobre T², mismo lazo
 * de control del "lavado áureo" (PWM que persigue RMS → φ) y el mismo
 * diagnóstico de Barbashin-LaSalle. Dos diferencias de fondo con el puerto
 * bare-metal original:
 *
 *   - aquí todo es `double`, y seno/coseno/raíz salen de q_sincos/q_sqrt
 *     (qsv.c) en vez de una serie de Taylor propia: mismos 53 bits de
 *     x87 que ya usa el resto de H7-OS, sin duplicar matemática.
 *   - los campos que en el original nunca se leían en pantalla y se
 *     quedaban siempre en 0 (audio_*, vortex_z, rayleigh_raw, l2_error,
 *     thermal_eff, node_density) no se portan: no cambian ningún número
 *     visible, así que cargarlos aquí solo sería ruido.
 *
 * Se le suma el NodeRouter (qernel/baremetal/src/node_router.c): cada tick
 * clasifica y ordena, de un solo hilo, las 64 intensidades del campo
 * toroidal en 4 nodos por rango de clave — el mismo diseño, sin pthreads ni
 * heap, que ya corre en el kernel bare-metal.
 *
 * Sin temporizador (IRQ0, ver README): el ritmo de cuadro es espera activa,
 * igual que el resto de H7-OS; con_poll() se consulta varias veces por
 * cuadro para que ESC salga al shell sin esperar a que termine.
 */
#include <stdbool.h>
#include <stdint.h>
#include "con.h"
#include "core.h"
#include "qsv.h"

#define QC_PHI     1.6180339887498949
#define QC_PI      3.1415926535897931
#define QC_PI_PHI  5.0832036921675656   /* PI * PHI: ancla el eje azimutal (ver h7.c) */
#define TORUS_DIM  8
#define QC_NODES   4
#define QC_ROUTER_CAP 32                /* 64 claves / 4 nodos: generoso para cualquier reparto */

enum { C_BLACK=0, C_GREEN=2, C_CYAN=3, C_RED=4, C_MAGENTA=5, C_LGRAY=7, C_DGRAY=8,
       C_LBLUE=9, C_YELLOW=0xE, C_WHITE=0xF };

typedef struct {
    double target_phi, current_rms, duty_cycle, kp, last_v, rms_acc;
} Launder;

typedef struct {
    double core_sync[QC_NODES];
    double bus_throughput, packet_loss;
} CoreBus;

typedef struct {
    int32_t data[QC_ROUTER_CAP];
    int     size;
} RouterBucket;

typedef struct {
    RouterBucket nodes[QC_NODES];
    int           dropped;
} NodeRouter;

typedef struct {
    double   time, stability, shear_flow;
    double   phi_re[TORUS_DIM][TORUS_DIM], phi_im[TORUS_DIM][TORUS_DIM];
    double   sync_clock_c, global_identity, breathing_state;
    Launder  launder;
    double   solenoid_filter, temperature, power_draw;
    double   lyapunov_v, lyapunov_dot;
    bool     is_lasalle_locked;
    CoreBus  bus;
    NodeRouter router;
} QCoreState;

/* ------------------------------------------------------------------------ */
/* Matemática: q_sincos/q_sqrt de qsv.c, mismos bits que el resto de H7-OS  */
/* ------------------------------------------------------------------------ */

static double qc_cos(double x) { double s, c; q_sincos(x, &s, &c); return c; }
static double qc_sin(double x) { double s, c; q_sincos(x, &s, &c); return s; }

static double phase_lock(double n) { return qc_cos(QC_PI * n) * qc_cos(QC_PI_PHI * n); }
static double golden_operator(double n) { return phase_lock(n) * phase_lock(n * QC_PHI); }

/* ------------------------------------------------------------------------ */
/* GoldenLaunder: PWM cuasiperiódico que persigue RMS -> phi                */
/* ------------------------------------------------------------------------ */

static void launder_init(Launder *l) {
    l->target_phi = QC_PHI;
    l->current_rms = 0.0;
    l->duty_cycle = (QC_PHI * QC_PHI) / 25.0;
    l->kp = 0.001;
    l->last_v = 0.0;
    l->rms_acc = 0.0;
}

static double launder_step(Launder *l, double t) {
    double error = l->target_phi - l->current_rms;
    l->duty_cycle += error * l->kp;
    if (l->duty_cycle < 0.01) l->duty_cycle = 0.01;   /* zona muerta */
    if (l->duty_cycle > 0.50) l->duty_cycle = 0.50;   /* protege el solenoide */

    double threshold = qc_cos(QC_PI * l->duty_cycle);
    double phase_mod = golden_operator(t) * QC_PI;
    l->last_v = (qc_cos(t * QC_PI * 2.0 + phase_mod) > threshold) ? 5.0 : 0.0;

    double v2 = l->last_v * l->last_v;
    l->rms_acc = 0.9995 * l->rms_acc + 0.0005 * v2;
    l->current_rms = q_sqrt(l->rms_acc);
    return l->last_v;
}

/* ------------------------------------------------------------------------ */
/* NodeRouter: un solo hilo, buckets fijos (sin heap en este kernel)        */
/* ------------------------------------------------------------------------ */

static void router_distribute_sort(NodeRouter *r, const int32_t *keys, int n) {
    for (int i = 0; i < QC_NODES; i++) r->nodes[i].size = 0;
    r->dropped = 0;
    if (n <= 0) return;

    int32_t mn = keys[0], mx = keys[0];
    for (int i = 1; i < n; i++) {
        if (keys[i] < mn) mn = keys[i];
        if (keys[i] > mx) mx = keys[i];
    }
    int32_t span = (mx - mn) + 1;    /* +1: sin división por cero si todas las claves son iguales */

    /* 32 bits a propósito, no 64: las claves son intensidad*1000 (acotadas
     * con margen de sobra), y H7-OS evita la división de 64 bits de libgcc
     * en todo el kernel (ver README, "Límites") — esto no es la excepción. */
    for (int i = 0; i < n; i++) {
        int node = ((keys[i] - mn) * QC_NODES) / span;
        if (node >= QC_NODES) node = QC_NODES - 1;
        if (node < 0) node = 0;
        RouterBucket *b = &r->nodes[node];
        if (b->size < QC_ROUTER_CAP) b->data[b->size++] = keys[i];
        else r->dropped++;
    }
    for (int node = 0; node < QC_NODES; node++) {
        RouterBucket *b = &r->nodes[node];
        for (int i = 1; i < b->size; i++) {
            int32_t key = b->data[i];
            int j = i;
            while (j > 0 && b->data[j - 1] > key) { b->data[j] = b->data[j - 1]; j--; }
            b->data[j] = key;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Motor: init + un paso de integración                                     */
/* ------------------------------------------------------------------------ */

static void qcore_init(QCoreState *s) {
    s->time = 0.0;
    s->stability = 50.0;
    s->shear_flow = 10.0;           /* Mach 10 por defecto: "canal abierto" */
    s->sync_clock_c = 0.0;
    s->global_identity = 0.0;
    s->breathing_state = 0.0;
    launder_init(&s->launder);
    s->solenoid_filter = 1.0;
    s->temperature = 22.0;          /* temperatura ambiente */
    s->power_draw = 0.0;
    s->lyapunov_v = 0.0;
    s->lyapunov_dot = 0.0;
    s->is_lasalle_locked = false;
    for (int i = 0; i < QC_NODES; i++) s->bus.core_sync[i] = 0.0;
    s->bus.bus_throughput = 0.0;
    s->bus.packet_loss = 0.0;
    for (int i = 0; i < QC_NODES; i++) s->router.nodes[i].size = 0;
    s->router.dropped = 0;

    for (int i = 0; i < TORUS_DIM; i++)
        for (int j = 0; j < TORUS_DIM; j++) {
            double theta = (double)i * 2.0 * QC_PI / TORUS_DIM;
            double phi   = (double)j * 2.0 * QC_PI / TORUS_DIM;
            s->phi_re[i][j] = qc_cos(theta);
            s->phi_im[i][j] = qc_cos(phi * QC_PHI);   /* mapeo con paridad fijada por phi */
        }
}

static void qcore_step(QCoreState *s, double dt) {
    s->time += dt;

    double target_stability = (s->shear_flow >= 9.9) ? 100.0 : (s->shear_flow * 8.0);

    /* Proyector respirante: rotación unitaria (Hamiltoniana) + arrastre
     * metripléctico (disipativo) sobre cada modo del campo toroidal. */
    double On = golden_operator(s->time);
    double dtheta = On * dt * 2.0;
    double cos_dt = qc_cos(dtheta), sin_dt = qc_sin(dtheta);
    double decay = (100.0 - s->stability) * 0.002;
    double pump  = (s->shear_flow / 10.0) * 0.1;

    for (int i = 0; i < TORUS_DIM; i++)
        for (int j = 0; j < TORUS_DIM; j++) {
            double r = s->phi_re[i][j], im = s->phi_im[i][j];
            s->phi_re[i][j] = r * cos_dt - im * sin_dt;
            s->phi_im[i][j] = r * sin_dt + im * cos_dt;

            double intensity = s->phi_re[i][j] * s->phi_re[i][j] + s->phi_im[i][j] * s->phi_im[i][j];
            double drive = (1.0 - intensity) * pump;
            s->phi_re[i][j] += s->phi_re[i][j] * (drive - decay) * dt;
            s->phi_im[i][j] += s->phi_im[i][j] * (drive - decay) * dt;
        }

    double sum = 0.0, on2 = On * On;
    for (int i = 0; i < TORUS_DIM; i++)
        for (int j = 0; j < TORUS_DIM; j++) {
            double intensity = s->phi_re[i][j] * s->phi_re[i][j] + s->phi_im[i][j] * s->phi_im[i][j];
            sum += intensity * on2;
        }
    s->sync_clock_c = sum / (double)(TORUS_DIM * TORUS_DIM);
    if (s->sync_clock_c > 0.5) s->global_identity += s->sync_clock_c * dt * 0.1;

    /* Acoplamiento metripléctico: la estabilidad solo sube en fase segura. */
    double phase_coherence = phase_lock(s->time);
    double stability_gate = phase_coherence * phase_coherence;
    double tor_boost = (s->sync_clock_c > 0.0) ? s->sync_clock_c * 10.0 : 0.0;
    double d_metr = ((target_stability - s->stability) * 0.2 + tor_boost) * stability_gate;
    s->stability += d_metr * dt;

    /* HAL del solenoide + filtro RMS (el "filtro físico"). */
    double v_pulse = launder_step(&s->launder, s->time);
    s->solenoid_filter = 1.0 / (1.0 + v_pulse * 0.1);

    /* Térmico: calentamiento Joule, enfriamiento disipativo. */
    s->power_draw = (v_pulse * v_pulse) / 10.0;
    double heating = s->power_draw;
    double cooling = (s->temperature - 22.0) * 0.05;
    s->temperature += (heating - cooling) * dt;
    if (s->temperature > 60.0) s->stability -= (s->temperature - 60.0) * 0.01 * dt;

    /* Diagnóstico de Barbashin-LaSalle: V = 0.5*(100-rho)^2 + 0.5*(rms-phi)^2 */
    double rho_err = 100.0 - s->stability;
    double phi_err = s->launder.current_rms - QC_PHI;
    double v_new = 0.5 * (rho_err * rho_err + phi_err * phi_err);
    s->lyapunov_dot = (v_new - s->lyapunov_v) / dt;
    s->lyapunov_v = v_new;
    s->is_lasalle_locked = (s->stability > 98.0) && (phi_err * phi_err < 0.001);

    /* Sincronía entre núcleos: desfase de fase áurea por núcleo. */
    for (int i = 0; i < QC_NODES; i++) {
        double core_phase = s->time + (double)i * (QC_PI / 2.0);
        double core_op = qc_cos(QC_PI * core_phase) * qc_cos(QC_PI * QC_PHI * core_phase);
        s->bus.core_sync[i] = (s->stability / 100.0) * (core_op * 0.5 + 0.5);
    }
    s->bus.bus_throughput = 0.0;   /* en el original depende de node_density, que nunca se actualiza */
    s->bus.packet_loss = (100.0 - s->stability) / 100.0;

    if (s->stability < 0.0)   s->stability = 0.0;
    if (s->stability > 100.0) s->stability = 100.0;

    /* NodeRouter: clasificar y ordenar las 64 intensidades del campo. */
    int32_t keys[TORUS_DIM * TORUS_DIM];
    int k = 0;
    for (int i = 0; i < TORUS_DIM; i++)
        for (int j = 0; j < TORUS_DIM; j++) {
            double intensity = s->phi_re[i][j] * s->phi_re[i][j] + s->phi_im[i][j] * s->phi_im[i][j];
            keys[k++] = (int32_t)(intensity * 1000.0);
        }
    router_distribute_sort(&s->router, keys, k);
}

/* ------------------------------------------------------------------------ */
/* Render: pantalla de posición fija (filas 1..24; la 0 es la barra de H7-OS)*/
/* ------------------------------------------------------------------------ */

static void put_str_at(int x, int y, const char *str, uint8_t a) {
    for (int i = 0; str[i]; i++) con_putc_at(x + i, y, str[i], a);
}

static void render_core(const QCoreState *s) {
    con_clear_rows(C_BLACK);
    char buf[32];

    put_str_at(2, 1, "SYSTEM STATUS:", C_LGRAY);
    if (s->stability > 90.0)      put_str_at(17, 1, "RESONANT LOCK", C_GREEN);
    else if (s->stability > 70.0) put_str_at(17, 1, "CANAL OPEN", C_CYAN);
    else                          put_str_at(17, 1, "DRIFTING", C_YELLOW);

    put_str_at(2, 2, "SYNC CLOCK C:", C_LGRAY);
    ksprintf(buf, sizeof buf, "%.3f", s->sync_clock_c);
    put_str_at(17, 2, buf, s->sync_clock_c > 0.5 ? C_GREEN : C_MAGENTA);

    put_str_at(2, 3, "BUS THROUGHPUT:", C_LGRAY);
    put_str_at(18, 3, s->bus.bus_throughput > 1.0 ? "MAXFLOW" : "IDLE",
               s->bus.bus_throughput > 1.0 ? C_LBLUE : C_DGRAY);

    put_str_at(2, 4, "LAUNDER V:", C_LGRAY);
    put_str_at(18, 4, s->launder.last_v > 0.1 ? "5.0V [ON]" : "0.0V [OFF]",
               s->launder.last_v > 0.1 ? C_YELLOW : C_DGRAY);

    put_str_at(2, 5, "V_RMS (HAL):", C_LGRAY);
    ksprintf(buf, sizeof buf, "%.3f", s->launder.current_rms);
    put_str_at(18, 5, buf, (s->launder.current_rms > 1.61 && s->launder.current_rms < 1.63) ? C_GREEN : C_RED);

    put_str_at(2, 6, "SYSTEM TEMP:", C_LGRAY);
    ksprintf(buf, sizeof buf, "%.0f C", s->temperature);
    put_str_at(18, 6, buf, s->temperature < 50.0 ? C_GREEN : s->temperature < 80.0 ? C_YELLOW : C_RED);

    put_str_at(2, 7, "POWER DRAW:", C_LGRAY);
    ksprintf(buf, sizeof buf, "%.2f W", s->power_draw);
    put_str_at(18, 7, buf, s->power_draw < 1.0 ? C_GREEN : s->power_draw < 2.0 ? C_YELLOW : C_RED);

    put_str_at(2, 8, "LASALLE LOCK:", C_LGRAY);
    put_str_at(18, 8, s->is_lasalle_locked ? "INVARIANT" : "DRIFTING",
               s->is_lasalle_locked ? C_GREEN : (s->lyapunov_dot <= 0.0 ? C_YELLOW : C_RED));

    /* Canal central (Z-pinch cizallado) */
    int cx = 35, center_y = 12;
    uint8_t chan_color = (s->shear_flow >= 9.9) ? C_WHITE : C_LGRAY;
    for (int i = 0; i < 15; i++) {
        double y_f = (double)i / 15.0;
        int y = center_y - 7 + i;
        double offset = qc_sin(y_f * 4.0 + s->time) * (100.0 - s->stability) * 0.05;
        int x_val = cx + (int)offset;

        char c_out = '#';
        if (s->solenoid_filter < 0.7) c_out = ' ';
        else if (s->solenoid_filter < 0.85) c_out = '.';
        if (c_out != ' ') con_putc_at(x_val, y, c_out, chan_color);

        if (s->breathing_state > 0.5 && (int)(s->time * 20) % 15 == i && s->solenoid_filter > 0.75)
            con_putc_at(x_val, y, '@', C_YELLOW);
    }

    /* Modulación toroidal (abanico alrededor del canal) */
    int tx = 55;
    uint8_t tor_color = (s->sync_clock_c > 0.5) ? C_CYAN : C_LBLUE;
    for (int i = 0; i < TORUS_DIM; i++)
        for (int j = 0; j < TORUS_DIM; j++) {
            double intensity = s->phi_re[i][j] * s->phi_re[i][j] + s->phi_im[i][j] * s->phi_im[i][j];
            if (intensity <= 0.3) continue;
            double angle  = (double)i * (2.0 * QC_PI / TORUS_DIM) + s->global_identity;
            double radius = 5.0 + (double)j * 0.4;
            int x_pos = tx + (int)(qc_cos(angle) * radius * 2.1);
            int y_pos = center_y + (int)(qc_sin(angle) * radius);

            char t_out = (s->breathing_state > 0.5) ? '*' : '.';
            if (s->solenoid_filter < 0.68) t_out = ' ';
            else if (s->solenoid_filter < 0.8 && t_out == '*') t_out = '.';
            if (t_out != ' ') con_putc_at(x_pos, y_pos, t_out, tor_color);
        }

    /* Núcleos virtuales + carga que el router les asignó este tick */
    for (int i = 0; i < QC_NODES; i++) {
        put_str_at(2, 11 + i, "CORE", C_LGRAY);
        con_putc_at(7, 11 + i, (char)('0' + i), C_WHITE);
        int sync_len = (int)(s->bus.core_sync[i] * 10.0);
        for (int dot = 0; dot < 10; dot++)
            con_putc_at(10 + dot, 11 + i, (dot < sync_len) ? '>' : '-', sync_len > 9 ? C_GREEN : C_DGRAY);

        put_str_at(22, 11 + i, "RT:", C_DGRAY);
        ksprintf(buf, sizeof buf, "%d", s->router.nodes[i].size);
        put_str_at(26, 11 + i, buf, s->router.nodes[i].size > 0 ? C_CYAN : C_DGRAY);
    }
    put_str_at(60, 11, "ROUTER DROP:", C_LGRAY);
    ksprintf(buf, sizeof buf, "%d", s->router.dropped);
    put_str_at(73, 11, buf, s->router.dropped > 0 ? C_RED : C_GREEN);

    put_str_at(2, 23, "ENV: H7-OS v0.7 // BRIDGE: TORUS-SHEAR // CORE: MULTIPLEX", C_DGRAY);
}

/* ------------------------------------------------------------------------ */
/* Comando                                                                   */
/* ------------------------------------------------------------------------ */

void cmd_core(void) {
    static QCoreState s;
    static bool seeded = false;
    if (!seeded) { qcore_init(&s); seeded = true; }

    con_status("Q-CORE // MOTOR METRIPLÉCTICO // ESC: volver al shell");

    bool stop = false;
    while (!stop) {
        qcore_step(&s, 0.05);
        render_core(&s);
        /* ~misma cadencia que el dashboard bare-metal original (4e6 ciclos
         * de espera activa por cuadro), repartida en tramos para que ESC
         * se note sin esperar a que termine el cuadro completo. */
        for (int chunk = 0; chunk < 400 && !stop; chunk++) {
            for (volatile int i = 0; i < 10000; i++) { }
            if (con_poll() == 27) stop = true;
        }
    }
    con_clear();   /* restaura la consola normal; shell_exec repinta la barra de estado */
}
