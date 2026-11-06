/*
 * EFI Helper Functions
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limoboot/efi.h>
#include <limoboot/protocol.h>

/* ============================================================================
 * Global EFI Pointers
 * ============================================================================ */

EFI_SYSTEM_TABLE *g_st = NULL;
EFI_BOOT_SERVICES *g_bs = NULL;
EFI_RUNTIME_SERVICES *g_rs = NULL;
EFI_HANDLE g_image_handle = NULL;

/* ============================================================================
 * Initialization
 * ============================================================================ */

void efi_init(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *st) {
    g_image_handle = image_handle;
    g_st = st;
    g_bs = st->BootServices;
    g_rs = st->RuntimeServices;
}

/* ============================================================================
 * Memory Allocation Helpers
 * ============================================================================ */

EFI_STATUS efi_allocate_pages(UINTN pages, EFI_PHYSICAL_ADDRESS *addr, UINTN type) {
    return g_bs->AllocatePages(AllocateAnyPages, type, pages, addr);
}

EFI_STATUS efi_allocate_pages_at(EFI_PHYSICAL_ADDRESS addr, UINTN pages, UINTN type) {
    return g_bs->AllocatePages(AllocateAddress, type, pages, &addr);
}

EFI_STATUS efi_free_pages(EFI_PHYSICAL_ADDRESS addr, UINTN pages) {
    return g_bs->FreePages(addr, pages);
}

void *efi_allocate_pool(UINTN size, UINTN type) {
    void *ptr = NULL;
    g_bs->AllocatePool(type, size, &ptr);
    return ptr;
}

EFI_STATUS efi_free_pool(void *ptr) {
    return g_bs->FreePool(ptr);
}

/* ============================================================================
 * Memory Map
 * ============================================================================ */

EFI_STATUS efi_get_memory_map(UINTN *map_size, EFI_MEMORY_DESCRIPTOR **map,
                               UINTN *map_key, UINTN *desc_size, UINT32 *desc_version) {
    EFI_STATUS status;
    UINTN size = *map_size;

    status = g_bs->GetMemoryMap(&size, *map, map_key, desc_size, desc_version);
    if (status == EFI_BUFFER_TOO_SMALL) {
        *map_size = size + 4096;  // Add some padding
        *map = efi_allocate_pool(*map_size, EfiBootServicesData);
        if (!*map) return EFI_OUT_OF_RESOURCES;
        status = g_bs->GetMemoryMap(map_size, *map, map_key, desc_size, desc_version);
    }
    return status;
}

/* ============================================================================
 * Protocol Helpers
 * ============================================================================ */

EFI_STATUS efi_locate_protocol(EFI_GUID *guid, void **interface) {
    return g_bs->LocateProtocol(guid, NULL, interface);
}

EFI_STATUS efi_handle_protocol(EFI_HANDLE handle, EFI_GUID *guid, void **interface) {
    return g_bs->HandleProtocol(handle, guid, interface);
}

EFI_STATUS efi_open_protocol(EFI_HANDLE handle, EFI_GUID *guid, void **interface,
                              EFI_HANDLE agent, EFI_HANDLE controller, UINT32 attributes) {
    return g_bs->OpenProtocol(handle, guid, interface, agent, controller, attributes);
}

/* ============================================================================
 * Device Path Helpers
 * ============================================================================ */

EFI_DEVICE_PATH_PROTOCOL *efi_append_device_path(EFI_DEVICE_PATH_PROTOCOL *path1,
                                                  EFI_DEVICE_PATH_PROTOCOL *path2) {
    // Simple implementation - in reality would need proper DP manipulation
    return NULL;  // Placeholder
}

CHAR16 *efi_device_path_to_str(EFI_DEVICE_PATH_PROTOCOL *path) {
    // Placeholder
    return L"Unknown Device";
}

/* ============================================================================
 * Event Helpers
 * ============================================================================ */

EFI_STATUS efi_create_event(UINT32 type, UINTN notify_tpl,
                             void (EFIAPI *notify)(EFI_EVENT, void *),
                             void *ctx, EFI_EVENT *event) {
    return g_bs->CreateEvent(type, notify_tpl, notify, ctx, event);
}

EFI_STATUS efi_set_timer(EFI_EVENT event, UINTN type, UINT64 trigger_time) {
    return g_bs->SetTimer(event, type, trigger_time);
}

EFI_STATUS efi_wait_for_event(UINTN num_events, EFI_EVENT *events, UINTN *index) {
    return g_bs->WaitForEvent(num_events, events, index);
}

EFI_STATUS efi_signal_event(EFI_EVENT event) {
    return g_bs->SignalEvent(event);
}

EFI_STATUS efi_close_event(EFI_EVENT event) {
    return g_bs->CloseEvent(event);
}

/* ============================================================================
 * Time Helpers
 * ============================================================================ */

EFI_STATUS efi_get_time(EFI_TIME *time, EFI_TIME_CAPABILITIES *caps) {
    return g_rs->GetTime(time, caps);
}

EFI_STATUS efi_set_time(EFI_TIME *time) {
    return g_rs->SetTime(time);
}

/* ============================================================================
 * Variable Helpers
 * ============================================================================ */

EFI_STATUS efi_get_variable(CHAR16 *name, EFI_GUID *guid, UINT32 *attrs,
                             UINTN *size, void *data) {
    return g_rs->GetVariable(name, guid, attrs, size, data);
}

EFI_STATUS efi_set_variable(CHAR16 *name, EFI_GUID *guid, UINT32 attrs,
                             UINTN size, void *data) {
    return g_rs->SetVariable(name, guid, attrs, size, data);
}

EFI_STATUS efi_get_next_variable(UINTN *name_size, CHAR16 *name, EFI_GUID *guid) {
    return g_rs->GetNextVariableName(name_size, name, guid);
}

/* ============================================================================
 * Reset
 * ============================================================================ */

EFI_STATUS efi_reset_system(EFI_RESET_TYPE type, EFI_STATUS status,
                             UINTN data_size, CHAR16 *data) {
    return g_rs->ResetSystem(type, status, data_size, data);
}

/* ============================================================================
 * Exit Boot Services
 * ============================================================================ */

EFI_STATUS efi_exit_boot_services(EFI_HANDLE image_handle, UINTN map_key) {
    return g_bs->ExitBootServices(image_handle, map_key);
}

/* ============================================================================
 * Configuration Table Lookup
 * ============================================================================ */

void *efi_get_config_table(EFI_GUID *guid) {
    for (UINTN i = 0; i < g_st->NumberOfTableEntries; i++) {
        if (guid_eq(&g_st->ConfigurationTable[i].VendorGuid, guid)) {
            return g_st->ConfigurationTable[i].VendorTable;
        }
    }
    return NULL;
}

/* ============================================================================
 * Image Load/Start
 * ============================================================================ */

EFI_STATUS efi_load_image(BOOLEAN boot_policy, EFI_DEVICE_PATH_PROTOCOL *path,
                           void *source, UINTN source_size, EFI_HANDLE *handle) {
    return g_bs->LoadImage(boot_policy, g_image_handle, path, source, source_size, handle);
}

EFI_STATUS efi_start_image(EFI_HANDLE handle, UINTN *exit_data_size, CHAR16 **exit_data) {
    return g_bs->StartImage(handle, exit_data_size, exit_data);
}

EFI_STATUS efi_unload_image(EFI_HANDLE handle) {
    return g_bs->UnloadImage(handle);
}

/* ============================================================================
 * Stall
 * ============================================================================ */

void efi_stall(UINTN microseconds) {
    g_bs->Stall(microseconds);
}

/* ============================================================================
 * Console Helpers
 * ============================================================================ */

void efi_clear_screen(void) {
    if (g_st && g_st->ConOut) {
        g_st->ConOut->ClearScreen(g_st->ConOut);
    }
}

void efi_set_cursor(UINTN col, UINTN row) {
    if (g_st && g_st->ConOut) {
        g_st->ConOut->SetCursorPosition(g_st->ConOut, col, row);
    }
}

void efi_enable_cursor(BOOLEAN visible) {
    if (g_st && g_st->ConOut) {
        g_st->ConOut->EnableCursor(g_st->ConOut, visible);
    }
}

void efi_set_attribute(UINTN attr) {
    if (g_st && g_st->ConOut) {
        g_st->ConOut->SetAttribute(g_st->ConOut, attr);
    }
}

/* ============================================================================
 * GUID Helpers
 * ============================================================================ */

int guid_eq(const EFI_GUID *g1, const EFI_GUID *g2) {
    return g1->Data1 == g2->Data1 &&
           g1->Data2 == g2->Data2 &&
           g1->Data3 == g2->Data3 &&
           __builtin_memcmp(g1->Data4, g2->Data4, 8) == 0;
}