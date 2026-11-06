/*
 * limoboot - Main Entry Point
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
#include "entropy.c"
#include "protocol/limoboot/limoboot.c"
#include "protocol/linux/linux.c"
#include "protocol/multiboot2/multiboot2.c"
#include "ui/menu.c"
#include "ui/theme.c"
#include "ui/graphics.c"
#include "ui/input.c"

/* ============================================================================
 * Architecture-Specific Functions (declared in arch/*/efi.c, memory.c, smp.c)
 * ============================================================================ */

extern EFI_STATUS arch_efi_init(EFI_HANDLE, EFI_SYSTEM_TABLE *);
extern EFI_STATUS arch_setup_page_tables(EFI_PHYSICAL_ADDRESS, UINT64);
extern void arch_load_page_tables(void);
extern void *arch_get_stack_top(void);
extern EFI_PHYSICAL_ADDRESS arch_get_stack_phys(void);
extern limoboot_handoff_t *arch_create_handoff(EFI_PHYSICAL_ADDRESS);
extern EFI_STATUS arch_build_smp_response(limoboot_smp_response_t *);
extern EFI_STATUS arch_map_kernel(EFI_PHYSICAL_ADDRESS, UINT64, UINT64);
extern EFI_STATUS arch_map_framebuffer(EFI_PHYSICAL_ADDRESS, UINT64, UINT64);
extern EFI_STATUS arch_map_bootloader_data(EFI_PHYSICAL_ADDRESS, UINT64, UINT64);
extern EFI_STATUS arch_smp_init(void);
extern uint32_t arch_get_bsp_lapic_id(void);

/* ============================================================================
 * Global State
 * ============================================================================ */

static limoboot_handoff_t *g_handoff = NULL;
static EFI_PHYSICAL_ADDRESS g_kernel_entry = 0;
static EFI_PHYSICAL_ADDRESS g_kernel_phys = 0;
static UINT64 g_kernel_size = 0;
static EFI_PHYSICAL_ADDRESS g_hhdm_offset = 0;
static UINT64 g_hhdm_size = 0;

/* ============================================================================
 * Load Kernel via Selected Protocol
 * ============================================================================ */

EFI_STATUS load_kernel(config_entry_t *entry) {
    EFI_STATUS status = EFI_UNSUPPORTED;

    if (strcmp(entry->protocol, "limoboot") == 0) {
        status = elf64_load_kernel(entry->kernel, &g_kernel_entry, &g_kernel_phys, &g_kernel_size);
    } else if (strcmp(entry->protocol, "linux") == 0) {
        EFI_PHYSICAL_ADDRESS boot_params;
        status = linux_load_kernel(entry->kernel, &g_kernel_entry, &g_kernel_phys, &g_kernel_size, &boot_params);
        if (!EFI_ERROR(status) && entry->module_count > 0) {
            for (int i = 0; i < entry->module_count; i++) {
                EFI_PHYSICAL_ADDRESS initrd_phys;
                UINT64 initrd_size;
                linux_load_initrd(entry->module_paths[i], &initrd_phys, &initrd_size);
                /* Would update boot_params with initrd info */
            }
        }
    } else if (strcmp(entry->protocol, "multiboot2") == 0) {
        EFI_PHYSICAL_ADDRESS info_phys;
        status = multiboot2_load_kernel(entry->kernel, &g_kernel_entry, &g_kernel_phys, &g_kernel_size, &info_phys);
    } else if (strcmp(entry->protocol, "chainload") == 0) {
        /* Chainload another EFI app */
        status = efi_load_image(FALSE, NULL, NULL, 0, NULL);
        /* Would need to parse entry->efi_path */
    }

    return status;
}

/* ============================================================================
 * Setup Memory Map and HHDM
 * ============================================================================ */

EFI_STATUS setup_memory(void) {
    /* Determine HHDM offset */
    g_hhdm_offset = X86_64_HHDM_OFFSET;
    g_hhdm_size = 0x800000000000ULL;  /* 512 GiB */

    /* Initialize architecture memory management */
    EFI_STATUS status = arch_setup_page_tables(g_hhdm_offset, g_hhdm_size);
    if (EFI_ERROR(status)) return status;

    /* Initialize memory tracker */
    status = mem_init(g_hhdm_offset, g_hhdm_size);
    if (EFI_ERROR(status)) return status;

    return EFI_SUCCESS;
}

/* ============================================================================
 * Load and Process Config
 * ============================================================================ */

EFI_STATUS load_config(void) {
    EFI_LOADED_IMAGE_PROTOCOL *loaded = NULL;
    EFI_STATUS status = g_bs->HandleProtocol(g_image_handle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, (VOID **)&loaded);
    if (EFI_ERROR(status)) return status;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    status = g_bs->HandleProtocol(loaded->DeviceHandle, &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, (VOID **)&fs);
    if (EFI_ERROR(status)) return status;

    EFI_FILE_PROTOCOL *fs_root = NULL;
    status = fs->OpenVolume(fs, &fs_root);
    if (EFI_ERROR(status)) return status;

    /* Try multiple config paths */
    const CHAR16 *config_paths[] = {
        L"\\limoboot.conf",
        L"\\EFI\\limoboot\\limoboot.conf",
        L"\\boot\\limoboot.conf",
        NULL
    };

    for (int i = 0; config_paths[i]; i++) {
        status = config_load_file(fs_root, config_paths[i]);
        if (!EFI_ERROR(status)) {
            efi_printf("Loaded config from %s\n", config_paths[i]);
            break;
        }
    }

    fs_root->Close(fs_root);
    return EFI_SUCCESS;
}

/* ============================================================================
 * Prepare Boot
 * ============================================================================ */

EFI_STATUS prepare_boot(config_entry_t *entry) {
    EFI_STATUS status;

    /* Load kernel */
    status = load_kernel(entry);
    if (EFI_ERROR(status)) {
        efi_printf("Failed to load kernel: %llx\n", status);
        return status;
    }

    efi_printf("Kernel loaded at %p (entry: %p, size: %llu bytes)\n",
               (void *)g_kernel_phys, (void *)g_kernel_entry, g_kernel_size);

    /* Map kernel in page tables */
    status = arch_map_kernel(g_kernel_phys, g_kernel_entry, g_kernel_size);
    if (EFI_ERROR(status)) return status;

    /* Map framebuffer */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    status = g_bs->LocateProtocol(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, NULL, (VOID **)&gop);
    if (!EFI_ERROR(status) && gop->Mode) {
        arch_map_framebuffer(gop->Mode->FrameBufferBase, gop->Mode->FrameBufferBase,
                             gop->Mode->FrameBufferSize);
    }

    /* Map bootloader data structures */
    arch_map_bootloader_data(virt_to_phys(g_handoff), g_handoff ? (uint64_t)g_handoff : 0, sizeof(limoboot_handoff_t));

    /* Process limoboot protocol requests */
    extern limoboot_request_t limoboot_requests[];
    status = limoboot_process_requests(limoboot_requests);
    if (EFI_ERROR(status)) return status;

    /* Update kernel response with actual load info */
    /* This would be done in limoboot_process_requests */

    return EFI_SUCCESS;
}

/* ============================================================================
 * Boot Entry
 * ============================================================================ */

EFI_STATUS boot_entry(config_entry_t *entry) {
    EFI_STATUS status;

    if (strcmp(entry->protocol, "limoboot") == 0) {
        status = limoboot_finalize_and_boot(g_kernel_entry, g_hhdm_offset);
    } else if (strcmp(entry->protocol, "linux") == 0) {
        /* Would get boot_params from linux_load_kernel */
        EFI_PHYSICAL_ADDRESS boot_params = 0;  /* Placeholder */
        status = linux_boot(g_kernel_entry, boot_params);
    } else if (strcmp(entry->protocol, "multiboot2") == 0) {
        EFI_PHYSICAL_ADDRESS info_phys = 0;  /* Placeholder */
        status = multiboot2_boot(g_kernel_entry, info_phys);
    } else if (strcmp(entry->protocol, "chainload") == 0) {
        /* Chainload */
        status = EFI_UNSUPPORTED;
    } else {
        status = EFI_UNSUPPORTED;
    }

    return status;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *st) {
    /* Initialize architecture */
    EFI_STATUS status = arch_efi_init(image_handle, st);
    if (EFI_ERROR(status)) {
        efi_printf("Arch init failed: %llx\n", status);
        return status;
    }

    efi_printf("limoboot starting...\n");

    /* Initialize SMP */
    arch_smp_init();

    /* Load configuration */
    status = load_config();
    if (EFI_ERROR(status)) {
        efi_printf("Config load failed: %llx\n", status);
    }

    /* Setup memory */
    status = setup_memory();
    if (EFI_ERROR(status)) {
        efi_printf("Memory setup failed: %llx\n", status);
        return status;
    }

    /* Run boot menu */
    config_entry_t *entry = menu_run();

    if (!entry) {
        /* User exited to command line - not implemented yet */
        efi_printf("Command line not implemented yet\n");
        g_bs->Stall(5000000);
        return EFI_ABORTED;
    }

    efi_printf("Booting: %s\n", entry->name);

    /* Prepare boot */
    status = prepare_boot(entry);
    if (EFI_ERROR(status)) {
        efi_printf("Boot prepare failed: %llx\n", status);
        g_bs->Stall(5000000);
        return status;
    }

    /* Boot! */
    status = boot_entry(entry);
    if (EFI_ERROR(status)) {
        efi_printf("Boot failed: %llx\n", status);
        g_bs->Stall(5000000);
    }

    return status;
}