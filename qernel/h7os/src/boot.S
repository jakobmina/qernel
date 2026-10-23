/*
 * boot.S — entrada multiboot de H7-OS (i386, modo protegido sin paginación)
 */
.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    (1 << 0) | (1 << 1)          /* módulos alineados + mapa de memoria */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    /* GDT propia y plana: el multiboot no garantiza que la suya siga válida,
     * y la IDT necesita un selector de código conocido (0x08). */
    lgdt gdt_ptr
    ljmp $0x08, $1f
1:  mov  $0x10, %cx
    mov  %cx, %ds
    mov  %cx, %es
    mov  %cx, %fs
    mov  %cx, %gs
    mov  %cx, %ss
    mov  $stack_top, %esp

    /* FPU x87: EM=0, TS=0, MP=1, NE=1; precisión 53 bits, redondeo al más
     * cercano, excepciones enmascaradas (0x027F). Así cada operación redondea
     * como un double IEEE y los registros coinciden con la referencia en x86-64. */
    mov  %cr0, %ecx
    and  $~((1 << 2) | (1 << 3)), %ecx
    or   $((1 << 1) | (1 << 5)), %ecx
    mov  %ecx, %cr0
    fninit
    fldcw fpu_cw

    push %ebx                   /* MultibootInfo* */
    push %eax                   /* magic 0x2BADB002 */
    call kernel_main

    cli
2:  hlt
    jmp  2b
.size _start, . - _start

.section .rodata
fpu_cw: .word 0x027F

.align 8
gdt:
    .quad 0x0000000000000000    /* nulo */
    .quad 0x00CF9A000000FFFF    /* 0x08: código, base 0, límite 4 GiB, ring 0 */
    .quad 0x00CF92000000FFFF    /* 0x10: datos */
gdt_ptr:
    .word gdt_ptr - gdt - 1
    .long gdt

.section .note.GNU-stack, "", @progbits
