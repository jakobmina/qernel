#include "i2c.h"

extern void serial_print(const char *s);
extern void serial_putc(char c);

void i2c_write_byte(uint8_t addr, uint8_t val) {
    // Simulated I2C communication over Serial
    // In a physical x86 system, this would use the PIIX4 SMBus I/O ports (usually 0x400-0x40F)

    static const char *hex = "0123456789ABCDEF";

    serial_print("[I2C] ");
    serial_putc(hex[(addr >> 4) & 0xF]);
    serial_putc(hex[addr & 0xF]);
    serial_print(" -> ");
    serial_putc(hex[(val >> 4) & 0xF]);
    serial_putc(hex[val & 0xF]);
    serial_print("\n");
}
