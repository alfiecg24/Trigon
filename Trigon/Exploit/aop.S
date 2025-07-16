// aop.S
// Trigon, 2025

.align 4
.global _aop_flush

_aop_flush:
    dmb     sy
    dsb     sy

    cbz     x0, 1f
    cbz     x1, 1f

    and     x8, x0, #0xffffffffffffffc0
    and     x9, x0, #0x3f
    add     x9, x1, x9
    sub     x9, x9, #0x1
    mov     x10, #-0x1
    eor     x9, x10, x9, lsr #6

0:
    add     x8, x8, #0x40
    add     x9, x9, #0x1
    cbnz    x9, 0b
    dsb     ish
    isb
    ret

1:
    ic      ialluis
    dsb     sy
    isb     sy
    ret
