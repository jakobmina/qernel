# Qernel Baremetal: QUOREMIND Kernel

Kernel bare-metal (freestanding, x86 32-bit, Multiboot v1) que porta el motor
físico metriplético del proyecto `qernel` a un entorno sin sistema operativo
anfitrión: arranca directamente vía GRUB/QEMU, dibuja el estado del sistema
en una pantalla VGA en modo texto 80x25, y refleja el mismo estado en un LCD
I2C 20x4 (PCF8574 + HD44780).

## Arquitectura

```
boot/boot.S              Header Multiboot v1 + stub de arranque (_start)
linker.ld                Script de enlace: carga a partir de 1 MiB
include/, src/
  qcore_metriplectic.*    Motor físico: operador áureo, lagrangiano,
                          campo toroidal Φ, invariante de Barbashin-LaSalle
  hal_golden_launder.*    Lazo de control PWM que persigue RMS → φ
  vga_driver.*            Driver de texto VGA (0xB8000)
  i2c.*                   Bus I2C: en QEMU es un stub que traza por serial
                          (en x86 real no hay bus I2C accesible sin un
                          controlador SMBus dedicado; ver nota abajo)
  i2c_lcd.*               Driver genérico HD44780 sobre expansor I2C
  kernel_main.c           Punto de entrada: init, bucle de render y
                          telemetría por puerto serie (0x3F8)
```

No hay libc: toda la trigonometría (`k_sin`, `k_cos`, `k_sqrt`, `k_exp`) está
implementada a mano en `qcore_metriplectic.c` mediante series de Taylor,
así el kernel no depende de `libm`.

## Compilar y correr

Requiere `gcc` con soporte `-m32` (paquete `gcc-multilib`) y
`qemu-system-i386`:

```bash
sudo apt-get install gcc-multilib qemu-system-x86
make            # genera build/kernel.elf
make run        # bootea en QEMU con la consola serial en stdio
```

QEMU puede bootear `build/kernel.elf` directamente con `-kernel` porque es
un ELF Multiboot v1 válido (no hace falta ISO ni GRUB).

## Nota sobre el LCD I2C

El PC estándar (y QEMU `pc-i440fx`) no expone un bus I2C de propósito
general accesible por puertos de E/S como lo tendría un microcontrolador;
lo más cercano es el SMBus del PIIX4 en `0x400-0x40F`. Por eso
`i2c_write_byte()` aquí es un *stub*: traza cada byte transmitido por el
puerto serie (`[I2C] <addr> -> <valor>`) en vez de tocar hardware real.
La API (`lcd_init`, `lcd_print`, etc.) es la misma que usaría un LCD físico,
así que portar esto a una Raspberry Pi o un microcontrolador con bus I2C
real solo requiere reemplazar `i2c_write_byte()` en `src/i2c.c`.
