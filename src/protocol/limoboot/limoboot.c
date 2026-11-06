/*
 * Limoboot Protocol Implementation
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

/* ============================================================================
 * External References
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
extern EFI_STATUS entropy_build_response(limoboot_entropy_response_t *);
extern EFI_STATUS mem_build_memmap_response(limoboot_memmap_response_t *, limoboot_memmap_entry_t **);
extern EFI_STATUS efi_exit_boot_services(EFI_HANDLE, UINTN);
extern void efi_stall(UINTN);

/* ============================================================================
 * Global State
 * ============================================================================ */

static limoboot_handoff_t *g_handoff = NULL;
static limoboot_request_t *g_requests = NULL;
static UINTN g_request_count = 0;

static limoboot_memmap_response_t *g_memmap_response = NULL;
static limoboot_memmap_entry_t *g_memmap_entries = NULL;

static limoboot_framebuffer_response_t *g_fb_response = NULL;
static limoboot_modules_response_t *g_modules_response = NULL;
static limoboot_module_t *g_modules = NULL;

static limoboot_smp_response_t *g_smp_response = NULL;
static limoboot_rsdp_response_t *g_rsdp_response = NULL;
static limoboot_smbios_response_t *g_smbios_response = NULL;
static limoboot_efi_system_table_response_t *g_efi_st_response = NULL;
static limoboot_entropy_response_t *g_entropy_response = NULL;
static limoboot_kernel_file_response_t *g_kernel_response = NULL;
static limoboot_cmdline_response_t *g_cmdline_response = NULL;
static limoboot_measured_boot_response_t *g_measured_response = NULL;
static limoboot_paging_mode_response_t *g_paging_response = NULL;

/* ============================================================================
 * Request Processing
 * ============================================================================ */

EFI_STATUS limoboot_process_requests(limoboot_request_t *requests) {
    g_requests = requests;

    /* Count requests */
    g_request_count = 0;
    while (requests[g_request_count].id != 0) {
        g_request_count++;
    }

    /* Process each request */
    for (UINTN i = 0; i < g_request_count; i++) {
        limoboot_request_t *req = &requests[i];
        EFI_STATUS status = EFI_SUCCESS;

        switch (req->id) {
            case LIMBOOT_REQUEST_MEMORY_MAP:
                status = limoboot_handle_memmap(req);
                break;
            case LIMBOOT_REQUEST_FRAMEBUFFER:
                status = limoboot_handle_framebuffer(req);
                break;
            case LIMBOOT_REQUEST_MODULES:
                status = limoboot_handle_modules(req);
                break;
            case LIMBOOT_REQUEST_SMP:
                status = limoboot_handle_smp(req);
                break;
            case LIMBOOT_REQUEST_RSDP:
                status = limoboot_handle_rsdp(req);
                break;
            case LIMBOOT_REQUEST_SMBIOS:
                status = limoboot_handle_smbios(req);
                break;
            case LIMBOOT_REQUEST_EFI_SYSTEM_TABLE:
                status = limoboot_handle_efi_system_table(req);
                break;
            case LIMBOOT_REQUEST_ENTROPY:
                status = limoboot_handle_entropy(req);
                break;
            case LIMBOOT_REQUEST_KERNEL_FILE:
                status = limoboot_handle_kernel_file(req);
                break;
            case LIMBOOT_REQUEST_CMDLINE:
                status = limoboot_handle_cmdline(req);
                break;
            case LIMBOOT_REQUEST_MEASURED_BOOT:
                status = limoboot_handle_measured_boot(req);
                break;
            case LIMBOOT_REQUEST_PAGING_MODE:
                status = limoboot_handle_paging_mode(req);
                break;
            default:
                if (req->flags & LIMBOOT_REQUEST_FLAG_REQUIRED) {
                    efi_printf("Unknown required request: %llx\n", req->id);
                    status = EFI_UNSUPPORTED;
                }
                break;
        }

        if (EFI_ERROR(status) && (req->flags & LIMBOOT_REQUEST_FLAG_REQUIRED)) {
            efi_printf("Required request %llx failed: %llx\n", req->id, status);
            return status;
        }
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * Individual Request Handlers
 * ============================================================================ */

EFI_STATUS limoboot_handle_memmap(limoboot_request_t *req) {
    g_memmap_response = efi_allocate_pool(sizeof(limoboot_memmap_response_t), EfiLoaderData);
    if (!g_memmap_response) return EFI_OUT_OF_RESOURCES;

    EFI_STATUS status = mem_build_memmap_response(g_memmap_response, &g_memmap_entries);
    if (!EFI_ERROR(status)) {
        req->response = virt_to_phys(g_memmap_response);
    }
    return status;
}

EFI_STATUS limoboot_handle_framebuffer(limoboot_request_t *req) {
    g_fb_response = efi_allocate_pool(sizeof(limoboot_framebuffer_response_t), EfiLoaderData);
    if (!g_fb_response) return EFI_OUT_OF_RESOURCES;

    /* Get GOP */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS status = g_bs->LocateProtocol(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, NULL, (VOID **)&gop);
    if (EFI_ERROR(status) || !gop || !gop->Mode) {
        efi_free_pool(g_fb_response);
        return EFI_NOT_FOUND;
    }

    g_fb_response->revision = 0;
    g_fb_response->framebuffer_addr = gop->Mode->FrameBufferBase;
    g_fb_response->width = gop->Mode->Info->HorizontalResolution;
    g_fb_response->height = gop->Mode->Info->VerticalResolution;
    g_fb_response->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
    g_fb_response->bpp = 32;

    switch (gop->Mode->Info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor:
            g_fb_response->format = LIMBOOT_FB_FORMAT_RGB;
            g_fb_response->red_mask = 0x00FF0000;
            g_fb_response->green_mask = 0x0000FF00;
            g_fb_response->blue_mask = 0x000000FF;
            g_fb_response->reserved_mask = 0xFF000000;
            break;
        case PixelBlueGreenRedReserved8BitPerColor:
            g_fb_response->format = LIMBOOT_FB_FORMAT_BGR;
            g_fb_response->red_mask = 0x000000FF;
            g_fb_response->green_mask = 0x0000FF00;
            g_fb_response->blue_mask = 0x00FF0000;
            g_fb_response->reserved_mask = 0xFF000000;
            break;
        case PixelBitMask:
            g_fb_response->format = LIMBOOT_FB_FORMAT_RGB;
            g_fb_response->red_mask = gop->Mode->Info->PixelInformation.RedMask;
            g_fb_response->green_mask = gop->Mode->Info->PixelInformation.GreenMask;
            g_fb_response->blue_mask = gop->Mode->Info->PixelInformation.BlueMask;
            g_fb_response->reserved_mask = gop->Mode->Info->PixelInformation.ReservedMask;
            break;
        default:
            g_fb_response->format = LIMBOOT_FB_FORMAT_RGB;
            g_fb_response->red_mask = 0x00FF0000;
            g_fb_response->green_mask = 0x0000FF00;
            g_fb_response->blue_mask = 0x000000FF;
            g_fb_response->reserved_mask = 0xFF000000;
            break;
    }

    req->response = virt_to_phys(g_fb_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_modules(limoboot_request_t *req) {
    config_t *config = config_get();
    config_entry_t *entry = config_get_default_entry();
    if (!entry || entry->module_count == 0) {
        return EFI_NOT_FOUND;
    }

    g_modules_response = efi_allocate_pool(sizeof(limoboot_modules_response_t), EfiLoaderData);
    g_modules = efi_allocate_pool(entry->module_count * sizeof(limoboot_module_t), EfiLoaderData);
    if (!g_modules_response || !g_modules) return EFI_OUT_OF_RESOURCES;

    /* Load modules from filesystem */
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

    for (int i = 0; i < entry->module_count; i++) {
        EFI_FILE_PROTOCOL *file = NULL;
        CHAR16 path[256];
        ascii_to_unicode(entry->module_paths[i], path, 256);

        status = fs_root->Open(fs_root, &file, path, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status)) {
            efi_printf("Failed to open module: %s\n", entry->module_paths[i]);
            continue;
        }

        EFI_FILE_INFO *info = NULL;
        UINTN info_size = 0;
        status = file->GetInfo(file, &EFI_FILE_INFO_ID, &info_size, info);
        if (status == EFI_BUFFER_TOO_SMALL) {
            info = efi_allocate_pool(info_size, EfiBootServicesData);
            status = file->GetInfo(file, &EFI_FILE_INFO_ID, &info_size, info);
        }

        if (EFI_ERROR(status)) {
            file->Close(file);
            continue;
        }

        UINT64 file_size = info->FileSize;
        efi_free_pool(info);

        EFI_PHYSICAL_ADDRESS module_phys;
        status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, (file_size + 4095) / 4096, &module_phys);
        if (EFI_ERROR(status)) {
            file->Close(file);
            continue;
        }

        void *module_virt = phys_to_virt(module_phys);
        UINTN read_size = (UINTN)file_size;
        status = file->Read(file, &read_size, module_virt);
        file->Close(file);

        if (EFI_ERROR(status)) {
            g_bs->FreePages(module_phys, (file_size + 4095) / 4096);
            continue;
        }

        g_modules[i].base = module_phys;
        g_modules[i].length = file_size;

        /* Store module name string */
        CHAR16 *name_str = efi_allocate_pool((wcslen(path) + 1) * 2, EfiLoaderData);
        if (name_str) {
            wcscpy(name_str, path);
            g_modules[i].string = virt_to_phys(name_str);
        }
    }

    fs_root->Close(fs_root);

    g_modules_response->revision = 0;
    g_modules_response->module_count = entry->module_count;
    g_modules_response->modules = virt_to_phys(g_modules);

    req->response = virt_to_phys(g_modules_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_smp(limoboot_request_t *req) {
    g_smp_response = efi_allocate_pool(sizeof(limoboot_smp_response_t), EfiLoaderData);
    if (!g_smp_response) return EFI_OUT_OF_RESOURCES;

    EFI_STATUS status = arch_build_smp_response(g_smp_response);
    if (!EFI_ERROR(status)) {
        req->response = virt_to_phys(g_smp_response);
    }
    return status;
}

EFI_STATUS limoboot_handle_rsdp(limoboot_request_t *req) {
    g_rsdp_response = efi_allocate_pool(sizeof(limoboot_rsdp_response_t), EfiLoaderData);
    if (!g_rsdp_response) return EFI_OUT_OF_RESOURCES;

    void *rsdp = efi_get_config_table(&EFI_ACPI_20_TABLE_GUID);
    if (!rsdp) {
        efi_free_pool(g_rsdp_response);
        return EFI_NOT_FOUND;
    }

    g_rsdp_response->revision = 0;
    g_rsdp_response->rsdp_addr = virt_to_phys(rsdp);

    req->response = virt_to_phys(g_rsdp_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_smbios(limoboot_request_t *req) {
    g_smbios_response = efi_allocate_pool(sizeof(limoboot_smbios_response_t), EfiLoaderData);
    if (!g_smbios_response) return EFI_OUT_OF_RESOURCES;

    void *smbios = efi_get_config_table(&EFI_SMBIOS3_TABLE_GUID);
    if (!smbios) {
        smbios = efi_get_config_table(&EFI_SMBIOS_TABLE_GUID);
    }
    if (!smbios) {
        efi_free_pool(g_smbios_response);
        return EFI_NOT_FOUND;
    }

    g_smbios_response->revision = 0;
    g_smbios_response->entry_point_addr = virt_to_phys(smbios);

    req->response = virt_to_phys(g_smbios_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_efi_system_table(limoboot_request_t *req) {
    g_efi_st_response = efi_allocate_pool(sizeof(limoboot_efi_system_table_response_t), EfiLoaderData);
    if (!g_efi_st_response) return EFI_OUT_OF_RESOURCES;

    g_efi_st_response->revision = 0;
    g_efi_st_response->system_table_addr = virt_to_phys(g_st);

    req->response = virt_to_phys(g_efi_st_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_entropy(limoboot_request_t *req) {
    g_entropy_response = efi_allocate_pool(sizeof(limoboot_entropy_response_t), EfiLoaderData);
    if (!g_entropy_response) return EFI_OUT_OF_RESOURCES;

    EFI_STATUS status = entropy_build_response(g_entropy_response);
    if (!EFI_ERROR(status)) {
        req->response = virt_to_phys(g_entropy_response);
    }
    return status;
}

EFI_STATUS limoboot_handle_kernel_file(limoboot_request_t *req) {
    g_kernel_response = efi_allocate_pool(sizeof(limoboot_kernel_file_response_t), EfiLoaderData);
    if (!g_kernel_response) return EFI_OUT_OF_RESOURCES;

    config_entry_t *entry = config_get_default_entry();
    if (!entry || !entry->kernel[0]) {
        efi_free_pool(g_kernel_response);
        return EFI_NOT_FOUND;
    }

    /* Kernel is already loaded by main, just report it */
    /* This would be filled in by the main bootloader logic */
    g_kernel_response->revision = 0;
    g_kernel_response->kernel_addr = 0;  /* Filled later */
    g_kernel_response->kernel_size = 0;  /* Filled later */

    req->response = virt_to_phys(g_kernel_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_cmdline(limoboot_request_t *req) {
    g_cmdline_response = efi_allocate_pool(sizeof(limoboot_cmdline_response_t), EfiLoaderData);
    if (!g_cmdline_response) return EFI_OUT_OF_RESOURCES;

    config_entry_t *entry = config_get_default_entry();
    if (!entry) {
        efi_free_pool(g_cmdline_response);
        return EFI_NOT_FOUND;
    }

    CHAR16 *cmdline_uni = efi_allocate_pool((strlen(entry->cmdline) + 1) * 2, EfiLoaderData);
    if (!cmdline_uni) {
        efi_free_pool(g_cmdline_response);
        return EFI_OUT_OF_RESOURCES;
    }

    ascii_to_unicode(entry->cmdline, cmdline_uni, strlen(entry->cmdline) + 1);

    g_cmdline_response->revision = 0;
    g_cmdline_response->cmdline_addr = virt_to_phys(cmdline_uni);

    req->response = virt_to_phys(g_cmdline_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_measured_boot(limoboot_request_t *req) {
    g_measured_response = efi_allocate_pool(sizeof(limoboot_measured_boot_response_t), EfiLoaderData);
    if (!g_measured_response) return EFI_OUT_OF_RESOURCES;

    /* Get TPM event log */
    EFI_TCG2_PROTOCOL *tcg2 = NULL;
    EFI_STATUS status = g_bs->LocateProtocol(&EFI_TCG2_PROTOCOL_GUID, NULL, (VOID **)&tcg2);
    if (EFI_ERROR(status) || !tcg2) {
        efi_free_pool(g_measured_response);
        return EFI_NOT_FOUND;
    }

    EFI_PHYSICAL_ADDRESS event_log = 0, last_entry = 0;
    BOOLEAN truncated = FALSE;
    status = tcg2->GetEventLog(tcg2, EFI_TCG2_EVENT_LOG_FORMAT_TCG_2, &event_log, &last_entry, &truncated);
    if (EFI_ERROR(status)) {
        efi_free_pool(g_measured_response);
        return status;
    }

    g_measured_response->revision = 0;
    g_measured_response->event_log_addr = event_log;
    g_measured_response->event_log_size = (UINT64)last_entry - event_log;
    g_measured_response->tpm_addr = 0;  /* Would need to find TPM device */

    req->response = virt_to_phys(g_measured_response);
    return EFI_SUCCESS;
}

EFI_STATUS limoboot_handle_paging_mode(limoboot_request_t *req) {
    g_paging_response = efi_allocate_pool(sizeof(limoboot_paging_mode_response_t), EfiLoaderData);
    if (!g_paging_response) return EFI_OUT_OF_RESOURCES;

    g_paging_response->revision = 0;

#if defined(__x86_64__)
    if (arch_has_feature("la57")) {
        g_paging_response->mode = LIMBOOT_PAGING_MODE_5LEVEL;
    } else {
        g_paging_response->mode = LIMBOOT_PAGING_MODE_4LEVEL;
    }
#elif defined(__aarch64__)
    g_paging_response->mode = LIMBOOT_PAGING_MODE_LPAE;
#elif defined(__riscv)
    g_paging_response->mode = LIMBOOT_PAGING_MODE_SV48;
#elif defined(__loongarch64)
    g_paging_response->mode = LIMBOOT_PAGING_MODE_LPAE;
#endif

    g_paging_response->flags = 0;
    req->response = virt_to_phys(g_paging_response);
    return EFI_SUCCESS;
}

/* ============================================================================
 * Finalize and Boot
 * ============================================================================ */

EFI_STATUS limoboot_finalize_and_boot(EFI_PHYSICAL_ADDRESS kernel_entry, EFI_PHYSICAL_ADDRESS hhdm_offset) {
    /* Create handoff structure */
    g_handoff = arch_create_handoff(hhdm_offset);
    if (!g_handoff) return EFI_OUT_OF_RESOURCES;

    g_handoff->entry_point = kernel_entry;
    g_handoff->response_ptr = 0;  /* Will be filled below */

    /* Build response array */
    UINTN response_count = 0;
    limoboot_request_t *req = g_requests;
    while (req->id != 0) {
        if (req->response != 0) response_count++;
        req++;
    }

    uint64_t *response_array = efi_allocate_pool(response_count * sizeof(uint64_t), EfiLoaderData);
    if (!response_array) return EFI_OUT_OF_RESOURCES;

    UINTN idx = 0;
    req = g_requests;
    while (req->id != 0) {
        if (req->response != 0) {
            response_array[idx++] = req->response;
        }
        req++;
    }

    g_handoff->response_ptr = virt_to_phys(response_array);
    g_handoff->response_count = response_count;

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
    if (EFI_ERROR(status)) {
        efi_free_pool(map);
        return status;
    }

    /* Load page tables */
    arch_load_page_tables();

    /* Jump to kernel entry point */
    /* Kernel expects: RAX=magic, RBX=handoff_phys, RSP=stack */
    __asm__ volatile (
        "mov %0, %%rax\n"
        "mov %1, %%rbx\n"
        "mov %2, %%rsp\n"
        "jmp *%3"
        :
        : "r"(LIMBOOT_PROTOCOL_MAGIC),
          "r"(virt_to_phys(g_handoff)),
          "r"(arch_get_stack_phys()),
          "r"(kernel_entry)
        : "rax", "rbx", "rsp", "memory"
    );

    /* Unreachable */
    return EFI_SUCCESS;
}