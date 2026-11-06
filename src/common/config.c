/*
 * Configuration Parser for limoboot
 * Simple INI-like format with nested entries
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include "string.c"
#include "memory.c"

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

#define MAX_CONFIG_ENTRIES  64
#define MAX_ENTRY_NAME      128
#define MAX_STRING_VALUE    1024
#define MAX_CMDLINE         4096
#define MAX_MODULES         16

typedef enum {
    CONFIG_TYPE_GLOBAL,
    CONFIG_TYPE_ENTRY
} config_section_type_t;

typedef struct {
    char key[64];
    char value[MAX_STRING_VALUE];
} config_kv_t;

typedef struct {
    char name[MAX_ENTRY_NAME];
    char protocol[32];           // "limoboot", "linux", "multiboot2", "chainload"
    char kernel[MAX_STRING_VALUE];
    char cmdline[MAX_CMDLINE];
    char module_paths[MAX_MODULES][MAX_STRING_VALUE];
    int module_count;
    config_kv_t extra[MAX_MODULES];
    int extra_count;
} config_entry_t;

typedef struct {
    // Global settings
    int timeout;                 // Menu timeout in seconds (-1 = infinite)
    int default_entry;           // Default entry index
    char theme[MAX_STRING_VALUE]; // Theme name
    int text_mode;               // Force text mode
    int log_level;               // 0=none, 1=error, 2=warn, 3=info, 4=debug
    int secure_boot;             // Require Secure Boot
    int measured_boot;           // Enable measured boot

    // Entries
    config_entry_t entries[MAX_CONFIG_ENTRIES];
    int entry_count;
} config_t;

static config_t g_config;

/* ============================================================================
 * Parser State
 * ============================================================================ */

typedef struct {
    const char *data;
    size_t size;
    size_t pos;
    int line;
} parser_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void parser_init(parser_t *p, const char *data, size_t size) {
    p->data = data;
    p->size = size;
    p->pos = 0;
    p->line = 1;
}

static int parser_eof(parser_t *p) {
    return p->pos >= p->size;
}

static char parser_peek(parser_t *p) {
    if (parser_eof(p)) return '\0';
    return p->data[p->pos];
}

static char parser_next(parser_t *p) {
    if (parser_eof(p)) return '\0';
    char c = p->data[p->pos++];
    if (c == '\n') p->line++;
    return c;
}

static void parser_skip_whitespace(parser_t *p) {
    while (!parser_eof(p)) {
        char c = parser_peek(p);
        if (c == ' ' || c == '\t' || c == '\r') {
            parser_next(p);
        } else break;
    }
}

static void parser_skip_line(parser_t *p) {
    while (!parser_eof(p)) {
        char c = parser_next(p);
        if (c == '\n') break;
    }
}

static int parser_read_identifier(parser_t *p, char *buf, size_t maxlen) {
    parser_skip_whitespace(p);
    size_t i = 0;
    while (!parser_eof(p) && i < maxlen - 1) {
        char c = parser_peek(p);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') {
            buf[i++] = parser_next(p);
        } else break;
    }
    buf[i] = '\0';
    return i > 0;
}

static int parser_read_string(parser_t *p, char *buf, size_t maxlen) {
    parser_skip_whitespace(p);
    if (parser_peek(p) != '"') return 0;
    parser_next(p);  // Skip opening quote

    size_t i = 0;
    while (!parser_eof(p) && i < maxlen - 1) {
        char c = parser_next(p);
        if (c == '"') {
            buf[i] = '\0';
            return 1;
        } else if (c == '\\') {
            c = parser_next(p);
            switch (c) {
                case 'n': buf[i++] = '\n'; break;
                case 't': buf[i++] = '\t'; break;
                case 'r': buf[i++] = '\r'; break;
                case '\\': buf[i++] = '\\'; break;
                case '"': buf[i++] = '"'; break;
                default: buf[i++] = c; break;
            }
        } else {
            buf[i++] = c;
        }
    }
    return 0;
}

static int parser_read_unquoted(parser_t *p, char *buf, size_t maxlen) {
    parser_skip_whitespace(p);
    size_t i = 0;
    while (!parser_eof(p) && i < maxlen - 1) {
        char c = parser_peek(p);
        if (c == '\n' || c == '\r' || c == '#' || c == ';' || c == '}') break;
        if (c == ' ' || c == '\t') break;
        buf[i++] = parser_next(p);
    }
    // Trim trailing whitespace
    while (i > 0 && (buf[i-1] == ' ' || buf[i-1] == '\t')) i--;
    buf[i] = '\0';
    return i > 0;
}

/* ============================================================================
 * Parse Entry Block
 * ============================================================================ */

static EFI_STATUS parse_entry(parser_t *p, config_entry_t *entry) {
    // Expect '{'
    parser_skip_whitespace(p);
    if (parser_peek(p) != '{') return EFI_INVALID_PARAMETER;
    parser_next(p);

    while (!parser_eof(p)) {
        parser_skip_whitespace(p);
        char c = parser_peek(p);
        if (c == '}') {
            parser_next(p);
            break;
        }
        if (c == '#' || c == ';') {
            parser_skip_line(p);
            continue;
        }

        char key[64];
        if (!parser_read_identifier(p, key, sizeof(key))) {
            parser_skip_line(p);
            continue;
        }

        parser_skip_whitespace(p);
        if (parser_peek(p) != '=') {
            parser_skip_line(p);
            continue;
        }
        parser_next(p);  // Skip '='

        char value[MAX_STRING_VALUE];
        if (!parser_read_string(p, value, sizeof(value))) {
            parser_read_unquoted(p, value, sizeof(value));
        }

        // Process key-value
        if (strcmp(key, "protocol") == 0) {
            strncpy(entry->protocol, value, sizeof(entry->protocol) - 1);
        } else if (strcmp(key, "kernel") == 0) {
            strncpy(entry->kernel, value, sizeof(entry->kernel) - 1);
        } else if (strcmp(key, "cmdline") == 0) {
            strncpy(entry->cmdline, value, sizeof(entry->cmdline) - 1);
        } else if (strcmp(key, "module") == 0) {
            if (entry->module_count < MAX_MODULES) {
                strncpy(entry->module_paths[entry->module_count++], value, MAX_STRING_VALUE - 1);
            }
        } else {
            // Extra key-value
            if (entry->extra_count < MAX_MODULES) {
                strncpy(entry->extra[entry->extra_count].key, key, sizeof(entry->extra[0].key) - 1);
                strncpy(entry->extra[entry->extra_count].value, value, sizeof(entry->extra[0].value) - 1);
                entry->extra_count++;
            }
        }
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * Main Parse Function
 * ============================================================================ */

EFI_STATUS config_parse(const char *data, size_t size) {
    parser_t p;
    parser_init(&p, data, size);

    // Initialize config
    memset(&g_config, 0, sizeof(g_config));
    g_config.timeout = 5;
    g_config.default_entry = 0;
    g_config.text_mode = 0;
    g_config.log_level = 2;
    g_config.secure_boot = 0;
    g_config.measured_boot = 0;
    strcpy(g_config.theme, "default");

    while (!parser_eof(&p)) {
        parser_skip_whitespace(&p);
        char c = parser_peek(&p);

        if (c == '\0') break;
        if (c == '#' || c == ';') {
            parser_skip_line(&p);
            continue;
        }
        if (c == '}') {
            // Stray closing brace
            parser_next(&p);
            continue;
        }

        char key[64];
        if (!parser_read_identifier(&p, key, sizeof(key))) {
            parser_skip_line(&p);
            continue;
        }

        parser_skip_whitespace(&p);
        c = parser_peek(&p);

        if (c == '{') {
            // Entry block
            if (g_config.entry_count < MAX_CONFIG_ENTRIES) {
                config_entry_t *entry = &g_config.entries[g_config.entry_count];
                memset(entry, 0, sizeof(config_entry_t));
                strncpy(entry->name, key, sizeof(entry->name) - 1);
                strcpy(entry->protocol, "limoboot");
                EFI_STATUS status = parse_entry(&p, entry);
                if (!EFI_ERROR(status)) {
                    g_config.entry_count++;
                }
            } else {
                // Skip block
                int depth = 1;
                parser_next(&p);  // Skip '{'
                while (!parser_eof(&p) && depth > 0) {
                    c = parser_next(&p);
                    if (c == '{') depth++;
                    else if (c == '}') depth--;
                }
            }
        } else if (c == '=') {
            // Global key-value
            parser_next(&p);  // Skip '='

            char value[MAX_STRING_VALUE];
            if (!parser_read_string(&p, value, sizeof(value))) {
                parser_read_unquoted(&p, value, sizeof(value));
            }

            if (strcmp(key, "timeout") == 0) {
                g_config.timeout = atoi(value);
            } else if (strcmp(key, "default") == 0) {
                g_config.default_entry = atoi(value);
            } else if (strcmp(key, "theme") == 0) {
                strncpy(g_config.theme, value, sizeof(g_config.theme) - 1);
            } else if (strcmp(key, "text_mode") == 0) {
                g_config.text_mode = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            } else if (strcmp(key, "log_level") == 0) {
                g_config.log_level = atoi(value);
            } else if (strcmp(key, "secure_boot") == 0) {
                g_config.secure_boot = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            } else if (strcmp(key, "measured_boot") == 0) {
                g_config.measured_boot = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            }
        }
    }

    return EFI_SUCCESS;
}

/* ============================================================================
 * Config Accessors
 * ============================================================================ */

config_t *config_get(void) {
    return &g_config;
}

config_entry_t *config_get_entry(int index) {
    if (index >= 0 && index < g_config.entry_count) {
        return &g_config.entries[index];
    }
    return NULL;
}

int config_get_entry_count(void) {
    return g_config.entry_count;
}

config_entry_t *config_get_default_entry(void) {
    if (g_config.default_entry >= 0 && g_config.default_entry < g_config.entry_count) {
        return &g_config.entries[g_config.default_entry];
    }
    if (g_config.entry_count > 0) {
        return &g_config.entries[0];
    }
    return NULL;
}

/* ============================================================================
 * Load Config from File
 * ============================================================================ */

EFI_STATUS config_load_file(EFI_FILE_PROTOCOL *root, const CHAR16 *path) {
    EFI_FILE_PROTOCOL *file = NULL;
    EFI_STATUS status = root->Open(root, &file, (CHAR16 *)path, EFI_FILE_MODE_READ, 0);
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

    if (file_size > 65536) {
        file->Close(file);
        return EFI_BAD_BUFFER_SIZE;
    }

    char *buffer = efi_allocate_pool((UINTN)file_size + 1, EfiBootServicesData);
    if (!buffer) {
        file->Close(file);
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN read_size = (UINTN)file_size;
    status = file->Read(file, &read_size, buffer);
    file->Close(file);

    if (EFI_ERROR(status)) {
        efi_free_pool(buffer);
        return status;
    }

    buffer[read_size] = '\0';
    status = config_parse(buffer, read_size);
    efi_free_pool(buffer);

    return status;
}