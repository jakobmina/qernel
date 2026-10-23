/* linker.ld — H7-OS se carga en 1 MiB; la cabecera multiboot va primero. */
ENTRY(_start)

SECTIONS
{
    . = 1M;
    _kernel_start = .;

    .text BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata*) }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data*) }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss*) }

    _kernel_end = .;

    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
