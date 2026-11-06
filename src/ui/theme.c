/*
 * Theme Engine for Boot Menu
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"
#include "memory.c"
#include "efi.c"
#include "printf.c"
#include "graphics.c"

/* ============================================================================
 * Theme Colors
 * ============================================================================ */

typedef struct {
    uint32_t background;
    uint32_t foreground;
    uint32_t selection_bg;
    uint32_t selection_fg;
    uint32_t border;
    uint32_t title_bg;
    uint32_t title_fg;
    uint32_t help_bg;
    uint32_t help_fg;
    uint32_t timeout_bg;
    uint32_t timeout_fg;
    uint32_t progress_bg;
    uint32_t progress_fg;
    uint32_t progress_bar;
} theme_colors_t;

typedef struct {
    const char *name;
    theme_colors_t colors;
    uint32_t font_size;
    uint32_t padding;
    uint32_t spacing;
    uint32_t border_width;
    uint32_t corner_radius;
    int use_background_image;
    char background_image_path[256];
} theme_t;

/* ============================================================================
 * Default Themes
 * ============================================================================ */

static theme_t g_themes[] = {
    {
        .name = "default",
        .colors = {
            .background = 0xFF1E1E2E,
            .foreground = 0xFFCDD6F4,
            .selection_bg = 0xFF89B4FA,
            .selection_fg = 0xFF1E1E2E,
            .border = 0xFF6C7086,
            .title_bg = 0xFF1E1E2E,
            .title_fg = 0xFF89B4FA,
            .help_bg = 0xFF313244,
            .help_fg = 0xFFBAC2DE,
            .timeout_bg = 0xFF313244,
            .timeout_fg = 0xFFFAB387,
            .progress_bg = 0xFF313244,
            .progress_fg = 0xFFA6E3A1,
            .progress_bar = 0xFF89B4FA,
        },
        .font_size = 16,
        .padding = 20,
        .spacing = 10,
        .border_width = 2,
        .corner_radius = 8,
        .use_background_image = 0,
    },
    {
        .name = "dark",
        .colors = {
            .background = 0xFF000000,
            .foreground = 0xFFFFFFFF,
            .selection_bg = 0xFF444444,
            .selection_fg = 0xFFFFFFFF,
            .border = 0xFF666666,
            .title_bg = 0xFF000000,
            .title_fg = 0xFF00FF00,
            .help_bg = 0xFF111111,
            .help_fg = 0xFF888888,
            .timeout_bg = 0xFF111111,
            .timeout_fg = 0xFFFFFF00,
            .progress_bg = 0xFF111111,
            .progress_fg = 0xFF00FF00,
            .progress_bar = 0xFF00AA00,
        },
        .font_size = 16,
        .padding = 10,
        .spacing = 5,
        .border_width = 1,
        .corner_radius = 0,
        .use_background_image = 0,
    },
    {
        .name = "light",
        .colors = {
            .background = 0xFFFFFFFF,
            .foreground = 0xFF000000,
            .selection_bg = 0xFF007ACC,
            .selection_fg = 0xFFFFFFFF,
            .border = 0xFFCCCCCC,
            .title_bg = 0xFFFFFFFF,
            .title_fg = 0xFF007ACC,
            .help_bg = 0xFFF0F0F0,
            .help_fg = 0xFF666666,
            .timeout_bg = 0xFFF0F0F0,
            .timeout_fg = 0xFFCC0000,
            .progress_bg = 0xFFF0F0F0,
            .progress_fg = 0xFF007ACC,
            .progress_bar = 0xFF005599,
        },
        .font_size = 16,
        .padding = 20,
        .spacing = 10,
        .border_width = 2,
        .corner_radius = 8,
        .use_background_image = 0,
    },
    {
        .name = "minimal",
        .colors = {
            .background = 0xFF000000,
            .foreground = 0xFF00FF00,
            .selection_bg = 0xFF003300,
            .selection_fg = 0xFF00FF00,
            .border = 0xFF00FF00,
            .title_bg = 0xFF000000,
            .title_fg = 0xFF00FF00,
            .help_bg = 0xFF000000,
            .help_fg = 0xFF008800,
            .timeout_bg = 0xFF000000,
            .timeout_fg = 0xFF00AA00,
            .progress_bg = 0xFF000000,
            .progress_fg = 0xFF00FF00,
            .progress_bar = 0xFF00FF00,
        },
        .font_size = 16,
        .padding = 5,
        .spacing = 2,
        .border_width = 1,
        .corner_radius = 0,
        .use_background_image = 0,
    },
};

#define NUM_THEMES (sizeof(g_themes) / sizeof(g_themes[0]))

static theme_t *g_current_theme = &g_themes[0];

/* ============================================================================
 * Theme Selection
 * ============================================================================ */

theme_t *theme_get_current(void) {
    return g_current_theme;
}

EFI_STATUS theme_set_by_name(const char *name) {
    for (size_t i = 0; i < NUM_THEMES; i++) {
        if (strcmp(g_themes[i].name, name) == 0) {
            g_current_theme = &g_themes[i];
            return EFI_SUCCESS;
        }
    }
    return EFI_NOT_FOUND;
}

const char *theme_get_name(void) {
    return g_current_theme->name;
}

/* ============================================================================
 * Theme Drawing Helpers
 * ============================================================================ */

void theme_draw_background(void) {
    graphics_clear(g_current_theme->colors.background);
}

void theme_draw_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    theme_colors_t *c = &g_current_theme->colors;
    uint32_t r = g_current_theme->corner_radius;
    uint32_t bw = g_current_theme->border_width;

    if (r > 0) {
        /* Rounded rectangle - simplified */
        graphics_fill_rect(x + r, y, w - 2 * r, h, c->background);
        graphics_fill_rect(x, y + r, r, h - 2 * r, c->background);
        graphics_fill_rect(x + w - r, y + r, r, h - 2 * r, c->background);
        /* Corners would need circle drawing */
    } else {
        graphics_fill_rect(x, y, w, h, c->background);
    }

    if (bw > 0) {
        graphics_draw_rect(x, y, w, h, c->border);
    }
}

void theme_draw_title(const char *title) {
    theme_colors_t *c = &g_current_theme->colors;
    uint32_t w = graphics_get_width();
    uint32_t h = graphics_get_font_height() + g_current_theme->padding * 2;

    graphics_fill_rect(0, 0, w, h, c->title_bg);
    graphics_draw_rect(0, 0, w, h, c->border);

    uint32_t title_w = graphics_measure_string(title);
    uint32_t x = (w - title_w) / 2;
    uint32_t y = g_current_theme->padding;
    graphics_draw_string(x, y, title, c->title_fg, c->title_bg);
}

void theme_draw_entry(uint32_t index, const char *text, int selected) {
    theme_colors_t *c = &g_current_theme->colors;
    uint32_t w = graphics_get_width();
    uint32_t entry_h = graphics_get_font_height() + g_current_theme->padding * 2;
    uint32_t y = 60 + index * (entry_h + g_current_theme->spacing);
    uint32_t x = g_current_theme->padding;
    uint32_t entry_w = w - 2 * g_current_theme->padding;

    if (selected) {
        graphics_fill_rect(x, y, entry_w, entry_h, c->selection_bg);
        graphics_draw_rect(x, y, entry_w, entry_h, c->border);
        graphics_draw_string(x + g_current_theme->padding, y + g_current_theme->padding,
                             text, c->selection_fg, c->selection_bg);
    } else {
        graphics_fill_rect(x, y, entry_w, entry_h, c->background);
        graphics_draw_string(x + g_current_theme->padding, y + g_current_theme->padding,
                             text, c->foreground, c->background);
    }
}

void theme_draw_help(const char *text) {
    theme_colors_t *c = &g_current_theme->colors;
    uint32_t w = graphics_get_width();
    uint32_t h = graphics_get_height();
    uint32_t help_h = graphics_get_font_height() + g_current_theme->padding * 2;
    uint32_t y = h - help_h;

    graphics_fill_rect(0, y, w, help_h, c->help_bg);
    graphics_draw_rect(0, y, w, help_h, c->border);
    graphics_draw_string(g_current_theme->padding, y + g_current_theme->padding,
                         text, c->help_fg, c->help_bg);
}

void theme_draw_timeout(int seconds) {
    theme_colors_t *c = &g_current_theme->colors;
    uint32_t w = graphics_get_width();
    uint32_t h = graphics_get_height();
    uint32_t timeout_h = graphics_get_font_height() + g_current_theme->padding * 2;
    uint32_t y = h - timeout_h - 40;

    char buf[64];
    snprintf(buf, sizeof(buf), "Booting in %d seconds...", seconds);

    uint32_t text_w = graphics_measure_string(buf);
    uint32_t x = (w - text_w) / 2;

    graphics_fill_rect(x - g_current_theme->padding, y, text_w + 2 * g_current_theme->padding, timeout_h, c->timeout_bg);
    graphics_draw_string(x, y + g_current_theme->padding, buf, c->timeout_fg, c->timeout_bg);
}

void theme_draw_progress(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int percent) {
    theme_colors_t *c = &g_current_theme->colors;

    graphics_fill_rect(x, y, w, h, c->progress_bg);
    graphics_draw_rect(x, y, w, h, c->border);

    uint32_t fill_w = (w - 4) * percent / 100;
    if (fill_w > 0) {
        graphics_fill_rect(x + 2, y + 2, fill_w, h - 4, c->progress_bar);
    }
}