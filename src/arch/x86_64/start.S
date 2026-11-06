/*
 * x86-64 UEFI Entry Point
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>

/* ============================================================================
 * UEFI Entry Point
 * ============================================================================ */

.section .text
.code64
.global efi_main
.type efi_main, @function

/*
 * efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
 *
 * Per UEFI spec, on x86-64:
 *   RCX = image_handle
 *   RDX = system_table
 *   R8, R9, stack = other params (not used)
 */
efi_main:
    /* Save registers per Microsoft x64 ABI (UEFI uses MS ABI on x86-64) */
    /* RCX, RDX, R8, R9 are volatile */
    /* RBX, RBP, RDI, RSI, R12-R15 are non-volatile */

    /* Save image_handle and system_table */
    mov %rcx, %rbx        /* RBX = image_handle (non-volatile) */
    mov %rdx, %rbp        /* RBP = system_table (non-volatile) */

    /* Align stack to 16 bytes */
    andq $-16, %rsp

    /* Call C initialization */
    call arch_efi_init

    /* Check return status */
    test %rax, %rax
    jz 1f

    /* Error - print and halt */
    mov %rax, %rdi
    call efi_print_status
    jmp halt_loop

1:
    /* Success - call main bootloader logic */
    call limoboot_main

    /* Should never return */
halt_loop:
    cli
    hlt
    jmp halt_loop

.size efi_main, .-efi_main


/* ============================================================================
 * Kernel Entry Point (called after ExitBootServices)
 * ============================================================================ */

.global kernel_entry
.type kernel_entry, @function

/*
 * kernel_entry(limoboot_handoff_t *handoff)
 *
 * Called with:
 *   RAX = LIMBOOT_PROTOCOL_MAGIC
 *   RBX = handoff pointer (physical)
 *   RSP = stack pointer (already set up)
 */
kernel_entry:
    /* We arrive here after ExitBootServices with paging enabled */
    /* RBX holds physical address of handoff */

    /* Save handoff pointer */
    push %rbx

    /* Verify magic */
    mov (%rbx), %rax
    cmp $0x4C4D4254, %rax      /* "LMBT" */
    jne kernel_entry_error

    /* Get kernel entry point from handoff */
    mov 0x28(%rbx), %rax       /* entry_point offset */

    /* Jump to kernel */
    jmp *%rax

kernel_entry_error:
    /* Halt on error */
    cli
    hlt
    jmp kernel_entry_error

.size kernel_entry, .-kernel_entry


/* ============================================================================
 * AP Trampoline (for SMP)
 * ============================================================================ */

.section .trampoline, "ax"
.global ap_trampoline_start
.global ap_trampoline_end
.type ap_trampoline_start, @function
.type ap_trampoline_end, @function

/* AP trampoline code - runs in real mode initially, then transitions to 64-bit */
/* This is a minimal trampoline; real implementation would be more complex */

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

    /* Load PML4 (identity mapped) */
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
    /* Fast A20 gate */
    in $0x92, %al
    or $2, %al
    out %al, $0x92
    ret


/* ============================================================================
 * External C Functions
 * ============================================================================ */

.extern arch_efi_init
.extern limoboot_main
.extern ap_main
.extern efi_print_status
.extern virt_to_phys