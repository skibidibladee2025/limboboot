# limoboot Usage Guide

## Boot Menu

When limoboot starts, it displays a graphical (or text) boot menu showing all configured entries.

### Navigation

| Key | Action |
|-----|--------|
| ↑ / ↓ | Select previous/next entry |
| Home | Jump to first entry |
| End | Jump to last entry |
| PgUp / PgDn | Scroll by 10 entries |
| 1-9 | Direct select entry 1-9 |
| Enter | Boot selected entry |
| Esc | Drop to command line |
| F1 | Show help |

### Timeout

If `timeout > 0` in config, the menu will auto-boot the default entry after the specified seconds. Press any key to cancel.

### Themes

Switch themes in config:
```ini
theme = "default"  # or "dark", "light", "minimal"
```

## Command Line

Press `Esc` at the boot menu to enter the command line interface.

### Commands

```
limoboot> help              # Show available commands
limoboot> boot [entry]      # Boot entry by name or index
limoboot> config            # Show current configuration
limoboot> set <key> <value> # Set a config option
limoboot> ls [path]         # List files on ESP
limoboot> cat <file>        # Display file contents
limoboot> load <kernel>     # Load kernel manually
limoboot> module <path>     # Add module/initrd
limoboot> cmdline <args>    # Set kernel command line
limoboot> go                # Boot with current settings
limoboot> reboot            # Reboot system
limoboot> poweroff          # Power off system
```

### Example Session

```
limoboot> ls /boot
vmlinuz-6.8.0
initrd.img-6.8.0
kernel.elf
limoboot.conf

limoboot> load /boot/kernel.elf
Loaded kernel at 0xffffffff80000000 (size: 2.3 MiB)

limoboot> module /boot/initrd.img
Loaded module at 0xffff800010000000 (size: 15.2 MiB)

limoboot> cmdline root=/dev/nvme0n1p2 rw loglevel=4

limoboot> go
Booting...
```

## Boot Protocols

### Limoboot Protocol (Native)

For custom 64-bit kernels. Kernel must:
- Be 64-bit ELF executable
- Link for higher half (e.g., 0xFFFFFFFF80000000)
- Provide `limoboot_requests` array in `.limoboot_requests` section

See `docs/PROTOCOL.md` for full specification.

### Linux Protocol

Standard Linux boot protocol for 64-bit kernels:
- Loads vmlinuz + initrd
- Passes EFI setup data (memmap, cmdline, RNG seed, TPM log, SMBIOS, ACPI)
- Supports handover protocol

### Multiboot2

Multiboot2 (64-bit only) for compatible kernels.

### Chainload

Load another EFI application (Windows, GRUB, etc.):
```ini
entry "Windows" {
    protocol = "chainload"
    efi_path = "/EFI/Microsoft/Boot/bootmgfw.efi"
}
```

## Configuration Management

### Show Config

```bash
limoboot config show
```

### Set Option

```bash
limoboot config set timeout 10
limoboot config set theme dark
```

### Add Entry

```bash
limoboot config add-entry "New Kernel"
# Then edit limoboot.conf to configure it
```

### Validate

```bash
limoboot config validate
```

## Secure Boot

### Check Status

```bash
limoboot config show | grep secure_boot
```

### Enroll Hash

```bash
# Enroll config hash (recommended)
sudo limoboot enroll-hash --config /boot/limoboot.conf

# Enroll bootloader hash
sudo mokutil --import-hash <sha256-of-bootloader>
```

## Measured Boot (TPM)

Enable in config:
```ini
measured_boot = true
```

View event log:
```bash
# From Linux
cat /sys/kernel/security/tpm0/binary_bios_measurements
```

## Debugging

### Verbose Logging

```ini
log_level = 4  # Debug
```

### Serial Console

Add to kernel cmdline:
```ini
cmdline = "console=ttyS0,115200 console=tty0 loglevel=7"
```

### GDB Stub (for kernel development)

In kernel, enable KGDB and add:
```ini
cmdline = "kgdboc=ttyS0,115200 kgdbwait"
```

## Advanced Usage

### Multiple ESPs

```bash
# Install to specific ESP
limoboot install --esp /mnt/esp2
```

### Custom Bootloader Name

```bash
# Install with custom name
cp build/limoboot-x86_64.efi /boot/efi/EFI/limoboot/myboot.efi
efibootmgr -c -L "My Boot" -l "\\EFI\\limoboot\\myboot.efi"
```

### Network Boot (PXE)

```bash
# Create PXE-bootable ISO
limoboot media --type iso --output pxe.iso \
    --bootloader build/limoboot-x86_64.efi \
    --config pxe.conf
```

### Recovery Entry

Add a recovery entry to config:
```ini
entry "Recovery Shell" {
    protocol = "limoboot"
    kernel = "/boot/recovery.efi"
    cmdline = "init=/bin/sh"
}
```

## Troubleshooting

### Menu doesn't appear
- Check `text_mode = true` in config
- Try `theme = "minimal"`
- Check firmware graphics support

### Kernel doesn't boot
- Verify kernel is 64-bit ELF
- Check command line syntax
- Ensure initrd path is correct

### Secure Boot fails
- Enroll config hash: `limoboot enroll-hash`
- Sign bootloader with enrolled key
- Check `mokutil --list-enrolled`

### Config not loading
- Run `limoboot config validate`
- Check file permissions
- Verify path in `limoboot install`

## Environment Variables

| Variable | Description |
|----------|-------------|
| `LIMBOOT_CONFIG` | Override config path |
| `LIMBOOT_ESP` | Override ESP path |
| `LIMBOOT_DEBUG` | Enable debug output |

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Invalid arguments |
| 3 | File not found |
| 4 | Permission denied |
| 5 | Secure Boot error |