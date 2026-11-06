/*
 * aarch64 Memory Management for Kernel Template
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

/* ============================================================================
 * Page Table Constants (48-bit LPAE)
 * ============================================================================ */

#define PAGE_VALID      (1ULL << 0)
#define PAGE_TABLE      (1ULL << 1)
#define PAGE_BLOCK      (1ULL << 1)
#define PAGE_AF         (1ULL << 10)
#define PAGE_NG         (1ULL << 11)
#define PAGE_AP_RO      (0ULL << 6)
#define PAGE_AP_RW      (1ULL << 6)
#define PAGE_AP_USER    (2ULL << 6)
#define PAGE_ATTR_IDX(n) ((n) << 2)
#define PAGE_PXN        (1ULL << 53)
#define PAGE_UXN        (1ULL << 54)

#define MAIR_ATTR(n, attr) ((uint64_t)(attr) << ((n) * 8))
#define MAIR_DEVICE_nGnRnE  0x00
#define MAIR_NORMAL_NC      0x44
#define MAIR_NORMAL_WT      0xBB
#define MAIR_NORMAL_WB      0xFF

#define PAGE_SIZE       4096
#define BLOCK_SIZE      (2 * 1024 * 1024)  /* 2MB block */
#define PAGE_SHIFT      12
#define BLOCK_SHIFT     21

typedef uint64_t pte_t;

extern limoboot_handoff_t *g_handoff;
static pte_t *g_ttbr0 = NULL;
static pte_t *g_ttbr1 = NULL;

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
    /* Configure MAIR */
    uint64_t mair = MAIR_ATTR(0, MAIR_DEVICE_nGnRnE) |
                    MAIR_ATTR(1, MAIR_NORMAL_NC) |
                    MAIR_ATTR(2, MAIR_NORMAL_WT) |
                    MAIR_ATTR(3, MAIR_NORMAL_WB);
    __asm__ volatile ("msr mair_el1, %0" :: "r"(mair));

    /* Get TTBR from bootloader */
    /* For now, assume limoboot set up identity + HHDM */

    /* Enable MMU */
    uint64_t sctlr;
    __asm__ volatile ("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0) | (1 << 2) | (1 << 12);  /* M + C + I */
    __asm__ volatile ("msr sctlr_el1, %0" :: "r"(sctlr));
    __asm__ volatile ("isb");
}

/* ============================================================================
 * Map Memory Region (2MB blocks)
 * ============================================================================ */

void arch_map_region(uint64_t phys, uint64_t virt, uint64_t size, uint64_t flags) {
    if (!g_ttbr1) return;

    for (uint64_t offset = 0; offset < size; offset += BLOCK_SIZE) {
        uint64_t p = phys + offset;
        uint64_t v = virt + offset;

        uint64_t l0_idx = (v >> 39) & 0x1FF;
        uint64_t l1_idx = (v >> 30) & 0x1FF;
        uint64_t l2_idx = (v >> 21) & 0x1FF;

        pte_t *l1 = (pte_t *)phys_to_virt(g_ttbr1[l0_idx] & ~0xFFF);
        if ((g_ttbr1[l0_idx] & PAGE_VALID) == 0) {
            l1 = alloc_page_table();
            g_ttbr1[l0_idx] = virt_to_phys(l1) | PAGE_VALID | PAGE_TABLE;
        }

        pte_t *l2 = (pte_t *)phys_to_virt(l1[l1_idx] & ~0xFFF);
        if ((l1[l1_idx] & PAGE_VALID) == 0) {
            l2 = alloc_page_table();
            l1[l1_idx] = virt_to_phys(l2) | PAGE_VALID | PAGE_TABLE;
        }

        l2[l2_idx] = p | PAGE_VALID | PAGE_BLOCK | PAGE_AF | PAGE_AP_RW |
                     PAGE_ATTR_IDX(3) | flags;
    }
}

/* ============================================================================
 * Flush TLB
 * ============================================================================ */

void arch_flush_tlb(void) {
    __asm__ volatile ("tlbi vmalle1is; dsb ish; isb" ::: "memory");
}

void arch_flush_tlb_addr(void *virt) {
    __asm__ volatile ("tlbi vae1is, %0; dsb ish; isb" :: "r"(virt) : "memory");
}

/* ============================================================================
 * Get TTBR
 * ============================================================================ */

pte_t *arch_get_ttbr0(void) {
    return g_ttbr0;
}

pte_t *arch_get_ttbr1(void) {
    return g_ttbr1;
}