/*
 * x86-64 SMP Support
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
 * x86-64 SMP Structures
 * ============================================================================ */

#define LAPIC_BASE_MSR          0x1B
#define LAPIC_ICR_LOW           0x300
#define LAPIC_ICR_HIGH          0x310
#define LAPIC_SVR               0x0F0
#define LAPIC_ESR               0x080
#define LAPIC_LVT_TIMER         0x320
#define LAPIC_LVT_THERMAL       0x330
#define LAPIC_LVT_PERF          0x340
#define LAPIC_LVT_LINT0         0x350
#define LAPIC_LVT_LINT1         0x360
#define LAPIC_LVT_ERROR         0x370
#define LAPIC_TIMER_INIT        0x380
#define LAPIC_TIMER_CURRENT     0x390
#define LAPIC_TIMER_DIVIDE      0x3E0

#define ICR_DELIVERY_FIXED      0x0
#define ICR_DELIVERY_SIPI       0x6
#define ICR_DEST_SHORTHAND_NONE 0x0
#define ICR_DEST_SHORTHAND_SELF 0x1
#define ICR_DEST_SHORTHAND_ALL  0x2
#define ICR_DEST_SHORTHAND_OTHERS 0x3
#define ICR_LEVEL_ASSERT        0x0
#define ICR_LEVEL_DEASSERT      0x1
#define ICR_TRIGGER_EDGE        0x0
#define ICR_TRIGGER_LEVEL       0x1
#define ICR_DEST_PHYSICAL       0x0
#define ICR_DEST_LOGICAL        0x1

/* ============================================================================
 * LAPIC Access
 * ============================================================================ */

static volatile uint32_t *g_lapic_base = NULL;

static inline void lapic_write(uint32_t reg, uint32_t val) {
    if (g_lapic_base) {
        g_lapic_base[reg / 4] = val;
    }
}

static inline uint32_t lapic_read(uint32_t reg) {
    if (g_lapic_base) {
        return g_lapic_base[reg / 4];
    }
    return 0;
}

static inline void lapic_icr_write(uint32_t high, uint32_t low) {
    lapic_write(LAPIC_ICR_HIGH, high);
    lapic_write(LAPIC_ICR_LOW, low);
}

/* ============================================================================
 * LAPIC Initialization
 * ============================================================================ */

EFI_STATUS arch_smp_init(void) {
    /* Get LAPIC base from MSR */
    uint64_t msr = arch_read_msr(LAPIC_BASE_MSR);
    EFI_PHYSICAL_ADDRESS lapic_phys = msr & ~0xFFF;

    /* Map LAPIC */
    EFI_STATUS status = arch_map_bootloader_data(lapic_phys, lapic_phys, 4096);
    if (EFI_ERROR(status)) return status;

    g_lapic_base = phys_to_virt(lapic_phys);

    /* Enable LAPIC */
    lapic_write(LAPIC_SVR, 0x1FF);  /* Enable, spurious vector 0xFF */

    /* Disable LINT0/1 */
    lapic_write(LAPIC_LVT_LINT0, 0x10000);  /* Masked */
    lapic_write(LAPIC_LVT_LINT1, 0x10000);  /* Masked */

    /* Disable error LVT */
    lapic_write(LAPIC_LVT_ERROR, 0x10000);

    return EFI_SUCCESS;
}

/* ============================================================================
 * Get BSP LAPIC ID
 * ============================================================================ */

uint32_t arch_get_bsp_lapic_id(void) {
    if (g_lapic_base) {
        return (lapic_read(0x020) >> 24) & 0xFF;  /* LAPIC ID register */
    }
    return 0;
}

/* ============================================================================
 * AP Startup (INIT-SIPI-SIPI)
 * ============================================================================ */

EFI_STATUS arch_start_ap(uint32_t apic_id, EFI_PHYSICAL_ADDRESS trampoline_phys) {
    if (!g_lapic_base) return EFI_NOT_READY;

    /* Convert trampoline to 16-bit segment:offset */
    uint16_t trampoline_seg = (uint16_t)(trampoline_phys >> 4);
    uint16_t trampoline_off = (uint16_t)(trampoline_phys & 0xF);

    /* INIT IPI */
    lapic_icr_write(
        (apic_id << 24),
        ICR_DELIVERY_FIXED | ICR_LEVEL_ASSERT | ICR_TRIGGER_EDGE | ICR_DEST_PHYSICAL | 0x0
    );
    g_bs->Stall(10000);  /* 10ms */

    /* SIPI #1 */
    lapic_icr_write(
        (apic_id << 24),
        ICR_DELIVERY_SIPI | ICR_LEVEL_ASSERT | ICR_TRIGGER_EDGE | ICR_DEST_PHYSICAL | (trampoline_seg >> 8)
    );
    g_bs->Stall(200);  /* 200us */

    /* SIPI #2 */
    lapic_icr_write(
        (apic_id << 24),
        ICR_DELIVERY_SIPI | ICR_LEVEL_ASSERT | ICR_TRIGGER_EDGE | ICR_DEST_PHYSICAL | (trampoline_seg >> 8)
    );
    g_bs->Stall(200);  /* 200us */

    return EFI_SUCCESS;
}

/* ============================================================================
 * Wait for AP to Start
 * ============================================================================ */

volatile uint32_t *g_ap_started = NULL;

EFI_STATUS arch_wait_for_ap(uint32_t apic_id, UINTN timeout_ms) {
    if (!g_ap_started) return EFI_NOT_READY;

    for (UINTN i = 0; i < timeout_ms; i++) {
        if (g_ap_started[apic_id]) {
            return EFI_SUCCESS;
        }
        g_bs->Stall(1000);  /* 1ms */
    }

    return EFI_TIMEOUT;
}

/* ============================================================================
 * SMP Response Building
 * ============================================================================ */

EFI_STATUS arch_build_smp_response(limoboot_smp_response_t *response) {
    /* For now, just report BSP */
    /* Real implementation would parse ACPI MADT or use EFI MP Services */

    limoboot_smp_cpu_t *cpus = efi_allocate_pool(sizeof(limoboot_smp_cpu_t), EfiLoaderData);
    if (!cpus) return EFI_OUT_OF_RESOURCES;

    cpus[0].lapic_id = arch_get_bsp_lapic_id();
    cpus[0].flags = LIMBOOT_SMP_CPU_ENABLED | LIMBOOT_SMP_CPU_BSP;
    memset(cpus[0].reserved, 0, sizeof(cpus[0].reserved));

    response->revision = 0;
    response->cpu_count = 1;
    response->bsp_index = 0;
    response->cpus = virt_to_phys(cpus);
    response->trampoline = 0;  /* Will be set by main */
    response->trampoline_size = 0;

    return EFI_SUCCESS;
}

/* ============================================================================
 * ACPI MADT Parsing (Placeholder)
 * ============================================================================ */

typedef struct {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    uint64_t oem_table_id;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t local_apic_addr;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_header_t;

#define MADT_LAPIC            0
#define MADT_IOAPIC           1
#define MADT_INT_SRC_OVERRIDE 2
#define MADT_NMI_SRC          3
#define MADT_LAPIC_NMI        4
#define MADT_LAPIC_ADDR_OVERRIDE 5
#define MADT_IO_SAPIC         6
#define MADT_LOCAL_SAPIC      7
#define MADT_PROC_INT_OVERRIDE 8
#define MADT_X2APIC           9

EFI_STATUS arch_parse_madt(void) {
    /* Find RSDP -> RSDT/XSDT -> MADT */
    void *rsdp = efi_get_config_table(&EFI_ACPI_20_TABLE_GUID);
    if (!rsdp) return EFI_NOT_FOUND;

    /* Parse ACPI tables to find MADT */
    /* This is a placeholder - full implementation would parse RSDT/XSDT */
    return EFI_UNSUPPORTED;
}