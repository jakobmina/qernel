/*
 * isr.S — 48 entradas de interrupción: excepciones 0-31 e IRQ 0-15 (0x20-0x2F)
 *
 * Cada entrada deja la pila como un IntFrame (int.h) y salta a isr_common,
 * que llama a isr_dispatch(IntFrame *) en C y vuelve con iret.
 */
.macro ISR_NOERR n
isr\n:
    push $0
    push $\n
    jmp  isr_common
.endm

.macro ISR_ERR n
isr\n:
    push $\n
    jmp  isr_common
.endm

.section .text
.irp n, 0,1,2,3,4,5,6,7,9,15,16,18,19,20,22,23,24,25,26,27,28,31
    ISR_NOERR \n
.endr
.irp n, 8,10,11,12,13,14,17,21,29,30
    ISR_ERR \n
.endr
.irp n, 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    ISR_NOERR \n
.endr

isr_common:
    pusha
    cld
    push %esp
    call isr_dispatch
    add  $4, %esp
    popa
    add  $8, %esp
    iret

.section .rodata
.global isr_table
.align 4
isr_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    .long isr\n
.endr

.section .note.GNU-stack, "", @progbits
