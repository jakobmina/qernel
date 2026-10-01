ENTRY(_start)

SECTIONS
{
    /* Sections gcc/as emit that carry no runtime meaning for a freestanding
     * kernel (stack-exec notes, build comments, unwind tables). Left
     * unplaced, ld inserts them as "orphan" sections ahead of .text and
     * resets the location counter, which silently relinks the whole kernel
     * at address 0 instead of the 1M load address below. */
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.comment)
        *(.eh_frame)
    }

    . = 1M;

    .multiboot ALIGN(4K) : {
        *(.multiboot)
    }

    .text ALIGN(4K) : {
        *(.text)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }
}
