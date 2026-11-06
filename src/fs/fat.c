/*
 * FAT12/16/32 Filesystem Driver
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
 * FAT Structures
 * ============================================================================ */

#pragma pack(1)

typedef struct {
    uint8_t  jump[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} fat_bpb_t;

typedef struct {
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat_bpb_fat32_t;

typedef struct {
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat_bpb_fat1216_t;

typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  ntres;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat_dir_entry_t;

#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LONG_NAME  0x0F

#define FAT_LONG_NAME_LAST  0x40

#pragma pack()

/* ============================================================================
 * FAT Context
 * ============================================================================ */

typedef struct {
    EFI_BLOCK_IO_PROTOCOL *block_io;
    EFI_DISK_IO_PROTOCOL *disk_io;
    EFI_PHYSICAL_ADDRESS partition_start;
    UINT64 partition_size;

    fat_bpb_t bpb;
    UINT32 fat_size;
    UINT32 root_dir_sectors;
    UINT32 first_data_sector;
    UINT32 first_fat_sector;
    UINT32 total_clusters;
    UINT8  fat_type;  /* 12, 16, 32 */

    UINT8 *fat_cache;
    UINT32 fat_cache_sector;
} fat_context_t;

/* ============================================================================
 * Read Sector
 * ============================================================================ */

static EFI_STATUS fat_read_sectors(fat_context_t *ctx, UINT64 lba, UINTN count, void *buffer) {
    if (ctx->disk_io) {
        return ctx->disk_io->ReadDisk(ctx->disk_io, ctx->block_io->Media->MediaId,
                                       (ctx->partition_start + lba) * ctx->block_io->Media->BlockSize,
                                       count * ctx->block_io->Media->BlockSize, buffer);
    }
    return ctx->block_io->ReadBlocks(ctx->block_io, ctx->block_io->Media->MediaId,
                                      ctx->partition_start + lba, count * ctx->block_io->Media->BlockSize, buffer);
}

/* ============================================================================
 * FAT Cache
 * ============================================================================ */

static EFI_STATUS fat_load_fat_sector(fat_context_t *ctx, UINT32 sector) {
    if (ctx->fat_cache_sector == sector) return EFI_SUCCESS;

    EFI_STATUS status = fat_read_sectors(ctx, ctx->first_fat_sector + sector, 1, ctx->fat_cache);
    if (!EFI_ERROR(status)) {
        ctx->fat_cache_sector = sector;
    }
    return status;
}

static UINT32 fat_get_next_cluster(fat_context_t *ctx, UINT32 cluster) {
    UINT32 fat_offset = cluster * (ctx->fat_type == 32 ? 4 : (ctx->fat_type == 16 ? 2 : 1.5));
    UINT32 sector = fat_offset / ctx->bpb.bytes_per_sector;
    UINT32 offset = fat_offset % ctx->bpb.bytes_per_sector;

    EFI_STATUS status = fat_load_fat_sector(ctx, sector);
    if (EFI_ERROR(status)) return 0xFFFFFFFF;

    if (ctx->fat_type == 32) {
        return *(uint32_t *)(ctx->fat_cache + offset) & 0x0FFFFFFF;
    } else if (ctx->fat_type == 16) {
        return *(uint16_t *)(ctx->fat_cache + offset);
    } else {
        /* FAT12 - handle 12-bit entries */
        uint16_t val = *(uint16_t *)(ctx->fat_cache + offset);
        if (cluster & 1) {
            return (val >> 4) & 0xFFF;
        } else {
            return val & 0xFFF;
        }
    }
}

/* ============================================================================
 * Cluster to LBA
 * ============================================================================ */

static UINT64 fat_cluster_to_lba(fat_context_t *ctx, UINT32 cluster) {
    if (cluster < 2) return 0;
    return ctx->first_data_sector + (UINT64)(cluster - 2) * ctx->bpb.sectors_per_cluster;
}

/* ============================================================================
 * Directory Entry Parsing
 * ============================================================================ */

static int fat_parse_dir_entry(fat_dir_entry_t *entry, char *name, size_t name_len,
                                UINT64 *file_size, UINT32 *first_cluster, UINT8 *attr) {
    if (entry->name[0] == 0x00) return 0;  /* End of directory */
    if (entry->name[0] == 0xE5) return 0;  /* Deleted */

    if (entry->attr == FAT_ATTR_LONG_NAME) {
        /* Long filename entry - skip for now */
        return 0;
    }

    if (entry->attr & FAT_ATTR_VOLUME_ID) return 0;

    *attr = entry->attr;

    /* Build filename */
    size_t pos = 0;
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        if (pos < name_len - 1) name[pos++] = entry->name[i];
    }
    if (entry->ext[0] != ' ') {
        if (pos < name_len - 1) name[pos++] = '.';
        for (int i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            if (pos < name_len - 1) name[pos++] = entry->ext[i];
        }
    }
    name[pos] = '\0';

    *file_size = entry->file_size;
    *first_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;

    return 1;
}

/* ============================================================================
 * Find File in Directory
 * ============================================================================ */

static EFI_STATUS fat_find_file(fat_context_t *ctx, UINT32 dir_cluster, const char *name,
                                 UINT64 *file_size, UINT32 *first_cluster, UINT8 *attr) {
    char entry_name[256];
    UINT32 cluster = dir_cluster;

    while (cluster < 0xFFFFFF8) {
        UINT64 lba = fat_cluster_to_lba(ctx, cluster);
        if (lba == 0 && cluster != 2) break;

        UINTN sectors = ctx->bpb.sectors_per_cluster;
        fat_dir_entry_t *entries = efi_allocate_pool(sectors * ctx->bpb.bytes_per_sector, EfiBootServicesData);
        if (!entries) return EFI_OUT_OF_RESOURCES;

        EFI_STATUS status = fat_read_sectors(ctx, lba, sectors, entries);
        if (EFI_ERROR(status)) {
            efi_free_pool(entries);
            return status;
        }

        UINT32 entries_per_sector = ctx->bpb.bytes_per_sector / sizeof(fat_dir_entry_t);
        UINT32 total_entries = entries_per_sector * sectors;

        for (UINT32 i = 0; i < total_entries; i++) {
            if (fat_parse_dir_entry(&entries[i], entry_name, sizeof(entry_name),
                                     file_size, first_cluster, attr)) {
                if (strcmp(entry_name, name) == 0) {
                    efi_free_pool(entries);
                    return EFI_SUCCESS;
                }
            }
        }

        efi_free_pool(entries);
        cluster = fat_get_next_cluster(ctx, cluster);
    }

    return EFI_NOT_FOUND;
}

/* ============================================================================
 * Read File
 * ============================================================================ */

EFI_STATUS fat_read_file(fat_context_t *ctx, UINT32 cluster, void *buffer, UINT64 size) {
    UINT64 remaining = size;
    uint8_t *dest = (uint8_t *)buffer;
    UINT32 current_cluster = cluster;

    while (remaining > 0 && current_cluster < 0xFFFFFF8) {
        UINT64 lba = fat_cluster_to_lba(ctx, current_cluster);
        if (lba == 0) break;

        UINTN sectors = ctx->bpb.sectors_per_cluster;
        UINT64 cluster_size = (UINT64)sectors * ctx->bpb.bytes_per_sector;
        UINT64 to_read = remaining < cluster_size ? remaining : cluster_size;

        EFI_STATUS status = fat_read_sectors(ctx, lba, sectors, dest);
        if (EFI_ERROR(status)) return status;

        dest += to_read;
        remaining -= to_read;
        current_cluster = fat_get_next_cluster(ctx, current_cluster);
    }

    return remaining == 0 ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

/* ============================================================================
 * Initialize FAT Context
 * ============================================================================ */

EFI_STATUS fat_init(EFI_HANDLE device_handle, EFI_PHYSICAL_ADDRESS partition_start,
                     UINT64 partition_size, fat_context_t **out_ctx) {
    EFI_BLOCK_IO_PROTOCOL *block_io = NULL;
    EFI_DISK_IO_PROTOCOL *disk_io = NULL;

    EFI_STATUS status = g_bs->HandleProtocol(device_handle, &EFI_BLOCK_IO_PROTOCOL_GUID, (VOID **)&block_io);
    if (EFI_ERROR(status)) return status;

    g_bs->HandleProtocol(device_handle, &EFI_DISK_IO_PROTOCOL_GUID, (VOID **)&disk_io);

    fat_context_t *ctx = efi_allocate_pool(sizeof(fat_context_t), EfiBootServicesData);
    if (!ctx) return EFI_OUT_OF_RESOURCES;
    memset(ctx, 0, sizeof(fat_context_t));

    ctx->block_io = block_io;
    ctx->disk_io = disk_io;
    ctx->partition_start = partition_start;
    ctx->partition_size = partition_size;

    /* Read BPB */
    status = fat_read_sectors(ctx, 0, 1, &ctx->bpb);
    if (EFI_ERROR(status)) {
        efi_free_pool(ctx);
        return status;
    }

    /* Validate BPB */
    if (ctx->bpb.bytes_per_sector != 512 &&
        ctx->bpb.bytes_per_sector != 1024 &&
        ctx->bpb.bytes_per_sector != 2048 &&
        ctx->bpb.bytes_per_sector != 4096) {
        efi_free_pool(ctx);
        return EFI_UNSUPPORTED;
    }

    if (ctx->bpb.sectors_per_cluster == 0 ||
        (ctx->bpb.sectors_per_cluster & (ctx->bpb.sectors_per_cluster - 1)) != 0) {
        efi_free_pool(ctx);
        return EFI_UNSUPPORTED;
    }

    /* Calculate FAT parameters */
    ctx->fat_size = ctx->bpb.fat_size_16 ? ctx->bpb.fat_size_16 :
                    ((fat_bpb_fat32_t *)((uint8_t *)&ctx->bpb + sizeof(fat_bpb_t)))->fat_size_32;

    ctx->root_dir_sectors = ((ctx->bpb.root_entries * 32) + (ctx->bpb.bytes_per_sector - 1)) / ctx->bpb.bytes_per_sector;
    ctx->first_fat_sector = ctx->bpb.reserved_sectors;
    ctx->first_data_sector = ctx->bpb.reserved_sectors + ctx->bpb.num_fats * ctx->fat_size + ctx->root_dir_sectors;

    UINT64 total_sectors = ctx->bpb.total_sectors_16 ? ctx->bpb.total_sectors_16 : ctx->bpb.total_sectors_32;
    UINT64 data_sectors = total_sectors - ctx->first_data_sector;
    ctx->total_clusters = data_sectors / ctx->bpb.sectors_per_cluster;

    /* Determine FAT type */
    if (ctx->total_clusters < 4085) {
        ctx->fat_type = 12;
    } else if (ctx->total_clusters < 65525) {
        ctx->fat_type = 16;
    } else {
        ctx->fat_type = 32;
    }

    /* Allocate FAT cache */
    ctx->fat_cache = efi_allocate_pool(ctx->bpb.bytes_per_sector, EfiBootServicesData);
    if (!ctx->fat_cache) {
        efi_free_pool(ctx);
        return EFI_OUT_OF_RESOURCES;
    }
    ctx->fat_cache_sector = 0xFFFFFFFF;

    *out_ctx = ctx;
    return EFI_SUCCESS;
}

/* ============================================================================
 * Open File by Path
 * ============================================================================ */

EFI_STATUS fat_open(fat_context_t *ctx, const char *path, void **buffer, UINT64 *size) {
    /* Parse path components */
    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *components[64];
    int num_components = 0;
    char *token = strtok(path_copy, "/\\");
    while (token && num_components < 64) {
        components[num_components++] = token;
        token = strtok(NULL, "/\\");
    }

    if (num_components == 0) return EFI_NOT_FOUND;

    /* Start from root directory */
    UINT32 dir_cluster;
    if (ctx->fat_type == 32) {
        fat_bpb_fat32_t *fat32 = (fat_bpb_fat32_t *)((uint8_t *)&ctx->bpb + sizeof(fat_bpb_t));
        dir_cluster = fat32->root_cluster;
    } else {
        dir_cluster = 0;  /* Root directory for FAT12/16 */
    }

    UINT64 file_size = 0;
    UINT32 first_cluster = 0;
    UINT8 attr = 0;

    for (int i = 0; i < num_components; i++) {
        EFI_STATUS status = fat_find_file(ctx, dir_cluster, components[i], &file_size, &first_cluster, &attr);
        if (EFI_ERROR(status)) return status;

        if (i == num_components - 1) {
            /* Last component - this is the file */
            if (attr & FAT_ATTR_DIRECTORY) return EFI_NOT_FOUND;

            *size = file_size;
            *buffer = efi_allocate_pool((UINTN)file_size, EfiBootServicesData);
            if (!*buffer) return EFI_OUT_OF_RESOURCES;

            return fat_read_file(ctx, first_cluster, *buffer, file_size);
        } else {
            /* Directory traversal */
            if (!(attr & FAT_ATTR_DIRECTORY)) return EFI_NOT_FOUND;
            dir_cluster = first_cluster;
        }
    }

    return EFI_NOT_FOUND;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void fat_free(fat_context_t *ctx) {
    if (ctx) {
        if (ctx->fat_cache) efi_free_pool(ctx->fat_cache);
        efi_free_pool(ctx);
    }
}