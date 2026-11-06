/*
 * x86-64 Kernel Startup for limoboot
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

.section .text.entry, "ax"
.code64
.global _start
.type _start, @function

/*
 * Entry from limoboot:
 *   RAX = LIMBOOT_PROTOCOL_MAGIC
 *   RBX = handoff pointer (physical)
 *   RSP = stack (16-byte aligned, 64 KiB)
 */

_start:
    /* Verify magic */
    cmp $0x4C4D4254, %rax
    jne _halt

    /* RBX already has handoff pointer */
    mov %rbx, %rdi        /* 1st argument to kmain */

    /* Call kmain */
    call kmain

_halt:
    cli
    hlt
    jmp _halt

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
    .code16
    /* Real mode entry */
    cli
    cld

    /* Set up segments */
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Enable A20 */
    call enable_a20

    /* Switch to protected mode */
    lgdt gdtr32
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0

    /* Far jump to 32-bit code */
    ljmp $0x08, $pmode32

.code32
pmode32:
    /* Set up 32-bit segments */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Enable PAE */
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    /* Load PML4 (will be set by BSP) */
    mov %cr3, %eax

    /* Enable long mode */
    mov $0xC0000080, %ecx      /* EFER MSR */
    rdmsr
    or $0x100, %eax            /* LME */
    wrmsr

    /* Enable paging */
    mov %cr0, %eax
    or $(1 << 31), %eax        /* PG */
    mov %eax, %cr0

    /* Far jump to 64-bit code */
    ljmp $0x10, $pmode64

.code64
pmode64:
    /* 64-bit mode */
    mov $0x18, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Set up stack */
    movq ap_stack_top, %rsp

    /* Call AP C entry */
    call ap_main

    /* Halt */
    cli
    hlt
    jmp $-2

/* GDT for trampoline */
.align 8
gdt64:
    .quad 0                                    /* NULL */
    .quad 0x00AF9A000000FFFF                  /* 64-bit code */
    .quad 0x00CF92000000FFFF                  /* 64-bit data */
    .quad 0x00AF9A000000FFFF                  /* 32-bit code (compat) */
    .quad 0x00CF92000000FFFF                  /* 32-bit data (compat) */
gdt64_end:

gdtr64:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

gdt32:
    .quad 0
    .quad 0x00CF9A000000FFFF                  /* 32-bit code */
    .quad 0x00CF92000000FFFF                  /* 32-bit data */
gdt32_end:

gdtr32:
    .word gdt32_end - gdt32 - 1
    .long gdt32

ap_stack_top:
    .quad 0

ap_trampoline_end:
.size ap_trampoline_start, ap_trampoline_end - ap_trampoline_start


/* ============================================================================
 * A20 Enable
 * ============================================================================ */

enable_a20:
    in $0x92, %al
    or $2, %al
    out %al, $0x92
    ret


/* ============================================================================
 * External Symbols
 * ============================================================================ */

.extern kmain
.extern ap_main