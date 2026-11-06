/*
 * Limoboot Protocol - Kernel Side Implementation
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

/* ============================================================================
 * Limoboot Protocol Kernel API
 * ============================================================================ */

/* Handoff structure pointer */
limoboot_handoff_t *limoboot_handoff = NULL;

/* Response pointers */
limoboot_memmap_response_t *limoboot_memmap = NULL;
limoboot_framebuffer_response_t *limoboot_framebuffer = NULL;
limoboot_modules_response_t *limoboot_modules = NULL;
limoboot_smp_response_t *limoboot_smp = NULL;
limoboot_rsdp_response_t *limoboot_rsdp = NULL;
limoboot_smbios_response_t *limoboot_smbios = NULL;
limoboot_efi_system_table_response_t *limoboot_efi_st = NULL;
limoboot_entropy_response_t *limoboot_entropy = NULL;
limoboot_kernel_file_response_t *limoboot_kernel_file = NULL;
limoboot_cmdline_response_t *limoboot_cmdline = NULL;
limoboot_measured_boot_response_t *limoboot_measured = NULL;
limoboot_paging_mode_response_t *limoboot_paging = NULL;

/* ============================================================================
 * Initialize from Handoff
 * ============================================================================ */

int limoboot_init(limoboot_handoff_t *handoff) {
    if (!handoff) return -1;
    if (handoff->magic != LIMBOOT_PROTOCOL_MAGIC) return -1;
    if (handoff->version != LIMBOOT_PROTOCOL_VERSION) return -1;

    limoboot_handoff = handoff;

    /* Parse response array */
    uint64_t *responses = (uint64_t *)handoff->response_ptr;
    for (uint64_t i = 0; i < handoff->response_count; i++) {
        uint64_t resp_ptr = responses[i];
        if (!resp_ptr) continue;

        /* Determine response type by checking magic/structure */
        /* We'll use the request IDs to match */
    }

    /* Better: iterate through known request IDs */
    static const uint64_t request_ids[] = {
        LIMBOOT_REQUEST_MEMORY_MAP,
        LIMBOOT_REQUEST_FRAMEBUFFER,
        LIMBOOT_REQUEST_MODULES,
        LIMBOOT_REQUEST_SMP,
        LIMBOOT_REQUEST_RSDP,
        LIMBOOT_REQUEST_SMBIOS,
        LIMBOOT_REQUEST_EFI_SYSTEM_TABLE,
        LIMBOOT_REQUEST_ENTROPY,
        LIMBOOT_REQUEST_KERNEL_FILE,
        LIMBOOT_REQUEST_CMDLINE,
        LIMBOOT_REQUEST_MEASURED_BOOT,
        LIMBOOT_REQUEST_PAGING_MODE,
    };

    extern limoboot_request_t limoboot_requests[];
    for (int i = 0; limoboot_requests[i].id != 0; i++) {
        if (limoboot_requests[i].response) {
            uint64_t resp = limoboot_requests[i].response;
            switch (limoboot_requests[i].id) {
                case LIMBOOT_REQUEST_MEMORY_MAP:
                    limoboot_memmap = (limoboot_memmap_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_FRAMEBUFFER:
                    limoboot_framebuffer = (limoboot_framebuffer_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_MODULES:
                    limoboot_modules = (limoboot_modules_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_SMP:
                    limoboot_smp = (limoboot_smp_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_RSDP:
                    limoboot_rsdp = (limoboot_rsdp_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_SMBIOS:
                    limoboot_smbios = (limoboot_smbios_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_EFI_SYSTEM_TABLE:
                    limoboot_efi_st = (limoboot_efi_system_table_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_ENTROPY:
                    limoboot_entropy = (limoboot_entropy_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_KERNEL_FILE:
                    limoboot_kernel_file = (limoboot_kernel_file_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_CMDLINE:
                    limoboot_cmdline = (limoboot_cmdline_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_MEASURED_BOOT:
                    limoboot_measured = (limoboot_measured_boot_response_t *)resp;
                    break;
                case LIMBOOT_REQUEST_PAGING_MODE:
                    limoboot_paging = (limoboot_paging_mode_response_t *)resp;
                    break;
            }
        }
    }

    return 0;
}

/* ============================================================================
 * Memory Map Access
 * ============================================================================ */

uint64_t limoboot_memmap_count(void) {
    return limoboot_memmap ? limoboot_memmap->entry_count : 0;
}

limoboot_memmap_entry_t *limoboot_memmap_entry(uint64_t index) {
    if (!limoboot_memmap || index >= limoboot_memmap->entry_count) return NULL;
    return &((limoboot_memmap_entry_t *)limoboot_memmap->entries)[index];
}

void limoboot_memmap_for_each(void (*callback)(limoboot_memmap_entry_t *, void *), void *ctx) {
    if (!limoboot_memmap) return;
    limoboot_memmap_entry_t *entries = (limoboot_memmap_entry_t *)limoboot_memmap->entries;
    for (uint64_t i = 0; i < limoboot_memmap->entry_count; i++) {
        callback(&entries[i], ctx);
    }
}

/* Find usable memory region of at least size bytes */
uint64_t limoboot_find_free_memory(uint64_t size, uint64_t align) {
    if (!limoboot_memmap) return 0;
    limoboot_memmap_entry_t *entries = (limoboot_memmap_entry_t *)limoboot_memmap->entries;
    for (uint64_t i = 0; i < limoboot_memmap->entry_count; i++) {
        if (entries[i].type == LIMBOOT_MEMORY_USABLE) {
            uint64_t start = (entries[i].base + align - 1) & ~(align - 1);
            if (start + size <= entries[i].base + entries[i].length) {
                return start;
            }
        }
    }
    return 0;
}

/* ============================================================================
 * Framebuffer Access
 * ============================================================================ */

int limoboot_fb_get_info(uint64_t *width, uint64_t *height, uint64_t *pitch,
                          uint64_t *bpp, uint64_t *format) {
    if (!limoboot_framebuffer) return -1;
    if (width) *width = limoboot_framebuffer->width;
    if (height) *height = limoboot_framebuffer->height;
    if (pitch) *pitch = limoboot_framebuffer->pitch;
    if (bpp) *bpp = limoboot_framebuffer->bpp;
    if (format) *format = limoboot_framebuffer->format;
    return 0;
}

void *limoboot_fb_get_addr(void) {
    if (!limoboot_framebuffer) return NULL;
    return (void *)limoboot_framebuffer->framebuffer_addr;
}

/* ============================================================================
 * Modules Access
 * ============================================================================ */

uint64_t limoboot_module_count(void) {
    return limoboot_modules ? limoboot_modules->module_count : 0;
}

limoboot_module_t *limoboot_module_get(uint64_t index) {
    if (!limoboot_modules || index >= limoboot_modules->module_count) return NULL;
    return &((limoboot_module_t *)limoboot_modules->modules)[index];
}

limoboot_module_t *limoboot_module_find(const char *name) {
    if (!limoboot_modules) return NULL;
    limoboot_module_t *modules = (limoboot_module_t *)limoboot_modules->modules;
    for (uint64_t i = 0; i < limoboot_modules->module_count; i++) {
        const char *mod_name = (const char *)modules[i].string;
        if (strcmp(mod_name, name) == 0) {
            return &modules[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * SMP Access
 * ============================================================================ */

uint64_t limoboot_smp_cpu_count(void) {
    return limoboot_smp ? limoboot_smp->cpu_count : 0;
}

limoboot_smp_cpu_t *limoboot_smp_cpu_get(uint64_t index) {
    if (!limoboot_smp || index >= limoboot_smp->cpu_count) return NULL;
    return &((limoboot_smp_cpu_t *)limoboot_smp->cpus)[index];
}

uint64_t limoboot_smp_bsp_index(void) {
    return limoboot_smp ? limoboot_smp->bsp_index : 0;
}

void *limoboot_smp_trampoline(void) {
    if (!limoboot_smp) return NULL;
    return (void *)limoboot_smp->trampoline;
}

uint64_t limoboot_smp_trampoline_size(void) {
    return limoboot_smp ? limoboot_smp->trampoline_size : 0;
}

/* ============================================================================
 * RSDP / ACPI
 * ============================================================================ */

void *limoboot_get_rsdp(void) {
    if (!limoboot_rsdp) return NULL;
    return (void *)limoboot_rsdp->rsdp_addr;
}

/* ============================================================================
 * SMBIOS
 * ============================================================================ */

void *limoboot_get_smbios(void) {
    if (!limoboot_smbios) return NULL;
    return (void *)limoboot_smbios->entry_point_addr;
}

/* ============================================================================
 * EFI System Table
 * ============================================================================ */

void *limoboot_get_efi_system_table(void) {
    if (!limoboot_efi_st) return NULL;
    return (void *)limoboot_efi_st->system_table_addr;
}

/* ============================================================================
 * Entropy
 * ============================================================================ */

void *limoboot_get_entropy(uint64_t *size) {
    if (!limoboot_entropy) return NULL;
    if (size) *size = limoboot_entropy->entropy_size;
    return (void *)limoboot_entropy->entropy_addr;
}

/* ============================================================================
 * Command Line
 * ============================================================================ */

const char *limoboot_get_cmdline(void) {
    if (!limoboot_cmdline) return "";
    return (const char *)limoboot_cmdline->cmdline_addr;
}

/* ============================================================================
 * Measured Boot
 * ============================================================================ */

void *limoboot_get_tpm_log(uint64_t *size) {
    if (!limoboot_measured) return NULL;
    if (size) *size = limoboot_measured->event_log_size;
    return (void *)limoboot_measured->event_log_addr;
}

/* ============================================================================
 * Paging Mode
 * ============================================================================ */

uint64_t limoboot_get_paging_mode(void) {
    if (!limoboot_paging) return 0;
    return limoboot_paging->mode;
}

/* ============================================================================
 * HHDM
 * ============================================================================ */

void *limoboot_phys_to_virt(uint64_t phys) {
    if (!limoboot_handoff) return (void *)phys;
    return (void *)(phys + limoboot_handoff->hhdm_offset);
}

uint64_t limoboot_virt_to_phys(void *virt) {
    if (!limoboot_handoff) return (uint64_t)virt;
    return (uint64_t)virt - limoboot_handoff->hhdm_offset;
}

uint64_t limoboot_get_hhdm_offset(void) {
    return limoboot_handoff ? limoboot_handoff->hhdm_offset : 0;
}

/* ============================================================================
 * Debug Print (using framebuffer)
 * ============================================================================ */

static uint32_t g_fb_x = 0, g_fb_y = 0;

void limoboot_putc(char c) {
    if (!limoboot_framebuffer) return;

    uint32_t *fb = (uint32_t *)limoboot_framebuffer->framebuffer_addr;
    uint32_t width = limoboot_framebuffer->width;
    uint32_t height = limoboot_framebuffer->height;
    uint32_t pitch = limoboot_framebuffer->pitch / 4;

    if (c == '\n') {
        g_fb_x = 0;
        g_fb_y += 16;
    } else if (c == '\r') {
        g_fb_x = 0;
    } else if (c >= 32) {
        /* Simple 8x8 font - placeholder */
        for (int dy = 0; dy < 16; dy++) {
            for (int dx = 0; dx < 8; dx++) {
                if (g_fb_y + dy < height && g_fb_x + dx < width) {
                    fb[(g_fb_y + dy) * pitch + g_fb_x + dx] = 0xFFFFFFFF;
                }
            }
        }
        g_fb_x += 8;
    }

    if (g_fb_y + 16 >= height) {
        /* Scroll */
        g_fb_y = 0;
    }
}

void limoboot_puts(const char *s) {
    while (*s) limoboot_putc(*s++);
}

void limoboot_puthex(uint64_t val) {
    char buf[18];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        buf[16 - i + 1] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    buf[18] = '\0';
    limoboot_puts(buf);
}

void limoboot_putptr(void *ptr) {
    limoboot_puthex((uint64_t)ptr);
}