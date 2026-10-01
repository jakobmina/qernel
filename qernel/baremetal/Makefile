CC      = gcc
AS      = gcc
LD      = ld

CFLAGS  = -std=gnu11 -m32 -ffreestanding -fno-pic -fno-stack-protector \
          -fno-builtin -Wall -Wextra -O2 -Iinclude
ASFLAGS = -m32 -ffreestanding -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib
LIBGCC  = $(shell $(CC) -m32 -print-libgcc-file-name)

BUILD   = build
SRCS    = $(wildcard src/*.c)
OBJS    = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS)) $(BUILD)/boot.o
KERNEL  = $(BUILD)/kernel.elf

.PHONY: all run clean

all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.S | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGCC)

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -serial stdio

clean:
	rm -rf $(BUILD)
