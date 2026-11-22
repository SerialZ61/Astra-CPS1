    .syntax unified
    .arm
    .text
    .align 2
    .global AstraIdleBranch
    .type AstraIdleBranch, %function
AstraIdleBranch:
    @ The dispatch table selects DBRA D0. All sites except Final Fight's
    @ task scanner take Cyclone's ordinary instruction handler.
    ldr r0, [r7, #0x60]
    sub r0, r4, r0
    sub r0, r0, #0x700
    cmp r0, #0x84
    bne AstraCycloneDbra
    mov r0, r7
    mov r1, r5
    @ The 3DS port keeps SP eight-byte aligned across every C callback.
    bl astraIdleBudget
    mov r5, r0
    b AstraCycloneDbra
    .size AstraIdleBranch, .-AstraIdleBranch
    .global AstraSf2IdleBranch
    .type AstraSf2IdleBranch, %function
AstraSf2IdleBranch:
    @ 910522 World task scanner, DBRA D0 at 0x0a58.
    ldr r0, [r7, #0x60]
    sub r0, r4, r0
    sub r0, r0, #0xa00
    cmp r0, #0x5a
    bne AstraCycloneDbra
    mov r0, r7
    mov r1, r5
    bl astraSf2IdleBudget
    mov r5, r0
    b AstraCycloneDbra
    .size AstraSf2IdleBranch, .-AstraSf2IdleBranch
    .macro action_idle name, page, offset, budget
    .global \name
    .type \name, %function
\name:
    ldr r0, [r7, #0x60]
    sub r0, r4, r0
    sub r0, r0, #\page
    cmp r0, #\offset
    bne AstraCycloneDbra
    mov r0, r7
    mov r1, r5
    bl \budget
    mov r5, r0
    b AstraCycloneDbra
    .size \name, .-\name
    .endm
    action_idle AstraKnightsBranch, 0xd00, 0xc6, astraKnightsIdleBudget
    action_idle AstraDaimakaiBranch, 0x600, 0x8c, astraDaimakaiIdleBudget
    .global AstraTaskBranch
    .type AstraTaskBranch, %function
AstraTaskBranch:
    ldr r0, [r7, #0x60]
    sub r0, r4, r0
    ldr r1, =astraTaskPc
    ldr r1, [r1]
    cmp r0, r1
    bne AstraCycloneDbra
    mov r0, r7
    mov r1, r5
    bl astraTaskBudget
    mov r5, r0
    b AstraCycloneDbra
    .size AstraTaskBranch, .-AstraTaskBranch
    .section .note.GNU-stack,"",%progbits
