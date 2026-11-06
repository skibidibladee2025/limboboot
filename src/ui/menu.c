/*
 * Boot Menu Implementation
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"
#include "memory.c"
#include "efi.c"
#include "printf.c"
#include "config.c"
#include "graphics.c"
#include "theme.c"
#include "input.c"

/* ============================================================================
 * Menu State
 * ============================================================================ */

typedef struct {
    config_entry_t *entries;
    int entry_count;
    int selected;
    int scroll_offset;
    int timeout;
    uint64_t timeout_start;
    int timeout_expired;
    int graphics_mode;
} menu_t;

static menu_t g_menu;

/* ============================================================================
 * Initialize Menu
 * ============================================================================ */

EFI_STATUS menu_init(void) {
    config_t *config = config_get();
    if (!config || config->entry_count == 0) {
        return EFI_NOT_FOUND;
    }

    g_menu.entries = config->entries;
    g_menu.entry_count = config->entry_count;
    g_menu.selected = config->default_entry;
    if (g_menu.selected < 0 || g_menu.selected >= g_menu.entry_count) {
        g_menu.selected = 0;
    }
    g_menu.scroll_offset = 0;
    g_menu.timeout = config->timeout;
    g_menu.timeout_expired = 0;
    g_menu.graphics_mode = !config->text_mode;

    /* Initialize graphics if not text mode */
    if (g_menu.graphics_mode) {
        EFI_STATUS status = graphics_init();
        if (EFI_ERROR(status)) {
            g_menu.graphics_mode = 0;
        }
    }

    /* Initialize theme */
    theme_set_by_name(config->theme);

    /* Initialize input */
    input_init();

    /* Record timeout start */
    EFI_TIME time;
    g_rs->GetTime(&time, NULL);
    g_menu.timeout_start = time.Second + time.Minute * 60 + time.Hour * 3600;

    return EFI_SUCCESS;
}

/* ============================================================================
 * Draw Menu (Graphics Mode)
 * ============================================================================ */

static void menu_draw_graphics(void) {
    theme_draw_background();
    theme_draw_title("limoboot");

    uint32_t w = graphics_get_width();
    uint32_t h = graphics_get_height();
    uint32_t entry_h = graphics_get_font_height() + theme_get_current()->padding * 2;
    uint32_t visible_entries = (h - 100) / (entry_h + theme_get_current()->spacing);

    /* Adjust scroll offset */
    if (g_menu.selected < g_menu.scroll_offset) {
        g_menu.scroll_offset = g_menu.selected;
    } else if (g_menu.selected >= g_menu.scroll_offset + visible_entries) {
        g_menu.scroll_offset = g_menu.selected - visible_entries + 1;
    }

    /* Draw entries */
    for (int i = 0; i < visible_entries; i++) {
        int idx = g_menu.scroll_offset + i;
        if (idx >= g_menu.entry_count) break;

        theme_draw_entry(i, g_menu.entries[idx].name, idx == g_menu.selected);
    }

    /* Draw help text */
    theme_draw_help("Use UP/DOWN to select, ENTER to boot, ESC for command line");

    /* Draw timeout */
    if (g_menu.timeout > 0) {
        EFI_TIME time;
        g_rs->GetTime(&time, NULL);
        uint64_t now = time.Second + time.Minute * 60 + time.Hour * 3600;
        int elapsed = (int)(now - g_menu.timeout_start);
        if (elapsed < 0) elapsed += 86400;  /* Handle midnight rollover */

        int remaining = g_menu.timeout - elapsed;
        if (remaining <= 0) {
            g_menu.timeout_expired = 1;
            remaining = 0;
        }
        theme_draw_timeout(remaining);
    }
}

/* ============================================================================
 * Draw Menu (Text Mode)
 * ============================================================================ */

static void menu_draw_text(void) {
    g_st->ConOut->ClearScreen(g_st->ConOut);
    g_st->ConOut->SetAttribute(g_st->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));

    g_st->ConOut->SetCursorPosition(g_st->ConOut, 0, 0);
    g_st->ConOut->OutputString(g_st->ConOut, L"limoboot - Boot Menu\n\n");

    for (int i = 0; i < g_menu.entry_count; i++) {
        CHAR16 prefix[4] = { i == g_menu.selected ? L'>' : L' ', L' ', L' ', 0 };
        CHAR16 name[256];
        ascii_to_unicode(g_menu.entries[i].name, name, 256);

        if (i == g_menu.selected) {
            g_st->ConOut->SetAttribute(g_st->ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
        } else {
            g_st->ConOut->SetAttribute(g_st->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        }

        g_st->ConOut->OutputString(g_st->ConOut, prefix);
        g_st->ConOut->OutputString(g_st->ConOut, name);
        g_st->ConOut->OutputString(g_st->ConOut, L"\n");
    }

    g_st->ConOut->SetAttribute(g_st->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLUE));
    g_st->ConOut->OutputString(g_st->ConOut, L"\nUse UP/DOWN to select, ENTER to boot, ESC for command line\n");

    if (g_menu.timeout > 0) {
        EFI_TIME time;
        g_rs->GetTime(&time, NULL);
        uint64_t now = time.Second + time.Minute * 60 + time.Hour * 3600;
        int elapsed = (int)(now - g_menu.timeout_start);
        if (elapsed < 0) elapsed += 86400;

        int remaining = g_menu.timeout - elapsed;
        if (remaining <= 0) {
            g_menu.timeout_expired = 1;
            remaining = 0;
        }

        CHAR16 buf[64];
        snprintf((char *)buf, sizeof(buf), L"Booting in %d seconds...\r", remaining);
        g_st->ConOut->OutputString(g_st->ConOut, buf);
    }
}

/* ============================================================================
 * Draw Menu
 * ============================================================================ */

void menu_draw(void) {
    if (g_menu.graphics_mode) {
        menu_draw_graphics();
    } else {
        menu_draw_text();
    }
}

/* ============================================================================
 * Handle Input
 * ============================================================================ */

int menu_handle_input(void) {
    int timeout_ms = g_menu.timeout > 0 ? 1000 : 0;
    int code = input_wait_key(timeout_ms);

    switch (code) {
        case KEY_UP:
            if (g_menu.selected > 0) g_menu.selected--;
            break;
        case KEY_DOWN:
            if (g_menu.selected < g_menu.entry_count - 1) g_menu.selected++;
            break;
        case KEY_HOME:
            g_menu.selected = 0;
            break;
        case KEY_END:
            g_menu.selected = g_menu.entry_count - 1;
            break;
        case KEY_PGUP:
            g_menu.selected = (g_menu.selected > 10) ? g_menu.selected - 10 : 0;
            break;
        case KEY_PGDN:
            g_menu.selected = (g_menu.selected + 10 < g_menu.entry_count) ?
                             g_menu.selected + 10 : g_menu.entry_count - 1;
            break;
        case KEY_ENTER:
            return 1;  /* Boot selected entry */
        case KEY_ESC:
            return -1;  /* Exit to command line */
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            if (code - '1' < g_menu.entry_count) {
                g_menu.selected = code - '1';
                return 1;
            }
            break;
        case 0:
            /* Timeout */
            if (g_menu.timeout_expired) {
                return 1;  /* Boot default */
            }
            break;
    }

    return 0;  /* Continue menu */
}

/* ============================================================================
 * Run Menu
 * ============================================================================ */

config_entry_t *menu_run(void) {
    EFI_STATUS status = menu_init();
    if (EFI_ERROR(status)) {
        return config_get_default_entry();
    }

    while (1) {
        menu_draw();

        int result = menu_handle_input();
        if (result == 1) {
            /* Boot selected entry */
            return &g_menu.entries[g_menu.selected];
        } else if (result == -1) {
            /* Exit to command line */
            return NULL;
        }
    }
}

/* ============================================================================
 * Get Selected Entry
 * ============================================================================ */

config_entry_t *menu_get_selected(void) {
    if (g_menu.selected >= 0 && g_menu.selected < g_menu.entry_count) {
        return &g_menu.entries[g_menu.selected];
    }
    return NULL;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void menu_free(void) {
    input_free();
}