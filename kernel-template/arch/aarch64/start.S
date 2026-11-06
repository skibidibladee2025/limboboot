/*
 * aarch64 Kernel Startup for limoboot
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

.section .text.entry, "ax"
.global _start
.type _start, %function

/*
 * Entry from limoboot:
 *   x0 = LIMBOOT_PROTOCOL_MAGIC
 *   x1 = handoff pointer (physical)
 *   sp = stack (16-byte aligned, 64 KiB)
 */

_start:
    /* Verify magic */
    cmp x0, #0x4C4D4254
    b.ne _halt

    /* x1 has handoff pointer */
    mov x0, x1        /* 1st argument to kmain */

    /* Call kmain */
    bl kmain

_halt:
    wfi
    b _halt

.size _start, .-_start


/* ============================================================================
 * AP Trampoline (for SMP)
 * ============================================================================ */

.section .ap_trampoline, "ax"
.global ap_trampoline_start
.global ap_trampoline_end
.type ap_trampoline_start, %function
.type ap_trampoline_end, %function

ap_trampoline_start:
    /* Wait for release (spin on mailbox) */
    /* This would be implemented based on platform */

    /* Set up exception vectors */
    adr x0, vectors
    msr vbar_el1, x0

    /* Enable MMU + caches */
    mrs x0, sctlr_el1
    orr x0, x0, #(1 << 0)   /* M - MMU enable */
    orr x0, x0, #(1 << 2)   /* C - Data cache enable */
    orr x0, x0, #(1 << 12)  /* I - Instruction cache enable */
    msr sctlr_el1, x0
    isb

    /* Set up stack */
    ldr sp, =ap_stack_top

    /* Call AP C entry */
    bl ap_main

    /* Halt */
    wfi
    b .

ap_trampoline_end:
.size ap_trampoline_start, ap_trampoline_end - ap_trampoline_start


/* ============================================================================
 * Exception Vectors (minimal)
 * ============================================================================ */

.align 11  /* 2KB alignment for VBAR */
vectors:
    /* Synchronous */
    .rept 16
        b sync_handler
    .endr
    /* IRQ */
    .rept 16
        b irq_handler
    .endr
    /* FIQ */
    .rept 16
        b fiq_handler
    .endr
    /* SError */
    .rept 16
        b serror_handler
    .endr

sync_handler:  eret
irq_handler:   eret
fiq_handler:   eret
serror_handler: eret


/* ============================================================================
 * External Symbols
 * ============================================================================ */

.extern kmain
.extern ap_main
.extern ap_stack_top