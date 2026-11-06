/*
 * Minimal Kernel Template for limoboot
 * Works on x86-64, aarch64, riscv64, loongarch64
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/protocol.h>

/* ============================================================================
 * Kernel Entry Point
 * ============================================================================ */

__attribute__((section(".limoboot_requests"), used))
limoboot_request_t limoboot_requests[] = {
    { .id = LIMBOOT_REQUEST_MEMORY_MAP,      .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_FRAMEBUFFER,     .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_MODULES,         .flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_SMP,             .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_RSDP,            .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_SMBIOS,          .flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_EFI_SYSTEM_TABLE,.flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_ENTROPY,         .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_CMDLINE,         .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_MEASURED_BOOT,   .flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_PAGING_MODE,     .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = 0 },  /* Terminator */
};

/* ============================================================================
 * Handoff Pointer (filled by bootloader)
 * ============================================================================ */

limoboot_handoff_t *g_handoff = NULL;

/* ============================================================================
 * Simple Console (framebuffer)
 * ============================================================================ */

static volatile uint32_t *g_fb = NULL;
static uint32_t g_fb_width = 0;
static uint32_t g_fb_height = 0;
static uint32_t g_fb_pitch = 0;
static uint32_t g_fb_bpp = 0;
static uint32_t g_cursor_x = 0;
static uint32_t g_cursor_y = 0;

static const uint8_t font_8x16[128][16] = {
    [' '] = {0},
    ['!'] = {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['"'] = {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* ... truncated for brevity - would include full font ... */
};

void console_init(limoboot_framebuffer_response_t *fb) {
    g_fb = (volatile uint32_t *)fb->framebuffer_addr;
    g_fb_width = fb->width;
    g_fb_height = fb->height;
    g_fb_pitch = fb->pitch;
    g_fb_bpp = fb->bpp;
    g_cursor_x = 0;
    g_cursor_y = 0;
}

void console_putc(char c) {
    if (c == '\n') {
        g_cursor_x = 0;
        g_cursor_y += 16;
    } else if (c == '\r') {
        g_cursor_x = 0;
    } else if (c >= 32 && c < 127) {
        if (g_cursor_x + 8 > g_fb_width) {
            g_cursor_x = 0;
            g_cursor_y += 16;
        }
        if (g_cursor_y + 16 > g_fb_height) {
            /* Scroll up - simplified */
            g_cursor_y = 0;
        }

        /* Draw character - placeholder */
        for (int dy = 0; dy < 16; dy++) {
            uint8_t row = font_8x16[(int)c][dy];
            for (int dx = 0; dx < 8; dx++) {
                if (row & (1 << (7 - dx))) {
                    uint32_t *pixel = (uint32_t *)((uint8_t *)g_fb +
                        (g_cursor_y + dy) * g_fb_pitch +
                        (g_cursor_x + dx) * 4);
                    *pixel = 0xFFFFFFFF;  /* White */
                }
            }
        }
        g_cursor_x += 8;
    }
}

void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

void console_puthex(uint64_t val) {
    char buf[18];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        buf[16 - i + 1] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    buf[18] = '\0';
    console_puts(buf);
}

void console_putptr(void *ptr) {
    console_puthex((uint64_t)ptr);
}

/* ============================================================================
 * Response Accessors
 * ============================================================================ */

limoboot_memmap_response_t *get_memmap_response(void) {
    for (uint64_t i = 0; i < g_handoff->response_count; i++) {
        uint64_t *responses = (uint64_t *)g_handoff->response_ptr;
        limoboot_request_t *req = &limoboot_requests[i];
        if (req->id == LIMBOOT_REQUEST_MEMORY_MAP && req->response) {
            return (limoboot_memmap_response_t *)req->response;
        }
    }
    return NULL;
}

limoboot_framebuffer_response_t *get_framebuffer_response(void) {
    for (uint64_t i = 0; i < g_handoff->response_count; i++) {
        uint64_t *responses = (uint64_t *)g_handoff->response_ptr;
        limoboot_request_t *req = &limoboot_requests[i];
        if (req->id == LIMBOOT_REQUEST_FRAMEBUFFER && req->response) {
            return (limoboot_framebuffer_response_t *)req->response;
        }
    }
    return NULL;
}

/* ============================================================================
 * Kernel Main
 * ============================================================================ */

void kmain(limoboot_handoff_t *handoff) {
    g_handoff = handoff;

    /* Verify magic */
    if (handoff->magic != LIMBOOT_PROTOCOL_MAGIC) {
        return;
    }

    /* Initialize console */
    limoboot_framebuffer_response_t *fb = get_framebuffer_response();
    if (fb) {
        console_init(fb);
    }

    console_puts("=== limoboot Kernel Template ===\n");
    console_puts("Protocol version: ");
    console_puthex(handoff->version);
    console_puts("\n");

    console_puts("HHDM offset: ");
    console_putptr((void *)handoff->hhdm_offset);
    console_puts("\n");

    console_puts("Entry point: ");
    console_putptr((void *)handoff->entry_point);
    console_puts("\n");

    console_puts("Flags: ");
    console_puthex(handoff->flags);
    console_puts("\n");

    /* Print memory map */
    limoboot_memmap_response_t *mm = get_memmap_response();
    if (mm) {
        console_puts("\nMemory Map:\n");
        limoboot_memmap_entry_t *entries = (limoboot_memmap_entry_t *)mm->entries;
        for (uint64_t i = 0; i < mm->entry_count; i++) {
            console_puts("  [");
            console_puthex(entries[i].base);
            console_puts(" - ");
            console_puthex(entries[i].base + entries[i].length);
            console_puts("] ");
            console_puthex(entries[i].length);
            console_puts(" bytes, type=");
            console_puthex(entries[i].type);
            console_puts("\n");
        }
    }

    /* Print command line */
    for (uint64_t i = 0; i < g_handoff->response_count; i++) {
        uint64_t *responses = (uint64_t *)g_handoff->response_ptr;
        limoboot_request_t *req = &limoboot_requests[i];
        if (req->id == LIMBOOT_REQUEST_CMDLINE && req->response) {
            limoboot_cmdline_response_t *cmd = (limoboot_cmdline_response_t *)req->response;
            CHAR16 *cmdline = (CHAR16 *)cmd->cmdline_addr;
            console_puts("\nCmdline: ");
            while (*cmdline) {
                console_putc((char)*cmdline++);
            }
            console_puts("\n");
            break;
        }
    }

    /* Print modules */
    for (uint64_t i = 0; i < g_handoff->response_count; i++) {
        uint64_t *responses = (uint64_t *)g_handoff->response_ptr;
        limoboot_request_t *req = &limoboot_requests[i];
        if (req->id == LIMBOOT_REQUEST_MODULES && req->response) {
            limoboot_modules_response_t *mods = (limoboot_modules_response_t *)req->response;
            limoboot_module_t *modules = (limoboot_module_t *)mods->modules;
            console_puts("\nModules:\n");
            for (uint64_t j = 0; j < mods->module_count; j++) {
                CHAR16 *name = (CHAR16 *)modules[j].string;
                console_puts("  ");
                while (*name) console_putc((char)*name++);
                console_puts(" @ ");
                console_putptr((void *)modules[j].base);
                console_puts(" (");
                console_puthex(modules[j].length);
                console_puts(" bytes)\n");
            }
            break;
        }
    }

    console_puts("\nKernel initialized successfully!\n");
    console_puts("Halting...\n");

    /* Halt */
    while (1) {
#if defined(__x86_64__)
        __asm__ volatile ("hlt");
#elif defined(__aarch64__)
        __asm__ volatile ("wfi");
#elif defined(__riscv)
        __asm__ volatile ("wfi");
#endif
    }
}

/* ============================================================================
 * Architecture-Specific Entry Points
 * ============================================================================ */

#if defined(__x86_64__)
/* Entry from limoboot: RAX=magic, RBX=handoff_phys, RSP=stack */
__attribute__((naked, section(".text.entry")))
void _start(void) {
    __asm__ volatile (
        "mov %rbx, %rdi\n"    /* handoff pointer -> 1st arg */
        "call kmain\n"
        "1: hlt; jmp 1b\n"
    );
}
#elif defined(__aarch64__)
/* Entry from limoboot: x0=magic, x1=handoff_phys */
__attribute__((naked, section(".text.entry")))
void _start(void) {
    __asm__ volatile (
        "mov x1, x0\n"        /* handoff pointer -> 1st arg */
        "bl kmain\n"
        "1: wfi; b 1b\n"
    );
}
#elif defined(__riscv)
/* Entry from limoboot: a0=magic, a1=handoff_phys */
__attribute__((naked, section(".text.entry")))
void _start(void) {
    __asm__ volatile (
        "mv a1, a0\n"         /* handoff pointer -> 1st arg */
        "call kmain\n"
        "1: wfi; j 1b\n"
    );
}
#elif defined(__loongarch64)
/* Entry from limoboot: a0=magic, a1=handoff_phys */
__attribute__((naked, section(".text.entry")))
void _start(void) {
    __asm__ volatile (
        "move a1, a0\n"       /* handoff pointer -> 1st arg */
        "bl kmain\n"
        "1: idle; b 1b\n"
    );
}
#endif