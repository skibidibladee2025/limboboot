/*
 * riscv64 Kernel Startup for limoboot
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

.section .text.entry, "ax"
.global _start
.type _start, @function

/*
 * Entry from limoboot:
 *   a0 = LIMBOOT_PROTOCOL_MAGIC
 *   a1 = handoff pointer (physical)
 *   sp = stack (16-byte aligned, 64 KiB)
 */

_start:
    /* Verify magic */
    li t0, 0x4C4D4254
    bne a0, t0, _halt

    /* a1 has handoff pointer */
    mv a0, a1        /* 1st argument to kmain */

    /* Call kmain */
    call kmain

_halt:
    wfi
    j _halt

.size _start, .-_start


/* ============================================================================
 * AP Trampoline (for SMP)
 * ============================================================================ */

.section .ap_trampoline, "ax"
.global ap_trampoline_start
.global ap_trampoline_end
.type ap_trampoline_start, @function
.type ap_trampoline_end, @function

ap_trampoline_start:
    /* Wait for release from hart 0 */
    /* Platform-specific: would use SBI or CLINT */

    /* Disable paging */
    csrw satp, zero
    sfence.vma

    /* Set up stack */
    la sp, ap_stack_top

    /* Call AP C entry */
    call ap_main

    /* Halt */
    wfi
    j .

ap_trampoline_end:
.size ap_trampoline_start, ap_trampoline_end - ap_trampoline_start


/* ============================================================================
 * External Symbols
 * ============================================================================ */

.extern kmain
.extern ap_main
.extern ap_stack_top