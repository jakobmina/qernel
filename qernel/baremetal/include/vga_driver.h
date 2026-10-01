#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H

#include <stdint.h>

#define TEXT_VGA_MEM ((uint16_t*)0xB8000)
#define COLS 80
#define ROWS 25

// Colors
#define BLACK      0x0
#define BLUE       0x1
#define GREEN      0x2
#define CYAN       0x3
#define RED        0x4
#define MAGENTA    0x5
#define BROWN      0x6
#define LIGHT_GRAY 0x7
#define DARK_GRAY  0x8
#define LIGHT_BLUE 0x9
#define YELLOW     0xE
#define WHITE      0xF

void k_clear(uint8_t bg);
void k_putc(int x, int y, char c, uint8_t fg, uint8_t bg);
void k_print_at(int x, int y, const char *s, uint8_t fg, uint8_t bg);

#endif
