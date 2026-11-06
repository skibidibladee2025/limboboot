/*
 * Linux Boot Protocol Implementation (64-bit only)
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
 * Linux Boot Protocol Constants
 * ============================================================================ */

#define LINUX_BOOT_MAGIC      0x53726448  /* "HdrS" */
#define LINUX_BOOT_PROTOCOL_VERSION  0x020F  /* 2.15 */

#define LINUX_KERNEL_ALIGN    (16 * 1024 * 1024)  /* 16MB alignment for 64-bit */

#define SETUP_NONE            0
#define SETUP_EFI             1

/* ============================================================================
 * Linux Boot Parameters (setup_header)
 * ============================================================================ */

typedef struct {
    uint16_t setup_sects;       /* 0x00: Sectors in setup */
    uint16_t root_flags;        /* 0x02: Root flags */
    uint32_t syssize;           /* 0x04: System size (obsolete) */
    uint16_t ram_size;          /* 0x08: RAM size (obsolete) */
    uint16_t vid_mode;          /* 0x0A: Video mode */
    uint16_t root_dev;          /* 0x0C: Root device */
    uint16_t boot_flag;         /* 0x0E: Boot flag (0xAA55) */
    uint16_t jump;              /* 0x10: Jump instruction */
    uint32_t header;            /* 0x12: Magic "HdrS" */
    uint16_t version;           /* 0x16: Protocol version */
    uint32_t realmode_swtch;    /* 0x18: Real mode switch hook */
    uint16_t start_sys;         /* 0x1C: Start of system (obsolete) */
    uint16_t kernel_version;    /* 0x1E: Kernel version pointer */
    uint8_t  type_of_loader;    /* 0x20: Bootloader type */
    uint8_t  loadflags;         /* 0x21: Load flags */
    uint16_t setup_move_size;   /* 0x22: Setup move size */
    uint32_t code32_start;      /* 0x24: 32-bit entry point */
    uint32_t ramdisk_image;     /* 0x28: Ramdisk image */
    uint32_t ramdisk_size;      /* 0x2C: Ramdisk size */
    uint32_t bootsect_kludge;   /* 0x30: Bootsect kludge */
    uint16_t heap_end_ptr;      /* 0x34: Heap end pointer */
    uint8_t  ext_loader_ver;    /* 0x36: Extended loader version */
    uint8_t  ext_loader_type;   /* 0x37: Extended loader type */
    uint32_t cmd_line_ptr;      /* 0x38: Command line pointer */
    uint32_t initrd_addr_max;   /* 0x3C: Max initrd address */
    uint32_t kernel_alignment;  /* 0x40: Kernel alignment */
    uint8_t  relocatable_kernel;/* 0x44: Relocatable kernel */
    uint8_t  min_alignment;     /* 0x45: Min alignment */
    uint16_t xloadflags;        /* 0x46: Extended load flags */
    uint32_t cmdline_size;      /* 0x48: Command line size */
    uint32_t hardware_subarch;  /* 0x4C: Hardware subarchitecture */
    uint64_t hardware_subarch_data; /* 0x50: Subarch data */
    uint32_t payload_offset;    /* 0x58: Payload offset */
    uint32_t payload_length;    /* 0x5C: Payload length */
    uint64_t setup_data;        /* 0x60: Setup data (EFI) */
    uint64_t pref_address;      /* 0x68: Preferred address */
    uint32_t init_size;         /* 0x70: Init size */
    uint32_t handover_offset;   /* 0x74: Handover offset */
    uint32_t kernel_info_offset;/* 0x78: Kernel info offset */
} __attribute__((packed)) linux_setup_header_t;

/* ============================================================================
 * Linux Boot Parameters (zero page)
 * ============================================================================ */

typedef struct {
    uint8_t  pad0[0x1F1];
    uint8_t  setup_sects;       /* 0x1F1 */
    uint16_t root_flags;        /* 0x1F2 */
    uint32_t syssize;           /* 0x1F4 */
    uint16_t ram_size;          /* 0x1F8 */
    uint16_t vid_mode;          /* 0x1FA */
    uint16_t root_dev;          /* 0x1FC */
    uint16_t boot_flag;         /* 0x1FE */
    uint16_t jump;              /* 0x200 */
    uint32_t header;            /* 0x202 */
    uint16_t version;           /* 0x206 */
    uint32_t realmode_swtch;    /* 0x208 */
    uint16_t start_sys;         /* 0x20C */
    uint16_t kernel_version;    /* 0x20E */
    uint8_t  type_of_loader;    /* 0x210 */
    uint8_t  loadflags;         /* 0x211 */
    uint16_t setup_move_size;   /* 0x212 */
    uint32_t code32_start;      /* 0x214 */
    uint32_t ramdisk_image;     /* 0x218 */
    uint32_t ramdisk_size;      /* 0x21C */
    uint32_t bootsect_kludge;   /* 0x220 */
    uint16_t heap_end_ptr;      /* 0x224 */
    uint8_t  ext_loader_ver;    /* 0x226 */
    uint8_t  ext_loader_type;   /* 0x227 */
    uint32_t cmd_line_ptr;      /* 0x228 */
    uint32_t initrd_addr_max;   /* 0x22C */
    uint32_t kernel_alignment;  /* 0x230 */
    uint8_t  relocatable_kernel;/* 0x234 */
    uint8_t  min_alignment;     /* 0x235 */
    uint16_t xloadflags;        /* 0x236 */
    uint32_t cmdline_size;      /* 0x238 */
    uint32_t hardware_subarch;  /* 0x23C */
    uint64_t hardware_subarch_data; /* 0x240 */
    uint32_t payload_offset;    /* 0x248 */
    uint32_t payload_length;    /* 0x24C */
    uint64_t setup_data;        /* 0x250 */
    uint64_t pref_address;      /* 0x258 */
    uint32_t init_size;         /* 0x260 */
    uint32_t handover_offset;   /* 0x264 */
    uint32_t kernel_info_offset;/* 0x268 */
    uint8_t  pad1[0x1000 - 0x26C];
} __attribute__((packed)) linux_boot_params_t;

/* ============================================================================
 * EFI Setup Data
 * ============================================================================ */

typedef struct {
    uint64_t next;
    uint32_t type;
    uint32_t size;
    uint8_t  data[];
} __attribute__((packed)) linux_efi_setup_data_t;

#define LINUX_EFI_SETUP_DATA_NONE         0
#define LINUX_EFI_SETUP_DATA_FDT          1
#define LINUX_EFI_SETUP_DATA_CMDLINE      2
#define LINUX_EFI_SETUP_DATA_MEMMAP       3
#define LINUX_EFI_SETUP_DATA_RNG_SEED     4
#define LINUX_EFI_SETUP_DATA_TPM_LOG      5
#define LINUX_EFI_SETUP_DATA_SMBIOS       6
#define LINUX_EFI_SETUP_DATA_ACPI         7
#define LINUX_EFI_SETUP_DATA_HANDOVER     8

/* ============================================================================
 * Load Linux Kernel
 * ============================================================================ */

EFI_STATUS linux_load_kernel(const char *path, EFI_PHYSICAL_ADDRESS *kernel_entry,
                              EFI_PHYSICAL_ADDRESS *kernel_phys, UINT64 *kernel_size,
                              EFI_PHYSICAL_ADDRESS *boot_params_phys) {
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

    /* Get file size */
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

    /* Read entire kernel */
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

    /* Validate as ELF64 */
    if (!elf64_validate(kernel_data, file_size)) {
        efi_free_pool(kernel_data);
        return EFI_UNSUPPORTED;
    }

    /* Check it's a 64-bit kernel */
    int machine = elf64_get_machine(kernel_data);
    if (machine != EM_X86_64 && machine != EM_AARCH64 && machine != EM_RISCV && machine != EM_LOONGARCH) {
        efi_free_pool(kernel_data);
        return EFI_UNSUPPORTED;
    }

    /* Find setup_header in kernel */
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)kernel_data;
    const elf64_phdr_t *phdr = (const elf64_phdr_t *)((uint8_t *)kernel_data + ehdr->phoff);

    linux_setup_header_t *setup_hdr = NULL;
    for (UINTN i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type == PT_LOAD && phdr[i].offset == 0) {
            /* First PT_LOAD contains setup header */
            setup_hdr = (linux_setup_header_t *)((uint8_t *)kernel_data + 0x1F1);
            break;
        }
    }

    if (!setup_hdr || setup_hdr->header != LINUX_BOOT_MAGIC) {
        efi_free_pool(kernel_data);
        return EFI_UNSUPPORTED;
    }

    if (setup_hdr->version < 0x020B) {  /* Need at least 2.11 for 64-bit */
        efi_free_pool(kernel_data);
        return EFI_UNSUPPORTED;
    }

    /* Check for 64-bit entry point */
    if (!(setup_hdr->xloadflags & 0x1)) {  /* KERNEL_IS_64BIT */
        efi_free_pool(kernel_data);
        return EFI_UNSUPPORTED;
    }

    /* Allocate memory for kernel (16MB aligned) */
    UINT64 align = setup_hdr->kernel_alignment ? setup_hdr->kernel_alignment : LINUX_KERNEL_ALIGN;
    UINT64 total_size = file_size + align;

    EFI_PHYSICAL_ADDRESS phys;
    status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, (total_size + 4095) / 4096, &phys);
    if (EFI_ERROR(status)) {
        efi_free_pool(kernel_data);
        return status;
    }

    /* Align kernel */
    EFI_PHYSICAL_ADDRESS kernel_base = (phys + align - 1) & ~(align - 1);
    void *kernel_dest = phys_to_virt(kernel_base);

    /* Copy kernel */
    memcpy(kernel_dest, kernel_data, file_size);
    efi_free_pool(kernel_data);

    /* Allocate boot params (zero page) */
    EFI_PHYSICAL_ADDRESS params_phys;
    status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &params_phys);
    if (EFI_ERROR(status)) {
        g_bs->FreePages(phys, (total_size + 4095) / 4096);
        return status;
    }

    linux_boot_params_t *params = phys_to_virt(params_phys);
    memset(params, 0, sizeof(linux_boot_params_t));

    /* Fill boot params */
    memcpy(&params->setup_sects, &setup_hdr->setup_sects,
           sizeof(linux_setup_header_t) - offsetof(linux_setup_header_t, setup_sects));

    /* Set type_of_loader (0xFF = EFI) */
    params->type_of_loader = 0xFF;

    /* Set loadflags */
    params->loadflags = 0x1;  /* LOADED_HIGH */

    /* Set heap_end_ptr */
    params->heap_end_ptr = 0xFE00;

    /* Set cmdline (will be filled later) */
    params->cmd_line_ptr = 0;
    params->cmdline_size = 0;

    /* Set initrd (will be filled later) */
    params->ramdisk_image = 0;
    params->ramdisk_size = 0;

    /* Set preferred address */
    params->pref_address = kernel_base;

    /* Set kernel info offset */
    params->kernel_info_offset = 0;  /* Not provided */

    /* Build EFI setup data */
    linux_build_efi_setup_data(params_phys);

    *kernel_entry = kernel_base + (setup_hdr->code32_start - 0x10000);  /* Adjust for 64-bit entry */
    *kernel_phys = kernel_base;
    *kernel_size = file_size;
    *boot_params_phys = params_phys;

    return EFI_SUCCESS;
}

/* ============================================================================
 * Build EFI Setup Data
 * ============================================================================ */

void linux_build_efi_setup_data(EFI_PHYSICAL_ADDRESS params_phys) {
    linux_boot_params_t *params = phys_to_virt(params_phys);

    /* Allocate setup data chain */
    EFI_PHYSICAL_ADDRESS current = 0;

    /* Memory map */
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

            UINT64 data_size = sizeof(linux_efi_setup_data_t) + map_size;
            linux_efi_setup_data_t *data = efi_allocate_pool(data_size, EfiLoaderData);
            if (data) {
                data->next = current;
                data->type = LINUX_EFI_SETUP_DATA_MEMMAP;
                data->size = map_size;
                memcpy(data->data, map, map_size);
                current = virt_to_phys(data);
            }
            efi_free_pool(map);
        }
    }

    /* Command line */
    config_entry_t *entry = config_get_default_entry();
    if (entry && entry->cmdline[0]) {
        UINT64 cmdline_len = strlen(entry->cmdline) + 1;
        UINT64 data_size = sizeof(linux_efi_setup_data_t) + cmdline_len;
        linux_efi_setup_data_t *data = efi_allocate_pool(data_size, EfiLoaderData);
        if (data) {
            data->next = current;
            data->type = LINUX_EFI_SETUP_DATA_CMDLINE;
            data->size = cmdline_len;
            ascii_to_unicode(entry->cmdline, (CHAR16 *)data->data, cmdline_len);
            current = virt_to_phys(data);
        }
    }

    /* RNG seed */
    void *entropy = efi_allocate_pool(256, EfiLoaderData);
    if (entropy) {
        size_t collected = 0;
        entropy_collect(entropy, 256, &collected);
        if (collected > 0) {
            UINT64 data_size = sizeof(linux_efi_setup_data_t) + collected;
            linux_efi_setup_data_t *data = efi_allocate_pool(data_size, EfiLoaderData);
            if (data) {
                data->next = current;
                data->type = LINUX_EFI_SETUP_DATA_RNG_SEED;
                data->size = collected;
                memcpy(data->data, entropy, collected);
                current = virt_to_phys(data);
            }
        }
        efi_free_pool(entropy);
    }

    /* TPM event log */
    EFI_TCG2_PROTOCOL *tcg2 = NULL;
    if (!EFI_ERROR(g_bs->LocateProtocol(&EFI_TCG2_PROTOCOL_GUID, NULL, (VOID **)&tcg2))) {
        EFI_PHYSICAL_ADDRESS event_log = 0, last_entry = 0;
        BOOLEAN truncated = FALSE;
        if (!EFI_ERROR(tcg2->GetEventLog(tcg2, EFI_TCG2_EVENT_LOG_FORMAT_TCG_2, &event_log, &last_entry, &truncated))) {
            UINT64 log_size = last_entry - event_log;
            if (log_size > 0) {
                UINT64 data_size = sizeof(linux_efi_setup_data_t) + log_size;
                linux_efi_setup_data_t *data = efi_allocate_pool(data_size, EfiLoaderData);
                if (data) {
                    data->next = current;
                    data->type = LINUX_EFI_SETUP_DATA_TPM_LOG;
                    data->size = log_size;
                    memcpy(data->data, phys_to_virt(event_log), log_size);
                    current = virt_to_phys(data);
                }
            }
        }
    }

    /* SMBIOS */
    void *smbios = efi_get_config_table(&EFI_SMBIOS3_TABLE_GUID);
    if (!smbios) smbios = efi_get_config_table(&EFI_SMBIOS_TABLE_GUID);
    if (smbios) {
        /* Calculate SMBIOS size */
        UINT64 smbios_size = 0;
        /* Would need to parse SMBIOS to get size */
        if (smbios_size > 0) {
            UINT64 data_size = sizeof(linux_efi_setup_data_t) + smbios_size;
            linux_efi_setup_data_t *data = efi_allocate_pool(data_size, EfiLoaderData);
            if (data) {
                data->next = current;
                data->type = LINUX_EFI_SETUP_DATA_SMBIOS;
                data->size = smbios_size;
                memcpy(data->data, smbios, smbios_size);
                current = virt_to_phys(data);
            }
        }
    }

    /* ACPI RSDP */
    void *rsdp = efi_get_config_table(&EFI_ACPI_20_TABLE_GUID);
    if (rsdp) {
        /* Get RSDP size */
        UINT64 rsdp_size = sizeof(uint64_t) + 36;  /* Simplified */
        UINT64 data_size = sizeof(linux_efi_setup_data_t) + rsdp_size;
        linux_efi_setup_data_t *data = efi_allocate_pool(data_size, EfiLoaderData);
        if (data) {
            data->next = current;
            data->type = LINUX_EFI_SETUP_DATA_ACPI;
            data->size = rsdp_size;
            memcpy(data->data, rsdp, rsdp_size);
            current = virt_to_phys(data);
        }
    }

    params->setup_data = current;
}

/* ============================================================================
 * Load Initrd
 * ============================================================================ */

EFI_STATUS linux_load_initrd(const char *path, EFI_PHYSICAL_ADDRESS *initrd_phys, UINT64 *initrd_size) {
    if (!path || !path[0]) return EFI_NOT_FOUND;

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

    EFI_PHYSICAL_ADDRESS phys;
    status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, (file_size + 4095) / 4096, &phys);
    if (EFI_ERROR(status)) {
        file->Close(file);
        return status;
    }

    void *dest = phys_to_virt(phys);
    UINTN read_size = (UINTN)file_size;
    status = file->Read(file, &read_size, dest);
    file->Close(file);

    if (EFI_ERROR(status)) {
        g_bs->FreePages(phys, (file_size + 4095) / 4096);
        return status;
    }

    *initrd_phys = phys;
    *initrd_size = file_size;
    return EFI_SUCCESS;
}

/* ============================================================================
 * Boot Linux Kernel
 * ============================================================================ */

EFI_STATUS linux_boot(EFI_PHYSICAL_ADDRESS kernel_entry, EFI_PHYSICAL_ADDRESS boot_params_phys) {
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
    /* Linux EFI handover expects:
     *   RDX = boot_params_phys
     *   R8  = system_table
     */
    __asm__ volatile (
        "mov %0, %%rax\n"
        "mov %1, %%rdx\n"
        "mov %2, %%r8\n"
        "jmp *%3"
        :
        : "r"(LINUX_BOOT_MAGIC),  /* Not actually used, but for consistency */
          "r"(boot_params_phys),
          "r"(g_st),
          "r"(kernel_entry)
        : "rax", "rdx", "r8", "memory"
    );

    return EFI_SUCCESS;
}