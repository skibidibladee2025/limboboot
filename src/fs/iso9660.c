/*
 * ISO9660 Filesystem Driver
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
 * ISO9660 Structures
 * ============================================================================ */

#pragma pack(1)

typedef struct {
    uint8_t  type;
    char     id[5];
    uint8_t  version;
} iso_volume_descriptor_t;

#define ISO_VD_PRIMARY      1
#define ISO_VD_SUPPLEMENTARY 2
#define ISO_VD_PARTITION    3
#define ISO_VD_TERMINATOR   255

typedef struct {
    uint8_t  type;
    char     id[5];
    uint8_t  version;
    uint8_t  unused;
    char     system_id[32];
    char     volume_id[32];
    uint8_t  unused2[8];
    uint32_t volume_space_size_le;
    uint32_t volume_space_size_be;
    uint8_t  unused3[32];
    uint16_t volume_set_size_le;
    uint16_t volume_set_size_be;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint16_t logical_block_size_le;
    uint16_t logical_block_size_be;
    uint32_t path_table_size_le;
    uint32_t path_table_size_be;
    uint32_t type_l_path_table;
    uint32_t opt_type_l_path_table;
    uint32_t type_m_path_table;
    uint32_t opt_type_m_path_table;
    uint8_t  root_dir_record[34];
    char     volume_set_id[128];
    char     publisher_id[128];
    char     preparer_id[128];
    char     application_id[128];
    char     copyright_file_id[37];
    char     abstract_file_id[37];
    char     bibliographic_file_id[37];
    uint8_t  creation_date[17];
    uint8_t  modification_date[17];
    uint8_t  expiration_date[17];
    uint8_t  effective_date[17];
    uint8_t  file_structure_version;
    uint8_t  unused4;
    uint8_t  application_use[512];
    uint8_t  reserved[653];
} iso_primary_descriptor_t;

typedef struct {
    uint8_t  length;
    uint8_t  extended_attr_length;
    uint32_t extent_location_le;
    uint32_t extent_location_be;
    uint32_t data_length_le;
    uint32_t data_length_be;
    uint8_t  recording_date[7];
    uint8_t  flags;
    uint8_t  file_unit_size;
    uint8_t  interleave_gap_size;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint8_t  name_len;
    char     name[];
} iso_dir_record_t;

#pragma pack()

/* ============================================================================
 * ISO9660 Context
 * ============================================================================ */

typedef struct {
    EFI_BLOCK_IO_PROTOCOL *block_io;
    EFI_DISK_IO_PROTOCOL *disk_io;
    EFI_PHYSICAL_ADDRESS partition_start;
    UINT64 partition_size;
    UINT32 block_size;
    UINT32 root_extent;
    UINT32 root_size;
    iso_primary_descriptor_t primary_desc;
} iso_context_t;

/* ============================================================================
 * Read Sector
 * ============================================================================ */

static EFI_STATUS iso_read_sectors(iso_context_t *ctx, UINT64 lba, UINTN count, void *buffer) {
    UINT64 offset = (ctx->partition_start + lba * ctx->block_size);
    UINTN size = count * ctx->block_size;

    if (ctx->disk_io) {
        return ctx->disk_io->ReadDisk(ctx->disk_io, ctx->block_io->Media->MediaId,
                                       offset, size, buffer);
    }
    return ctx->block_io->ReadBlocks(ctx->block_io, ctx->block_io->Media->MediaId,
                                      ctx->partition_start / ctx->block_size + lba,
                                      size, buffer);
}

/* ============================================================================
 * Initialize ISO9660
 * ============================================================================ */

EFI_STATUS iso9660_init(EFI_HANDLE device_handle, EFI_PHYSICAL_ADDRESS partition_start,
                         UINT64 partition_size, iso_context_t **out_ctx) {
    EFI_BLOCK_IO_PROTOCOL *block_io = NULL;
    EFI_DISK_IO_PROTOCOL *disk_io = NULL;

    EFI_STATUS status = g_bs->HandleProtocol(device_handle, &EFI_BLOCK_IO_PROTOCOL_GUID, (VOID **)&block_io);
    if (EFI_ERROR(status)) return status;

    g_bs->HandleProtocol(device_handle, &EFI_DISK_IO_PROTOCOL_GUID, (VOID **)&disk_io);

    iso_context_t *ctx = efi_allocate_pool(sizeof(iso_context_t), EfiBootServicesData);
    if (!ctx) return EFI_OUT_OF_RESOURCES;
    memset(ctx, 0, sizeof(iso_context_t));

    ctx->block_io = block_io;
    ctx->disk_io = disk_io;
    ctx->partition_start = partition_start;
    ctx->partition_size = partition_size;

    /* Read volume descriptor at sector 16 */
    status = iso_read_sectors(ctx, 16, 1, &ctx->primary_desc);
    if (EFI_ERROR(status)) {
        efi_free_pool(ctx);
        return status;
    }

    /* Validate */
    if (ctx->primary_desc.type != ISO_VD_PRIMARY ||
        memcmp(ctx->primary_desc.id, "CD001", 5) != 0) {
        efi_free_pool(ctx);
        return EFI_UNSUPPORTED;
    }

    ctx->block_size = ctx->primary_desc.logical_block_size_le;
    ctx->root_extent = ctx->primary_desc.type_l_path_table;
    ctx->root_size = ctx->primary_desc.path_table_size_le;

    /* Root directory is in the root directory record */
    iso_dir_record_t *root_record = (iso_dir_record_t *)ctx->primary_desc.root_dir_record;
    ctx->root_extent = root_record->extent_location_le;
    ctx->root_size = root_record->data_length_le;

    *out_ctx = ctx;
    return EFI_SUCCESS;
}

/* ============================================================================
 * Find File in Directory
 * ============================================================================ */

static EFI_STATUS iso_find_file(iso_context_t *ctx, UINT32 extent, UINT32 size,
                                 const char *name, UINT32 *file_extent, UINT32 *file_size) {
    UINT8 *buffer = efi_allocate_pool(size, EfiBootServicesData);
    if (!buffer) return EFI_OUT_OF_RESOURCES;

    UINT32 sectors = (size + ctx->block_size - 1) / ctx->block_size;
    EFI_STATUS status = iso_read_sectors(ctx, extent, sectors, buffer);
    if (EFI_ERROR(status)) {
        efi_free_pool(buffer);
        return status;
    }

    UINT8 *ptr = buffer;
    UINT8 *end = buffer + size;

    while (ptr < end) {
        iso_dir_record_t *record = (iso_dir_record_t *)ptr;
        if (record->length == 0) break;

        char entry_name[256];
        UINT8 name_len = record->name_len;
        if (name_len > 0 && name_len < sizeof(entry_name)) {
            memcpy(entry_name, record->name, name_len);
            entry_name[name_len] = '\0';

            /* Remove version number (;1) */
            char *semicolon = strchr(entry_name, ';');
            if (semicolon) *semicolon = '\0';

            if (strcasecmp(entry_name, name) == 0) {
                *file_extent = record->extent_location_le;
                *file_size = record->data_length_le;
                efi_free_pool(buffer);
                return EFI_SUCCESS;
            }
        }

        ptr += record->length;
    }

    efi_free_pool(buffer);
    return EFI_NOT_FOUND;
}

/* ============================================================================
 * Read File
 * ============================================================================ */

EFI_STATUS iso_read_file(iso_context_t *ctx, UINT32 extent, UINT32 size,
                          void *buffer) {
    UINT32 sectors = (size + ctx->block_size - 1) / ctx->block_size;
    return iso_read_sectors(ctx, extent, sectors, buffer);
}

/* ============================================================================
 * Open File by Path
 * ============================================================================ */

EFI_STATUS iso_open(iso_context_t *ctx, const char *path, void **buffer, UINT32 *size) {
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

    UINT32 current_extent = ctx->root_extent;
    UINT32 current_size = ctx->root_size;

    for (int i = 0; i < num_components; i++) {
        UINT32 file_extent, file_size;
        EFI_STATUS status = iso_find_file(ctx, current_extent, current_size,
                                           components[i], &file_extent, &file_size);
        if (EFI_ERROR(status)) return status;

        if (i == num_components - 1) {
            *size = file_size;
            *buffer = efi_allocate_pool(file_size, EfiBootServicesData);
            if (!*buffer) return EFI_OUT_OF_RESOURCES;
            return iso_read_file(ctx, file_extent, file_size, *buffer);
        } else {
            current_extent = file_extent;
            current_size = file_size;
        }
    }

    return EFI_NOT_FOUND;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void iso_free(iso_context_t *ctx) {
    if (ctx) efi_free_pool(ctx);
}