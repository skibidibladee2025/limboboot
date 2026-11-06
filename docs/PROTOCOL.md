# Limoboot Protocol Specification

**Version**: 1.0 (Draft)
**Status**: Design Phase

---

## Overview

The Limoboot Protocol is a clean, modern, 64-bit-only boot protocol for loading kernels and providing runtime services. It is inspired by the Limine protocol but redesigned from scratch for purity — no 32-bit baggage, no legacy paths, no compromises.

## Design Principles

1. **Pure 64-bit**: All pointers, addresses, and sizes are 64-bit. No 32-bit truncation.
2. **Higher-half only**: Kernels must be linked for the higher half (typically `0xFFFFFFFF80000000` or above).
3. **Request/Response**: Feature negotiation via request/response structures.
4. **HHDM (Higher Half Direct Map)**: Direct physical-to-virtual mapping for all memory.
5. **Clean ABI**: Per-architecture ABI definitions (System V AMD64, AAPCS64, RISC-V LP64, LoongArch LP64).
6. **Extensible**: New features via new request types, backward compatible.

---

## Protocol Entry Point

The bootloader transfers control to the kernel's entry point with the following register state:

### x86-64 (System V AMD64 ABI)
```
RAX = LIMBOOT_PROTOCOL_MAGIC (0x4C4D4254)  // "LMBT"
RBX = pointer to limoboot_handoff_t (64-bit physical address)
RCX = 0
RDX = 0
RSI = 0
RDI = 0
RBP = 0
RSP = valid stack pointer (16-byte aligned, 64 KiB minimum)
RIP = kernel entry point
```

### aarch64 (AAPCS64)
```
x0 = LIMBOOT_PROTOCOL_MAGIC (0x4C4D4254)
x1 = pointer to limoboot_handoff_t (64-bit physical address)
x2 = 0
x3 = 0
sp = valid stack pointer (16-byte aligned, 64 KiB minimum)
pc = kernel entry point
```

### riscv64 (LP64)
```
a0 = LIMBOOT_PROTOCOL_MAGIC (0x4C4D4254)
a1 = pointer to limoboot_handoff_t (64-bit physical address)
a2 = 0
a3 = 0
sp = valid stack pointer (16-byte aligned, 64 KiB minimum)
pc = kernel entry point
```

### loongarch64 (LP64)
```
a0 = LIMBOOT_PROTOCOL_MAGIC (0x4C4D4254)
a1 = pointer to limoboot_handoff_t (64-bit physical address)
a2 = 0
a3 = 0
sp = valid stack pointer (16-byte aligned, 64 KiB minimum)
pc = kernel entry point
```

---

## Magic Constants

```c
#define LIMBOOT_PROTOCOL_MAGIC       0x4C4D4254  // "LMBT" in ASCII
#define LIMBOOT_PROTOCOL_VERSION     1
#define LIMBOOT_PROTOCOL_REVISION    0
```

---

## Handoff Structure

```c
typedef struct limoboot_handoff {
    uint64_t magic;              // LIMBOOT_PROTOCOL_MAGIC
    uint64_t version;            // Protocol version (1)
    uint64_t revision;           // Protocol revision (0)
    uint64_t flags;              // Flags (see below)
    uint64_t entry_point;        // Kernel entry point (virtual address)
    uint64_t stack_top;          // Initial stack top (virtual address)
    uint64_t hhdm_offset;        // HHDM offset (physical -> virtual: virt = phys + hhdm_offset)
    uint64_t response_ptr;       // Physical address of response array
    uint64_t response_count;     // Number of responses
    uint64_t reserved[8];        // Reserved for future use
} limoboot_handoff_t;
```

### Flags

```c
#define LIMBOOT_FLAG_HHDM_PRESENT    (1ULL << 0)  // HHDM is set up
#define LIMBOOT_FLAG_SMP             (1ULL << 1)  // SMP info available
#define LIMBOOT_FLAG_MEASURED_BOOT   (1ULL << 2)  // Measured boot / TPM event log
#define LIMBOOT_FLAG_SECURE_BOOT     (1ULL << 3)  // Secure Boot active
#define LIMBOOT_FLAG_EFI_RUNTIME     (1ULL << 4)  // EFI runtime services available
```

---

## Request/Response System

The kernel communicates required and optional features via **requests**. The bootloader provides **responses**.

### Request Header

```c
typedef struct limoboot_request {
    uint64_t id;                 // Request ID (see below)
    uint64_t revision;           // Request revision (0)
    uint64_t flags;              // Flags
    uint64_t response;           // Physical address of response (filled by bootloader)
} limoboot_request_t;
```

### Request IDs

```c
#define LIMBOOT_REQUEST_MEMORY_MAP       0x00000001
#define LIMBOOT_REQUEST_FRAMEBUFFER      0x00000002
#define LIMBOOT_REQUEST_MODULES          0x00000003
#define LIMBOOT_REQUEST_SMP              0x00000004
#define LIMBOOT_REQUEST_RSDP             0x00000005
#define LIMBOOT_REQUEST_SMBIOS           0x00000006
#define LIMBOOT_REQUEST_EFI_SYSTEM_TABLE 0x00000007
#define LIMBOOT_REQUEST_ENTROPY          0x00000008
#define LIMBOOT_REQUEST_KERNEL_FILE      0x00000009
#define LIMBOOT_REQUEST_CMDLINE          0x0000000A
#define LIMBOOT_REQUEST_MEASURED_BOOT    0x0000000B
#define LIMBOOT_REQUEST_PAGING_MODE      0x0000000C
```

### Request Flags

```c
#define LIMBOOT_REQUEST_FLAG_REQUIRED    (1ULL << 0)  // Boot fails if not provided
#define LIMBOOT_REQUEST_FLAG_OPTIONAL    (1ULL << 1)  // Optional feature
```

---

## Responses

Each request type has a corresponding response structure. The bootloader fills these and sets the `response` field in the request to point to the response.

### Memory Map Response

```c
#define LIMBOOT_MEMORY_USABLE            0
#define LIMBOOT_MEMORY_RESERVED          1
#define LIMBOOT_MEMORY_ACPI_RECLAIMABLE  2
#define LIMBOOT_MEMORY_ACPI_NVS          3
#define LIMBOOT_MEMORY_BAD               4
#define LIMBOOT_MEMORY_BOOTLOADER        5
#define LIMBOOT_MEMORY_KERNEL            6
#define LIMBOOT_MEMORY_MODULE            7
#define LIMBOOT_MEMORY_FRAMEBUFFER       8
#define LIMBOOT_MEMORY_HHDM              9
#define LIMBOOT_MEMORY_EFI_RUNTIME       10
#define LIMBOOT_MEMORY_EFI_BOOT_SERVICES 11
#define LIMBOOT_MEMORY_PERSISTENT        12

typedef struct limoboot_memmap_entry {
    uint64_t base;       // Physical base address
    uint64_t length;     // Length in bytes
    uint64_t type;       // Entry type (see above)
    uint64_t attributes; // Memory attributes (EFI memory attributes)
} limoboot_memmap_entry_t;

typedef struct limoboot_memmap_response {
    uint64_t revision;           // 0
    uint64_t entry_count;        // Number of entries
    uint64_t entry_size;         // Size of each entry (sizeof(limoboot_memmap_entry_t))
    uint64_t entries;            // Physical address of entry array
} limoboot_memmap_response_t;
```

### Framebuffer Response

```c
typedef struct limoboot_framebuffer_response {
    uint64_t revision;           // 0
    uint64_t framebuffer_addr;   // Physical address of framebuffer
    uint64_t width;              // Width in pixels
    uint64_t height;             // Height in pixels
    uint64_t pitch;              // Bytes per scanline
    uint64_t bpp;                // Bits per pixel (32)
    uint64_t format;             // Pixel format (see below)
    uint64_t red_mask;           // Red mask
    uint64_t green_mask;         // Green mask
    uint64_t blue_mask;          // Blue mask
    uint64_t reserved_mask;      // Reserved mask
} limoboot_framebuffer_response_t;

#define LIMBOOT_FB_FORMAT_RGB  0  // RGB (8-8-8)
#define LIMBOOT_FB_FORMAT_BGR  1  // BGR (8-8-8)
#define LIMBOOT_FB_FORMAT_ARGB 2  // ARGB (8-8-8-8)
#define LIMBOOT_FB_FORMAT_ABGR 3  // ABGR (8-8-8-8)
```

### Modules Response

```c
typedef struct limoboot_module {
    uint64_t base;       // Physical base address
    uint64_t length;     // Length in bytes
    uint64_t string;     // Physical address of NUL-terminated string (path/name)
} limoboot_module_t;

typedef struct limoboot_modules_response {
    uint64_t revision;        // 0
    uint64_t module_count;    // Number of modules
    uint64_t modules;         // Physical address of module array
} limoboot_modules_response_t;
```

### SMP Response

```c
typedef struct limoboot_smp_cpu {
    uint64_t lapic_id;       // Local APIC ID (x86) / MPIDR (ARM) / Hart ID (RISC-V)
    uint64_t flags;          // CPU flags
    uint64_t reserved[6];    // Reserved
} limoboot_smp_cpu_t;

typedef struct limoboot_smp_response {
    uint64_t revision;       // 0
    uint64_t cpu_count;      // Number of CPUs
    uint64_t bsp_index;      // Index of BSP in cpu array
    uint64_t cpus;           // Physical address of CPU array
    uint64_t trampoline;     // Physical address of AP trampoline (or 0)
    uint64_t trampoline_size;// Size of trampoline
} limoboot_smp_response_t;

#define LIMBOOT_SMP_CPU_ENABLED  (1ULL << 0)
#define LIMBOOT_SMP_CPU_BSP      (1ULL << 1)
```

### RSDP Response

```c
typedef struct limoboot_rsdp_response {
    uint64_t revision;     // 0
    uint64_t rsdp_addr;    // Physical address of RSDP
} limoboot_rsdp_response_t;
```

### SMBIOS Response

```c
typedef struct limoboot_smbios_response {
    uint64_t revision;         // 0
    uint64_t entry_point_addr; // Physical address of SMBIOS entry point (32-bit or 64-bit)
} limoboot_smbios_response_t;
```

### EFI System Table Response

```c
typedef struct limoboot_efi_system_table_response {
    uint64_t revision;             // 0
    uint64_t system_table_addr;    // Physical address of EFI System Table
} limoboot_efi_system_table_response_t;
```

### Entropy Response

```c
typedef struct limoboot_entropy_response {
    uint64_t revision;       // 0
    uint64_t entropy_addr;   // Physical address of entropy buffer
    uint64_t entropy_size;   // Size in bytes (minimum 256 bytes)
} limoboot_entropy_response_t;
```

### Kernel File Response

```c
typedef struct limoboot_kernel_file_response {
    uint64_t revision;       // 0
    uint64_t kernel_addr;    // Physical address of loaded kernel
    uint64_t kernel_size;    // Size in bytes
} limoboot_kernel_file_response_t;
```

### Cmdline Response

```c
typedef struct limoboot_cmdline_response {
    uint64_t revision;       // 0
    uint64_t cmdline_addr;   // Physical address of NUL-terminated cmdline string
} limoboot_cmdline_response_t;
```

### Measured Boot Response

```c
typedef struct limoboot_measured_boot_response {
    uint64_t revision;           // 0
    uint64_t event_log_addr;     // Physical address of TPM event log
    uint64_t event_log_size;     // Size in bytes
    uint64_t tpm_addr;           // Physical address of TPM device (or 0)
} limoboot_measured_boot_response_t;
```

### Paging Mode Response

```c
#define LIMBOOT_PAGING_MODE_4LEVEL   0  // x86-64: 4-level (default)
#define LIMBOOT_PAGING_MODE_5LEVEL   1  // x86-64: 5-level (LA57)
#define LIMBOOT_PAGING_MODE_SV39     2  // RISC-V: Sv39
#define LIMBOOT_PAGING_MODE_SV48     3  // RISC-V: Sv48
#define LIMBOOT_PAGING_MODE_SV57     4  // RISC-V: Sv57
#define LIMBOOT_PAGING_MODE_LPAE     5  // ARM: LPAE (48-bit)
#define LIMBOOT_PAGING_MODE_L57      6  // ARM: 52-bit (FEAT_LPA2)

typedef struct limoboot_paging_mode_response {
    uint64_t revision;       // 0
    uint64_t mode;           // Paging mode (see above)
    uint64_t flags;          // Flags
} limoboot_paging_mode_response_t;
```

---

## Kernel Request Structure

The kernel provides an array of requests at a known location (typically in `.rodata` or `.data`). The bootloader finds this array via a known symbol or by scanning.

### Request Array Symbol

The kernel must define a symbol `limoboot_requests` pointing to an array of `limoboot_request_t` terminated by an entry with `id = 0`.

```c
// In kernel (C)
__attribute__((section(".limoboot_requests"), used))
limoboot_request_t limoboot_requests[] = {
    { .id = LIMBOOT_REQUEST_MEMORY_MAP,      .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_FRAMEBUFFER,     .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_MODULES,         .flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_SMP,             .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_RSDP,            .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_SMBIOS,          .flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_EFI_SYSTEM_TABLE,.flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_ENTROPY,         .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_CMDLINE,         .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = LIMBOOT_REQUEST_MEASURED_BOOT,   .flags = LIMBOOT_REQUEST_FLAG_OPTIONAL },
    { .id = LIMBOOT_REQUEST_PAGING_MODE,     .flags = LIMBOOT_REQUEST_FLAG_REQUIRED },
    { .id = 0 },  // Terminator
};
```

### Finding the Request Array

The bootloader locates the request array by:
1. Scanning the kernel's ELF sections for `.limoboot_requests`
2. Or using a known symbol `limoboot_requests` (preferred)

---

## Memory Layout Requirements

### Kernel Loading

- Kernel must be a 64-bit ELF executable (ET_EXEC or ET_DYN with fixed address)
- Loaded at its preferred virtual address (higher half)
- Physical memory allocated and mapped via HHDM
- Entry point called with virtual addressing enabled

### HHDM (Higher Half Direct Map)

The bootloader sets up a 1:1 physical-to-virtual mapping for all usable memory:

```
Virtual Address = Physical Address + hhdm_offset
```

Typical `hhdm_offset` values:
- x86-64: `0xFFFF800000000000` (canonical higher half)
- aarch64: `0xFFFF000000000000` or `0xFF00000000000000`
- riscv64: `0xFFFFFFFF80000000` (or kernel-defined)
- loongarch64: `0xFFFF000000000000`

### Stack

- Minimum 64 KiB, 16-byte aligned
- Located in higher-half memory
- Guard page below if possible

### Page Tables

- Bootloader sets up initial page tables for kernel
- Identity mapping for first 1-2 MiB (for trampoline/AP startup)
- Higher-half mapping for kernel and HHDM
- Paging mode per architecture (4-level, 5-level, Sv39, Sv48, LPAE, etc.)

---

## Boot Flow

1. **Bootloader starts** (UEFI application)
2. **Parse config** (`limoboot.conf`)
3. **Show menu** (if timeout > 0 or multiple entries)
4. **Load kernel** (ELF, via filesystem driver)
5. **Load modules** (initrd, etc.)
6. **Allocate memory** for responses, handoff, page tables
7. **Set up page tables** (HHDM, kernel mappings, identity map for trampoline)
8. **Prepare request responses** (fill response structures)
9. **Exit Boot Services** (EFI)
10. **Jump to kernel entry point** with handoff in registers

---

## Secure Boot & Measured Boot

### Secure Boot

- Bootloader signed with enrolled key
- Config hash enrolled in `db`/`dbx` or via MOK
- `LIMBOOT_FLAG_SECURE_BOOT` set in handoff

### Measured Boot

- Bootloader extends PCRs with:
  - Kernel hash
  - Config hash
  - Module hashes
  - Command line
- TPM event log provided via `LIMBOOT_REQUEST_MEASURED_BOOT`
- `LIMBOOT_FLAG_MEASURED_BOOT` set in handoff

---

## ELF Note for Kernel Identification

Kernels supporting the Limoboot Protocol should include an ELF note:

```c
// In kernel linker script or assembly
NOTE(limoboot) {
    .magic = "LIMBOOT",
    .type = 1,
    .data = {
        .protocol_version = 1,
        .protocol_revision = 0,
        .flags = 0,
    }
}
```

This allows the bootloader to verify kernel compatibility.

---

## Versioning

- **Version**: Major protocol changes (incompatible)
- **Revision**: Minor changes (backward compatible)
- Current: Version 1, Revision 0

---

## Future Extensions

- `LIMBOOT_REQUEST_DEBUG` — Debug info (serial, log)
- `LIMBOOT_REQUEST_NETWORK` — PXE/network info
- `LIMBOOT_REQUEST_ACPI` — Full ACPI tables
- `LIMBOOT_REQUEST_DEVICETREE` — Device tree (ARM/RISC-V)
- `LIMBOOT_REQUEST_HYPERVISOR` — Hypervisor info (if booted under one)

---

## Reference Implementation

See `include/limoboot/protocol.h` for the canonical C header definitions.

---

*End of Protocol Specification*