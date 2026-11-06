/*
 * loongarch64 Memory Management for Kernel Template
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

/* ============================================================================
 * Page Table Constants (48-bit LA48 / 52-bit LA52)
 * ============================================================================ */

#define PAGE_V          (1ULL << 0)
#define PAGE_D          (1ULL << 1)
#define PAGE_PLV(n)     ((n) << 2)
#define PAGE_MAT(n)     ((n) << 4)
#define PAGE_G          (1ULL << 7)
#define PAGE_R          (1ULL << 8)
#define PAGE_W          (1ULL << 9)
#define PAGE_X          (1ULL << 10)
#define PAGE_U          (1ULL << 11)

#define MAT_CC          0  /* Cacheable, coherent */
#define MAT_CG          1  /* Cacheable, non-coherent */
#define MAT_UC          2  /* Uncacheable */
#define MAT_WC          3  /* Write-combining */

#define PAGE_SIZE       4096
#define LARGE_PAGE_SIZE (2 * 1024 * 1024)  /* 2MB */

typedef uint64_t pte_t;

extern limoboot_handoff_t *g_handoff;
static pte_t *g_pgdl = NULL;  /* PGDL - Page Global Directory Low */
static pte_t *g_pgdh = NULL;  /* PGDH - Page Global Directory High */

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
 * Initialize Page Tables
 * ============================================================================ */

void arch_memory_init(void) {
    /* Get PGDL/PGDH from bootloader */
    /* For now, assume limoboot set up LA48 with identity + HHDM */

    /* Enable paging */
    uint64_t crmd;
    __asm__ volatile ("csrr %0, crmd" : "=r"(crmd));
    crmd |= (1ULL << 0);  /* PG */
    __asm__ volatile ("csrw crmd, %0" :: "r"(crmd));
}

/* ============================================================================
 * Map Memory Region (2MB large pages)
 * ============================================================================ */

void arch_map_region(uint64_t phys, uint64_t virt, uint64_t size, uint64_t flags) {
    if (!g_pgdh) return;

    for (uint64_t offset = 0; offset < size; offset += LARGE_PAGE_SIZE) {
        uint64_t p = phys + offset;
        uint64_t v = virt + offset;

        /* LA48: PGDH[9:0] -> PUD[9:0] -> PMD[9:0] -> PTE[9:0] */
        uint64_t pgdh_idx = (v >> 39) & 0x3FF;
        uint64_t pud_idx = (v >> 30) & 0x1FF;
        uint64_t pmd_idx = (v >> 21) & 0x1FF;

        pte_t *pud = (pte_t *)phys_to_virt(g_pgdh[pgdh_idx] & ~0xFFF);
        if ((g_pgdh[pgdh_idx] & PAGE_V) == 0) {
            pud = alloc_page_table();
            g_pgdh[pgdh_idx] = (virt_to_phys(pud) >> 12) | PAGE_V | PAGE_MAT(MAT_CC);
        }

        pte_t *pmd = (pte_t *)phys_to_virt(pud[pud_idx] & ~0xFFF);
        if ((pud[pud_idx] & PAGE_V) == 0) {
            pmd = alloc_page_table();
            pud[pud_idx] = (virt_to_phys(pmd) >> 12) | PAGE_V | PAGE_MAT(MAT_CC);
        }

        pmd[pmd_idx] = (p >> 12) | PAGE_V | PAGE_D | PAGE_MAT(MAT_CC) |
                       PAGE_G | PAGE_R | PAGE_W | PAGE_X | flags;
    }
}

/* ============================================================================
 * Flush TLB
 * ============================================================================ */

void arch_flush_tlb(void) {
    __asm__ volatile ("tlbi 0x0; dsb; isb" ::: "memory");
}

void arch_flush_tlb_addr(void *virt) {
    __asm__ volatile ("tlbi 0x1, %0; dsb; isb" :: "r"(virt) : "memory");
}

/* ============================================================================
 * Get PGDH
 * ============================================================================ */

pte_t *arch_get_pgdh(void) {
    return g_pgdh;
}