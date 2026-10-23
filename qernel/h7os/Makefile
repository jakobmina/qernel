# H7-OS v0.7 — kernel i386 multiboot con shell, almacén Z7 + Hamming, Steane en OpenQASM
#             y dos experimentos sobre un vector de estado: eco y unitalidad;
#             v0.7: fase áurea exacta (phase) y lectura GHZ de n mod 14 (q ghz)
#   make         compila build/h7os.elf
#   make run     QEMU con ventana VGA; el shell también responde en esta terminal
#   make test    arranque sin pantalla con un script de comandos + comprobaciones
#   make iso     build/h7os.iso: arrancable en VirtualBox, VMware, en una USB real, etc.

CC      := gcc
HOSTCC  := gcc
LD      := ld
QEMU    := qemu-system-i386
EXITDEV := -device isa-debug-exit,iobase=0xf4,iosize=0x04

CFLAGS  := -m32 -march=i686 -std=gnu11 -O2 -Wall -Wextra \
           -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector \
           -fno-asynchronous-unwind-tables -mno-sse -mno-sse2 -mno-mmx -mfpmath=387
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

OBJ := build/boot.o build/isr.o build/con.o build/int.o build/h7.o build/rs.o \
       build/qsim.o build/qsv.o build/qasm.o build/steane.o build/qtext.o build/echo.o build/unital.o \
       build/ghz.o build/shell.o build/kernel.o

all: build/h7os.elf

build:
	mkdir -p build

build/%.o: src/%.c src/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.S | build
	$(CC) -m32 -c $< -o $@

build/h7os.elf: $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

build/ref: ref/h7_hexa_dec.c | build
	$(HOSTCC) -O3 -Wall -Wextra $< -o $@ -lm

# el mismo generador de circuitos que usa el kernel, compilado para Linux
QHOST := src/qasm.c src/qsim.c src/qsv.c

build/steane_gen: tools/steane_gen.c src/steane.c $(QHOST) src/*.h | build
	$(HOSTCC) -O2 -Wall -Wextra -Isrc tools/steane_gen.c src/steane.c $(QHOST) -o $@ -lm

# el experimento del eco con el mismo código del kernel, para comparar con numpy
build/echo_exp: tools/echo_exp.c src/echo.c src/qtext.c $(QHOST) src/*.h | build
	$(HOSTCC) -O2 -Wall -Wextra -Isrc tools/echo_exp.c src/echo.c src/qtext.c $(QHOST) -o $@ -lm

# la prueba de unitalidad, igual
build/unital_exp: tools/unital_exp.c src/unital.c src/qtext.c $(QHOST) src/*.h | build
	$(HOSTCC) -O2 -Wall -Wextra -Isrc tools/unital_exp.c src/unital.c src/qtext.c $(QHOST) -o $@ -lm

# covarianza/asimetria exactas (sin Aer, sin shots) del hash QNN de Jako
# (h7_qnn_hash.py + h7_bridge.py), con rotate_pow/n_z7 de h7.c: ver src/qnnhash.c
build/qnnhash_tool: tools/qnnhash_tool.c src/qnnhash.c src/h7.c $(QHOST) src/*.h | build
	$(HOSTCC) -O2 -Wall -Wextra -Isrc tools/qnnhash_tool.c src/qnnhash.c src/h7.c $(QHOST) -o $@ -lm

# lectura GHZ de n mod 14 (q ghz) con el mismo código del kernel
build/ghz_exp: tools/ghz_exp.c src/ghz.c src/h7.c src/qtext.c $(QHOST) src/*.h | build
	$(HOSTCC) -O2 -Wall -Wextra -Isrc tools/ghz_exp.c src/ghz.c src/h7.c src/qtext.c $(QHOST) -o $@ -lm

# fase áurea exacta (h7_label, h7_golden) para la referencia con decimal
build/phase_tool: tools/phase_tool.c src/h7.c $(QHOST) src/*.h | build
	$(HOSTCC) -O2 -Wall -Wextra -Isrc tools/phase_tool.c src/h7.c $(QHOST) -o $@ -lm

qasm/steane.qasm: build/steane_gen
	./build/steane_gen > $@

run: build/h7os.elf
	@$(QEMU) -kernel $< -serial stdio $(EXITDEV) || true

# cualquier QASM por el vector de estado del kernel (amplitudes antes de medir)
build/qasm_sv: tools/qasm_sv.c $(QHOST) src/*.h | build
	$(HOSTCC) -O2 -Wall -Wextra -Isrc tools/qasm_sv.c $(QHOST) -o $@ -lm

test: build/h7os.elf build/ref build/steane_gen build/echo_exp build/unital_exp build/qasm_sv build/qnnhash_tool \
      build/phase_tool build/ghz_exp
	@sh test/test.sh

# ISO arrancable: GRUB2 (multiboot v1) + el kernel, nada más.
# Necesita grub-pc-bin, grub-common y xorriso (apt install grub-pc-bin grub-common xorriso).
build/h7os.iso: build/h7os.elf boot/grub.cfg
	mkdir -p build/iso/boot/grub
	cp build/h7os.elf build/iso/boot/h7os.elf
	cp boot/grub.cfg build/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/iso

iso: build/h7os.iso

clean:
	rm -rf build

.PHONY: all run test iso clean
