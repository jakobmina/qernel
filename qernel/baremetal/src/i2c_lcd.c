#include "i2c_lcd.h"

// External prototypes for delay and I2C
extern void i2c_write_byte(uint8_t addr, uint8_t val);

void k_delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 4000; i++); // Very rough approximation for QEMU
}

void k_delay_us(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 4; i++); // Very rough approximation
}

void lcd_init(LcdI2c *lcd, uint8_t addr, uint8_t cols, uint8_t rows) {
    lcd->addr = addr;
    lcd->cols = cols;
    lcd->rows = rows;
    lcd->backlight_val = LCD_BACKLIGHT;

    lcd->display_function = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    if (rows > 1) {
        lcd->display_function |= LCD_2LINE;
    }

    // Datasheet needs 40ms after power-up
    k_delay_ms(50);

    // Initial state: all bits low
    lcd_expander_write(lcd, lcd->backlight_val);
    k_delay_ms(100);

    // Put into 4-bit mode (Sequence from HD44780 Page 46)
    lcd_write4bits(lcd, 0x03 << 4);
    k_delay_us(4500);

    lcd_write4bits(lcd, 0x03 << 4);
    k_delay_us(4500);

    lcd_write4bits(lcd, 0x03 << 4);
    k_delay_us(150);

    lcd_write4bits(lcd, 0x02 << 4);

    // Set # lines, font size
    lcd_command(lcd, LCD_FUNCTIONSET | lcd->display_function);

    // Turn display on, no cursor, no blink
    lcd->display_control = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->display_control);

    // Clear display
    lcd_clear(lcd);

    // Entry mode
    lcd->display_mode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->display_mode);

    lcd_home(lcd);
}

void lcd_clear(LcdI2c *lcd) {
    lcd_command(lcd, LCD_CLEARDISPLAY);
    k_delay_ms(2);
}

void lcd_home(LcdI2c *lcd) {
    lcd_command(lcd, LCD_RETURNHOME);
    k_delay_ms(2);
}

void lcd_set_cursor(LcdI2c *lcd, uint8_t col, uint8_t row) {
    int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    if (row >= lcd->rows) row = lcd->rows - 1;
    lcd_command(lcd, LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void lcd_backlight(LcdI2c *lcd, uint8_t on) {
    lcd->backlight_val = on ? LCD_BACKLIGHT : LCD_NOBACKLIGHT;
    lcd_expander_write(lcd, 0);
}

void lcd_print(LcdI2c *lcd, const char *str) {
    while (*str) {
        lcd_putc(lcd, *str++);
    }
}

void lcd_putc(LcdI2c *lcd, char c) {
    lcd_send(lcd, (uint8_t)c, LCD_RS);
}

void lcd_command(LcdI2c *lcd, uint8_t value) {
    lcd_send(lcd, value, 0);
}

void lcd_send(LcdI2c *lcd, uint8_t value, uint8_t mode) {
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = (value << 4) & 0xF0;
    lcd_write4bits(lcd, highnib | mode);
    lcd_write4bits(lcd, lownib | mode);
}

void lcd_write4bits(LcdI2c *lcd, uint8_t value) {
    lcd_expander_write(lcd, value);
    lcd_pulse_enable(lcd, value);
}

void lcd_expander_write(LcdI2c *lcd, uint8_t data) {
    i2c_write_byte(lcd->addr, data | lcd->backlight_val);
}

void lcd_pulse_enable(LcdI2c *lcd, uint8_t data) {
    lcd_expander_write(lcd, data | LCD_EN);
    k_delay_us(1);
    lcd_expander_write(lcd, data & ~LCD_EN);
    k_delay_us(50);
}
