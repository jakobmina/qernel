#include "vga_driver.h"

void k_clear(uint8_t bg) {
    for (int i = 0; i < COLS * ROWS; i++) {
        TEXT_VGA_MEM[i] = (uint16_t)' ' | ((uint16_t)(bg << 4) << 8);
    }
}

void k_putc(int x, int y, char c, uint8_t fg, uint8_t bg) {
    if (x >= 0 && x < COLS && y >= 0 && y < ROWS) {
        TEXT_VGA_MEM[y * COLS + x] = (uint16_t)c | (uint16_t)((fg | (bg << 4)) << 8);
    }
}

void k_print_at(int x, int y, const char *s, uint8_t fg, uint8_t bg) {
    int i = 0;
    while (s[i]) {
        k_putc(x + i, y, s[i], fg, bg);
        i++;
    }
}
