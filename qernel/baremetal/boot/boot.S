/* Multiboot v1 header: lets GRUB or QEMU's -kernel loader boot this image
 * directly in 32-bit protected mode, with EAX=magic and EBX=multiboot info
 * pointer already set up per the Multiboot spec. */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Stack for kernel_main (no stack is set up by the bootloader) */
.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16 KiB */
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp

    /* kernel_main(uint32_t magic, void *mbi) -- cdecl: push args right-to-left */
    push %ebx   /* mbi   (EBX set by bootloader) */
    push %eax   /* magic (EAX set by bootloader) */
    call kernel_main

    /* kernel_main never returns, but halt safely just in case */
    cli
.hang:
    hlt
    jmp .hang
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
