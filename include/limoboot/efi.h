/*
 * UEFI Types and Definitions for limoboot
 * Minimal freestanding UEFI header - no external dependencies
 *
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef LIMBOOT_EFI_H
#define LIMBOOT_EFI_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Basic Types
 * ============================================================================ */

typedef uint8_t   BOOLEAN;
typedef int8_t    INT8;
typedef uint8_t   UINT8;
typedef int16_t   INT16;
typedef uint16_t  UINT16;
typedef int32_t   INT32;
typedef uint32_t  UINT32;
typedef int64_t   INT64;
typedef uint64_t  UINT64;
typedef char      CHAR8;
typedef uint16_t  CHAR16;
typedef void      VOID;
typedef UINTN     UINTN;
typedef INTN      INTN;

#if defined(__x86_64__)
typedef uint64_t  UINTN;
typedef int64_t   INTN;
#elif defined(__aarch64__)
typedef uint64_t  UINTN;
typedef int64_t   INTN;
#elif defined(__riscv) && (__riscv_xlen == 64)
typedef uint64_t  UINTN;
typedef int64_t   INTN;
#elif defined(__loongarch64)
typedef uint64_t  UINTN;
typedef int64_t   INTN;
#else
#error "Unsupported architecture"
#endif

/* ============================================================================
 * Status Codes
 * ============================================================================ */

typedef UINTN EFI_STATUS;

#define EFI_SUCCESS                0
#define EFI_LOAD_ERROR           (1ULL | (1ULL << 63))
#define EFI_INVALID_PARAMETER    (2ULL | (1ULL << 63))
#define EFI_UNSUPPORTED          (3ULL | (1ULL << 63))
#define EFI_BAD_BUFFER_SIZE      (4ULL | (1ULL << 63))
#define EFI_BUFFER_TOO_SMALL     (5ULL | (1ULL << 63))
#define EFI_NOT_READY            (6ULL | (1ULL << 63))
#define EFI_DEVICE_ERROR         (7ULL | (1ULL << 63))
#define EFI_WRITE_PROTECTED      (8ULL | (1ULL << 63))
#define EFI_OUT_OF_RESOURCES     (9ULL | (1ULL << 63))
#define EFI_VOLUME_CORRUPTED    (10ULL | (1ULL << 63))
#define EFI_VOLUME_FULL         (11ULL | (1ULL << 63))
#define EFI_NO_MEDIA            (12ULL | (1ULL << 63))
#define EFI_MEDIA_CHANGED       (13ULL | (1ULL << 63))
#define EFI_NOT_FOUND           (14ULL | (1ULL << 63))
#define EFI_ACCESS_DENIED       (15ULL | (1ULL << 63))
#define EFI_NO_RESPONSE         (16ULL | (1ULL << 63))
#define EFI_NO_MAPPING          (17ULL | (1ULL << 63))
#define EFI_TIMEOUT             (18ULL | (1ULL << 63))
#define EFI_NOT_STARTED         (19ULL | (1ULL << 63))
#define EFI_ALREADY_STARTED     (20ULL | (1ULL << 63))
#define EFI_ABORTED             (21ULL | (1ULL << 63))
#define EFI_ICMP_ERROR          (22ULL | (1ULL << 63))
#define EFI_TFTP_ERROR          (23ULL | (1ULL << 63))
#define EFI_PROTOCOL_ERROR      (24ULL | (1ULL << 63))
#define EFI_INCOMPATIBLE_VERSION (25ULL | (1ULL << 63))
#define EFI_SECURITY_VIOLATION  (26ULL | (1ULL << 63))
#define EFI_CRC_ERROR           (27ULL | (1ULL << 63))
#define EFI_END_OF_MEDIA        (28ULL | (1ULL << 63))
#define EFI_END_OF_FILE         (31ULL | (1ULL << 63))
#define EFI_INVALID_LANGUAGE    (32ULL | (1ULL << 63))
#define EFI_COMPROMISED_DATA    (33ULL | (1ULL << 63))
#define EFI_IP_ADDRESS_CONFLICT (34ULL | (1ULL << 63))
#define EFI_HTTP_ERROR          (35ULL | (1ULL << 63))

#define EFI_ERROR(status) (((EFI_STATUS)(status)) & (1ULL << 63))

/* ============================================================================
 * GUID
 * ============================================================================ */

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

#define EFI_GUID_EQ(g1, g2) \
    ((g1).Data1 == (g2).Data1 && \
     (g1).Data2 == (g2).Data2 && \
     (g1).Data3 == (g2).Data3 && \
     __builtin_memcmp((g1).Data4, (g2).Data4, 8) == 0)

/* ============================================================================
 * Table Header
 * ============================================================================ */

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* ============================================================================
 * Memory Types
 * ============================================================================ */

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/* ============================================================================
 * Memory Descriptor
 * ============================================================================ */

typedef struct {
    UINT32 Type;
    UINT32 Padding;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

#define EFI_MEMORY_UC              (1ULL << 0)
#define EFI_MEMORY_WC              (1ULL << 1)
#define EFI_MEMORY_WT              (1ULL << 2)
#define EFI_MEMORY_WB              (1ULL << 3)
#define EFI_MEMORY_UCE             (1ULL << 4)
#define EFI_MEMORY_WP              (1ULL << 5)
#define EFI_MEMORY_RP              (1ULL << 6)
#define EFI_MEMORY_XP              (1ULL << 7)
#define EFI_MEMORY_NV              (1ULL << 8)
#define EFI_MEMORY_MORE_RELIABLE   (1ULL << 9)
#define EFI_MEMORY_RO              (1ULL << 10)
#define EFI_MEMORY_RUNTIME         (1ULL << 11)
#define EFI_MEMORY_ISA_VALID       (1ULL << 12)
#define EFI_MEMORY_ISA_MASK        (0xFFULL << 13)
#define EFI_MEMORY_CPU_CRYPTO      (1ULL << 21)
#define EFI_MEMORY_HOT_PLUGGABLE   (1ULL << 22)
#define EFI_MEMORY_ALLOCATED       (1ULL << 23)

/* ============================================================================
 * Time
 * ============================================================================ */

typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

typedef struct {
    UINT32 Resolution;
    UINT32 Accuracy;
    BOOLEAN SetsToZero;
} EFI_TIME_CAPABILITIES;

/* ============================================================================
 * Reset Types
 * ============================================================================ */

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

/* ============================================================================
 * Capsule
 * ============================================================================ */

typedef struct {
    EFI_GUID CapsuleGuid;
    UINT32 HeaderSize;
    UINT32 Flags;
    UINT32 CapsuleImageSize;
} EFI_CAPSULE_HEADER;

#define CAPSULE_FLAGS_PERSIST_ACROSS_RESET  (1ULL << 0)
#define CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE (1ULL << 1)
#define CAPSULE_FLAGS_INITIATE_RESET        (1ULL << 2)

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct _EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;
typedef struct _EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL;
typedef struct _EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE_PROTOCOL;
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;
typedef struct _EFI_DISK_IO_PROTOCOL EFI_DISK_IO_PROTOCOL;
typedef struct _EFI_TCG2_PROTOCOL EFI_TCG2_PROTOCOL;

/* ============================================================================
 * Boot Services
 * ============================================================================ */

typedef EFI_STATUS (EFIAPI *EFI_RAISE_TPL)(UINTN NewTpl);
typedef EFI_STATUS (EFIAPI *EFI_RESTORE_TPL)(UINTN OldTpl);

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES)(
    UINTN Type, UINTN MemoryType, UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory
);
typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES)(
    EFI_PHYSICAL_ADDRESS Memory, UINTN Pages
);
typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    UINTN *MemoryMapSize, EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion
);
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(
    UINTN PoolType, UINTN Size, VOID **Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(VOID *Buffer);

typedef EFI_STATUS (EFIAPI *EFI_CREATE_EVENT)(
    UINT32 Type, UINTN NotifyTpl,
    VOID (EFIAPI *NotifyFunction)(EFI_EVENT Event, VOID *Context),
    VOID *Context, EFI_EVENT *Event
);
typedef EFI_STATUS (EFIAPI *EFI_SET_TIMER)(
    EFI_EVENT Event, UINTN Type, UINT64 TriggerTime
);
typedef EFI_STATUS (EFIAPI *EFI_WAIT_FOR_EVENT)(
    UINTN NumberOfEvents, EFI_EVENT *Event, UINTN *Index
);
typedef EFI_STATUS (EFIAPI *EFI_SIGNAL_EVENT)(EFI_EVENT Event);
typedef EFI_STATUS (EFIAPI *EFI_CLOSE_EVENT)(EFI_EVENT Event);
typedef EFI_STATUS (EFIAPI *EFI_CHECK_EVENT)(EFI_EVENT Event);

typedef EFI_STATUS (EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE *Handle, EFI_GUID *Protocol, UINTN InterfaceType, VOID *Interface
);
typedef EFI_STATUS (EFIAPI *EFI_REINSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE Handle, EFI_GUID *Protocol, VOID *OldInterface, VOID *NewInterface
);
typedef EFI_STATUS (EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE Handle, EFI_GUID *Protocol, VOID *Interface
);
typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(
    EFI_HANDLE Handle, EFI_GUID *Protocol, VOID **Interface
);
typedef EFI_STATUS (EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY)(
    EFI_GUID *Protocol, EFI_EVENT Event, VOID **Registration
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE)(
    UINTN SearchType, EFI_GUID *Protocol, VOID *SearchKey,
    UINTN *BufferSize, EFI_HANDLE *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_DEVICE_PATH)(
    EFI_GUID *Protocol, EFI_DEVICE_PATH_PROTOCOL **DevicePath, EFI_HANDLE *Device
);
typedef EFI_STATUS (EFIAPI *EFI_INSTALL_CONFIGURATION_TABLE)(
    EFI_GUID *Guid, VOID *Table
);

typedef EFI_STATUS (EFIAPI *EFI_IMAGE_LOAD)(
    BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath, VOID *SourceBuffer,
    UINTN SourceSize, EFI_HANDLE *ImageHandle
);
typedef EFI_STATUS (EFIAPI *EFI_IMAGE_START)(
    EFI_HANDLE ImageHandle, UINTN *ExitDataSize, CHAR16 **ExitData
);
typedef EFI_STATUS (EFIAPI *EFI_EXIT)(
    EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus,
    UINTN ExitDataSize, CHAR16 *ExitData
);
typedef EFI_STATUS (EFIAPI *EFI_IMAGE_UNLOAD)(EFI_HANDLE ImageHandle);
typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle, UINTN MapKey
);
typedef EFI_STATUS (EFIAPI *EFI_GET_NEXT_MONOTONIC_COUNT)(UINT64 *Count);
typedef EFI_STATUS (EFIAPI *EFI_STALL)(UINTN Microseconds);
typedef EFI_STATUS (EFIAPI *EFI_SET_WATCHDOG_TIMER)(
    UINTN Timeout, UINT64 WatchdogCode, UINTN DataSize, CHAR16 *WatchdogData
);
typedef EFI_STATUS (EFIAPI *EFI_CONNECT_CONTROLLER)(
    EFI_HANDLE ControllerHandle, EFI_HANDLE *DriverImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath, BOOLEAN Recursive
);
typedef EFI_STATUS (EFIAPI *EFI_DISCONNECT_CONTROLLER)(
    EFI_HANDLE ControllerHandle, EFI_HANDLE DriverImageHandle,
    EFI_HANDLE ChildHandle
);
typedef EFI_STATUS (EFIAPI *EFI_OPEN_PROTOCOL)(
    EFI_HANDLE Handle, EFI_GUID *Protocol, VOID **Interface,
    EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle, UINT32 Attributes
);
typedef EFI_STATUS (EFIAPI *EFI_CLOSE_PROTOCOL)(
    EFI_HANDLE Handle, EFI_GUID *Protocol, EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle
);
typedef EFI_STATUS (EFIAPI *EFI_OPEN_PROTOCOL_INFORMATION)(
    EFI_HANDLE Handle, EFI_GUID *Protocol,
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer, UINTN *EntryCount
);
typedef EFI_STATUS (EFIAPI *EFI_PROTOCOLS_PER_HANDLE)(
    EFI_HANDLE Handle, EFI_GUID ***ProtocolBuffer, UINTN *ProtocolBufferCount
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(
    UINTN SearchType, EFI_GUID *Protocol, VOID *SearchKey,
    UINTN *NoHandles, EFI_HANDLE **Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(
    EFI_GUID *Protocol, VOID *Registration, VOID **Interface
);
typedef EFI_STATUS (EFIAPI *EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    EFI_HANDLE Handle, ...
);
typedef EFI_STATUS (EFIAPI *EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    EFI_HANDLE Handle, ...
);

typedef EFI_STATUS (EFIAPI *EFI_CALCULATE_CRC32)(
    VOID *Data, UINTN DataSize, UINT32 *Crc32
);
typedef EFI_STATUS (EFIAPI *EFI_COPY_MEM)(VOID *Destination, VOID *Source, UINTN Length);
typedef EFI_STATUS (EFIAPI *EFI_SET_MEM)(VOID *Buffer, UINTN Size, UINT8 Value);
typedef EFI_STATUS (EFIAPI *EFI_CREATE_EVENT_EX)(
    UINT32 Type, UINTN NotifyTpl,
    VOID (EFIAPI *NotifyFunction)(EFI_EVENT Event, VOID *Context),
    VOID *Context, EFI_GUID *EventGroup, EFI_EVENT *Event
);

/* ============================================================================
 * Boot Services Table
 * ============================================================================ */

struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    EFI_RAISE_TPL RaiseTPL;
    EFI_RESTORE_TPL RestoreTPL;
    EFI_ALLOCATE_PAGES AllocatePages;
    EFI_FREE_PAGES FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;
    EFI_CREATE_EVENT CreateEvent;
    EFI_SET_TIMER SetTimer;
    EFI_WAIT_FOR_EVENT WaitForEvent;
    EFI_SIGNAL_EVENT SignalEvent;
    EFI_CLOSE_EVENT CloseEvent;
    EFI_CHECK_EVENT CheckEvent;
    EFI_INSTALL_PROTOCOL_INTERFACE InstallProtocolInterface;
    EFI_REINSTALL_PROTOCOL_INTERFACE ReinstallProtocolInterface;
    EFI_UNINSTALL_PROTOCOL_INTERFACE UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    VOID *Reserved;
    EFI_REGISTER_PROTOCOL_NOTIFY RegisterProtocolNotify;
    EFI_LOCATE_HANDLE LocateHandle;
    EFI_LOCATE_DEVICE_PATH LocateDevicePath;
    EFI_INSTALL_CONFIGURATION_TABLE InstallConfigurationTable;
    EFI_IMAGE_LOAD LoadImage;
    EFI_IMAGE_START StartImage;
    EFI_EXIT Exit;
    EFI_IMAGE_UNLOAD UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;
    EFI_GET_NEXT_MONOTONIC_COUNT GetNextMonotonicCount;
    EFI_STALL Stall;
    EFI_SET_WATCHDOG_TIMER SetWatchdogTimer;
    EFI_CONNECT_CONTROLLER ConnectController;
    EFI_DISCONNECT_CONTROLLER DisconnectController;
    EFI_OPEN_PROTOCOL OpenProtocol;
    EFI_CLOSE_PROTOCOL CloseProtocol;
    EFI_OPEN_PROTOCOL_INFORMATION OpenProtocolInformation;
    EFI_PROTOCOLS_PER_HANDLE ProtocolsPerHandle;
    EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
    EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES InstallMultipleProtocolInterfaces;
    EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES UninstallMultipleProtocolInterfaces;
    EFI_CALCULATE_CRC32 CalculateCrc32;
    EFI_COPY_MEM CopyMem;
    EFI_SET_MEM SetMem;
    EFI_CREATE_EVENT_EX CreateEventEx;
};

/* ============================================================================
 * Runtime Services
 * ============================================================================ */

typedef EFI_STATUS (EFIAPI *EFI_GET_TIME)(EFI_TIME *Time, EFI_TIME_CAPABILITIES *Capabilities);
typedef EFI_STATUS (EFIAPI *EFI_SET_TIME)(EFI_TIME *Time);
typedef EFI_STATUS (EFIAPI *EFI_GET_WAKEUP_TIME)(BOOLEAN *Enabled, BOOLEAN *Pending, EFI_TIME *Time);
typedef EFI_STATUS (EFIAPI *EFI_SET_WAKEUP_TIME)(BOOLEAN Enabled, EFI_TIME *Time);
typedef EFI_STATUS (EFIAPI *EFI_SET_VIRTUAL_ADDRESS_MAP)(
    UINTN MemoryMapSize, UINTN DescriptorSize, UINT32 DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap
);
typedef EFI_STATUS (EFIAPI *EFI_CONVERT_POINTER)(UINTN DebugDisposition, VOID **Address);
typedef EFI_STATUS (EFIAPI *EFI_GET_VARIABLE)(
    CHAR16 *VariableName, EFI_GUID *VendorGuid, UINT32 *Attributes,
    UINTN *DataSize, VOID *Data
);
typedef EFI_STATUS (EFIAPI *EFI_GET_NEXT_VARIABLE_NAME)(
    UINTN *VariableNameSize, CHAR16 *VariableName, EFI_GUID *VendorGuid
);
typedef EFI_STATUS (EFIAPI *EFI_SET_VARIABLE)(
    CHAR16 *VariableName, EFI_GUID *VendorGuid, UINT32 Attributes,
    UINTN DataSize, VOID *Data
);
typedef EFI_STATUS (EFIAPI *EFI_GET_NEXT_HIGH_MONOTONIC_COUNT)(UINT32 *HighCount);
typedef EFI_STATUS (EFIAPI *EFI_RESET_SYSTEM)(
    EFI_RESET_TYPE ResetType, EFI_STATUS ResetStatus, UINTN DataSize, CHAR16 *ResetData
);
typedef EFI_STATUS (EFIAPI *EFI_UPDATE_CAPSULE)(
    EFI_CAPSULE_HEADER **CapsuleHeaderArray, UINTN CapsuleCount,
    EFI_PHYSICAL_ADDRESS ScatterGatherList
);
typedef EFI_STATUS (EFIAPI *EFI_QUERY_CAPSULE_CAPABILITIES)(
    EFI_CAPSULE_HEADER **CapsuleHeaderArray, UINTN CapsuleCount,
    UINT64 *MaximumCapsuleSize, EFI_RESET_TYPE *ResetType
);
typedef EFI_STATUS (EFIAPI *EFI_QUERY_VARIABLE_INFO)(
    UINT32 Attributes, UINT64 *MaximumVariableStorageSize,
    UINT64 *RemainingVariableStorageSize, UINT64 *MaximumVariableSize
);

struct _EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER Hdr;
    EFI_GET_TIME GetTime;
    EFI_SET_TIME SetTime;
    EFI_GET_WAKEUP_TIME GetWakeupTime;
    EFI_SET_WAKEUP_TIME SetWakeupTime;
    EFI_SET_VIRTUAL_ADDRESS_MAP SetVirtualAddressMap;
    EFI_CONVERT_POINTER ConvertPointer;
    EFI_GET_VARIABLE GetVariable;
    EFI_GET_NEXT_VARIABLE_NAME GetNextVariableName;
    EFI_SET_VARIABLE SetVariable;
    EFI_GET_NEXT_HIGH_MONOTONIC_COUNT GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM ResetSystem;
    EFI_UPDATE_CAPSULE UpdateCapsule;
    EFI_QUERY_CAPSULE_CAPABILITIES QueryCapsuleCapabilities;
    EFI_QUERY_VARIABLE_INFO QueryVariableInfo;
};

/* ============================================================================
 * Configuration Table
 * ============================================================================ */

typedef struct {
    EFI_GUID VendorGuid;
    VOID *VendorTable;
} EFI_CONFIGURATION_TABLE;

/* ============================================================================
 * System Table
 * ============================================================================ */

struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
};

/* ============================================================================
 * Device Path Protocol
 * ============================================================================ */

struct _EFI_DEVICE_PATH_PROTOCOL {
    UINT8 Type;
    UINT8 SubType;
    UINT16 Length;
    /* Variable length data follows */
};

#define END_DEVICE_PATH_TYPE  0x7F
#define END_ENTIRE_SUBTYPE    0xFF
#define END_INSTANCE_SUBTYPE  0x01

/* ============================================================================
 * Loaded Image Protocol
 * ============================================================================ */

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    { 0x5B1B31A1, 0x9562, 0x11D2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef struct _EFI_LOADED_IMAGE_PROTOCOL {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    VOID *Reserved;
    UINT32 LoadOptionsSize;
    VOID *LoadOptions;
    VOID *ImageBase;
    UINT64 ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    EFI_STATUS (EFIAPI *Unload)(EFI_HANDLE ImageHandle);
} EFI_LOADED_IMAGE_PROTOCOL;

/* ============================================================================
 * Simple File System Protocol
 * ============================================================================ */

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    { 0x0964E5B2, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_FILE_SYSTEM_OPEN_VOLUME)(
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, EFI_FILE_PROTOCOL **Root
);

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_SIMPLE_FILE_SYSTEM_OPEN_VOLUME OpenVolume;
};

/* ============================================================================
 * File Protocol
 * ============================================================================ */

#define EFI_FILE_PROTOCOL_REVISION  0x00010000
#define EFI_FILE_PROTOCOL_REVISION2 0x00020000
#define EFI_FILE_PROTOCOL_LATEST_REVISION EFI_FILE_PROTOCOL_REVISION2

#define EFI_FILE_MODE_READ   0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE  0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

#define EFI_FILE_READ_ONLY   0x0000000000000001ULL
#define EFI_FILE_HIDDEN      0x0000000000000002ULL
#define EFI_FILE_SYSTEM      0x0000000000000004ULL
#define EFI_FILE_RESERVED    0x0000000000000008ULL
#define EFI_FILE_DIRECTORY   0x0000000000000010ULL
#define EFI_FILE_ARCHIVE     0x0000000000000020ULL
#define EFI_FILE_VALID_ATTR  0x0000000000000037ULL

typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName, UINT64 OpenMode, UINT64 Attributes
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(EFI_FILE_PROTOCOL *This);
typedef EFI_STATUS (EFIAPI *EFI_FILE_DELETE)(EFI_FILE_PROTOCOL *This);
typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    EFI_FILE_PROTOCOL *This, UINTN *BufferSize, VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_WRITE)(
    EFI_FILE_PROTOCOL *This, UINTN *BufferSize, VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_POSITION)(
    EFI_FILE_PROTOCOL *This, UINT64 *Position
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION)(
    EFI_FILE_PROTOCOL *This, UINT64 Position
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    EFI_FILE_PROTOCOL *This, EFI_GUID *InformationType,
    UINTN *BufferSize, VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_INFO)(
    EFI_FILE_PROTOCOL *This, EFI_GUID *InformationType,
    UINTN BufferSize, VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FILE_FLUSH)(EFI_FILE_PROTOCOL *This);

struct _EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    EFI_FILE_DELETE Delete;
    EFI_FILE_READ Read;
    EFI_FILE_WRITE Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO GetInfo;
    EFI_FILE_SET_INFO SetInfo;
    EFI_FILE_FLUSH Flush;
};

/* ============================================================================
 * File Info
 * ============================================================================ */

#define EFI_FILE_INFO_ID \
    { 0x09576E92, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[];
} EFI_FILE_INFO;

/* ============================================================================
 * Graphics Output Protocol
 * ============================================================================ */

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } }

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_QUERY_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber,
    UINTN *SizeOfInfo, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
);
typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_SET_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber
);
typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_BLT)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer, EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    UINTN SourceX, UINTN SourceY, UINTN DestinationX, UINTN DestinationY,
    UINTN Width, UINTN Height, UINTN Delta
);

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_GRAPHICS_OUTPUT_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_SET_MODE SetMode;
    EFI_GRAPHICS_OUTPUT_BLT Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

/* ============================================================================
 * Simple Text Input Protocol
 * ============================================================================ */

#define EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID \
    { 0x387477C1, 0x69C7, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, BOOLEAN ExtendedVerification
);
typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key
);

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    EFI_EVENT WaitForKey;
};

/* ============================================================================
 * Simple Text Output Protocol
 * ============================================================================ */

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
    { 0x387477C2, 0x69C7, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN ExtendedVerification
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_TEST_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_QUERY_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber,
    UINTN *Columns, UINTN *Rows
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN ModeNumber
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Attribute
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_CURSOR_POSITION)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Column, UINTN Row
);
typedef EFI_STATUS (EFIAPI *EFI_TEXT_ENABLE_CURSOR)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, BOOLEAN Visible
);

#define EFI_BLACK  0x00
#define EFI_BLUE   0x01
#define EFI_GREEN  0x02
#define EFI_CYAN   0x03
#define EFI_RED    0x04
#define EFI_MAGENTA 0x05
#define EFI_BROWN  0x06
#define EFI_LIGHTGRAY 0x07
#define EFI_DARKGRAY 0x08
#define EFI_LIGHTBLUE 0x09
#define EFI_LIGHTGREEN 0x0A
#define EFI_LIGHTCYAN 0x0B
#define EFI_LIGHTRED 0x0C
#define EFI_LIGHTMAGENTA 0x0D
#define EFI_YELLOW 0x0E
#define EFI_WHITE  0x0F

#define EFI_TEXT_ATTR(fg, bg) ((fg) | ((bg) << 4))

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    EFI_TEXT_TEST_STRING TestString;
    EFI_TEXT_QUERY_MODE QueryMode;
    EFI_TEXT_SET_MODE SetMode;
    EFI_TEXT_SET_ATTRIBUTE SetAttribute;
    EFI_TEXT_CLEAR_SCREEN ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION SetCursorPosition;
    EFI_TEXT_ENABLE_CURSOR EnableCursor;
    SIMPLE_TEXT_OUTPUT_MODE *Mode;
};

typedef struct {
    INT32 MaxMode;
    INT32 Mode;
    INT32 Attribute;
    INT32 Column;
    INT32 Row;
    BOOLEAN CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

/* ============================================================================
 * Block IO Protocol
 * ============================================================================ */

#define EFI_BLOCK_IO_PROTOCOL_GUID \
    { 0x964E5B21, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef struct {
    UINT32 MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    UINT32 BlockSize;
    UINT32 IoAlign;
    EFI_LBA LastBlock;
} EFI_BLOCK_IO_MEDIA;

typedef EFI_STATUS (EFIAPI *EFI_BLOCK_IO_RESET)(
    EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN ExtendedVerification
);
typedef EFI_STATUS (EFIAPI *EFI_BLOCK_IO_READ_BLOCKS)(
    EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA LBA,
    UINTN BufferSize, VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_BLOCK_IO_WRITE_BLOCKS)(
    EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA LBA,
    UINTN BufferSize, VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_BLOCK_IO_FLUSH_BLOCKS)(
    EFI_BLOCK_IO_PROTOCOL *This
);

struct _EFI_BLOCK_IO_PROTOCOL {
    EFI_BLOCK_IO_MEDIA *Media;
    EFI_BLOCK_IO_RESET Reset;
    EFI_BLOCK_IO_READ_BLOCKS ReadBlocks;
    EFI_BLOCK_IO_WRITE_BLOCKS WriteBlocks;
    EFI_BLOCK_IO_FLUSH_BLOCKS FlushBlocks;
};

/* ============================================================================
 * Disk IO Protocol
 * ============================================================================ */

#define EFI_DISK_IO_PROTOCOL_GUID \
    { 0xCE345171, 0xBA0B, 0x11D2, { 0x8E, 0x4F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef EFI_STATUS (EFIAPI *EFI_DISK_IO_READ_DISK)(
    EFI_DISK_IO_PROTOCOL *This, UINT32 MediaId, UINT64 Offset,
    UINTN BufferSize, VOID *Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_DISK_IO_WRITE_DISK)(
    EFI_DISK_IO_PROTOCOL *This, UINT32 MediaId, UINT64 Offset,
    UINTN BufferSize, VOID *Buffer
);

struct _EFI_DISK_IO_PROTOCOL {
    UINT64 Revision;
    EFI_DISK_IO_READ_DISK ReadDisk;
    EFI_DISK_IO_WRITE_DISK WriteDisk;
};

/* ============================================================================
 * TCG2 Protocol (TPM 2.0)
 * ============================================================================ */

#define EFI_TCG2_PROTOCOL_GUID \
    { 0x607F766C, 0x745E, 0x4236, { 0x8E, 0x93, 0x3D, 0x5A, 0x6A, 0x6D, 0x4A, 0x8E } }

typedef struct {
    UINT8 Major;
    UINT8 Minor;
    UINT8 RevMajor;
    UINT8 RevMinor;
} EFI_TCG2_VERSION;

typedef EFI_STATUS (EFIAPI *EFI_TCG2_GET_CAPABILITY)(
    EFI_TCG2_PROTOCOL *This, EFI_TCG2_BOOT_SERVICE_CAPABILITY *ProtocolCapability
);
typedef EFI_STATUS (EFIAPI *EFI_TCG2_HASH_LOG_EXTEND_EVENT)(
    EFI_TCG2_PROTOCOL *This, UINT64 Flags, EFI_PHYSICAL_ADDRESS DataToHash,
    UINT64 DataToHashLen, EFI_TCG2_EVENT *Event
);
typedef EFI_STATUS (EFIAPI *EFI_TCG2_SUBMIT_COMMAND)(
    EFI_TCG2_PROTOCOL *This, UINT32 InputParameterBlockSize,
    VOID *InputParameterBlock, UINT32 OutputParameterBlockSize, VOID *OutputParameterBlock
);
typedef EFI_STATUS (EFIAPI *EFI_TCG2_GET_EVENT_LOG)(
    EFI_TCG2_PROTOCOL *This, EFI_TCG2_EVENT_LOG_FORMAT EventLogFormat,
    EFI_PHYSICAL_ADDRESS *EventLogLocation, EFI_PHYSICAL_ADDRESS *EventLogLastEntry,
    BOOLEAN *EventLogTruncated
);

struct _EFI_TCG2_PROTOCOL {
    EFI_TCG2_GET_CAPABILITY GetCapability;
    EFI_TCG2_HASH_LOG_EXTEND_EVENT HashLogExtendEvent;
    EFI_TCG2_SUBMIT_COMMAND SubmitCommand;
    EFI_TCG2_GET_EVENT_LOG GetEventLog;
    EFI_TCG2_VERSION ProtocolVersion;
};

typedef struct {
    UINT32 Size;
    UINT32 StructureVersion;
    UINT32 ProtocolSpecVersion;
    UINT32 HashAlgorithmBitmap;
    UINT32 SupportedEventLogs;
    BOOLEAN TPM20Present;
    UINT16 MaxCommandSize;
    UINT16 MaxResponseSize;
    UINT32 ManufacturerID;
} EFI_TCG2_BOOT_SERVICE_CAPABILITY;

typedef struct {
    UINT32 HeaderSize;
    UINT16 HeaderVersion;
    UINT32 PCRIndex;
    UINT32 EventType;
} EFI_TCG2_EVENT;

#define EFI_TCG2_EVENT_LOG_FORMAT_TCG_1_2  0x01
#define EFI_TCG2_EVENT_LOG_FORMAT_TCG_2    0x02

/* ============================================================================
 * ACPI Table GUIDs
 * ============================================================================ */

#define EFI_ACPI_TABLE_GUID \
    { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } }

#define EFI_ACPI_20_TABLE_GUID \
    { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } }

/* ============================================================================
 * SMBIOS Table GUIDs
 * ============================================================================ */

#define EFI_SMBIOS_TABLE_GUID \
    { 0xEB9D2D31, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }

#define EFI_SMBIOS3_TABLE_GUID \
    { 0xF2FD1544, 0x9794, 0x4A2C, { 0x99, 0x2E, 0xE5, 0xBB, 0xCF, 0x20, 0xE3, 0x94 } }

/* ============================================================================
 * HII Database Protocol (for fonts, etc.)
 * ============================================================================ */

#define EFI_HII_DATABASE_PROTOCOL_GUID \
    { 0xEF9FC172, 0xA1E2, 0x4693, { 0xB3, 0x27, 0x6F, 0x6C, 0x5D, 0x7A, 0x3A, 0x8C } }

/* ============================================================================
 * Loaded Image Device Path
 * ============================================================================ */

#define EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID \
    { 0xBC62157E, 0x3E33, 0x4FEC, { 0x99, 0x20, 0x2D, 0x3B, 0x36, 0xD7, 0x50, 0xDF } }

#endif /* LIMBOOT_EFI_H */