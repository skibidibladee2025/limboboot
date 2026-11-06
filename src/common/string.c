/*
 * String and memory functions for freestanding environment
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>

/* ============================================================================
 * Memory Functions
 * ============================================================================ */

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) {
        *p++ = (uint8_t)c;
    }
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const uint8_t *p = (const uint8_t *)s;
    while (n--) {
        if (*p == (uint8_t)c) {
            return (void *)p;
        }
        p++;
    }
    return NULL;
}

/* ============================================================================
 * String Functions
 * ============================================================================ */

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && *s++) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++)) {}
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n-- && (*d++ = *src++)) {}
    while (n--) *d++ = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++)) {}
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest + strlen(dest);
    while (n-- && (*d++ = *src++)) {}
    *d = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if ((char)c == '\0') return (char *)s;
    return NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if ((char)c == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) {
            h++; n++;
        }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

/* ============================================================================
 * Wide String Functions (CHAR16)
 * ============================================================================ */

size_t wcslen(const CHAR16 *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int wcscmp(const CHAR16 *s1, const CHAR16 *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int wcsncmp(const CHAR16 *s1, const CHAR16 *s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *s1 - *s2;
}

CHAR16 *wcscpy(CHAR16 *dest, const CHAR16 *src) {
    CHAR16 *d = dest;
    while ((*d++ = *src++)) {}
    return dest;
}

CHAR16 *wcsncpy(CHAR16 *dest, const CHAR16 *src, size_t n) {
    CHAR16 *d = dest;
    while (n-- && (*d++ = *src++)) {}
    while (n--) *d++ = 0;
    return dest;
}

CHAR16 *wcscat(CHAR16 *dest, const CHAR16 *src) {
    CHAR16 *d = dest + wcslen(dest);
    while ((*d++ = *src++)) {}
    return dest;
}

const CHAR16 *wcsstr(const CHAR16 *haystack, const CHAR16 *needle) {
    if (!*needle) return haystack;
    for (; *haystack; haystack++) {
        const CHAR16 *h = haystack, *n = needle;
        while (*h && *n && *h == *n) {
            h++; n++;
        }
        if (!*n) return haystack;
    }
    return NULL;
}

/* ============================================================================
 * ASCII/Unicode Conversion
 * ============================================================================ */

void ascii_to_unicode(const char *src, CHAR16 *dest, size_t maxlen) {
    size_t i = 0;
    while (i < maxlen && *src) {
        dest[i++] = (CHAR16)*src++;
    }
    if (i < maxlen) dest[i] = 0;
}

void unicode_to_ascii(const CHAR16 *src, char *dest, size_t maxlen) {
    size_t i = 0;
    while (i < maxlen && *src) {
        dest[i++] = (char)*src++;
    }
    if (i < maxlen) dest[i] = '\0';
}

/* ============================================================================
 * GUID Comparison
 * ============================================================================ */

int guid_eq(const EFI_GUID *g1, const EFI_GUID *g2) {
    return g1->Data1 == g2->Data1 &&
           g1->Data2 == g2->Data2 &&
           g1->Data3 == g2->Data3 &&
           memcmp(g1->Data4, g2->Data4, 8) == 0;
}

void guid_copy(EFI_GUID *dest, const EFI_GUID *src) {
    *dest = *src;
}

#endif /* LIMBOOT_STRING_C */