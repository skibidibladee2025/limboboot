/*
 * Multiboot2 Protocol Implementation (64-bit only)
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"
#include "memory.c"
#include "efi.c"
#include "printf.c"
#include "elf.c"
#include "config.c"

/* ============================================================================
 * Multiboot2 Constants
 * ============================================================================ */

#define MULTIBOOT2_MAGIC           0xE85250D6
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

#define MULTIBOOT2_HEADER_ALIGN    8
#define MULTIBOOT2_INFO_ALIGN      8

/* Tag Types */
#define MULTIBOOT2_TAG_TYPE_END                    0
#define MULTIBOOT2_TAG_TYPE_CMDLINE                1
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME       2
#define MULTIBOOT2_TAG_TYPE_MODULE                 3
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO          4
#define MULTIBOOT2_TAG_TYPE_BOOTDEV                5
#define MULTIBOOT2_TAG_TYPE_MMAP                   6
#define MULTIBOOT2_TAG_TYPE_VBE                    7
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER            8
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS           9
#define MULTIBOOT2_TAG_TYPE_APM                    10
#define MULTIBOOT2_TAG_TYPE_EFI32                  11
#define MULTIBOOT2_TAG_TYPE_EFI64                  12
#define MULTIBOOT2_TAG_TYPE_SMBIOS                 13
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD               14
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW               15
#define MULTIBOOT2_TAG_TYPE_NETWORK                16
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP               17
#define MULTIBOOT2_TAG_TYPE_EFI_BS                 18
#define MULTIBOOT2_TAG_TYPE_EFI32_IH               19
#define MULTIBOOT2_TAG_TYPE_EFI64_IH               20
#define MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR         21

/* Header Tags */
#define MULTIBOOT2_HEADER_TAG_END             0
#define MULTIBOOT2_HEADER_TAG_INFORMATION_REQUEST 1
#define MULTIBOOT2_HEADER_TAG_ADDRESS         2
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS   3
#define MULTIBOOT2_HEADER_TAG_CONSOLE_FLAGS   4
#define MULTIBOOT2_HEADER_TAG_FRAMEBUFFER     5
#define MULTIBOOT2_HEADER_TAG_MODULE_ALIGN    6
#define MULTIBOOT2_HEADER_TAG_EFI_BS          7
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS_EFI64 8
#define MULTIBOOT2_HEADER_TAG_RELOCATABLE     9

/* Architecture */
#define MULTIBOOT2_ARCHITECTURE_X86    0
#define MULTIBOOT2_ARCHITECTURE_MIPS32 4
#define MULTIBOOT2_ARCHITECTURE_X86_64 5
#define MULTIBOOT2_ARCHITECTURE_AARCH64 8
#define MULTIBOOT2_ARCHITECTURE_RISCV64 10
#define MULTIBOOT2_ARCHITECTURE_LOONGARCH64 11

/* ============================================================================
 * Multiboot2 Structures
 * ============================================================================ */

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) multiboot2_tag_t;

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) multiboot2_header_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t architecture;
    uint32_t header_length;
    uint32_t checksum;
} __attribute__((packed)) multiboot2_header_tag_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t flags;
    uint32_t architecture;
    uint64_t header_addr;
    uint64_t load_addr;
    uint64_t load_end_addr;
    uint64_t bss_end_addr;
} __attribute__((packed)) multiboot2_header_tag_address_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t flags;
    uint64_t entry_addr;
} __attribute__((packed)) multiboot2_header_tag_entry_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t num;
    uint32_t tags[];
} __attribute__((packed)) multiboot2_header_tag_info_req_t;

/* Info Tags */
typedef struct {
    multiboot2_tag_t tag;
    char cmdline[];
} __attribute__((packed)) multiboot2_tag_cmdline_t;

typedef struct {
    multiboot2_tag_t tag;
    char name[];
} __attribute__((packed)) multiboot2_tag_bootloader_name_t;

typedef struct {
    multiboot2_tag_t tag;
    uint64_t mod_start;
    uint64_t mod_end;
    char cmdline[];
} __attribute__((packed)) multiboot2_tag_module_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t mem_lower;
    uint32_t mem_upper;
} __attribute__((packed)) multiboot2_tag_basic_meminfo_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t biosdev;
    uint32_t slice;
    uint32_t part;
} __attribute__((packed)) multiboot2_tag_bootdev_t;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) multiboot2_mmap_entry_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot2_mmap_entry_t entries[];
} __attribute__((packed)) multiboot2_tag_mmap_t;

typedef struct {
    multiboot2_tag_t tag;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
    union {
        struct {
            uint32_t framebuffer_red_mask_size;
            uint32_t framebuffer_red_field_position;
            uint32_t framebuffer_green_mask_size;
            uint32_t framebuffer_green_field_position;
            uint32_t framebuffer_blue_mask_size;
            uint32_t framebuffer_blue_field_position;
        };
        struct {
            uint32_t framebuffer_palette[256];
        };
    };
} __attribute__((packed)) multiboot2_tag_framebuffer_t;

typedef struct {
    multiboot2_tag_t tag;
    uint64_t efi_system_table;
} __attribute__((packed)) multiboot2_tag_efi64_t;

typedef struct {
    multiboot2_tag_t tag;
    uint8_t  major;
    uint8_t  minor;
    uint8_t  reserved[6];
    uint8_t  tables[];
} __attribute__((packed)) multiboot2_tag_smbios_t;

typedef struct {
    multiboot2_tag_t tag;
    uint8_t  rsdp[];
} __attribute__((packed)) multiboot2_tag_acpi_new_t;

typedef struct {
    multiboot2_tag_t tag;
    uint64_t load_base_addr;
} __attribute__((packed)) multiboot2_tag_load_base_addr_t;

/* ============================================================================
 * Validate Multiboot2 Header
 * ============================================================================ */

int multiboot2_validate(const void *data, size_t size) {
    if (size < sizeof(multiboot2_header_t) + sizeof(multiboot2_header_tag_t)) return 0;

    const multiboot2_header_t *hdr = (const multiboot2_header_t *)data;

    if (hdr->total_size > size) return 0;

    /* Check for header tag */
    const multiboot2_header_tag_t *tag = (const multiboot2_header_tag_t *)((uint8_t *)data + sizeof(multiboot2_header_t));

    while (tag->type != MULTIBOOT2_HEADER_TAG_END) {
        if (tag->type == MULTIBOOT2_HEADER_TAG_ADDRESS) {
            const multiboot2_header_tag_address_t *addr_tag = (const multiboot2_header_tag_address_t *)tag;
            if (addr_tag->architecture == MULTIBOOT2_ARCHITECTURE_X86_64 ||
                addr_tag->architecture == MULTIBOOT2_ARCHITECTURE_AARCH64 ||
                addr_tag->architecture == MULTIBOOT2_ARCHITECTURE_RISCV64 ||
                addr_tag->architecture == MULTIBOOT2_ARCHITECTURE_LOONGARCH64) {
                return 1;
            }
        }
        tag = (const multiboot2_header_tag_t *)((uint8_t *)tag + tag->size);
    }

    return 0;
}

/* ============================================================================
 * Load Multiboot2 Kernel
 * ============================================================================ */

EFI_STATUS multiboot2_load_kernel(const char *path, EFI_PHYSICAL_ADDRESS *entry_point,
                                   EFI_PHYSICAL_ADDRESS *kernel_phys, UINT64 *kernel_size,
                                   EFI_PHYSICAL_ADDRESS *info_phys) {
    /* Load kernel as ELF */
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_STATUS status = g_bs->HandleProtocol(g_image_handle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, (VOID **)&root);
    if (EFI_ERROR(status)) return status;

    EFI_LOADED_IMAGE_PROTOCOL *loaded = NULL;
    status = g_bs->HandleProtocol(g_image_handle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, (VOID **)&loaded);
    if (EFI_ERROR(status)) return status;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    status = g_bs->HandleProtocol(loaded->DeviceHandle, &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, (VOID **)&fs);
    if (EFI_ERROR(status)) return status;

    EFI_FILE_PROTOCOL *fs_root = NULL;
    status = fs->OpenVolume(fs, &fs_root);
    if (EFI_ERROR(status)) return status;

    CHAR16 wpath[256];
    ascii_to_unicode(path, wpath, 256);

    EFI_FILE_PROTOCOL *file = NULL;
    status = fs_root->Open(fs_root, &file, wpath, EFI_FILE_MODE_READ, 0);
    fs_root->Close(fs_root);
    if (EFI_ERROR(status)) return status;

    EFI_FILE_INFO *info = NULL;
    UINTN info_size = 0;
    status = file->GetInfo(file, &EFI_FILE_INFO_ID, &info_size, info);
    if (status == EFI_BUFFER_TOO_SMALL) {
        info = efi_allocate_pool(info_size, EfiBootServicesData);
        status = file->GetInfo(file, &EFI_FILE_INFO_ID, &info_size, info);
    }
    if (EFI_ERROR(status)) {
        file->Close(file);
        return status;
    }

    UINT64 file_size = info->FileSize;
    efi_free_pool(info);

    void *kernel_data = efi_allocate_pool((UINTN)file_size, EfiBootServicesData);
    if (!kernel_data) {
        file->Close(file);
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN read_size = (UINTN)file_size;
    status = file->Read(file, &read_size, kernel_data);
    file->Close(file);
    if (EFI_ERROR(status)) {
        efi_free_pool(kernel_data);
        return status;
    }

    /* Validate Multiboot2 header */
    if (!multiboot2_validate(kernel_data, file_size)) {
        efi_free_pool(kernel_data);
        return EFI_UNSUPPORTED;
    }

    /* Load as ELF (Multiboot2 kernels are typically ELF) */
    if (elf64_validate(kernel_data, file_size)) {
        EFI_PHYSICAL_ADDRESS load_base, entry;
        UINT64 img_size;
        status = elf64_load(kernel_data, file_size, &load_base, &entry, &img_size);
        efi_free_pool(kernel_data);
        if (EFI_ERROR(status)) return status;

        /* Build Multiboot2 info structure */
        status = multiboot2_build_info(load_base, img_size, entry, info_phys);
        if (EFI_ERROR(status)) {
            g_bs->FreePages(load_base, img_size / 4096);
            return status;
        }

        *entry_point = entry;
        *kernel_phys = load_base;
        *kernel_size = img_size;
        return EFI_SUCCESS;
    }

    /* Raw binary? */
    efi_free_pool(kernel_data);
    return EFI_UNSUPPORTED;
}

/* ============================================================================
 * Build Multiboot2 Info Structure
 * ============================================================================ */

EFI_STATUS multiboot2_build_info(EFI_PHYSICAL_ADDRESS kernel_base, UINT64 kernel_size,
                                  EFI_PHYSICAL_ADDRESS entry_point, EFI_PHYSICAL_ADDRESS *info_phys) {
    /* Calculate required size */
    UINT64 total_size = sizeof(multiboot2_header_t);

    /* End tag */
    total_size += sizeof(multiboot2_tag_t);

    /* Cmdline tag */
    config_entry_t *entry = config_get_default_entry();
    if (entry && entry->cmdline[0]) {
        total_size = (total_size + 7) & ~7;
        total_size += sizeof(multiboot2_tag_cmdline_t) + strlen(entry->cmdline) + 1;
    }

    /* Bootloader name tag */
    total_size = (total_size + 7) & ~7;
    total_size += sizeof(multiboot2_tag_bootloader_name_t) + strlen("limoboot") + 1;

    /* Module tags */
    if (entry) {
        for (int i = 0; i < entry->module_count; i++) {
            total_size = (total_size + 7) & ~7;
            total_size += sizeof(multiboot2_tag_module_t) + strlen(entry->module_paths[i]) + 1;
        }
    }

    /* Memory map tag */
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key, desc_size;
    UINT32 desc_version;
    g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (map_size > 0) {
        map_size += 4096;
        map = efi_allocate_pool(map_size, EfiBootServicesData);
        if (map) {
            g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
            UINTN entries = map_size / desc_size;
            total_size = (total_size + 7) & ~7;
            total_size += sizeof(multiboot2_tag_mmap_t) + entries * sizeof(multiboot2_mmap_entry_t);
            efi_free_pool(map);
        }
    }

    /* Framebuffer tag */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    if (!EFI_ERROR(g_bs->LocateProtocol(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, NULL, (VOID **)&gop)) && gop->Mode) {
        total_size = (total_size + 7) & ~7;
        total_size += sizeof(multiboot2_tag_framebuffer_t);
    }

    /* EFI64 tag */
    total_size = (total_size + 7) & ~7;
    total_size += sizeof(multiboot2_tag_efi64_t);

    /* SMBIOS tag */
    void *smbios = efi_get_config_table(&EFI_SMBIOS3_TABLE_GUID);
    if (!smbios) smbios = efi_get_config_table(&EFI_SMBIOS_TABLE_GUID);
    if (smbios) {
        total_size = (total_size + 7) & ~7;
        total_size += sizeof(multiboot2_tag_smbios_t) + 64;  /* Approximate */
    }

    /* ACPI tag */
    void *rsdp = efi_get_config_table(&EFI_ACPI_20_TABLE_GUID);
    if (rsdp) {
        total_size = (total_size + 7) & ~7;
        total_size += sizeof(multiboot2_tag_acpi_new_t) + 36;
    }

    /* Load base addr tag */
    total_size = (total_size + 7) & ~7;
    total_size += sizeof(multiboot2_tag_load_base_addr_t);

    /* Allocate info structure */
    EFI_PHYSICAL_ADDRESS phys;
    status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, (total_size + 4095) / 4096, &phys);
    if (EFI_ERROR(status)) return status;

    void *info = phys_to_virt(phys);
    memset(info, 0, total_size);

    uint8_t *ptr = (uint8_t *)info;

    /* Header */
    multiboot2_header_t *hdr = (multiboot2_header_t *)ptr;
    hdr->total_size = total_size;
    hdr->reserved = 0;
    ptr += sizeof(multiboot2_header_t);

    /* Cmdline */
    if (entry && entry->cmdline[0]) {
        ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
        multiboot2_tag_cmdline_t *cmdline_tag = (multiboot2_tag_cmdline_t *)ptr;
        cmdline_tag->tag.type = MULTIBOOT2_TAG_TYPE_CMDLINE;
        cmdline_tag->tag.size = sizeof(multiboot2_tag_cmdline_t) + strlen(entry->cmdline) + 1;
        strcpy(cmdline_tag->cmdline, entry->cmdline);
        ptr += cmdline_tag->tag.size;
    }

    /* Bootloader name */
    ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
    multiboot2_tag_bootloader_name_t *name_tag = (multiboot2_tag_bootloader_name_t *)ptr;
    name_tag->tag.type = MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME;
    name_tag->tag.size = sizeof(multiboot2_tag_bootloader_name_t) + strlen("limoboot") + 1;
    strcpy(name_tag->name, "limoboot");
    ptr += name_tag->tag.size;

    /* Modules */
    if (entry) {
        for (int i = 0; i < entry->module_count; i++) {
            ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
            multiboot2_tag_module_t *mod_tag = (multiboot2_tag_module_t *)ptr;
            mod_tag->tag.type = MULTIBOOT2_TAG_TYPE_MODULE;
            mod_tag->mod_start = 0;  /* Would be filled with actual module addresses */
            mod_tag->mod_end = 0;
            mod_tag->tag.size = sizeof(multiboot2_tag_module_t) + strlen(entry->module_paths[i]) + 1;
            strcpy(mod_tag->cmdline, entry->module_paths[i]);
            ptr += mod_tag->tag.size;
        }
    }

    /* Memory map */
    map_size = 0;
    map = NULL;
    g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (map_size > 0) {
        map_size += 4096;
        map = efi_allocate_pool(map_size, EfiBootServicesData);
        if (map) {
            g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
            UINTN entries = map_size / desc_size;

            ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
            multiboot2_tag_mmap_t *mmap_tag = (multiboot2_tag_mmap_t *)ptr;
            mmap_tag->tag.type = MULTIBOOT2_TAG_TYPE_MMAP;
            mmap_tag->tag.size = sizeof(multiboot2_tag_mmap_t) + entries * sizeof(multiboot2_mmap_entry_t);
            mmap_tag->entry_size = sizeof(multiboot2_mmap_entry_t);
            mmap_tag->entry_version = 0;

            multiboot2_mmap_entry_t *mmap_entries = mmap_tag->entries;
            for (UINTN i = 0; i < entries; i++) {
                EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);
                mmap_entries[i].base_addr = desc->PhysicalStart;
                mmap_entries[i].length = desc->NumberOfPages * 4096;

                switch (desc->Type) {
                    case EfiConventionalMemory:
                    case EfiPersistentMemory:
                        mmap_entries[i].type = 1;  /* Available */
                        break;
                    case EfiACPIReclaimMemory:
                        mmap_entries[i].type = 3;  /* ACPI Reclaimable */
                        break;
                    case EfiACPIMemoryNVS:
                        mmap_entries[i].type = 4;  /* ACPI NVS */
                        break;
                    case EfiUnusableMemory:
                        mmap_entries[i].type = 5;  /* Bad */
                        break;
                    default:
                        mmap_entries[i].type = 2;  /* Reserved */
                        break;
                }
                mmap_entries[i].reserved = 0;
            }

            ptr += mmap_tag->tag.size;
            efi_free_pool(map);
        }
    }

    /* Framebuffer */
    if (gop && gop->Mode) {
        ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
        multiboot2_tag_framebuffer_t *fb_tag = (multiboot2_tag_framebuffer_t *)ptr;
        fb_tag->tag.type = MULTIBOOT2_TAG_TYPE_FRAMEBUFFER;
        fb_tag->tag.size = sizeof(multiboot2_tag_framebuffer_t);
        fb_tag->framebuffer_addr = gop->Mode->FrameBufferBase;
        fb_tag->framebuffer_width = gop->Mode->Info->HorizontalResolution;
        fb_tag->framebuffer_height = gop->Mode->Info->VerticalResolution;
        fb_tag->framebuffer_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
        fb_tag->framebuffer_bpp = 32;
        fb_tag->framebuffer_type = 1;  /* RGB */
        fb_tag->framebuffer_red_mask_size = 8;
        fb_tag->framebuffer_red_field_position = 16;
        fb_tag->framebuffer_green_mask_size = 8;
        fb_tag->framebuffer_green_field_position = 8;
        fb_tag->framebuffer_blue_mask_size = 8;
        fb_tag->framebuffer_blue_field_position = 0;
        ptr += fb_tag->tag.size;
    }

    /* EFI64 */
    ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
    multiboot2_tag_efi64_t *efi64_tag = (multiboot2_tag_efi64_t *)ptr;
    efi64_tag->tag.type = MULTIBOOT2_TAG_TYPE_EFI64;
    efi64_tag->tag.size = sizeof(multiboot2_tag_efi64_t);
    efi64_tag->efi_system_table = (uint64_t)g_st;
    ptr += efi64_tag->tag.size;

    /* SMBIOS */
    if (smbios) {
        ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
        multiboot2_tag_smbios_t *smbios_tag = (multiboot2_tag_smbios_t *)ptr;
        smbios_tag->tag.type = MULTIBOOT2_TAG_TYPE_SMBIOS;
        /* Would need to parse SMBIOS to get actual size */
        smbios_tag->tag.size = sizeof(multiboot2_tag_smbios_t) + 64;
        smbios_tag->major = 3;
        smbios_tag->minor = 0;
        memset(smbios_tag->reserved, 0, 6);
        /* Copy SMBIOS tables */
        ptr += smbios_tag->tag.size;
    }

    /* ACPI New */
    if (rsdp) {
        ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
        multiboot2_tag_acpi_new_t *acpi_tag = (multiboot2_tag_acpi_new_t *)ptr;
        acpi_tag->tag.type = MULTIBOOT2_TAG_TYPE_ACPI_NEW;
        acpi_tag->tag.size = sizeof(multiboot2_tag_acpi_new_t) + 36;
        memcpy(acpi_tag->rsdp, rsdp, 36);
        ptr += acpi_tag->tag.size;
    }

    /* Load base addr */
    ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
    multiboot2_tag_load_base_addr_t *base_tag = (multiboot2_tag_load_base_addr_t *)ptr;
    base_tag->tag.type = MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR;
    base_tag->tag.size = sizeof(multiboot2_tag_load_base_addr_t);
    base_tag->load_base_addr = kernel_base;
    ptr += base_tag->tag.size;

    /* End tag */
    ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~7);
    multiboot2_tag_t *end_tag = (multiboot2_tag_t *)ptr;
    end_tag->type = MULTIBOOT2_TAG_TYPE_END;
    end_tag->size = sizeof(multiboot2_tag_t);

    *info_phys = phys;
    return EFI_SUCCESS;
}

/* ============================================================================
 * Boot Multiboot2 Kernel
 * ============================================================================ */

EFI_STATUS multiboot2_boot(EFI_PHYSICAL_ADDRESS entry_point, EFI_PHYSICAL_ADDRESS info_phys) {
    /* Exit Boot Services */
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key, desc_size;
    UINT32 desc_version;

    EFI_STATUS status = g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (status == EFI_BUFFER_TOO_SMALL) {
        map_size += 4096;
        map = efi_allocate_pool(map_size, EfiBootServicesData);
        status = g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    }

    if (EFI_ERROR(status)) {
        efi_free_pool(map);
        return status;
    }

    status = g_bs->ExitBootServices(g_image_handle, map_key);
    efi_free_pool(map);
    if (EFI_ERROR(status)) return status;

    /* Jump to kernel */
    /* Multiboot2 entry expects:
     *   RAX = MULTIBOOT2_BOOTLOADER_MAGIC
     *   RBX = info_phys
     */
    __asm__ volatile (
        "mov %0, %%rax\n"
        "mov %1, %%rbx\n"
        "jmp *%2"
        :
        : "r"(MULTIBOOT2_BOOTLOADER_MAGIC),
          "r"(info_phys),
          "r"(entry_point)
        : "rax", "rbx", "memory"
    );

    return EFI_SUCCESS;
}