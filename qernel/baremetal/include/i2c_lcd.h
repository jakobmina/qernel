#ifndef I2C_LCD_H
#define I2C_LCD_H

#include <stdint.h>
#include <stddef.h>

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// flags for backlight control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define LCD_EN 0x04  // Enable bit
#define LCD_RW 0x02  // Read/Write bit
#define LCD_RS 0x01  // Register select bit

typedef struct {
    uint8_t addr;
    uint8_t cols;
    uint8_t rows;
    uint8_t backlight_val;
    uint8_t display_function;
    uint8_t display_control;
    uint8_t display_mode;
} LcdI2c;

// API
void lcd_init(LcdI2c *lcd, uint8_t addr, uint8_t cols, uint8_t rows);
void lcd_clear(LcdI2c *lcd);
void lcd_home(LcdI2c *lcd);
void lcd_set_cursor(LcdI2c *lcd, uint8_t col, uint8_t row);
void lcd_backlight(LcdI2c *lcd, uint8_t on);
void lcd_print(LcdI2c *lcd, const char *str);
void lcd_putc(LcdI2c *lcd, char c);

// Low-level commands
void lcd_command(LcdI2c *lcd, uint8_t value);
void lcd_send(LcdI2c *lcd, uint8_t value, uint8_t mode);
void lcd_write4bits(LcdI2c *lcd, uint8_t value);
void lcd_expander_write(LcdI2c *lcd, uint8_t data);
void lcd_pulse_enable(LcdI2c *lcd, uint8_t data);

// Delay utilities (must be provided or implemented in kernel)
void k_delay_ms(uint32_t ms);
void k_delay_us(uint32_t us);

#endif // I2C_LCD_H
