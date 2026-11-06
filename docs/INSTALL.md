# limoboot Installation Guide

## Requirements

- UEFI firmware (x86-64, aarch64, riscv64, or loongarch64)
- EFI System Partition (ESP) formatted as FAT32
- Secure Boot keys (optional, for Secure Boot support)
- TPM 2.0 (optional, for measured boot)

## Quick Installation

### Using the Host Utility (Recommended)

```bash
# Build limoboot
meson setup build
ninja -C build

# Install to ESP (auto-detected)
sudo ./build/limoboot install

# Or specify ESP path explicitly
sudo ./build/limoboot install --esp /boot/efi

# Enroll config hash for Secure Boot
sudo ./build/limoboot enroll-hash --config /boot/limoboot.conf
```

### Manual Installation

```bash
# 1. Mount ESP
sudo mount /dev/nvme0n1p1 /mnt/esp

# 2. Create limoboot directory
sudo mkdir -p /mnt/esp/EFI/limoboot

# 3. Copy bootloader (match your architecture)
sudo cp build/limoboot-x86_64.efi /mnt/esp/EFI/limoboot/limoboot.efi

# 4. Copy configuration
sudo cp limoboot.conf /mnt/esp/limoboot.conf

# 5. Add UEFI boot entry
sudo efibootmgr -c -d /dev/nvme0n1 -p 1 \
    -L "limoboot" -l "\\EFI\\limoboot\\limoboot.efi"
```

## Architecture-Specific Notes

### x86-64
- File: `limoboot-x86_64.efi`
- Secure Boot: Enroll `limoboot-x86_64.efi` hash in db or use MOK

### aarch64 / arm64
- File: `limoboot-aarch64.efi`
- Requires UEFI firmware with ARM64 support

### riscv64
- File: `limoboot-riscv64.efi`
- Requires UEFI firmware with RISC-V support (e.g., OpenSBI + U-Boot)

### loongarch64
- File: `limoboot-loongarch64.efi`
- Requires UEFI firmware with LoongArch support

## Secure Boot Setup

### Option 1: MOK (Machine Owner Key)

```bash
# Generate MOK key pair
openssl req -newkey rsa:2048 -nodes -keyout MOK.key -out MOK.csr -subj "/CN=limoboot MOK/"
openssl x509 -req -in MOK.csr -signkey MOK.key -out MOK.crt

# Enroll in MOK
sudo mokutil --import MOK.crt

# Sign bootloader
sbsign --key MOK.key --cert MOK.crt --output limoboot.efi limoboot-x86_64.efi
```

### Option 2: Direct db Enrollment

```bash
# Compute hash
sha256sum limoboot-x86_64.efi

# Enroll hash in db (requires efivarfs and root)
echo -n "<hash>" | sudo tee /sys/firmware/efi/efivars/db-...
```

### Option 3: Config Hash Enrollment

```bash
# Enroll config hash (limoboot will verify config at boot)
sudo ./build/limoboot enroll-hash --config /boot/limoboot.conf
```

## Measured Boot (TPM)

```bash
# Enable in config
measured_boot = true

# Or via command line
limoboot config set measured_boot true
```

The bootloader will extend PCRs with:
- Kernel hash
- Config hash
- Module hashes
- Command line

## Verification

```bash
# Check boot entry
efibootmgr -v

# Test boot (will show menu)
# Reboot and select limoboot from firmware menu
```

## Uninstallation

```bash
# Remove boot entry
sudo efibootmgr -b XXXX -B

# Remove files
sudo rm -rf /boot/efi/EFI/limoboot
sudo rm /boot/limoboot.conf
```

## Troubleshooting

### "No bootloader found"
- Ensure ESP is mounted at `/boot/efi` or specify with `--esp`
- Check that `limoboot-<arch>.efi` exists in build directory

### "Secure Boot violation"
- Sign bootloader with enrolled key
- Or disable Secure Boot in firmware
- Or enroll hash using `enroll-hash`

### "Config not found"
- Place `limoboot.conf` at `/boot/limoboot.conf` or ESP root
- Check `limoboot config show` for parsed config

### Black screen / no menu
- Try `text_mode = true` in config
- Check firmware supports graphics output protocol
- Try different theme: `theme = "minimal"`

## Building from Source

```bash
# Dependencies
# - meson >= 0.60
# - ninja
# - clang or gcc (with UEFI cross-compile support)
# - gnu-efi (for headers)

# Configure
meson setup build --cross-file cross-x86_64.txt

# Build
ninja -C build

# Install
sudo ninja -C build install
```

### Cross-Compilation

Create cross-file for target architecture:

```ini
# cross-x86_64.txt
[binaries]
c = 'x86_64-linux-gnu-gcc'
ar = 'x86_64-linux-gnu-ar'
objcopy = 'x86_64-linux-gnu-objcopy'

[host_machine]
system = 'linux'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
c_args = ['-ffreestanding', '-fno-stack-protector', '-mno-red-zone', ...]
```

## Creating Bootable Media

### ISO Image

```bash
./build/limoboot media --type iso --output limoboot.iso \
    --bootloader build/limoboot-x86_64.efi \
    --config limoboot.conf \
    --kernel /path/to/kernel.elf
```

### USB Drive

```bash
sudo ./build/limoboot media --type usb --output /dev/sdX \
    --bootloader build/limoboot-x86_64.efi \
    --config limoboot.conf
```

## Firmware-Specific Notes

### Intel/AMD (x86-64)
- Most modern UEFI implementations work out of the box
- Some older firmware may need CSM disabled

### ARM64 (Raspberry Pi, etc.)
- Use UEFI firmware (e.g., EDK2 for Pi)
- May need `config.txt` modifications

### RISC-V (SiFive, StarFive, etc.)
- Use OpenSBI + U-Boot as UEFI payload
- Configure `u-boot.efi` as bootloader

### LoongArch (Loongson)
- Use LoongArch UEFI firmware
- Limited hardware availability