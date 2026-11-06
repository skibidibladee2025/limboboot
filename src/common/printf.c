/*
 * Minimal printf implementation for freestanding environment
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <stdarg.h>

/* Forward declaration for output function */
typedef void (*putc_fn)(char c, void *ctx);

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void putc_wrapper(char c, void *ctx) {
    putc_fn fn = (putc_fn)ctx;
    fn(c, (void *)((uintptr_t)ctx + sizeof(void *)));
}

static void print_string(putc_fn putc, void *ctx, const char *s) {
    while (*s) {
        putc(*s++, ctx);
    }
}

static void print_wstring(putc_fn putc, void *ctx, const CHAR16 *s) {
    while (*s) {
        if (*s <= 0x7F) {
            putc((char)*s, ctx);
        } else {
            putc('?', ctx);
        }
        s++;
    }
}

static void print_int(putc_fn putc, void *ctx, uint64_t val, int base, int width, char pad, int uppercase) {
    char buf[32];
    int i = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val) {
            buf[i++] = digits[val % base];
            val /= base;
        }
    }

    while (i < width) {
        putc(pad, ctx);
        width--;
    }

    while (i--) {
        putc(buf[i], ctx);
    }
}

static void print_ptr(putc_fn putc, void *ctx, uintptr_t val) {
    putc('0', ctx);
    putc('x', ctx);
    print_int(putc, ctx, val, 16, 2 * sizeof(uintptr_t), '0', 0);
}

/* ============================================================================
 * Core vsnprintf
 * ============================================================================ */

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t written = 0;

    auto putc = ^(char c) {
        if (written < size - 1) {
            buf[written++] = c;
        }
    };

    while (*fmt) {
        if (*fmt != '%') {
            putc(*fmt++);
            continue;
        }

        fmt++;  // Skip '%'

        // Flags
        int left_align = 0;
        char pad = ' ';
        if (*fmt == '-') {
            left_align = 1;
            pad = ' ';
            fmt++;
        } else if (*fmt == '0') {
            pad = '0';
            fmt++;
        }

        // Width
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt++ - '0');
        }

        // Length modifier
        int long_long = 0;
        if (*fmt == 'l' && *(fmt + 1) == 'l') {
            long_long = 1;
            fmt += 2;
        } else if (*fmt == 'l') {
            fmt++;
        } else if (*fmt == 'z') {
            fmt++;
        } else if (*fmt == 'j') {
            fmt++;
        } else if (*fmt == 't') {
            fmt++;
        }

        // Specifier
        switch (*fmt++) {
            case 'c': {
                int c = va_arg(ap, int);
                putc((char)c);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                if (!left_align) {
                    int len = strlen(s);
                    while (len < width--) putc(pad);
                }
                print_string((putc_fn)putc, NULL, s);
                if (left_align) {
                    int len = strlen(s);
                    while (len < width--) putc(' ');
                }
                break;
            }
            case 'S': {
                const CHAR16 *s = va_arg(ap, const CHAR16 *);
                if (!s) s = L"(null)";
                print_wstring((putc_fn)putc, NULL, s);
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = long_long ? va_arg(ap, int64_t) : va_arg(ap, int);
                if (val < 0) {
                    putc('-');
                    val = -val;
                    width--;
                }
                print_int((putc_fn)putc, NULL, val, 10, width, pad, 0);
                break;
            }
            case 'u': {
                uint64_t val = long_long ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
                print_int((putc_fn)putc, NULL, val, 10, width, pad, 0);
                break;
            }
            case 'x': {
                uint64_t val = long_long ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
                print_int((putc_fn)putc, NULL, val, 16, width, pad, 0);
                break;
            }
            case 'X': {
                uint64_t val = long_long ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
                print_int((putc_fn)putc, NULL, val, 16, width, pad, 1);
                break;
            }
            case 'p': {
                uintptr_t val = va_arg(ap, uintptr_t);
                print_ptr((putc_fn)putc, NULL, val);
                break;
            }
            case '%':
                putc('%');
                break;
            default:
                putc('%');
                putc(fmt[-1]);
                break;
        }
    }

    buf[written] = '\0';
    return (int)written;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

/* ============================================================================
 * EFI Console Output
 * ============================================================================ */

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *g_conout = NULL;

void efi_init_console(EFI_SYSTEM_TABLE *st) {
    g_conout = st->ConOut;
}

static void efi_putc(char c, void *ctx) {
    (void)ctx;
    if (g_conout) {
        CHAR16 wc = (CHAR16)c;
        if (c == '\n') {
            CHAR16 crlf[2] = { '\r', '\n' };
            g_conout->OutputString(g_conout, crlf);
        } else {
            CHAR16 wcs[2] = { wc, 0 };
            g_conout->OutputString(g_conout, wcs);
        }
    }
}

void efi_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    for (char *p = buf; *p; p++) {
        efi_putc(*p, NULL);
    }
}

void efi_vprintf(const char *fmt, va_list ap) {
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);

    for (char *p = buf; *p; p++) {
        efi_putc(*p, NULL);
    }
}

/* ============================================================================
 * Debug Output (to serial if available)
 * ============================================================================ */

static EFI_SERIAL_IO_PROTOCOL *g_serial = NULL;

void efi_init_serial(EFI_HANDLE handle, EFI_SYSTEM_TABLE *st) {
    EFI_GUID serial_guid = EFI_SERIAL_IO_PROTOCOL_GUID;
    st->BootServices->HandleProtocol(handle, &serial_guid, (VOID **)&g_serial);
    if (g_serial) {
        g_serial->SetAttributes(g_serial, 115200, 0, 0, 8, 1);
    }
}

static void serial_putc(char c, void *ctx) {
    (void)ctx;
    if (g_serial) {
        g_serial->Write(g_serial, 1, &c);
    }
}

void serial_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    for (char *p = buf; *p; p++) {
        serial_putc(*p, NULL);
    }
}