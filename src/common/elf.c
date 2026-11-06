/*
 * ELF64 Loader for limoboot
 * Pure 64-bit ELF loading
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>
#include "string.c"
#include "memory.c"

/* ============================================================================
 * ELF64 Definitions
 * ============================================================================ */

#define ELF_MAGIC 0x464C457F  // "\x7FELF"

#define ELF_CLASS_64    2
#define ELF_DATA_LSB    1
#define ELF_OSABI_NONE  0

#define ET_EXEC   2
#define ET_DYN    3

#define EM_X86_64    62
#define EM_AARCH64   183
#define EM_RISCV     243
#define EM_LOONGARCH 258

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7

#define PF_X  (1 << 0)
#define PF_W  (1 << 1)
#define PF_R  (1 << 2)

#define SHN_UNDEF 0

typedef struct {
    uint32_t magic;
    uint8_t  class;
    uint8_t  data;
    uint8_t  version;
    uint8_t  osabi;
    uint8_t  abiversion;
    uint8_t  pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed)) elf64_phdr_t;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
} __attribute__((packed)) elf64_shdr_t;

typedef struct {
    uint32_t name;
    uint8_t  info;
    uint8_t  other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
} __attribute__((packed)) elf64_sym_t;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} __attribute__((packed)) elf64_rela_t;

#define ELF64_R_SYM(info)  ((info) >> 32)
#define ELF64_R_TYPE(info) ((info) & 0xFFFFFFFF)

/* x86-64 relocations */
#define R_X86_64_NONE        0
#define R_X86_64_64          1
#define R_X86_64_PC32        2
#define R_X86_64_GOT32       3
#define R_X86_64_PLT32       4
#define R_X86_64_COPY        5
#define R_X86_64_GLOB_DAT    6
#define R_X86_64_JUMP_SLOT   7
#define R_X86_64_RELATIVE    8
#define R_X86_64_GOTPCREL    9
#define R_X86_64_32          10
#define R_X86_64_32S         11
#define R_X86_64_16          12
#define R_X86_64_PC16        13
#define R_X86_64_8           14
#define R_X86_64_PC8         15
#define R_X86_64_DTPMOD64    16
#define R_X86_64_DTPOFF64    17
#define R_X86_64_TPOFF64     18
#define R_X86_64_TLSGD       19
#define R_X86_64_TLSLD       20
#define R_X86_64_DTPOFF32    21
#define R_X86_64_GOTTPOFF    22
#define R_X86_64_TPOFF32     23
#define R_X86_64_PC64        24
#define R_X86_64_GOTOFF64    25
#define R_X86_64_GOTPC32     26

/* aarch64 relocations */
#define R_AARCH64_NONE           0
#define R_AARCH64_ABS64          257
#define R_AARCH64_ABS32          258
#define R_AARCH64_ABS16          259
#define R_AARCH64_PREL64         260
#define R_AARCH64_PREL32         261
#define R_AARCH64_PREL16         262
#define R_AARCH64_MOVW_UABS_G0   263
#define R_AARCH64_MOVW_UABS_G0_NC 264
#define R_AARCH64_MOVW_UABS_G1   265
#define R_AARCH64_MOVW_UABS_G1_NC 266
#define R_AARCH64_MOVW_UABS_G2   267
#define R_AARCH64_MOVW_UABS_G2_NC 268
#define R_AARCH64_MOVW_UABS_G3   269
#define R_AARCH64_MOVW_SABS_G0   270
#define R_AARCH64_MOVW_SABS_G1   271
#define R_AARCH64_MOVW_SABS_G2   272
#define R_AARCH64_LD_PREL_LO19   273
#define R_AARCH64_ADR_PREL_LO21  274
#define R_AARCH64_ADR_PREL_PG_HI21 275
#define R_AARCH64_ADR_PREL_PG_HI21_NC 276
#define R_AARCH64_ADD_ABS_LO12_NC 277
#define R_AARCH64_LDST8_ABS_LO12_NC 278
#define R_AARCH64_TSTBR14        279
#define R_AARCH64_CONDBR19       280
#define R_AARCH64_JUMP26         282
#define R_AARCH64_CALL26         283
#define R_AARCH64_LDST16_ABS_LO12_NC 284
#define R_AARCH64_LDST32_ABS_LO12_NC 285
#define R_AARCH64_LDST64_ABS_LO12_NC 286
#define R_AARCH64_MOVW_PREL_G0   287
#define R_AARCH64_MOVW_PREL_G0_NC 288
#define R_AARCH64_MOVW_PREL_G1   289
#define R_AARCH64_MOVW_PREL_G1_NC 290
#define R_AARCH64_MOVW_PREL_G2   291
#define R_AARCH64_MOVW_PREL_G2_NC 292
#define R_AARCH64_MOVW_PREL_G3   293
#define R_AARCH64_LDST128_ABS_LO12_NC 299
#define R_AARCH64_MOVW_GOTOFF_G0 300
#define R_AARCH64_MOVW_GOTOFF_G0_NC 301
#define R_AARCH64_MOVW_GOTOFF_G1 302
#define R_AARCH64_MOVW_GOTOFF_G1_NC 303
#define R_AARCH64_MOVW_GOTOFF_G2 304
#define R_AARCH64_MOVW_GOTOFF_G2_NC 305
#define R_AARCH64_MOVW_GOTOFF_G3 306
#define R_AARCH64_GOTREL64       307
#define R_AARCH64_GOTREL32       308
#define R_AARCH64_GOT_LD_PREL19  309
#define R_AARCH64_ADR_GOT_PAGE   310
#define R_AARCH64_LD64_GOT_LO12_NC 311
#define R_AARCH64_ADR_GOT_PAGE_LO15 312
#define R_AARCH64_LD32_GOT_LO12_NC 313
#define R_AARCH64_LD64_GOTPAGE_LO15 314
#define R_AARCH64_TLSGD_ADR_PREL21 512
#define R_AARCH64_TLSGD_ADR_PAGE21 513
#define R_AARCH64_TLSGD_ADD_LO12_NC 514
#define R_AARCH64_TLSGD_MOVW_G1  515
#define R_AARCH64_TLSGD_MOVW_G0_NC 516
#define R_AARCH64_TLSLD_ADR_PREL21 517
#define R_AARCH64_TLSLD_ADR_PAGE21 518
#define R_AARCH64_TLSLD_ADD_LO12_NC 519
#define R_AARCH64_TLSLD_MOVW_G1  520
#define R_AARCH64_TLSLD_MOVW_G0_NC 521
#define R_AARCH64_TLSLE_MOVW_TPREL_G2 522
#define R_AARCH64_TLSLE_MOVW_TPREL_G1 523
#define R_AARCH64_TLSLE_MOVW_TPREL_G1_NC 524
#define R_AARCH64_TLSLE_MOVW_TPREL_G0 525
#define R_AARCH64_TLSLE_MOVW_TPREL_G0_NC 526
#define R_AARCH64_TLSLE_ADD_TPREL_LO12_NC 527
#define R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC 528
#define R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC 529
#define R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC 530
#define R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC 531
#define R_AARCH64_TLSLE_LDST128_TPREL_LO12_NC 532
#define R_AARCH64_TLSDESC_LD_PREL19 533
#define R_AARCH64_TLSDESC_ADR_PREL21 534
#define R_AARCH64_TLSDESC_ADR_PAGE21 535
#define R_AARCH64_TLSDESC_ADD_LO12_NC 536
#define R_AARCH64_TLSDESC_MOVW_G1  537
#define R_AARCH64_TLSDESC_MOVW_G0_NC 538
#define R_AARCH64_TLSDESC_ADD_LO12 539
#define R_AARCH64_TLSDESC_LD32_LO12_NC 540
#define R_AARCH64_TLSDESC_LD64_LO12_NC 541
#define R_AARCH64_COPY           1024
#define R_AARCH64_GLOB_DAT       1025
#define R_AARCH64_JUMP_SLOT      1026
#define R_AARCH64_RELATIVE       1027
#define R_AARCH64_TLS_DTPMOD64   1028
#define R_AARCH64_TLS_DTPOFF64   1029
#define R_AARCH64_TLS_TPREL64    1030
#define R_AARCH64_TLSDESC        1031

/* RISC-V relocations */
#define R_RISCV_NONE          0
#define R_RISCV_32            1
#define R_RISCV_64            2
#define R_RISCV_RELATIVE      3
#define R_RISCV_COPY          4
#define R_RISCV_JUMP_SLOT     5
#define R_RISCV_TLS_DTPMOD64  6
#define R_RISCV_TLS_DTPOFF64  7
#define R_RISCV_TLS_TPREL64   8
#define R_RISCV_BRANCH        16
#define R_RISCV_JAL           17
#define R_RISCV_CALL          18
#define R_RISCV_CALL_PLT      19
#define R_RISCV_GOT_HI20      20
#define R_RISCV_TLS_GOT_HI20  21
#define R_RISCV_TLS_GD_HI20   22
#define R_RISCV_PCREL_HI20    22
#define R_RISCV_PCREL_LO12_I  23
#define R_RISCV_PCREL_LO12_S  24
#define R_RISCV_HI20          25
#define R_RISCV_LO12_I        26
#define R_RISCV_LO12_S        27
#define R_RISCV_TPREL_HI20    28
#define R_RISCV_TPREL_LO12_I  29
#define R_RISCV_TPREL_LO12_S  30
#define R_RISCV_TPREL_ADD     31

/* LoongArch relocations */
#define R_LARCH_NONE          0
#define R_LARCH_32            1
#define R_LARCH_64            2
#define R_LARCH_RELATIVE      3
#define R_LARCH_COPY          4
#define R_LARCH_JUMP_SLOT     5
#define R_LARCH_TLS_DTPMOD64  6
#define R_LARCH_TLS_DTPOFF64  7
#define R_LARCH_TLS_TPREL64   8

/* ============================================================================
 * ELF Note for Limoboot Protocol
 * ============================================================================ */

#define NT_LIMBOOT  0x4C4D4254  /* "LMBT" */

typedef struct {
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
    char name[8];  // "LIMBOOT\0"
    uint32_t protocol_version;
    uint32_t protocol_revision;
    uint32_t flags;
} __attribute__((packed)) limoboot_note_t;

/* ============================================================================
 * ELF Validation
 * ============================================================================ */

int elf64_validate(const void *data, size_t size) {
    if (size < sizeof(elf64_ehdr_t)) return 0;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)data;

    if (ehdr->magic != ELF_MAGIC) return 0;
    if (ehdr->class != ELF_CLASS_64) return 0;
    if (ehdr->data != ELF_DATA_LSB) return 0;
    if (ehdr->version != 1) return 0;
    if (ehdr->type != ET_EXEC && ehdr->type != ET_DYN) return 0;
    if (ehdr->phentsize != sizeof(elf64_phdr_t)) return 0;
    if (ehdr->phnum == 0) return 0;
    if (ehdr->phoff + ehdr->phnum * sizeof(elf64_phdr_t) > size) return 0;

    return 1;
}

/* ============================================================================
 * ELF Load
 * ============================================================================ */

EFI_STATUS elf64_load(const void *data, size_t size,
                       EFI_PHYSICAL_ADDRESS *load_base,
                       EFI_PHYSICAL_ADDRESS *entry_point,
                       UINT64 *image_size) {
    if (!elf64_validate(data, size)) {
        return EFI_UNSUPPORTED;
    }

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)data;
    const elf64_phdr_t *phdr = (const elf64_phdr_t *)((uint8_t *)data + ehdr->phoff);

    // Find load range
    EFI_PHYSICAL_ADDRESS min_vaddr = (EFI_PHYSICAL_ADDRESS)-1;
    EFI_PHYSICAL_ADDRESS max_vaddr = 0;
    UINT64 total_memsz = 0;

    for (UINTN i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type == PT_LOAD) {
            if (phdr[i].vaddr < min_vaddr) min_vaddr = phdr[i].vaddr;
            EFI_PHYSICAL_ADDRESS end = phdr[i].vaddr + phdr[i].memsz;
            if (end > max_vaddr) max_vaddr = end;
            total_memsz += phdr[i].memsz;
        }
    }

    if (min_vaddr == (EFI_PHYSICAL_ADDRESS)-1) {
        return EFI_UNSUPPORTED;
    }

    // Allocate memory for entire image
    UINT64 image_sz = max_vaddr - min_vaddr;
    image_sz = (image_sz + 4095) & ~4095ULL;

    EFI_PHYSICAL_ADDRESS phys_base;
    EFI_STATUS status = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                             image_sz / 4096, &phys_base);
    if (EFI_ERROR(status)) {
        return status;
    }

    void *virt_base = phys_to_virt(phys_base);
    memset(virt_base, 0, image_sz);

    // Load each PT_LOAD segment
    for (UINTN i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type != PT_LOAD) continue;

        EFI_PHYSICAL_ADDRESS seg_virt = phys_base + (phdr[i].vaddr - min_vaddr);
        void *seg_dest = phys_to_virt(seg_virt);

        if (phdr[i].filesz > 0) {
            memcpy(seg_dest, (uint8_t *)data + phdr[i].offset, phdr[i].filesz);
        }
        // memsz > filesz: bss is already zeroed

        // Set memory permissions (would need page table updates)
        // For now, we just load - permissions handled by page table setup
    }

    // Process relocations if ET_DYN
    if (ehdr->type == ET_DYN) {
        // Find dynamic section
        for (UINTN i = 0; i < ehdr->phnum; i++) {
            if (phdr[i].type == PT_DYNAMIC) {
                // Process dynamic relocations
                // This is complex - placeholder for now
                break;
            }
        }
    }

    // Check for Limoboot note
    for (UINTN i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type == PT_NOTE) {
            const uint8_t *note_ptr = (uint8_t *)data + phdr[i].offset;
            const uint8_t *note_end = note_ptr + phdr[i].filesz;

            while (note_ptr < note_end) {
                const limoboot_note_t *note = (const limoboot_note_t *)note_ptr;
                if (note->type == NT_LIMBOOT && note->namesz == 8 &&
                    memcmp(note->name, "LIMBOOT", 8) == 0) {
                    // Found Limoboot note - kernel supports our protocol
                    efi_printf("Kernel supports Limoboot Protocol v%u.%u\n",
                               note->protocol_version, note->protocol_revision);
                }
                note_ptr += sizeof(uint32_t) * 3 + ((note->namesz + 3) & ~3) +
                           ((note->descsz + 3) & ~3);
            }
        }
    }

    *load_base = phys_base;
    *entry_point = phys_base + (ehdr->entry - min_vaddr);
    *image_size = image_sz;

    return EFI_SUCCESS;
}

/* ============================================================================
 * ELF Symbol Lookup (for dynamic linking)
 * ============================================================================ */

const elf64_sym_t *elf64_find_symbol(const void *data, size_t size, const char *name) {
    if (!elf64_validate(data, size)) return NULL;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)data;
    const elf64_shdr_t *shdr = (const elf64_shdr_t *)((uint8_t *)data + ehdr->shoff);

    // Find string table and symbol table
    const elf64_shdr_t *strtab = NULL;
    const elf64_shdr_t *symtab = NULL;

    for (UINTN i = 0; i < ehdr->shnum; i++) {
        if (shdr[i].type == 3) {  // SHT_STRTAB
            if (strtab == NULL) strtab = &shdr[i];
        } else if (shdr[i].type == 2) {  // SHT_SYMTAB
            symtab = &shdr[i];
        }
    }

    if (!strtab || !symtab) return NULL;

    const char *strtab_data = (const char *)data + strtab->offset;
    const elf64_sym_t *symtab_data = (const elf64_sym_t *)((uint8_t *)data + symtab->offset);
    UINTN sym_count = symtab->size / sizeof(elf64_sym_t);

    for (UINTN i = 0; i < sym_count; i++) {
        if (strcmp(strtab_data + symtab_data[i].name, name) == 0) {
            return &symtab_data[i];
        }
    }

    return NULL;
}

/* ============================================================================
 * Architecture Detection
 * ============================================================================ */

int elf64_get_machine(const void *data) {
    if (!elf64_validate(data, 0)) return -1;
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)data;
    return ehdr->machine;
}

const char *elf64_machine_to_str(int machine) {
    switch (machine) {
        case EM_X86_64:    return "x86-64";
        case EM_AARCH64:   return "aarch64";
        case EM_RISCV:     return "riscv64";
        case EM_LOONGARCH: return "loongarch64";
        default:           return "unknown";
    }
}