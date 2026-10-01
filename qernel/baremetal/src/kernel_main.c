#include "qcore_metriplectic.h"
#include "vga_driver.h"
#include "i2c_lcd.h"

// Global state for predictability in the freestanding environment
SystemState state;
LcdI2c lcd;

// Custom itoa for freestanding environment
void itoa(int n, char s[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';

    // Reverse the string
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char c = s[j];
        s[j] = s[k];
        s[k] = c;
    }
}

// Port I/O
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void serial_putc(char c) {
    while ((inb(0x3fd) & 0x20) == 0);
    outb(0x3f8, c);
}
void serial_print(const char *s) {
    while (*s) serial_putc(*s++);
}

void kernel_main(uint32_t magic, void* mbi) {
    (void)magic; (void)mbi;

    serial_print("\n--- QUOREMIND KERNEL OS BOOTED (TEXT MODE) ---\n");
    init_system(&state);
    k_clear(BLACK);

    // Initialize I2C LCD
    lcd_init(&lcd, 0x27, 20, 4);
    lcd_clear(&lcd);
    lcd_set_cursor(&lcd, 0, 0);
    lcd_print(&lcd, "Q-CORE HEARTBEAT");

    int step = 0;
    while (1) {
        solve_step(&state, 0.05f);

        // Render UI
        k_clear(BLACK);

        // Header
        k_print_at(2, 0, "QUOREMIND Q-CORE // SHEARED TORUS KERNEL v3.0", LIGHT_BLUE, BLACK);
        k_print_at(2, 1, "|==============================================|", DARK_GRAY, BLACK);

        k_print_at(2, 3, "SYSTEM STATUS:", LIGHT_GRAY, BLACK);
        if (state.stability > 90.0f) k_print_at(17, 3, "RESONANT LOCK", GREEN, BLACK);
        else if (state.stability > 70.0f) k_print_at(17, 3, "CANAL OPEN", CYAN, BLACK);
        else k_print_at(17, 3, "DRIFTING", YELLOW, BLACK);

        k_print_at(2, 4, "SYNC CLOCK C:", LIGHT_GRAY, BLACK);
        char c_buf[16];
        int c_int = (int)(state.sync_clock_c * 1000);
        if (c_int < 0) { c_buf[0] = '-'; c_int = -c_int; } else { c_buf[0] = ' '; }
        c_buf[1] = '0' + (c_int / 1000) % 10;
        c_buf[2] = '.';
        c_buf[3] = '0' + (c_int / 100) % 10;
        c_buf[4] = '0' + (c_int / 10) % 10;
        c_buf[5] = '0' + (c_int % 10);
        c_buf[6] = '\0';
        k_print_at(17, 4, c_buf, (state.sync_clock_c > 0.5f) ? GREEN : MAGENTA, BLACK);

        k_print_at(2, 5, "BUS THROUGHPUT:", LIGHT_GRAY, BLACK);
        if (state.bus.bus_throughput > 1.0f) k_print_at(18, 5, "MAXFLOW", LIGHT_BLUE, BLACK);
        else k_print_at(18, 5, "IDLE", DARK_GRAY, BLACK);

        k_print_at(2, 6, "LAUNDER V:", LIGHT_GRAY, BLACK);
        if (state.launder.last_v > 0.1f) k_print_at(18, 6, "5.0V [ON]", YELLOW, BLACK);
        else k_print_at(18, 6, "0.0V [OFF]", DARK_GRAY, BLACK);

        k_print_at(2, 7, "V_RMS (HAL):", LIGHT_GRAY, BLACK);
        float v_rms = state.launder.current_rms;
        char rms_buf[16];
        int rms_int = (int)(v_rms * 1000);
        rms_buf[0] = '0' + (rms_int / 1000) % 10;
        rms_buf[1] = '.';
        rms_buf[2] = '0' + (rms_int / 100) % 10;
        rms_buf[3] = '0' + (rms_int / 10) % 10;
        rms_buf[4] = '0' + (rms_int % 10);
        rms_buf[5] = '\0';
        k_print_at(18, 7, rms_buf, (v_rms > 1.61f && v_rms < 1.63f) ? GREEN : RED, BLACK);

        k_print_at(2, 8, "SYSTEM TEMP:", LIGHT_GRAY, BLACK);
        int temp_int = (int)state.temperature;
        char temp_buf[8];
        temp_buf[0] = '0' + (temp_int / 100) % 10;
        temp_buf[1] = '0' + (temp_int / 10) % 10;
        temp_buf[2] = '0' + (temp_int % 10);
        temp_buf[3] = ' '; temp_buf[4] = 'C'; temp_buf[5] = '\0';
        k_print_at(18, 8, temp_buf, (state.temperature < 50.0f) ? GREEN : (state.temperature < 80.0f) ? YELLOW : RED, BLACK);

        k_print_at(2, 9, "POWER DRAW:", LIGHT_GRAY, BLACK);
        char power_buf[16];
        int p_int_part = (int)state.power_draw;
        int p_frac_part = (int)((state.power_draw - p_int_part) * 100);

        char int_str[8];
        itoa(p_int_part, int_str);

        int i = 0;
        while(int_str[i] != '\0') {
            power_buf[i] = int_str[i];
            i++;
        }
        power_buf[i++] = '.';
        power_buf[i++] = '0' + (p_frac_part / 10) % 10;
        power_buf[i++] = '0' + (p_frac_part % 10);
        power_buf[i++] = ' ';
        power_buf[i++] = 'W';
        power_buf[i] = '\0';
        k_print_at(18, 9, power_buf, (state.power_draw < 1.0f) ? GREEN : (state.power_draw < 2.0f) ? YELLOW : RED, BLACK);

        k_print_at(2, 10, "LASALLE LOCK:", LIGHT_GRAY, BLACK);
        if (state.is_lasalle_locked) k_print_at(18, 10, "INVARIANT", GREEN, BLACK);
        else k_print_at(18, 10, "DRIFTING ", (state.lyapunov_dot <= 0.0f) ? YELLOW : RED, BLACK);

        // Render Holistic Visualization
        // 1. Central Sheared Channel (Z-Pinch)
        int cx = 35;
        int center_y = 14;
        uint8_t chan_color = (state.shear_flow >= 9.9f) ? WHITE : LIGHT_GRAY;
        for (int i = 0; i < 15; i++) {
            float y_f = (float)i / 15.0f;
            int y = center_y - 7 + i;
            float offset = k_sin(y_f * 4.0f + state.time) * (100.0f - state.stability) * 0.05f;
            int x_val = cx + (int)offset;

            // Fading logic for channel (driven by Solenoid Filter)
            char c_out = '#';
            if (state.solenoid_filter < 0.7f) c_out = ' ';
            else if (state.solenoid_filter < 0.85f) c_out = '.';

            if (c_out != ' ') k_putc(x_val, y, c_out, chan_color, BLACK);

            // Pulsing Node in Channel (Only if visible)
            // Pulsing Node in Channel (Only if visible / unfiltered)
            if (state.breathing_state > 0.5f && (int)(state.time * 20) % 15 == i && state.solenoid_filter > 0.75f) {
                k_putc(x_val, y, '@', YELLOW, BLACK);
            }
        }

        // 2. Toroidal Modulation (Fan around the channel)
        int tx = 55;
        uint8_t tor_color = (state.sync_clock_c > 0.5f) ? CYAN : LIGHT_BLUE;
        for (int i = 0; i < TORUS_DIM; i++) {
            for (int j = 0; j < TORUS_DIM; j++) {
                float intensity = state.phi_re[i][j]*state.phi_re[i][j] + state.phi_im[i][j]*state.phi_im[i][j];
                if (intensity > 0.3f) {
                    float angle = (float)i * (2.0f * PI / TORUS_DIM) + state.global_identity;
                    float radius = 5.0f + (float)j * 0.4f;
                    int x_pos = tx + (int)(k_cos(angle) * radius * 2.1f);
                    int y_pos = center_y + (int)(k_sin(angle) * radius);

                    char t_out = (state.breathing_state > 0.5f) ? '*' : '.';
                    if (state.solenoid_filter < 0.68f) t_out = ' ';
                    else if (state.solenoid_filter < 0.8f && t_out == '*') t_out = '.';

                    if (t_out != ' ') k_putc(x_pos, y_pos, t_out, tor_color, BLACK);
                }
            }
        }

        // 3. Core Indicators
        for(int i=0; i<4; i++) {
            k_print_at(2, 13 + i, "CORE", LIGHT_GRAY, BLACK);
            k_putc(7, 13 + i, '0' + i, WHITE, BLACK);
            int sync_len = (int)(state.bus.core_sync[i] * 10);
            for(int s=0; s<10; s++) k_putc(10+s, 13+i, (s < sync_len) ? '>' : '-', (sync_len > 9) ? GREEN : DARK_GRAY, BLACK);
        }

        k_print_at(2, ROWS-2, "ENV: QEMU-I386 // BRIDGE: TORUS-SHEAR // CORE: MULTIPLEX", DARK_GRAY, BLACK);

        if (step % 5 == 0) {
            static const char spinner[] = {'|', '/', '-', '\\'};
            serial_print("\r[HOLISTIC_SYNC] ");
            serial_putc(spinner[(step / 5) % 4]);
            serial_print("  BUS_TP: ");
            if (state.bus.bus_throughput > 1.0f) serial_print("MAX  ");
            else serial_print("IDLE ");
            serial_print("  BRTH: ");
            if (state.breathing_state > 0.5f) serial_print("ON ");
            else serial_print("OFF");

            // Update LCD with Stability & RMS
            lcd_set_cursor(&lcd, 0, 1);
            lcd_print(&lcd, "STAB: ");
            int stab_val = (int)state.stability;
            char s_buf[4];
            s_buf[0] = '0' + (stab_val / 100) % 10;
            s_buf[1] = '0' + (stab_val / 10) % 10;
            s_buf[2] = '0' + (stab_val % 10);
            s_buf[3] = '\0';
            lcd_print(&lcd, s_buf);
            lcd_print(&lcd, "%");

            lcd_set_cursor(&lcd, 0, 2);
            lcd_print(&lcd, "RMS: ");
            lcd_print(&lcd, rms_buf);

            lcd_set_cursor(&lcd, 0, 3);
            lcd_print(&lcd, "TEMP: ");
            lcd_print(&lcd, temp_buf);

            lcd_set_cursor(&lcd, 11, 3);
            lcd_print(&lcd, "PWR: ");
            lcd_print(&lcd, power_buf);
        }
        step++;

        // Delay loop for visibility (approx 60fps in QEMU environment)
        for (volatile int i = 0; i < 4000000; i++);
    }
}
