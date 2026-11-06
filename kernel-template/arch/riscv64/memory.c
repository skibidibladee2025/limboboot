/*
 * riscv64 Memory Management for Kernel Template
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

/* ============================================================================
 * Page Table Constants (Sv48)
 * ============================================================================ */

#define PAGE_V          (1ULL << 0)
#define PAGE_R          (1ULL << 1)
#define PAGE_W          (1ULL << 2)
#define PAGE_X          (1ULL << 3)
#define PAGE_U          (1ULL << 4)
#define PAGE_G          (1ULL << 5)
#define PAGE_A          (1ULL << 6)
#define PAGE_D          (1ULL << 7)
#define PAGE_RSW        (3ULL << 8)

#define PAGE_SIZE       4096
#define MEGAPAGE_SIZE   (2 * 1024 * 1024)  /* 2MB megapage */
#define GIGAPAGE_SIZE   (1ULL * 1024 * 1024 * 1024)  /* 1GB gigapage */

#define PPN_SHIFT       10
#define VPN_SHIFT       12

typedef uint64_t pte_t;

extern limoboot_handoff_t *g_handoff;
static pte_t *g_satp = NULL;

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
    /* Get SATP from bootloader */
    /* For now, assume limoboot set up Sv48 with identity + HHDM */

    /* Enable S-mode paging */
    uint64_t satp = 0;
    __asm__ volatile ("csrr %0, satp" : "=r"(satp));
    if ((satp >> 60) == 0) {
        /* Not using Sv48 yet - would need to build page tables */
    }
}

/* ============================================================================
 * Map Memory Region (2MB megapages)
 * ============================================================================ */

void arch_map_region(uint64_t phys, uint64_t virt, uint64_t size, uint64_t flags) {
    if (!g_satp) return;

    for (uint64_t offset = 0; offset < size; offset += MEGAPAGE_SIZE) {
        uint64_t p = phys + offset;
        uint64_t v = virt + offset;

        uint64_t vpn2 = (v >> 30) & 0x1FF;
        uint64_t vpn1 = (v >> 21) & 0x1FF;
        uint64_t vpn0 = (v >> 12) & 0x1FF;

        pte_t *l2 = (pte_t *)phys_to_virt((g_satp & 0xFFFFFFFFFFFFFULL) << 12);
        pte_t *l1 = (pte_t *)phys_to_virt(l2[vpn2] & ~0xFFF);
        if ((l2[vpn2] & PAGE_V) == 0) {
            l1 = alloc_page_table();
            l2[vpn2] = (virt_to_phys(l1) >> 12) | PAGE_V;
        }

        pte_t *l0 = (pte_t *)phys_to_virt(l1[vpn1] & ~0xFFF);
        if ((l1[vpn1] & PAGE_V) == 0) {
            l0 = alloc_page_table();
            l1[vpn1] = (virt_to_phys(l0) >> 12) | PAGE_V;
        }

        l0[vpn0] = (p >> 12) | PAGE_V | PAGE_R | PAGE_W | PAGE_X | PAGE_G | flags;
    }
}

/* ============================================================================
 * Flush TLB
 * ============================================================================ */

void arch_flush_tlb(void) {
    __asm__ volatile ("sfence.vma" ::: "memory");
}

void arch_flush_tlb_addr(void *virt) {
    __asm__ volatile ("sfence.vma %0" :: "r"(virt) : "memory");
}

/* ============================================================================
 * Get SATP
 * ============================================================================ */

pte_t *arch_get_satp(void) {
    return g_satp;
}