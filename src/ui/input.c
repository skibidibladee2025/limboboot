/*
 * Input Handling for Boot Menu
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
 * Input State
 * ============================================================================ */

typedef struct {
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *con_in;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *con_in_ex;
    EFI_EVENT timer_event;
    int key_pending;
    EFI_KEY_DATA last_key;
} input_t;

static input_t g_input;

/* ============================================================================
 * Initialize Input
 * ============================================================================ */

EFI_STATUS input_init(void) {
    g_input.con_in = g_st->ConIn;
    if (!g_input.con_in) return EFI_UNSUPPORTED;

    /* Try to get extended input protocol for better key handling */
    EFI_GUID ex_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    g_bs->HandleProtocol(g_st->ConsoleInHandle, &ex_guid, (VOID **)&g_input.con_in_ex);

    /* Create timer for polling */
    EFI_STATUS status = g_bs->CreateEvent(EVT_TIMER, TPL_NOTIFY, NULL, NULL, &g_input.timer_event);
    if (!EFI_ERROR(status)) {
        g_bs->SetTimer(g_input.timer_event, TimerPeriodic, 500000);  /* 50ms */
    }

    g_input.key_pending = 0;
    return EFI_SUCCESS;
}

/* ============================================================================
 * Key Codes
 * ============================================================================ */

#define KEY_UP      0x0001
#define KEY_DOWN    0x0002
#define KEY_LEFT    0x0003
#define KEY_RIGHT   0x0004
#define KEY_ENTER   0x0005
#define KEY_ESC     0x0006
#define KEY_TAB     0x0007
#define KEY_HOME    0x0008
#define KEY_END     0x0009
#define KEY_PGUP    0x000A
#define KEY_PGDN    0x000B
#define KEY_INS     0x000C
#define KEY_DEL     0x000D
#define KEY_F1      0x0010
#define KEY_F2      0x0011
#define KEY_F3      0x0012
#define KEY_F4      0x0013
#define KEY_F5      0x0014
#define KEY_F6      0x0015
#define KEY_F7      0x0016
#define KEY_F8      0x0017
#define KEY_F9      0x0018
#define KEY_F10     0x0019
#define KEY_F11     0x001A
#define KEY_F12     0x001B

/* ============================================================================
 * Translate EFI Key to Internal Code
 * ============================================================================ */

static uint16_t translate_key(EFI_KEY_DATA *key) {
    if (key->Key.ScanCode) {
        switch (key->Key.ScanCode) {
            case SCAN_UP:    return KEY_UP;
            case SCAN_DOWN:  return KEY_DOWN;
            case SCAN_LEFT:  return KEY_LEFT;
            case SCAN_RIGHT: return KEY_RIGHT;
            case SCAN_HOME:  return KEY_HOME;
            case SCAN_END:   return KEY_END;
            case SCAN_PGUP:  return KEY_PGUP;
            case SCAN_PGDN:  return KEY_PGDN;
            case SCAN_INS:   return KEY_INS;
            case SCAN_DEL:   return KEY_DEL;
            case SCAN_F1:    return KEY_F1;
            case SCAN_F2:    return KEY_F2;
            case SCAN_F3:    return KEY_F3;
            case SCAN_F4:    return KEY_F4;
            case SCAN_F5:    return KEY_F5;
            case SCAN_F6:    return KEY_F6;
            case SCAN_F7:    return KEY_F7;
            case SCAN_F8:    return KEY_F8;
            case SCAN_F9:    return KEY_F9;
            case SCAN_F10:   return KEY_F10;
            case SCAN_F11:   return KEY_F11;
            case SCAN_F12:   return KEY_F12;
            default:         return 0;
        }
    }

    switch (key->Key.UnicodeChar) {
        case '\r': return KEY_ENTER;
        case '\n': return KEY_ENTER;
        case '\t': return KEY_TAB;
        case 0x1B: return KEY_ESC;  /* ESC */
        case 0x08: return KEY_DEL;  /* Backspace */
        default:
            if (key->Key.UnicodeChar >= ' ' && key->Key.UnicodeChar <= '~') {
                return key->Key.UnicodeChar;
            }
            return 0;
    }
}

/* ============================================================================
 * Poll for Key
 * ============================================================================ */

int input_poll_key(void) {
    if (!g_input.con_in) return 0;

    EFI_KEY_DATA key_data;
    EFI_STATUS status;

    if (g_input.con_in_ex) {
        status = g_input.con_in_ex->ReadKeyStrokeEx(g_input.con_in_ex, &key_data);
    } else {
        EFI_INPUT_KEY key;
        status = g_input.con_in->ReadKeyStroke(g_input.con_in, &key);
        if (!EFI_ERROR(status)) {
            key_data.Key = key;
            key_data.KeyState.KeyShiftState = 0;
            key_data.KeyState.KeyToggleState = 0;
        }
    }

    if (EFI_ERROR(status)) return 0;

    uint16_t code = translate_key(&key_data);
    if (code) {
        g_input.last_key = key_data;
        return code;
    }

    return 0;
}

/* ============================================================================
 * Wait for Key (with timeout)
 * ============================================================================ */

int input_wait_key(UINTN timeout_ms) {
    if (!g_input.con_in) return 0;

    EFI_EVENT events[2] = { g_input.con_in->WaitForKey, g_input.timer_event };
    UINTN index;
    UINT64 timer = 0;

    if (timeout_ms > 0) {
        g_bs->SetTimer(g_input.timer_event, TimerRelative, timeout_ms * 10000);
    } else {
        g_bs->SetTimer(g_input.timer_event, TimerPeriodic, 500000);
    }

    while (1) {
        EFI_STATUS status = g_bs->WaitForEvent(2, events, &index);
        if (EFI_ERROR(status)) break;

        if (index == 0) {
            /* Key pressed */
            int code = input_poll_key();
            if (code) return code;
        } else if (timeout_ms > 0) {
            /* Timeout */
            return 0;
        }
    }

    return 0;
}

/* ============================================================================
 * Check if Key Pending (non-blocking)
 * ============================================================================ */

int input_key_pending(void) {
    return input_poll_key() != 0;
}

/* ============================================================================
 * Get Last Key Unicode Char
 * ============================================================================ */

CHAR16 input_get_last_char(void) {
    return g_input.last_key.Key.UnicodeChar;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void input_free(void) {
    if (g_input.timer_event) {
        g_bs->CloseEvent(g_input.timer_event);
        g_input.timer_event = NULL;
    }
}