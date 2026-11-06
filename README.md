# limoboot

A modern, secure, portable, multiprotocol bootloader and boot manager — **strictly 64-bit only**.

limoboot deliberately rejects all 32-bit support, BIOS/legacy support, and any form of real-mode or 32-bit protected-mode code. The entire codebase, build system, protocols, and supported platforms are pure 64-bit from day one.

## Features

- **Pure 64-bit**: No IA-32, no BIOS, no CSM, no legacy boot paths
- **Multi-architecture**: x86-64, aarch64/arm64, riscv64, loongarch64 (all UEFI only)
- **Modern protocols**: Limoboot Protocol (clean 64-bit redesign), Linux boot protocol, EFI chainloading
- **Secure Boot friendly**: Config hash enrollment, measured boot / TPM event log support
- **Beautiful boot menu**: Themeable, graphical and text modes, configurable
- **Simple configuration**: `limoboot.conf` with clean syntax
- **Host utility**: `limoboot` tool for installation, config enrollment, media generation
- **Small & fast**: Minimal attack surface, fast boot times

## Supported Architectures

| Architecture | Firmware | Status |
|--------------|----------|--------|
| x86-64 | UEFI | ✅ Primary |
| aarch64 / arm64 | UEFI | 🚧 Planned |
| riscv64 | UEFI | 🚧 Planned |
| loongarch64 | UEFI | 🚧 Optional |

## Boot Protocols

- **Limoboot Protocol** (primary) — Clean, modern, 64-bit-only protocol inspired by Limine but redesigned for purity
- **Linux boot protocol** — 64-bit kernel support
- **EFI chainloading** — Load other EFI applications
- **Multiboot2** (optional) — 64-bit only, for compatibility

## Quick Start

### Building

```bash
meson setup build
ninja -C build
```

### Installation

```bash
# Install to ESP
limoboot install --esp /boot/efi

# Enroll config hash for Secure Boot
limoboot enroll-hash --config /boot/limoboot.conf
```

### Configuration

Create `/boot/limoboot.conf`:

```ini
# limoboot configuration
timeout = 5

entry "My Kernel" {
    protocol = "limoboot"
    kernel = "/boot/kernel.elf"
    cmdline = "loglevel=4"
    module = "/boot/initrd.img"
}
```

## Project Structure

```
limoboot/
├── docs/                 # Documentation
├── include/              # Public headers (protocol, ABI)
├── src/
│   ├── common/           # Architecture-independent code
│   ├── arch/             # Architecture-specific code
│   │   ├── x86_64/
│   │   ├── aarch64/
│   │   ├── riscv64/
│   │   └── loongarch64/
│   ├── protocol/         # Protocol implementations
│   │   ├── limoboot/
│   │   ├── linux/
│   │   └── multiboot2/
│   ├── fs/               # Filesystem drivers
│   ├── ui/               # Boot menu & theme engine
│   └── util/             # Host utility (limoboot tool)
├── kernel-template/      # Minimal kernel template
└── meson.build
```

## Documentation

- [Protocol Specification](docs/PROTOCOL.md)
- [Configuration Guide](docs/CONFIG.md)
- [Installation Guide](docs/INSTALL.md)
- [Usage Guide](docs/USAGE.md)

## License

BSD-2-Clause — see [LICENSE](LICENSE) for details.

## Contributing

Contributions welcome! Please read our contributing guidelines before submitting PRs.

---

**limoboot** — *Pure 64-bit. No compromises.*