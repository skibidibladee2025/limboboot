# limoboot Configuration Guide

## Overview

limoboot uses a simple, human-readable configuration file (`limoboot.conf`) to define boot entries and global settings. The configuration format is inspired by INI files with support for nested entry blocks.

## File Location

Default configuration paths (searched in order):
1. `/boot/limoboot.conf`
2. `/boot/efi/limoboot.conf`
3. `/efi/limoboot.conf`
4. `EFI/limoboot/limoboot.conf` on ESP

## Global Settings

```ini
# Boot menu timeout in seconds (0 = instant, -1 = infinite)
timeout = 5

# Default entry index (0-based)
default = 0

# Theme name (default, dark, light, minimal)
theme = "default"

# Force text mode (disable graphical menu)
text_mode = false

# Log level: 0=none, 1=error, 2=warn, 3=info, 4=debug
log_level = 2

# Require Secure Boot
secure_boot = false

# Enable measured boot (TPM event log)
measured_boot = false
```

## Boot Entries

Each boot entry is defined in an `entry` block:

```ini
entry "Entry Name" {
    # Boot protocol: limoboot, linux, multiboot2, chainload
    protocol = "limoboot"

    # Kernel path (relative to ESP root or absolute)
    kernel = "/boot/kernel.elf"

    # Kernel command line
    cmdline = "loglevel=4"

    # Modules (initrd, etc.) - can specify multiple
    module = "/boot/initrd.img"
    module = "/boot/microcode.bin"

    # Protocol-specific options
    # For linux protocol:
    #   initrd = "/boot/initrd.img"  (alternative to module)
    # For chainload:
    #   efi_path = "/EFI/other/boot.efi"
}
```

### Protocol Types

| Protocol | Description |
|----------|-------------|
| `limoboot` | Native Limoboot Protocol (64-bit ELF kernels) |
| `linux` | Linux kernel boot protocol (vmlinuz + initrd) |
| `multiboot2` | Multiboot2 (64-bit only) |
| `chainload` | Chainload another EFI application |

### Limoboot Protocol Entry

```ini
entry "My Kernel" {
    protocol = "limoboot"
    kernel = "/boot/kernel.elf"
    cmdline = "console=tty0 loglevel=4"
    module = "/boot/initrd.img"
}
```

### Linux Protocol Entry

```ini
entry "Linux" {
    protocol = "linux"
    kernel = "/boot/vmlinuz-6.8.0"
    cmdline = "root=/dev/nvme0n1p2 rw quiet"
    module = "/boot/initrd.img-6.8.0"
    module = "/boot/amd-ucode.img"
}
```

### Chainload Entry

```ini
entry "Windows" {
    protocol = "chainload"
    efi_path = "/EFI/Microsoft/Boot/bootmgfw.efi"
}
```

## Example Configuration

```ini
# limoboot.conf - Example configuration

# Global settings
timeout = 10
default = 0
theme = "default"
text_mode = false
log_level = 2

# Entry 1: Linux with initrd
entry "Linux 6.8 (Default)" {
    protocol = "linux"
    kernel = "/boot/vmlinuz-6.8.0"
    cmdline = "root=UUID=xxxx-xxxx rw quiet loglevel=3"
    module = "/boot/initrd.img-6.8.0"
    module = "/boot/amd-ucode.img"
}

# Entry 2: Linux with different kernel
entry "Linux 6.8 (Recovery)" {
    protocol = "linux"
    kernel = "/boot/vmlinuz-6.8.0"
    cmdline = "root=UUID=xxxx-xxxx rw single"
    module = "/boot/initrd.img-6.8.0"
}

# Entry 3: Custom kernel using Limoboot Protocol
entry "My OS" {
    protocol = "limoboot"
    kernel = "/boot/myos.elf"
    cmdline = "console=serial loglevel=4"
    module = "/boot/myos_initrd.img"
}

# Entry 4: Chainload Windows
entry "Windows 11" {
    protocol = "chainload"
    efi_path = "/EFI/Microsoft/Boot/bootmgfw.efi"
}

# Entry 5: Multiboot2 kernel
entry "Other OS (Multiboot2)" {
    protocol = "multiboot2"
    kernel = "/boot/other.elf"
    cmdline = "debug=1"
}
```

## Syntax Reference

### Comments
Lines starting with `#` or `;` are comments.

### Strings
Strings can be quoted with double quotes (`"..."`) or unquoted for simple values.
Quoted strings support escape sequences: `\n`, `\t`, `\\`, `\"`.

### Booleans
`true`, `false`, `1`, `0`, `yes`, `no`, `on`, `off`

### Numbers
Decimal integers. Negative values allowed for timeout.

### Entry Names
Entry names can contain spaces and special characters when quoted.

## Validation

Validate your configuration:
```bash
limoboot config validate
```

Show current configuration:
```bash
limoboot config show
```

## Tips

1. **Paths**: Use absolute paths from ESP root (e.g., `/boot/kernel.elf`) or relative paths.
2. **Multiple modules**: Specify `module` multiple times for initrd, microcode, etc.
3. **Order matters**: Modules are loaded in the order specified.
4. **Secure Boot**: Enable `secure_boot = true` to require validated config hash.
5. **Themes**: Try `dark`, `light`, or `minimal` themes for different looks.