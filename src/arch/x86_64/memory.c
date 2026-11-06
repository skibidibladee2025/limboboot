/*
 * x86-64 Memory Management
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
 * x86-64 Specific Memory Constants
 * ============================================================================ */

#define X86_64_HHDM_OFFSET      0xFFFF800000000000ULL
#define X86_64_KERNEL_BASE      0xFFFFFFFF80000000ULL
#define X86_64_PAGE_SIZE        4096
#define X86_64_LARGE_PAGE_SIZE  (2 * 1024 * 1024)  /* 2MB */

/* ============================================================================
 * Memory Region Types (Extended)
 * ============================================================================ */

#define MEM_TYPE_EFI_RUNTIME    10
#define MEM_TYPE_EFI_BOOT_SVC   11
#define MEM_TYPE_HHDM_MAP       12

/* ============================================================================
 * Page Table Management (4-level)
 * ============================================================================ */

#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT   21
#define PT_SHIFT   12

#define PML4_MASK  (0x1FFULL << PML4_SHIFT)
#define PDPT_MASK  (0x1FFULL << PDPT_SHIFT)
#define PD_MASK    (0x1FFULL << PD_SHIFT)
#define PT_MASK    (0x1FFULL << PT_SHIFT)

#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITE      (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_PWT        (1ULL << 3)
#define PAGE_PCD        (1ULL << 4)
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_PSE        (1ULL << 7)   /* 2MB page at PD level */
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NX         (1ULL << 63)

typedef uint64_t pte_t;

/* ============================================================================
 * Page Table Allocation
 * ============================================================================ */

static pte_t *alloc_page_table(void) {
    EFI_PHYSICAL_ADDRESS phys;
    EFI_STATUS status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &phys);
    if (EFI_ERROR(status)) return NULL;
    pte_t *table = phys_to_virt(phys);
    memset(table, 0, 512 * sizeof(pte_t));
    return table;
}

/* ============================================================================
 * Identity Mapping (for trampoline and low memory)
 * ============================================================================ */

EFI_STATUS arch_identity_map(UINT64 base, UINT64 size, UINT64 flags) {
    pte_t *pml4 = phys_to_virt(g_pml4_phys);

    for (UINT64 addr = base; addr < base + size; addr += X86_64_LARGE_PAGE_SIZE) {
        UINTN pml4_idx = (addr >> PML4_SHIFT) & 0x1FF;
        UINTN pdpt_idx = (addr >> PDPT_SHIFT) & 0x1FF;
        UINTN pd_idx = (addr >> PD_SHIFT) & 0x1FF;

        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFF);
        if ((pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = addr | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE | PAGE_GLOBAL | flags;
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * Higher-Half Direct Map (HHDM)
 * ============================================================================ */

EFI_STATUS arch_setup_hhdm(EFI_PHYSICAL_ADDRESS hhdm_offset, UINT64 hhdm_size) {
    pte_t *pml4 = phys_to_virt(g_pml4_phys);

    for (UINT64 addr = 0; addr < hhdm_size; addr += X86_64_LARGE_PAGE_SIZE) {
        UINT64 virt = hhdm_offset + addr;
        UINTN pml4_idx = (virt >> PML4_SHIFT) & 0x1FF;
        UINTN pdpt_idx = (virt >> PDPT_SHIFT) & 0x1FF;
        UINTN pd_idx = (virt >> PD_SHIFT) & 0x1FF;

        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFF);
        if ((pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = addr | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE | PAGE_GLOBAL;
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * Kernel Mapping
 * ============================================================================ */

EFI_STATUS arch_map_kernel(EFI_PHYSICAL_ADDRESS kernel_phys, UINT64 kernel_virt, UINT64 kernel_size) {
    pte_t *pml4 = phys_to_virt(g_pml4_phys);

    UINT64 aligned_phys = kernel_phys & ~(X86_64_LARGE_PAGE_SIZE - 1);
    UINT64 aligned_virt = kernel_virt & ~(X86_64_LARGE_PAGE_SIZE - 1);
    UINT64 end = kernel_phys + kernel_size;
    end = (end + X86_64_LARGE_PAGE_SIZE - 1) & ~(X86_64_LARGE_PAGE_SIZE - 1);

    for (UINT64 phys = aligned_phys, virt = aligned_virt; phys < end; phys += X86_64_LARGE_PAGE_SIZE, virt += X86_64_LARGE_PAGE_SIZE) {
        UINTN pml4_idx = (virt >> PML4_SHIFT) & 0x1FF;
        UINTN pdpt_idx = (virt >> PDPT_SHIFT) & 0x1FF;
        UINTN pd_idx = (virt >> PD_SHIFT) & 0x1FF;

        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFF);
        if ((pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = phys | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE | PAGE_GLOBAL;
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * Framebuffer Mapping
 * ============================================================================ */

EFI_STATUS arch_map_framebuffer(EFI_PHYSICAL_ADDRESS fb_phys, UINT64 fb_virt, UINT64 fb_size) {
    return arch_map_kernel(fb_phys, fb_virt, fb_size);
}

/* ============================================================================
 * Bootloader Data Mapping
 * ============================================================================ */

EFI_STATUS arch_map_bootloader_data(EFI_PHYSICAL_ADDRESS phys, UINT64 virt, UINT64 size) {
    pte_t *pml4 = phys_to_virt(g_pml4_phys);

    for (UINT64 offset = 0; offset < size; offset += X86_64_PAGE_SIZE) {
        UINT64 p = phys + offset;
        UINT64 v = virt + offset;
        UINTN pml4_idx = (v >> PML4_SHIFT) & 0x1FF;
        UINTN pdpt_idx = (v >> PDPT_SHIFT) & 0x1FF;
        UINTN pd_idx = (v >> PD_SHIFT) & 0x1FF;
        UINTN pt_idx = (v >> PT_SHIFT) & 0x1FF;

        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFF);
        if ((pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pt = (pte_t *)phys_to_virt(pd[pd_idx] & ~0xFFF);
        if ((pd[pd_idx] & PAGE_PRESENT) == 0) {
            pt = alloc_page_table();
            if (!pt) return EFI_OUT_OF_RESOURCES;
            pd[pd_idx] = virt_to_phys(pt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pt[pt_idx] = p | PAGE_PRESENT | PAGE_WRITE | PAGE_GLOBAL;
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * 5-Level Paging (LA57) Support
 * ============================================================================ */

EFI_STATUS arch_setup_5level_paging(EFI_PHYSICAL_ADDRESS hhdm_offset, UINT64 hhdm_size) {
    if (!arch_has_feature("la57")) {
        return EFI_UNSUPPORTED;
    }

    /* Enable 5-level paging in CR4 */
    uint64_t cr4 = arch_read_cr4();
    cr4 |= (1ULL << 16);  /* LA57 */
    arch_write_cr4(cr4);

    /* Enable in EFER */
    uint64_t efer = arch_read_msr(0xC0000080);
    efer |= (1ULL << 10);  /* LMA5 */
    arch_write_msr(0xC0000080, efer);

    /* Would need to rebuild page tables with 5 levels */
    return EFI_UNSUPPORTED;  /* Not fully implemented */
}

/* ============================================================================
 * Memory Attributes
 * ============================================================================ */

UINT64 arch_efi_attr_to_pte(UINT64 efi_attr) {
    UINT64 pte = 0;
    if (efi_attr & EFI_MEMORY_RP) pte |= PAGE_NX;
    if (efi_attr & EFI_MEMORY_RO) pte |= 0;  /* Write bit not set = read-only */
    if (efi_attr & EFI_MEMORY_WB) pte |= 0;  /* Write-back is default */
    if (efi_attr & EFI_MEMORY_WT) pte |= PAGE_PWT;
    if (efi_attr & EFI_MEMORY_UC) pte |= (PAGE_PWT | PAGE_PCD);
    if (efi_attr & EFI_MEMORY_WP) pte |= 0;  /* Handled by CR0.WP */
    return pte;
}

/* ============================================================================
 * Page Table Dump (Debug)
 * ============================================================================ */

void arch_dump_page_tables(void) {
    pte_t *pml4 = phys_to_virt(g_pml4_phys);
    efi_printf("PML4 at %p\n", pml4);

    for (int i = 0; i < 512; i++) {
        if (pml4[i] & PAGE_PRESENT) {
            efi_printf("  PML4[%d] = %llx\n", i, pml4[i]);
            pte_t *pdpt = (pte_t *)phys_to_virt(pml4[i] & ~0xFFF);
            for (int j = 0; j < 512; j++) {
                if (pdpt[j] & PAGE_PRESENT) {
                    efi_printf("    PDPT[%d] = %llx\n", j, pdpt[j]);
                    if (pdpt[j] & PAGE_PSE) {
                        /* 1GB page */
                    } else {
                        pte_t *pd = (pte_t *)phys_to_virt(pdpt[j] & ~0xFFF);
                        for (int k = 0; k < 512; k++) {
                            if (pd[k] & PAGE_PRESENT) {
                                efi_printf("      PD[%d] = %llx\n", k, pd[k]);
                                if (pd[k] & PAGE_PSE) {
                                    /* 2MB page */
                                } else {
                                    pte_t *pt = (pte_t *)phys_to_virt(pd[k] & ~0xFFF);
                                    for (int l = 0; l < 512; l++) {
                                        if (pt[l] & PAGE_PRESENT) {
                                            efi_printf("        PT[%d] = %llx\n", l, pt[l]);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ============================================================================
 * TLB Flush
 * ============================================================================ */

void arch_flush_tlb(void) {
    /* Reload CR3 to flush TLB */
    uint64_t cr3 = arch_read_cr3();
    arch_write_cr3(cr3);
}

void arch_flush_tlb_addr(void *virt) {
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}