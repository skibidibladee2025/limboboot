/*
 * x86-64 UEFI Architecture-Specific Code
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"
#include "memory.c"
#include "efi.c"
#include "printf.c"

/* ============================================================================
 * x86-64 Specific Globals
 * ============================================================================ */

static EFI_PHYSICAL_ADDRESS g_pml4_phys = 0;
static EFI_PHYSICAL_ADDRESS g_stack_phys = 0;
static void *g_stack_virt = NULL;
static limoboot_handoff_t *g_handoff = NULL;

/* ============================================================================
 * Architecture Initialization
 * ============================================================================ */

EFI_STATUS arch_efi_init(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *st) {
    efi_init(image_handle, st);

    /* Allocate stack (64 KiB) */
    EFI_PHYSICAL_ADDRESS stack_phys;
    EFI_STATUS status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 16, &stack_phys);
    if (EFI_ERROR(status)) return status;

    g_stack_phys = stack_phys;
    g_stack_virt = phys_to_virt(stack_phys + 64 * 1024);  /* Top of stack */

    return EFI_SUCCESS;
}

/* ============================================================================
 * Page Table Setup (4-level paging)
 * ============================================================================ */

#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITE      (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_PWT        (1ULL << 3)
#define PAGE_PCD        (1ULL << 4)
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_PSE        (1ULL << 7)   /* 2MB page */
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NX         (1ULL << 63)

#define PML4_ENTRIES 512
#define PDPT_ENTRIES 512
#define PD_ENTRIES   512
#define PT_ENTRIES   512

typedef uint64_t pte_t;

static pte_t *alloc_page_table(void) {
    EFI_PHYSICAL_ADDRESS phys;
    EFI_STATUS status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &phys);
    if (EFI_ERROR(status)) return NULL;
    return phys_to_virt(phys);
}

EFI_STATUS arch_setup_page_tables(EFI_PHYSICAL_ADDRESS hhdm_offset, UINT64 hhdm_size) {
    /* Allocate PML4 */
    pte_t *pml4 = alloc_page_table();
    if (!pml4) return EFI_OUT_OF_RESOURCES;
    memset(pml4, 0, PML4_ENTRIES * sizeof(pte_t));
    g_pml4_phys = virt_to_phys(pml4);

    /* Identity map first 1GB (for trampoline and low memory) */
    for (UINT64 addr = 0; addr < 0x40000000; addr += 0x200000) {  /* 2MB pages */
        UINTN pml4_idx = (addr >> 39) & 0x1FF;
        UINTN pdpt_idx = (addr >> 30) & 0x1FF;
        UINTN pd_idx = (addr >> 21) & 0x1FF;

        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFF);
        if ((pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            memset(pdpt, 0, PDPT_ENTRIES * sizeof(pte_t));
            pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            memset(pd, 0, PD_ENTRIES * sizeof(pte_t));
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = addr | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE | PAGE_GLOBAL;
    }

    /* Higher-half direct map (HHDM) */
    /* Map physical memory at hhdm_offset */
    for (UINT64 addr = 0; addr < hhdm_size; addr += 0x200000) {
        UINT64 virt = hhdm_offset + addr;
        UINTN pml4_idx = (virt >> 39) & 0x1FF;
        UINTN pdpt_idx = (virt >> 30) & 0x1FF;
        UINTN pd_idx = (virt >> 21) & 0x1FF;

        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFF);
        if ((pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            memset(pdpt, 0, PDPT_ENTRIES * sizeof(pte_t));
            pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            memset(pd, 0, PD_ENTRIES * sizeof(pte_t));
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = addr | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE | PAGE_GLOBAL;
    }

    /* Map kernel at its virtual address (will be filled in later) */
    /* Map framebuffer */
    /* Map bootloader data structures */

    return EFI_SUCCESS;
}

void arch_load_page_tables(void) {
    /* Load CR3 with PML4 physical address */
    __asm__ volatile ("mov %0, %%cr3" :: "r"(g_pml4_phys) : "memory");
}

EFI_STATUS arch_map_range(EFI_PHYSICAL_ADDRESS phys, EFI_PHYSICAL_ADDRESS virt,
                           UINT64 size, UINT64 flags) {
    /* Map a physical range to virtual address */
    for (UINT64 offset = 0; offset < size; offset += 0x200000) {
        UINT64 p = phys + offset;
        UINT64 v = virt + offset;
        UINTN pml4_idx = (v >> 39) & 0x1FF;
        UINTN pdpt_idx = (v >> 30) & 0x1FF;
        UINTN pd_idx = (v >> 21) & 0x1FF;

        pte_t *pml4 = phys_to_virt(g_pml4_phys);
        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFF);
        if ((pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            memset(pdpt, 0, PDPT_ENTRIES * sizeof(pte_t));
            pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            memset(pd, 0, PD_ENTRIES * sizeof(pte_t));
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = p | PAGE_PRESENT | PAGE_WRITE | flags | PAGE_PSE | PAGE_GLOBAL;
    }
    return EFI_SUCCESS;
}

/* ============================================================================
 * Stack Management
 * ============================================================================ */

void *arch_get_stack_top(void) {
    return g_stack_virt;
}

EFI_PHYSICAL_ADDRESS arch_get_stack_phys(void) {
    return g_stack_phys + 64 * 1024;
}

/* ============================================================================
 * Handoff Structure Setup
 * ============================================================================ */

limoboot_handoff_t *arch_create_handoff(EFI_PHYSICAL_ADDRESS hhdm_offset) {
    limoboot_handoff_t *handoff = efi_allocate_pool(sizeof(limoboot_handoff_t), EfiLoaderData);
    if (!handoff) return NULL;

    memset(handoff, 0, sizeof(limoboot_handoff_t));
    handoff->magic = LIMBOOT_PROTOCOL_MAGIC;
    handoff->version = LIMBOOT_PROTOCOL_VERSION;
    handoff->revision = LIMBOOT_PROTOCOL_REVISION;
    handoff->flags = LIMBOOT_FLAG_HHDM_PRESENT;
    handoff->hhdm_offset = hhdm_offset;
    handoff->stack_top = arch_get_stack_phys();

    g_handoff = handoff;
    return handoff;
}

/* ============================================================================
 * CPU Features
 * ============================================================================ */

typedef struct {
    uint32_t eax, ebx, ecx, edx;
} cpuid_regs_t;

static void cpuid(cpuid_regs_t *regs) {
    __asm__ volatile ("cpuid"
        : "=a"(regs->eax), "=b"(regs->ebx), "=c"(regs->ecx), "=d"(regs->edx)
        : "a"(regs->eax), "c"(regs->ecx));
}

int arch_has_feature(const char *feature) {
    cpuid_regs_t regs = {0};

    if (strcmp(feature, "rdrand") == 0) {
        regs.eax = 1;
        cpuid(&regs);
        return (regs.ecx & (1 << 30)) != 0;
    } else if (strcmp(feature, "rdseed") == 0) {
        regs.eax = 7;
        regs.ecx = 0;
        cpuid(&regs);
        return (regs.ebx & (1 << 18)) != 0;
    } else if (strcmp(feature, "xsave") == 0) {
        regs.eax = 1;
        cpuid(&regs);
        return (regs.ecx & (1 << 26)) != 0;
    } else if (strcmp(feature, "fsgsbase") == 0) {
        regs.eax = 7;
        regs.ecx = 0;
        cpuid(&regs);
        return (regs.ebx & (1 << 0)) != 0;
    } else if (strcmp(feature, "smep") == 0) {
        regs.eax = 7;
        regs.ecx = 0;
        cpuid(&regs);
        return (regs.ebx & (1 << 7)) != 0;
    } else if (strcmp(feature, "smap") == 0) {
        regs.eax = 7;
        regs.ecx = 0;
        cpuid(&regs);
        return (regs.ebx & (1 << 20)) != 0;
    } else if (strcmp(feature, "pku") == 0) {
        regs.eax = 7;
        regs.ecx = 0;
        cpuid(&regs);
        return (regs.ecx & (1 << 3)) != 0;
    } else if (strcmp(feature, "la57") == 0) {  /* 5-level paging */
        regs.eax = 7;
        regs.ecx = 0;
        cpuid(&regs);
        return (regs.ecx & (1 << 16)) != 0;
    }

    return 0;
}

/* ============================================================================
 * MSR Access
 * ============================================================================ */

uint64_t arch_read_msr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void arch_write_msr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

/* ============================================================================
 * TSC
 * ============================================================================ */

uint64_t arch_read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* ============================================================================
 * AP Startup
 * ============================================================================ */

EFI_STATUS arch_start_ap(UINTN apic_id, void (*entry)(void)) {
    /* Send INIT-SIPI-SIPI sequence via local APIC */
    /* This is a simplified version - real implementation would use LAPIC */
    (void)apic_id;
    (void)entry;
    return EFI_UNSUPPORTED;  /* Placeholder */
}

/* ============================================================================
 * CR0/CR3/CR4 Access
 * ============================================================================ */

uint64_t arch_read_cr0(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

void arch_write_cr0(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr0" :: "r"(val) : "memory");
}

uint64_t arch_read_cr3(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

void arch_write_cr3(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(val) : "memory");
}

uint64_t arch_read_cr4(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

void arch_write_cr4(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr4" :: "r"(val) : "memory");
}