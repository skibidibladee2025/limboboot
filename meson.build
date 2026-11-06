# limoboot - Pure 64-bit bootloader
# Build system using Meson

project('limoboot', 'c',
  version: '0.1.0',
  license: 'BSD-2-Clause',
  default_options: [
    'c_std=c17',
    'warning_level=3',
    'werror=true',
    'optimization=2',
    'debug=true',
    'b_lto=true',
    'b_pie=false',          # No PIE for UEFI
    'b_staticpic=false',    # No PIC for UEFI
  ])

# ============================================================================
# Configuration
# ============================================================================

# Host machine detection
host_machine = host_machine
target_machine = target_machine

# Architecture detection
arch = target_machine.cpu_family()
is_x86_64 = arch == 'x86_64' or arch == 'x86-64'
is_aarch64 = arch == 'aarch64'
is_riscv64 = arch == 'riscv64'
is_loongarch64 = arch == 'loongarch64'

# UEFI target
efi_arch = 'unknown'
if is_x86_64
  efi_arch = 'x86_64'
elif is_aarch64
  efi_arch = 'aarch64'
elif is_riscv64
  efi_arch = 'riscv64'
elif is_loongarch64
  efi_arch = 'loongarch64'
endif

# ============================================================================
# Compiler flags for UEFI
# ============================================================================

# Common UEFI flags
efi_cflags = [
  '-ffreestanding',
  '-fno-stack-protector',
  '-fno-stack-check',
  '-fshort-wchar',
  '-mno-red-zone',
  '-fno-asynchronous-unwind-tables',
  '-fno-unwind-tables',
  '-fno-exceptions',
  '-fno-rtti',
  '-nostdlibinc',
  '-nostdlib',
  '-DGNU_EFI_USE_MS_ABI=0',
  '-D__UEFI__',
  '-D__x86_64__' if is_x86_64 else '',
  '-D__aarch64__' if is_aarch64 else '',
  '-D__riscv' if is_riscv64 else '',
  '-D__loongarch64' if is_loongarch64 else '',
]

# Architecture-specific flags
if is_x86_64
  efi_cflags += [
    '-m64',
    '-mno-sse',
    '-mno-mmx',
    '-mno-sse2',
    '-mno-3dnow',
    '-mcmodel=kernel',
    '-maccumulate-outgoing-args',
  ]
elif is_aarch64
  efi_cflags += [
    '-mgeneral-regs-only',
    '-mabi=lp64',
  ]
elif is_riscv64
  efi_cflags += [
    '-march=rv64imafdc',
    '-mabi=lp64d',
    '-mcmodel=medany',
  ]
elif is_loongarch64
  efi_cflags += [
    '-march=loongarch64',
    '-mabi=lp64d',
    '-mcmodel=extreme',
  ]
endif

# Linker flags for UEFI
efi_ldflags = [
  '-nostdlib',
  '-Wl,--no-dynamic-linker',
  '-Wl,--gc-sections',
  '-Wl,-z,max-page-size=0x1000',
  '-Wl,-z,common-page-size=0x1000',
]

if is_x86_64
  efi_ldflags += [
    '-Wl,-e,efi_main',
    '-Wl,--image-base=0',
    '-Wl,--section-alignment=0x1000',
    '-Wl,--file-alignment=0x1000',
    '-Wl,--subsystem=10',  # EFI_APPLICATION
  ]
endif

# ============================================================================
# Include directories
# ============================================================================

inc_common = include_directories('include', 'src/common')
inc_arch = include_directories('src/arch/@0@'.format(arch))
inc_protocol = include_directories('src/protocol')
inc_fs = include_directories('src/fs')
inc_ui = include_directories('src/ui')

# ============================================================================
# Source files
# ============================================================================

# Common sources
common_sources = files(
  'src/common/efi.c',
  'src/common/memory.c',
  'src/common/string.c',
  'src/common/printf.c',
  'src/common/elf.c',
  'src/common/config.c',
  'src/common/entropy.c',
)

# Architecture-specific sources
arch_sources = files(
  'src/arch/@0@/start.S'.format(arch),
  'src/arch/@0@/efi.c'.format(arch),
  'src/arch/@0@/memory.c'.format(arch),
  'src/arch/@0@/smp.c'.format(arch),
)

# Protocol implementations
protocol_sources = files(
  'src/protocol/limoboot/limoboot.c',
  'src/protocol/linux/linux.c',
  'src/protocol/multiboot2/multiboot2.c',
)

# Filesystem drivers
fs_sources = files(
  'src/fs/fat.c',
  'src/fs/iso9660.c',
)

# UI / Boot menu
ui_sources = files(
  'src/ui/menu.c',
  'src/ui/theme.c',
  'src/ui/graphics.c',
  'src/ui/input.c',
)

# Main entry point
main_sources = files('src/main.c')

# All bootloader sources
bootloader_sources = common_sources + arch_sources + protocol_sources + fs_sources + ui_sources + main_sources

# ============================================================================
# Build the bootloader (UEFI executable)
# ============================================================================

bootloader = executable(
  'limoboot-@0@'.format(efi_arch),
  bootloader_sources,
  c_args: efi_cflags,
  link_args: efi_ldflags,
  include_directories: [inc_common, inc_arch, inc_protocol, inc_fs, inc_ui],
  install: false,  # We'll handle installation separately
)

# Rename to .efi extension
bootloader_efi = custom_target(
  'limoboot-@0@.efi'.format(efi_arch),
  input: bootloader,
  output: 'limoboot-@0@.efi'.format(efi_arch),
  command: [find_program('objcopy'), '@INPUT@', '-O', 'efi-app-@0@'.format(efi_arch), '@OUTPUT@'],
  install: true,
  install_dir: 'lib/limoboot',
)

# ============================================================================
# Host utility (limoboot tool)
# ============================================================================

util_sources = files(
  'src/util/main.c',
  'src/util/install.c',
  'src/util/enroll.c',
  'src/util/config.c',
  'src/util/media.c',
  'src/util/common.c',
)

util_deps = dependency('openssl', required: false)

limoboot_tool = executable(
  'limoboot',
  util_sources,
  c_args: ['-D_GNU_SOURCE', '-D_FILE_OFFSET_BITS=64'],
  dependencies: util_deps,
  install: true,
  install_dir: 'bin',
)

# ============================================================================
# Kernel template
# ============================================================================

kernel_template_dir = 'kernel-template'
kernel_sources = files(
  '@0@/kernel.c'.format(kernel_template_dir),
  '@0@/arch/x86_64/start.S'.format(kernel_template_dir),
  '@0@/arch/x86_64/memory.c'.format(kernel_template_dir),
  '@0@/arch/aarch64/start.S'.format(kernel_template_dir),
  '@0@/arch/aarch64/memory.c'.format(kernel_template_dir),
  '@0@/arch/riscv64/start.S'.format(kernel_template_dir),
  '@0@/arch/riscv64/memory.c'.format(kernel_template_dir),
  '@0@/arch/loongarch64/start.S'.format(kernel_template_dir),
  '@0@/arch/loongarch64/memory.c'.format(kernel_template_dir),
  '@0@/limoboot.c'.format(kernel_template_dir),
)

# Build kernel template for each arch
foreach karch : ['x86_64', 'aarch64', 'riscv64', 'loongarch64']
  k_efi_arch = karch
  if karch == 'x86_64'
    k_efi_arch = 'x86_64'
  endif

  kernel_exe = executable(
    'kernel-template-@0@'.format(karch),
    kernel_sources,
    c_args: efi_cflags + ['-DKERNEL_TEMPLATE'],
    link_args: efi_ldflags,
    include_directories: [inc_common, inc_protocol],
    install: false,
  )

  kernel_efi = custom_target(
    'kernel-template-@0@.efi'.format(karch),
    input: kernel_exe,
    output: 'kernel-template-@0@.efi'.format(karch),
    command: [find_program('objcopy'), '@INPUT@', '-O', 'efi-app-@0@'.format(k_efi_arch), '@OUTPUT@'],
    install: true,
    install_dir: 'share/limoboot/kernel-template',
  )
endforeach

# ============================================================================
# Documentation
# ============================================================================

docs = configure_file(
  input: 'docs/README.md.in',
  output: 'README.md',
  configuration: {
    'VERSION': meson.project_version(),
    'ARCH': efi_arch,
  },
  install: true,
  install_dir: 'share/doc/limoboot',
)

# Install documentation files
install_data(
  'docs/PROTOCOL.md',
  'docs/CONFIG.md',
  'docs/INSTALL.md',
  'docs/USAGE.md',
  install_dir: 'share/doc/limoboot',
)

# ============================================================================
# Configuration file example
# ============================================================================

install_data(
  'limoboot.conf.example',
  install_dir: 'share/limoboot',
  rename: 'limoboot.conf.example',
)

# ============================================================================
# Test targets (placeholder)
# ============================================================================

test('build-test', bootloader, env: {'LIMBOOT_TEST': '1'})

# ============================================================================
# Summary
# ============================================================================

summary = {
  'Project': meson.project_name(),
  'Version': meson.project_version(),
  'License': meson.project_license(),
  'Target Architecture': arch,
  'EFI Architecture': efi_arch,
  'Bootloader': '@0@.efi'.format(efi_arch),
  'Host Tool': 'limoboot',
  'Kernel Templates': 'kernel-template-*.efi',
}

foreach k, v: summary
  message('  @0@: @1@'.format(k, v))
endforeach