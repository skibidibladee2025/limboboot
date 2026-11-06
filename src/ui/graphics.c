/*
 * Graphics and Framebuffer Support
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"
#include "memory.c"
#include "efi.c"
#include "printf.c"

/* ============================================================================
 * Graphics State
 * ============================================================================ */

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t format;
    void *framebuffer;
    UINTN framebuffer_size;
} graphics_t;

static graphics_t g_graphics;

/* ============================================================================
 * Initialize Graphics
 * ============================================================================ */

EFI_STATUS graphics_init(void) {
    EFI_STATUS status = g_bs->LocateProtocol(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, NULL, (VOID **)&g_graphics.gop);
    if (EFI_ERROR(status)) return status;

    if (!g_graphics.gop->Mode || !g_graphics.gop->Mode->Info) {
        return EFI_UNSUPPORTED;
    }

    g_graphics.mode_info = g_graphics.gop->Mode->Info;
    g_graphics.width = g_graphics.mode_info->HorizontalResolution;
    g_graphics.height = g_graphics.mode_info->VerticalResolution;
    g_graphics.pitch = g_graphics.mode_info->PixelsPerScanLine * 4;
    g_graphics.bpp = 32;
    g_graphics.framebuffer = (void *)g_graphics.gop->Mode->FrameBufferBase;
    g_graphics.framebuffer_size = g_graphics.gop->Mode->FrameBufferSize;

    switch (g_graphics.mode_info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor:
            g_graphics.format = 0;  /* RGB */
            break;
        case PixelBlueGreenRedReserved8BitPerColor:
            g_graphics.format = 1;  /* BGR */
            break;
        case PixelBitMask:
            g_graphics.format = 2;  /* Custom */
            break;
        default:
            g_graphics.format = 0;
            break;
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * Get Graphics Info
 * ============================================================================ */

uint32_t graphics_get_width(void) { return g_graphics.width; }
uint32_t graphics_get_height(void) { return g_graphics.height; }
uint32_t graphics_get_pitch(void) { return g_graphics.pitch; }
void *graphics_get_framebuffer(void) { return g_graphics.framebuffer; }

/* ============================================================================
 * Pixel Operations
 * ============================================================================ */

static uint32_t graphics_make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    switch (g_graphics.format) {
        case 0:  /* RGB */
            return (r << 16) | (g << 8) | b | (a << 24);
        case 1:  /* BGR */
            return (b << 16) | (g << 8) | r | (a << 24);
        default:
            return (r << 16) | (g << 8) | b | (a << 24);
    }
}

void graphics_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_graphics.width || y >= g_graphics.height) return;
    uint32_t *fb = (uint32_t *)g_graphics.framebuffer;
    fb[y * (g_graphics.pitch / 4) + x] = color;
}

uint32_t graphics_get_pixel(uint32_t x, uint32_t y) {
    if (x >= g_graphics.width || y >= g_graphics.height) return 0;
    uint32_t *fb = (uint32_t *)g_graphics.framebuffer;
    return fb[y * (g_graphics.pitch / 4) + x];
}

void graphics_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t dy = 0; dy < h; dy++) {
        for (uint32_t dx = 0; dx < w; dx++) {
            graphics_set_pixel(x + dx, y + dy, color);
        }
    }
}

void graphics_clear(uint32_t color) {
    graphics_fill_rect(0, 0, g_graphics.width, g_graphics.height, color);
}

/* ============================================================================
 * Line Drawing (Bresenham)
 * ============================================================================ */

void graphics_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        graphics_set_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ============================================================================
 * Rectangle Drawing
 * ============================================================================ */

void graphics_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    graphics_draw_line(x, y, x + w - 1, y, color);
    graphics_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    graphics_draw_line(x, y, x, y + h - 1, color);
    graphics_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

/* ============================================================================
 * Text Rendering (Simple 8x16 font)
 * ============================================================================ */

static const uint8_t font_8x16[256][16] = {
    /* Space */
    [' '] = {0},
    /* Basic ASCII - simplified, would need full font data */
};

void graphics_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if (c < 32 || c >= 127) c = '?';

    /* Simple 8x16 bitmap font - placeholder */
    /* In reality, would use a proper font bitmap */
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            graphics_set_pixel(x + dx, y + dy, bg);
        }
    }
}

void graphics_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        graphics_draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

void graphics_draw_wstring(uint32_t x, uint32_t y, const CHAR16 *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        if (*str < 128) {
            graphics_draw_char(x, y, (char)*str, fg, bg);
        }
        x += 8;
        str++;
    }
}

/* ============================================================================
 * Measure Text
 * ============================================================================ */

uint32_t graphics_measure_string(const char *str) {
    return strlen(str) * 8;
}

uint32_t graphics_measure_wstring(const CHAR16 *str) {
    return wcslen(str) * 8;
}

uint32_t graphics_get_font_height(void) {
    return 16;
}

/* ============================================================================
 * Blit / Copy
 * ============================================================================ */

void graphics_blit(void *src, uint32_t src_pitch, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t *dst = (uint32_t *)g_graphics.framebuffer;
    uint32_t *src_ptr = (uint32_t *)src;
    uint32_t dst_pitch = g_graphics.pitch / 4;

    for (uint32_t dy = 0; dy < h; dy++) {
        memcpy(&dst[(y + dy) * dst_pitch + x], &src_ptr[dy * (src_pitch / 4)], w * 4);
    }
}

/* ============================================================================
 * Mode Setting
 * ============================================================================ */

EFI_STATUS graphics_set_mode(uint32_t mode) {
    return g_graphics.gop->SetMode(g_graphics.gop, mode);
}

uint32_t graphics_get_mode_count(void) {
    return g_graphics.gop->Mode->MaxMode;
}

EFI_STATUS graphics_query_mode(uint32_t mode, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **info) {
    UINTN size;
    return g_graphics.gop->QueryMode(g_graphics.gop, mode, &size, info);
}