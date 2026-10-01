CC      = gcc
AS      = gcc
LD      = ld

CFLAGS  = -std=gnu11 -m32 -ffreestanding -fno-pic -fno-stack-protector \
          -fno-builtin -Wall -Wextra -O2 -Iinclude
ASFLAGS = -m32 -ffreestanding -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib
LIBGCC  = $(shell $(CC) -m32 -print-libgcc-file-name)

BUILD    = build
SRCS     = $(wildcard src/*.c)
OBJS     = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS)) $(BUILD)/boot.o
KERNEL   = $(BUILD)/kernel.elf

LIMINE_DIR = limine
ISO_ROOT   = $(BUILD)/iso_root
ISO        = $(BUILD)/qernel.iso

.PHONY: all run iso run-iso clean

all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.S | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGCC)

# QEMU's -kernel flag loads a Multiboot ELF directly; fine for dev, but it
# only works inside QEMU. `iso` below produces a real bootable image.
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -serial stdio

# Real bootable CD/USB image via Limine (BIOS, El Torito "no emulation" boot).
# Limine's own binaries aren't built here -- drop the official release's
# limine-bios.sys and limine-bios-cd.bin into limine/ first (see
# limine/README.md). limine.cfg here is ours and is tracked in git.
iso: $(KERNEL)
	@if [ ! -f $(LIMINE_DIR)/limine-bios.sys ] || [ ! -f $(LIMINE_DIR)/limine-bios-cd.bin ]; then \
		echo "Missing $(LIMINE_DIR)/limine-bios.sys and/or $(LIMINE_DIR)/limine-bios-cd.bin."; \
		echo "Grab the official Limine binary release and copy both files into $(LIMINE_DIR)/ -- see $(LIMINE_DIR)/README.md."; \
		exit 1; \
	fi
	mkdir -p $(ISO_ROOT)/boot
	cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
	cp $(LIMINE_DIR)/limine.cfg $(ISO_ROOT)/boot/limine.cfg
	cp $(LIMINE_DIR)/limine.cfg $(ISO_ROOT)/limine.cfg
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/limine-bios.sys
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/limine-bios-cd.bin
	xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--protective-msdos-label \
		$(ISO_ROOT) -o $(ISO)

run-iso: iso
	qemu-system-i386 -cdrom $(ISO) -serial stdio

clean:
	rm -rf $(BUILD)
