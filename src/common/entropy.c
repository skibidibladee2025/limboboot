/*
 * Entropy Gathering for limoboot
 * Collects entropy from various sources for kernel
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"
#include "memory.c"

/* ============================================================================
 * Entropy Sources
 * ============================================================================ */

#define ENTROPY_MIN_SIZE    256
#define ENTROPY_MAX_SIZE    4096

/* ============================================================================
 * RDRAND / RDSEED (x86-64)
 * ============================================================================ */

#if defined(__x86_64__)

static inline int has_rdrand(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (ecx & (1 << 30)) != 0;
}

static inline int has_rdseed(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
    return (ebx & (1 << 18)) != 0;
}

static inline int rdrand64(uint64_t *out) {
    unsigned char ok;
    __asm__ volatile ("rdrand %0; setc %1" : "=r"(*out), "=qm"(ok) :: "cc");
    return ok;
}

static inline int rdseed64(uint64_t *out) {
    unsigned char ok;
    __asm__ volatile ("rdseed %0; setc %1" : "=r"(*out), "=qm"(ok) :: "cc");
    return ok;
}

size_t entropy_x86_rdrand(void *buf, size_t max) {
    uint64_t *p = (uint64_t *)buf;
    size_t count = max / 8;
    size_t generated = 0;

    if (has_rdseed()) {
        for (size_t i = 0; i < count; i++) {
            uint64_t val;
            int retries = 10;
            while (retries-- > 0) {
                if (rdseed64(&val)) {
                    p[i] = val;
                    generated += 8;
                    break;
                }
            }
        }
    } else if (has_rdrand()) {
        for (size_t i = 0; i < count; i++) {
            uint64_t val;
            int retries = 10;
            while (retries-- > 0) {
                if (rdrand64(&val)) {
                    p[i] = val;
                    generated += 8;
                    break;
                }
            }
        }
    }

    return generated;
}

#endif /* __x86_64__ */

/* ============================================================================
 * ARM RNG (aarch64)
 * ============================================================================ */

#if defined(__aarch64__)

static inline int has_rng(void) {
    // Check ID_AA64ISAR0_EL1 for RNG
    uint64_t isar0;
    __asm__ volatile ("mrs %0, id_aa64isar0_el1" : "=r"(isar0));
    return ((isar0 >> 20) & 0xF) >= 1;  // RNG implemented
}

static inline uint64_t arm_rng_read(void) {
    uint64_t val;
    __asm__ volatile ("mrs %0, rng" : "=r"(val));
    return val;
}

size_t entropy_arm_rng(void *buf, size_t max) {
    if (!has_rng()) return 0;

    uint64_t *p = (uint64_t *)buf;
    size_t count = max / 8;
    size_t generated = 0;

    for (size_t i = 0; i < count; i++) {
        p[i] = arm_rng_read();
        generated += 8;
    }

    return generated;
}

#endif /* __aarch64__ */

/* ============================================================================
 * RISC-V Entropy (using TRNG if available)
 * ============================================================================ */

#if defined(__riscv)

static inline int has_trng(void) {
    // Check for Zkr or entropy source extension
    // For now, return 0 - would need platform-specific detection
    return 0;
}

size_t entropy_riscv_trng(void *buf, size_t max) {
    (void)buf; (void)max;
    return 0;
}

#endif /* __riscv */

/* ============================================================================
 * EFI Time-based Entropy
 * ============================================================================ */

size_t entropy_efi_time(void *buf, size_t max) {
    if (!g_rs) return 0;

    EFI_TIME time;
    EFI_TIME_CAPS caps;
    EFI_STATUS status = g_rs->GetTime(&time, &caps);
    if (EFI_ERROR(status)) return 0;

    uint8_t *p = (uint8_t *)buf;
    size_t generated = 0;

    // Mix time fields
    uint64_t mix = 0;
    mix ^= (uint64_t)time.Year << 48;
    mix ^= (uint64_t)time.Month << 40;
    mix ^= (uint64_t)time.Day << 32;
    mix ^= (uint64_t)time.Hour << 24;
    mix ^= (uint64_t)time.Minute << 16;
    mix ^= (uint64_t)time.Second << 8;
    mix ^= (uint64_t)time.Nanosecond;

    // Also use monotonic counter if available
    UINT64 monotonic = 0;
    g_bs->GetNextMonotonicCount(&monotonic);
    mix ^= monotonic;

    // Write mix
    for (size_t i = 0; i < sizeof(mix) && generated < max; i++) {
        p[generated++] = (mix >> (i * 8)) & 0xFF;
    }

    return generated;
}

/* ============================================================================
 * EFI Variable Entropy
 * ============================================================================ */

size_t entropy_efi_variables(void *buf, size_t max) {
    if (!g_rs) return 0;

    uint8_t *p = (uint8_t *)buf;
    size_t generated = 0;

    UINTN name_size = 0;
    CHAR16 name[256];
    EFI_GUID guid;

    while (generated < max) {
        EFI_STATUS status = g_rs->GetNextVariableName(&name_size, name, &guid);
        if (status == EFI_NOT_FOUND) break;
        if (EFI_ERROR(status)) break;

        // Mix variable name and GUID
        for (size_t i = 0; i < name_size && generated < max; i += 2) {
            p[generated++] = name[i] & 0xFF;
            if (i + 1 < name_size && generated < max) {
                p[generated++] = (name[i + 1]) & 0xFF;
            }
        }

        for (int i = 0; i < 16 && generated < max; i++) {
            p[generated++] = guid.Data4[i];
        }
    }

    return generated;
}

/* ============================================================================
 * Memory Map Entropy
 * ============================================================================ */

size_t entropy_memory_map(void *buf, size_t max) {
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key, desc_size;
    UINT32 desc_version;

    EFI_STATUS status = g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) return 0;

    map_size += 4096;
    map = efi_allocate_pool(map_size, EfiBootServicesData);
    if (!map) return 0;

    status = g_bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        efi_free_pool(map);
        return 0;
    }

    uint8_t *p = (uint8_t *)buf;
    size_t generated = 0;

    for (UINTN i = 0; i < map_size / desc_size && generated < max; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);

        // Mix physical address and size
        uint64_t val = desc->PhysicalStart ^ desc->NumberOfPages ^ desc->Type ^ desc->Attribute;
        for (int j = 0; j < 8 && generated < max; j++) {
            p[generated++] = (val >> (j * 8)) & 0xFF;
        }
    }

    efi_free_pool(map);
    return generated;
}

/* ============================================================================
 * CPU Timer Entropy (TSC / CNTVCT)
 * ============================================================================ */

#if defined(__x86_64__)
static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#elif defined(__aarch64__)
static inline uint64_t read_cntvct(void) {
    uint64_t val;
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}
#elif defined(__riscv)
static inline uint64_t read_cycle(void) {
    uint64_t val;
    __asm__ volatile ("rdcycle %0" : "=r"(val));
    return val;
}
#else
static inline uint64_t read_timer(void) { return 0; }
#endif

size_t entropy_cpu_timer(void *buf, size_t max) {
    uint8_t *p = (uint8_t *)buf;
    size_t generated = 0;

    // Sample timer multiple times with small delays
    for (int round = 0; round < 16 && generated < max; round++) {
        uint64_t t1 = 0, t2 = 0;

#if defined(__x86_64__)
        t1 = read_tsc();
        g_bs->Stall(1);
        t2 = read_tsc();
#elif defined(__aarch64__)
        t1 = read_cntvct();
        g_bs->Stall(1);
        t2 = read_cntvct();
#elif defined(__riscv)
        t1 = read_cycle();
        g_bs->Stall(1);
        t2 = read_cycle();
#endif

        uint64_t diff = t2 - t1;
        for (int i = 0; i < 8 && generated < max; i++) {
            p[generated++] = (diff >> (i * 8)) & 0xFF;
        }
    }

    return generated;
}

/* ============================================================================
 * Hash Mixing (Simple)
 * ============================================================================ */

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void entropy_mix(void *buf, size_t size) {
    // Simple mixing using splitmix64
    uint64_t state = 0;

    // Initialize state from buffer
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0; i < size && i < 8; i++) {
        state ^= (uint64_t)p[i] << (i * 8);
    }

    // Re-mix entire buffer
    uint64_t *q = (uint64_t *)buf;
    size_t count = size / 8;
    for (size_t i = 0; i < count; i++) {
        q[i] ^= splitmix64(&state);
    }

    // Handle remainder
    size_t rem = size % 8;
    if (rem) {
        uint64_t mix = splitmix64(&state);
        for (size_t i = 0; i < rem; i++) {
            p[count * 8 + i] ^= (mix >> (i * 8)) & 0xFF;
        }
    }
}

/* ============================================================================
 * Main Entropy Collection
 * ============================================================================ */

EFI_STATUS entropy_collect(void *buf, size_t size, size_t *collected) {
    if (size < ENTROPY_MIN_SIZE) return EFI_BUFFER_TOO_SMALL;
    if (size > ENTROPY_MAX_SIZE) size = ENTROPY_MAX_SIZE;

    memset(buf, 0, size);
    size_t total = 0;
    uint8_t *p = (uint8_t *)buf;

    // Collect from all available sources
    total += entropy_cpu_timer(p + total, size - total);
    total += entropy_efi_time(p + total, size - total);
    total += entropy_efi_variables(p + total, size - total);
    total += entropy_memory_map(p + total, size - total);

#if defined(__x86_64__)
    total += entropy_x86_rdrand(p + total, size - total);
#elif defined(__aarch64__)
    total += entropy_arm_rng(p + total, size - total);
#elif defined(__riscv)
    total += entropy_riscv_trng(p + total, size - total);
#endif

    // If still not enough, fill with timer noise
    while (total < ENTROPY_MIN_SIZE && total < size) {
        total += entropy_cpu_timer(p + total, size - total);
    }

    // Final mixing
    entropy_mix(buf, total);

    if (collected) *collected = total;
    return total >= ENTROPY_MIN_SIZE ? EFI_SUCCESS : EFI_NOT_READY;
}

/* ============================================================================
 * Generate Response
 * ============================================================================ */

EFI_STATUS entropy_build_response(limoboot_entropy_response_t *response) {
    void *buf = efi_allocate_pool(ENTROPY_MAX_SIZE, EfiLoaderData);
    if (!buf) return EFI_OUT_OF_RESOURCES;

    size_t collected = 0;
    EFI_STATUS status = entropy_collect(buf, ENTROPY_MAX_SIZE, &collected);
    if (EFI_ERROR(status)) {
        efi_free_pool(buf);
        return status;
    }

    response->revision = 0;
    response->entropy_addr = virt_to_phys(buf);
    response->entropy_size = collected;

    return EFI_SUCCESS;
}