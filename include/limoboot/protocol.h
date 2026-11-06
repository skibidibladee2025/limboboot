/*
 * Limoboot Protocol - Public Header
 * Pure 64-bit boot protocol for modern kernels
 *
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef LIMBOOT_PROTOCOL_H
#define LIMBOOT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Magic Constants
 * ============================================================================ */

#define LIMBOOT_PROTOCOL_MAGIC       0x4C4D4254ULL  /* "LMBT" in ASCII */
#define LIMBOOT_PROTOCOL_VERSION     1
#define LIMBOOT_PROTOCOL_REVISION    0

/* ============================================================================
 * Handoff Structure
 * ============================================================================ */

#define LIMBOOT_FLAG_HHDM_PRESENT     (1ULL << 0)
#define LIMBOOT_FLAG_SMP              (1ULL << 1)
#define LIMBOOT_FLAG_MEASURED_BOOT    (1ULL << 2)
#define LIMBOOT_FLAG_SECURE_BOOT      (1ULL << 3)
#define LIMBOOT_FLAG_EFI_RUNTIME      (1ULL << 4)

typedef struct limoboot_handoff {
    uint64_t magic;
    uint64_t version;
    uint64_t revision;
    uint64_t flags;
    uint64_t entry_point;
    uint64_t stack_top;
    uint64_t hhdm_offset;
    uint64_t response_ptr;
    uint64_t response_count;
    uint64_t reserved[8];
} limoboot_handoff_t;

/* ============================================================================
 * Request/Response System
 * ============================================================================ */

#define LIMBOOT_REQUEST_FLAG_REQUIRED (1ULL << 0)
#define LIMBOOT_REQUEST_FLAG_OPTIONAL (1ULL << 1)

typedef struct limoboot_request {
    uint64_t id;
    uint64_t revision;
    uint64_t flags;
    uint64_t response;  /* Filled by bootloader: physical address of response */
} limoboot_request_t;

/* Request IDs */
#define LIMBOOT_REQUEST_MEMORY_MAP        0x00000001ULL
#define LIMBOOT_REQUEST_FRAMEBUFFER       0x00000002ULL
#define LIMBOOT_REQUEST_MODULES           0x00000003ULL
#define LIMBOOT_REQUEST_SMP               0x00000004ULL
#define LIMBOOT_REQUEST_RSDP              0x00000005ULL
#define LIMBOOT_REQUEST_SMBIOS            0x00000006ULL
#define LIMBOOT_REQUEST_EFI_SYSTEM_TABLE  0x00000007ULL
#define LIMBOOT_REQUEST_ENTROPY           0x00000008ULL
#define LIMBOOT_REQUEST_KERNEL_FILE       0x00000009ULL
#define LIMBOOT_REQUEST_CMDLINE           0x0000000AULL
#define LIMBOOT_REQUEST_MEASURED_BOOT     0x0000000BULL
#define LIMBOOT_REQUEST_PAGING_MODE       0x0000000CULL

/* ============================================================================
 * Memory Map
 * ============================================================================ */

#define LIMBOOT_MEMORY_USABLE             0
#define LIMBOOT_MEMORY_RESERVED           1
#define LIMBOOT_MEMORY_ACPI_RECLAIMABLE   2
#define LIMBOOT_MEMORY_ACPI_NVS           3
#define LIMBOOT_MEMORY_BAD                4
#define LIMBOOT_MEMORY_BOOTLOADER         5
#define LIMBOOT_MEMORY_KERNEL             6
#define LIMBOOT_MEMORY_MODULE             7
#define LIMBOOT_MEMORY_FRAMEBUFFER        8
#define LIMBOOT_MEMORY_HHDM               9
#define LIMBOOT_MEMORY_EFI_RUNTIME        10
#define LIMBOOT_MEMORY_EFI_BOOT_SERVICES  11
#define LIMBOOT_MEMORY_PERSISTENT         12

typedef struct limoboot_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
    uint64_t attributes;
} limoboot_memmap_entry_t;

typedef struct limoboot_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    uint64_t entry_size;
    uint64_t entries;  /* Physical address of limoboot_memmap_entry_t array */
} limoboot_memmap_response_t;

/* ============================================================================
 * Framebuffer
 * ============================================================================ */

#define LIMBOOT_FB_FORMAT_RGB   0
#define LIMBOOT_FB_FORMAT_BGR   1
#define LIMBOOT_FB_FORMAT_ARGB  2
#define LIMBOOT_FB_FORMAT_ABGR  3

typedef struct limoboot_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_addr;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint64_t bpp;
    uint64_t format;
    uint64_t red_mask;
    uint64_t green_mask;
    uint64_t blue_mask;
    uint64_t reserved_mask;
} limoboot_framebuffer_response_t;

/* ============================================================================
 * Modules
 * ============================================================================ */

typedef struct limoboot_module {
    uint64_t base;
    uint64_t length;
    uint64_t string;  /* Physical address of NUL-terminated string */
} limoboot_module_t;

typedef struct limoboot_modules_response {
    uint64_t revision;
    uint64_t module_count;
    uint64_t modules;  /* Physical address of limoboot_module_t array */
} limoboot_modules_response_t;

/* ============================================================================
 * SMP
 * ============================================================================ */

#define LIMBOOT_SMP_CPU_ENABLED  (1ULL << 0)
#define LIMBOOT_SMP_CPU_BSP      (1ULL << 1)

typedef struct limoboot_smp_cpu {
    uint64_t lapic_id;  /* x86: LAPIC ID, ARM: MPIDR, RISC-V: Hart ID */
    uint64_t flags;
    uint64_t reserved[6];
} limoboot_smp_cpu_t;

typedef struct limoboot_smp_response {
    uint64_t revision;
    uint64_t cpu_count;
    uint64_t bsp_index;
    uint64_t cpus;              /* Physical address of limoboot_smp_cpu_t array */
    uint64_t trampoline;        /* Physical address of AP trampoline */
    uint64_t trampoline_size;   /* Size of trampoline */
} limoboot_smp_response_t;

/* ============================================================================
 * RSDP
 * ============================================================================ */

typedef struct limoboot_rsdp_response {
    uint64_t revision;
    uint64_t rsdp_addr;
} limoboot_rsdp_response_t;

/* ============================================================================
 * SMBIOS
 * ============================================================================ */

typedef struct limoboot_smbios_response {
    uint64_t revision;
    uint64_t entry_point_addr;
} limoboot_smbios_response_t;

/* ============================================================================
 * EFI System Table
 * ============================================================================ */

typedef struct limoboot_efi_system_table_response {
    uint64_t revision;
    uint64_t system_table_addr;
} limoboot_efi_system_table_response_t;

/* ============================================================================
 * Entropy
 * ============================================================================ */

typedef struct limoboot_entropy_response {
    uint64_t revision;
    uint64_t entropy_addr;
    uint64_t entropy_size;
} limoboot_entropy_response_t;

/* ============================================================================
 * Kernel File
 * ============================================================================ */

typedef struct limoboot_kernel_file_response {
    uint64_t revision;
    uint64_t kernel_addr;
    uint64_t kernel_size;
} limoboot_kernel_file_response_t;

/* ============================================================================
 * Command Line
 * ============================================================================ */

typedef struct limoboot_cmdline_response {
    uint64_t revision;
    uint64_t cmdline_addr;
} limoboot_cmdline_response_t;

/* ============================================================================
 * Measured Boot
 * ============================================================================ */

typedef struct limoboot_measured_boot_response {
    uint64_t revision;
    uint64_t event_log_addr;
    uint64_t event_log_size;
    uint64_t tpm_addr;
} limoboot_measured_boot_response_t;

/* ============================================================================
 * Paging Mode
 * ============================================================================ */

#define LIMBOOT_PAGING_MODE_4LEVEL  0  /* x86-64: 4-level */
#define LIMBOOT_PAGING_MODE_5LEVEL  1  /* x86-64: 5-level (LA57) */
#define LIMBOOT_PAGING_MODE_SV39    2  /* RISC-V: Sv39 */
#define LIMBOOT_PAGING_MODE_SV48    3  /* RISC-V: Sv48 */
#define LIMBOOT_PAGING_MODE_SV57    4  /* RISC-V: Sv57 */
#define LIMBOOT_PAGING_MODE_LPAE    5  /* ARM: LPAE (48-bit) */
#define LIMBOOT_PAGING_MODE_L57     6  /* ARM: 52-bit (FEAT_LPA2) */

typedef struct limoboot_paging_mode_response {
    uint64_t revision;
    uint64_t mode;
    uint64_t flags;
} limoboot_paging_mode_response_t;

/* ============================================================================
 * Architecture-specific ABI Notes
 * ============================================================================ */

/*
 * x86-64 (System V AMD64 ABI):
 *   RAX = LIMBOOT_PROTOCOL_MAGIC
 *   RBX = handoff pointer (physical)
 *   RSP = 16-byte aligned stack (64 KiB minimum)
 *
 * aarch64 (AAPCS64):
 *   x0 = LIMBOOT_PROTOCOL_MAGIC
 *   x1 = handoff pointer (physical)
 *   sp = 16-byte aligned stack (64 KiB minimum)
 *
 * riscv64 (LP64):
 *   a0 = LIMBOOT_PROTOCOL_MAGIC
 *   a1 = handoff pointer (physical)
 *   sp = 16-byte aligned stack (64 KiB minimum)
 *
 * loongarch64 (LP64):
 *   a0 = LIMBOOT_PROTOCOL_MAGIC
 *   a1 = handoff pointer (physical)
 *   sp = 16-byte aligned stack (64 KiB minimum)
 */

#endif /* LIMBOOT_PROTOCOL_H */