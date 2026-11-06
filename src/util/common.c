/*
 * Host Utility Common Functions
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* ============================================================================
 * Logging
 * ============================================================================ */

typedef enum {
    LOG_NONE = 0,
    LOG_ERROR = 1,
    LOG_WARN = 2,
    LOG_INFO = 3,
    LOG_DEBUG = 4,
} log_level_t;

static log_level_t g_log_level = LOG_INFO;

void log_set_level(log_level_t level) {
    g_log_level = level;
}

void log_message(log_level_t level, const char *fmt, ...) {
    if (level > g_log_level) return;

    const char *prefix;
    switch (level) {
        case LOG_ERROR: prefix = "ERROR: "; break;
        case LOG_WARN:  prefix = "WARN:  "; break;
        case LOG_INFO:  prefix = "INFO:  "; break;
        case LOG_DEBUG: prefix = "DEBUG: "; break;
        default:        prefix = ""; break;
    }

    fprintf(stderr, "%s", prefix);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

#define log_error(fmt, ...)  log_message(LOG_ERROR, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)   log_message(LOG_WARN, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)   log_message(LOG_INFO, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)  log_message(LOG_DEBUG, fmt, ##__VA_ARGS__)

/* ============================================================================
 * File Operations
 * ============================================================================ */

int read_file(const char *path, void **data, size_t *size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_error("Failed to open %s: %s", path, strerror(errno));
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }

    *size = st.st_size;
    *data = malloc(*size);
    if (!*data) {
        close(fd);
        return -1;
    }

    ssize_t read_bytes = read(fd, *data, *size);
    close(fd);

    if (read_bytes != (ssize_t)*size) {
        free(*data);
        return -1;
    }

    return 0;
}

int write_file(const char *path, const void *data, size_t size) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        log_error("Failed to create %s: %s", path, strerror(errno));
        return -1;
    }

    ssize_t written = write(fd, data, size);
    close(fd);

    if (written != (ssize_t)size) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * String Utilities
 * ============================================================================ */

char *strdup_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (len < 0) return NULL;

    char *str = malloc(len + 1);
    if (!str) return NULL;

    va_start(ap, fmt);
    vsnprintf(str, len + 1, fmt, ap);
    va_end(ap);

    return str;
}

void str_trim(char *str) {
    if (!str) return;

    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end-- = '\0';
    }

    char *start = str;
    while (*start == ' ' || *start == '\t') start++;

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

int str_starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int str_ends_with(const char *str, const char *suffix) {
    size_t slen = strlen(str);
    size_t suflen = strlen(suffix);
    if (suflen > slen) return 0;
    return strcmp(str + slen - suflen, suffix) == 0;
}

/* ============================================================================
 * Path Utilities
 * ============================================================================ */

char *path_join(const char *a, const char *b) {
    if (!a || !*a) return strdup(b);
    if (!b || !*b) return strdup(a);

    size_t alen = strlen(a);
    size_t blen = strlen(b);
    int need_sep = (a[alen - 1] != '/' && a[alen - 1] != '\\');

    char *result = malloc(alen + blen + 2);
    if (!result) return NULL;

    strcpy(result, a);
    if (need_sep) strcat(result, "/");
    strcat(result, b);

    return result;
}

char *path_dirname(const char *path) {
    char *copy = strdup(path);
    if (!copy) return NULL;

    char *last_slash = strrchr(copy, '/');
    if (!last_slash) last_slash = strrchr(copy, '\\');

    if (last_slash) {
        *last_slash = '\0';
    } else {
        strcpy(copy, ".");
    }

    return copy;
}

char *path_basename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) last_slash = strrchr(path, '\\');
    return strdup(last_slash ? last_slash + 1 : path);
}

/* ============================================================================
 * Hex Dump
 * ============================================================================ */

void hex_dump(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < size; i += 16) {
        printf("%08zx: ", i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", p[i + j]);
            } else {
                printf("   ");
            }
        }
        printf(" ");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                char c = p[i + j];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            }
        }
        printf("\n");
    }
}

/* ============================================================================
 * Endian Helpers
 * ============================================================================ */

uint16_t le16_to_cpu(uint16_t val) { return val; }
uint32_t le32_to_cpu(uint32_t val) { return val; }
uint64_t le64_to_cpu(uint64_t val) { return val; }

uint16_t cpu_to_le16(uint16_t val) { return val; }
uint32_t cpu_to_le32(uint32_t val) { return val; }
uint64_t cpu_to_le64(uint64_t val) { return val; }

uint16_t be16_to_cpu(uint16_t val) {
    return (val >> 8) | (val << 8);
}
uint32_t be32_to_cpu(uint32_t val) {
    return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}
uint64_t be64_to_cpu(uint64_t val) {
    return (be32_to_cpu(val & 0xFFFFFFFF) << 32) | be32_to_cpu(val >> 32);
}