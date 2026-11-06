/*
 * Memory Management for limoboot
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"

/* ============================================================================
 * Memory Types (internal)
 * ============================================================================ */

#define MEM_TYPE_FREE         0
#define MEM_TYPE_KERNEL       1
#define MEM_TYPE_MODULE       2
#define MEM_TYPE_HANDOFF      3
#define MEM_TYPE_RESPONSE     4
#define MEM_TYPE_PAGE_TABLES  5
#define MEM_TYPE_FRAMEBUFFER  6
#define MEM_TYPE_BOOTLOADER   7
#define MEM_TYPE_STACK        8
#define MEM_TYPE_TRAMPOLINE   9

/* ============================================================================
 * Memory Region Tracking
 * ============================================================================ */

typedef struct mem_region {
    EFI_PHYSICAL_ADDRESS base;
    UINT64 size;
    UINT64 type;
    UINT64 attributes;
    struct mem_region *next;
} mem_region_t;

static mem_region_t *g_mem_regions = NULL;
static EFI_PHYSICAL_ADDRESS g_hhdm_offset = 0;
static UINT64 g_hhdm_size = 0;

/* ============================================================================
 * Memory Map Processing
 * ============================================================================ */

EFI_STATUS mem_init(EFI_PHYSICAL_ADDRESS hhdm_offset, UINT64 hhdm_size) {
    g_hhdm_offset = hhdm_offset;
    g_hhdm_size = hhdm_size;

    // Get EFI memory map
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key, desc_size;
    UINT32 desc_version;

    EFI_STATUS status = g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    map_size += 4096;
    map = efi_allocate_pool(map_size, EfiBootServicesData);
    if (!map) return EFI_OUT_OF_RESOURCES;

    status = g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        efi_free_pool(map);
        return status;
    }

    // Process memory map entries
    for (UINTN i = 0; i < map_size / desc_size; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);

        // Only track usable memory
        if (desc->Type == EfiConventionalMemory ||
            desc->Type == EfiPersistentMemory) {
            mem_region_t *region = efi_allocate_pool(sizeof(mem_region_t), EfiBootServicesData);
            if (!region) continue;

            region->base = desc->PhysicalStart;
            region->size = desc->NumberOfPages * 4096;
            region->type = MEM_TYPE_FREE;
            region->attributes = desc->Attribute;
            region->next = g_mem_regions;
            g_mem_regions = region;
        }
    }

    efi_free_pool(map);
    return EFI_SUCCESS;
}

/* ============================================================================
 * Physical Memory Allocation
 * ============================================================================ */

EFI_PHYSICAL_ADDRESS mem_alloc_phys(UINT64 size, UINT64 alignment, UINT64 type) {
    // Align size to page boundary
    size = (size + 4095) & ~4095ULL;

    // Search for suitable region
    mem_region_t **prev = &g_mem_regions;
    mem_region_t *region = g_mem_regions;

    while (region) {
        if (region->type == MEM_TYPE_FREE) {
            // Align base address
            EFI_PHYSICAL_ADDRESS aligned_base = (region->base + alignment - 1) & ~(alignment - 1);
            UINT64 padding = aligned_base - region->base;

            if (region->size >= padding + size) {
                // Found suitable region
                if (padding > 0) {
                    // Split: keep first part as free
                    mem_region_t *new_region = efi_allocate_pool(sizeof(mem_region_t), EfiBootServicesData);
                    if (new_region) {
                        new_region->base = region->base;
                        new_region->size = padding;
                        new_region->type = MEM_TYPE_FREE;
                        new_region->attributes = region->attributes;
                        new_region->next = region->next;
                        region->next = new_region;
                    }
                    region->base = aligned_base;
                    region->size -= padding;
                }

                if (region->size > size) {
                    // Split: keep remainder as free
                    mem_region_t *new_region = efi_allocate_pool(sizeof(mem_region_t), EfiBootServicesData);
                    if (new_region) {
                        new_region->base = region->base + size;
                        new_region->size = region->size - size;
                        new_region->type = MEM_TYPE_FREE;
                        new_region->attributes = region->attributes;
                        new_region->next = region->next;
                        region->next = new_region;
                    }
                    region->size = size;
                }

                region->type = type;
                return region->base;
            }
        }
        prev = &region->next;
        region = region->next;
    }

    // Fallback: use EFI AllocatePages
    EFI_PHYSICAL_ADDRESS addr = 0;
    UINTN pages = (size + 4095) / 4096;
    EFI_STATUS status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &addr);
    if (EFI_ERROR(status)) {
        return 0;
    }
    return addr;
}

/* ============================================================================
 * Virtual Memory (HHDM)
 * ============================================================================ */

void *phys_to_virt(EFI_PHYSICAL_ADDRESS phys) {
    return (void *)(phys + g_hhdm_offset);
}

EFI_PHYSICAL_ADDRESS virt_to_phys(void *virt) {
    return (EFI_PHYSICAL_ADDRESS)virt - g_hhdm_offset;
}

/* ============================================================================
 * Page Table Management (x86-64)
 * ============================================================================ */

#if defined(__x86_64__)

#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITE      (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_PWT        (1ULL << 3)
#define PAGE_PCD        (1ULL << 4)
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_PSE        (1ULL << 7)  // 2MB page
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NX         (1ULL << 63)

typedef struct {
    uint64_t entries[512];
} page_table_t;

static page_table_t *g_pml4 = NULL;

EFI_STATUS mem_setup_page_tables(void) {
    // Allocate PML4
    EFI_PHYSICAL_ADDRESS pml4_phys;
    EFI_STATUS status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pml4_phys);
    if (EFI_ERROR(status)) return status;

    g_pml4 = phys_to_virt(pml4_phys);
    memset(g_pml4, 0, sizeof(page_table_t));

    // Identity map first 2MB (for trampoline)
    page_table_t *pdpt = phys_to_virt(pml4_phys + 4096);
    memset(pdpt, 0, sizeof(page_table_t));
    g_pml4->entries[0] = (uint64_t)pml4_phys + 4096 | PAGE_PRESENT | PAGE_WRITE;

    page_table_t *pd = phys_to_virt(pml4_phys + 8192);
    memset(pd, 0, sizeof(page_table_t));
    pdpt->entries[0] = (uint64_t)pml4_phys + 8192 | PAGE_PRESENT | PAGE_WRITE;

    // 2MB page for first 2MB
    pd->entries[0] = 0 | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE | PAGE_GLOBAL;

    // Higher-half map for kernel and HHDM
    // This is architecture-specific and would be filled in by arch code
    return EFI_SUCCESS;
}

void mem_load_page_tables(void) {
    if (g_pml4) {
        __asm__ volatile ("mov %0, %%cr3" :: "r"(virt_to_phys(g_pml4)) : "memory");
    }
}

#endif /* __x86_64__ */

/* ============================================================================
 * Memory Map Response Generation
 * ============================================================================ */

EFI_STATUS mem_build_memmap_response(limoboot_memmap_response_t *response,
                                      limoboot_memmap_entry_t **entries) {
    // Count entries
    UINTN count = 0;
    mem_region_t *region = g_mem_regions;
    while (region) {
        count++;
        region = region->next;
    }

    // Allocate entry array
    *entries = efi_allocate_pool(count * sizeof(limoboot_memmap_entry_t), EfiLoaderData);
    if (!*entries) return EFI_OUT_OF_RESOURCES;

    // Fill entries
    region = g_mem_regions;
    for (UINTN i = 0; i < count; i++) {
        (*entries)[i].base = region->base;
        (*entries)[i].length = region->size;

        // Convert EFI type to limoboot type
        switch (region->attributes & 0xFF) {
            case EfiConventionalMemory:
            case EfiPersistentMemory:
                (*entries)[i].type = LIMBOOT_MEMORY_USABLE;
                break;
            case EfiACPIReclaimMemory:
                (*entries)[i].type = LIMBOOT_MEMORY_ACPI_RECLAIMABLE;
                break;
            case EfiACPIMemoryNVS:
                (*entries)[i].type = LIMBOOT_MEMORY_ACPI_NVS;
                break;
            case EfiUnusableMemory:
                (*entries)[i].type = LIMBOOT_MEMORY_BAD;
                break;
            case EfiLoaderCode:
            case EfiLoaderData:
            case EfiBootServicesCode:
            case EfiBootServicesData:
                (*entries)[i].type = LIMBOOT_MEMORY_BOOTLOADER;
                break;
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
                (*entries)[i].type = LIMBOOT_MEMORY_EFI_RUNTIME;
                break;
            case EfiMemoryMappedIO:
            case EfiMemoryMappedIOPortSpace:
                (*entries)[i].type = LIMBOOT_MEMORY_RESERVED;
                break;
            default:
                (*entries)[i].type = LIMBOOT_MEMORY_RESERVED;
                break;
        }

        // Override with internal type
        switch (region->type) {
            case MEM_TYPE_KERNEL:
                (*entries)[i].type = LIMBOOT_MEMORY_KERNEL;
                break;
            case MEM_TYPE_MODULE:
                (*entries)[i].type = LIMBOOT_MEMORY_MODULE;
                break;
            case MEM_TYPE_HANDOFF:
            case MEM_TYPE_RESPONSE:
            case MEM_TYPE_PAGE_TABLES:
            case MEM_TYPE_STACK:
            case MEM_TYPE_TRAMPOLINE:
                (*entries)[i].type = LIMBOOT_MEMORY_BOOTLOADER;
                break;
            case MEM_TYPE_FRAMEBUFFER:
                (*entries)[i].type = LIMBOOT_MEMORY_FRAMEBUFFER;
                break;
        }

        (*entries)[i].attributes = region->attributes;
        region = region->next;
    }

    response->revision = 0;
    response->entry_count = count;
    response->entry_size = sizeof(limoboot_memmap_entry_t);
    response->entries = virt_to_phys(*entries);

    return EFI_SUCCESS;
}

/* ============================================================================
 * Memory Attributes
 * ============================================================================ */

UINT64 mem_efi_attr_to_limoboot(UINT64 efi_attr) {
    UINT64 attr = 0;
    if (efi_attr & EFI_MEMORY_UC) attr |= 1ULL << 0;
    if (efi_attr & EFI_MEMORY_WC) attr |= 1ULL << 1;
    if (efi_attr & EFI_MEMORY_WT) attr |= 1ULL << 2;
    if (efi_attr & EFI_MEMORY_WB) attr |= 1ULL << 3;
    if (efi_attr & EFI_MEMORY_UCE) attr |= 1ULL << 4;
    if (efi_attr & EFI_MEMORY_WP) attr |= 1ULL << 5;
    if (efi_attr & EFI_MEMORY_RP) attr |= 1ULL << 6;
    if (efi_attr & EFI_MEMORY_XP) attr |= 1ULL << 7;
    if (efi_attr & EFI_MEMORY_NV) attr |= 1ULL << 8;
    if (efi_attr & EFI_MEMORY_RO) attr |= 1ULL << 9;
    if (efi_attr & EFI_MEMORY_RUNTIME) attr |= 1ULL << 10;
    return attr;
}