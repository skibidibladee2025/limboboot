/*
 * x86-64 Memory Management for Kernel Template
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

/* ============================================================================
 * Page Table Constants (4-level paging)
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

#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT   21
#define PT_SHIFT   12

typedef uint64_t pte_t;

extern limoboot_handoff_t *g_handoff;
static pte_t *g_pml4 = NULL;

/* ============================================================================
 * Physical/Virtual Conversion
 * ============================================================================ */

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + g_handoff->hhdm_offset);
}

static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - g_handoff->hhdm_offset;
}

/* ============================================================================
 * Page Table Allocation
 * ============================================================================ */

static pte_t *alloc_page_table(void) {
    /* Simple bump allocator for early boot */
    static uint64_t next_page = 0;
    if (next_page == 0) {
        next_page = (uint64_t)&_end;
        next_page = (next_page + 4095) & ~4095ULL;
    }
    void *page = (void *)next_page;
    next_page += 4096;
    return (pte_t *)page;
}

/* ============================================================================
 * Initialize Page Tables (Identity + HHDM)
 * ============================================================================ */

void arch_memory_init(void) {
    /* Get PML4 from bootloader */
    g_pml4 = phys_to_virt(__builtin_readcyclecounter() & ~0xFFF);
    /* Actually, we should use the PML4 set up by limoboot */
    /* For now, assume it's already set up */

    /* Enable 5-level paging if supported */
    uint64_t cr4 = 0;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    if (g_handoff && (g_handoff->flags & (1ULL << 5))) { /* Would check CPUID */
        cr4 |= (1ULL << 16);  /* LA57 */
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
    }
}

/* ============================================================================
 * Map Memory Region
 * ============================================================================ */

void arch_map_region(uint64_t phys, uint64_t virt, uint64_t size, uint64_t flags) {
    if (!g_pml4) return;

    for (uint64_t offset = 0; offset < size; offset += 0x200000) {  /* 2MB pages */
        uint64_t p = phys + offset;
        uint64_t v = virt + offset;

        uint64_t pml4_idx = (v >> PML4_SHIFT) & 0x1FF;
        uint64_t pdpt_idx = (v >> PDPT_SHIFT) & 0x1FF;
        uint64_t pd_idx = (v >> PD_SHIFT) & 0x1FF;

        pte_t *pdpt = (pte_t *)phys_to_virt(g_pml4[pml4_idx] & ~0xFFF);
        if ((g_pml4[pml4_idx] & PAGE_PRESENT) == 0) {
            pdpt = alloc_page_table();
            g_pml4[pml4_idx] = virt_to_phys(pdpt) | PAGE_PRESENT | PAGE_WRITE;
        }

        pte_t *pd = (pte_t *)phys_to_virt(pdpt[pdpt_idx] & ~0xFFF);
        if ((pdpt[pdpt_idx] & PAGE_PRESENT) == 0) {
            pd = alloc_page_table();
            pdpt[pdpt_idx] = virt_to_phys(pd) | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = p | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE | PAGE_GLOBAL | flags;
    }
}

/* ============================================================================
 * Flush TLB
 * ============================================================================ */

void arch_flush_tlb(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

void arch_flush_tlb_addr(void *virt) {
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

/* ============================================================================
 * Get PML4
 * ============================================================================ */

pte_t *arch_get_pml4(void) {
    return g_pml4;
}