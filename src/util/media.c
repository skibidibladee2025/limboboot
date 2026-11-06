/*
 * Media Command - Create bootable media (ISO, USB)
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.c"

/* ============================================================================
 * Media Options
 * ============================================================================ */

typedef struct {
    const char *output;
    const char *bootloader;
    const char *config;
    const char *kernel;
    const char *initrd;
    const char *type;  /* iso, usb */
    int verbose;
} media_options_t;

static media_options_t g_media_opts;

/* ============================================================================
 * Usage
 * ============================================================================ */

static void print_media_usage(const char *prog) {
    printf("Usage: %s media [options]\n\n", prog);
    printf("Create bootable media\n\n");
    printf("Options:\n");
    printf("  --output PATH        Output file (ISO) or device (USB)\n");
    printf("  --bootloader PATH    Path to limoboot-<arch>.efi\n");
    printf("  --config PATH        Path to limoboot.conf\n");
    printf("  --kernel PATH        Kernel to include\n");
    printf("  --initrd PATH        Initrd to include\n");
    printf("  --type TYPE          Media type: iso, usb (default: iso)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help\n");
}

/* ============================================================================
 * Create ISO9660 (simplified)
 * ============================================================================ */

static int create_iso(const char *output) {
    log_warn("ISO creation not fully implemented");
    log_info("Would create bootable ISO with:");
    log_info("  - limoboot bootloader");
    log_info("  - Configuration");
    log_info("  - Kernel and initrd (if specified)");
    log_info("Output: %s", output);

    /* This would use libisofs or genisoimage to create proper ISO */
    /* For now, just create a placeholder */
    FILE *f = fopen(output, "wb");
    if (!f) {
        log_error("Failed to create %s: %s", output, strerror(errno));
        return -1;
    }

    const char *placeholder = "limoboot ISO placeholder\n";
    fwrite(placeholder, 1, strlen(placeholder), f);
    fclose(f);

    return 0;
}

/* ============================================================================
 * Create USB (dd-style)
 * ============================================================================ */

static int create_usb(const char *device) {
    log_warn("USB creation not fully implemented");
    log_info("Would write hybrid MBR + ESP to %s", device);
    log_info("Requires root privileges");

    /* This would: */
    /* 1. Partition the device (GPT + hybrid MBR) */
    /* 2. Create FAT32 ESP partition */
    /* 3. Copy bootloader, config, kernel, initrd */
    /* 4. Make bootable */

    return 0;
}

/* ============================================================================
 * Media Main
 * ============================================================================ */

static struct option media_long_opts[] = {
    { "output",     required_argument, 0, 'o' },
    { "bootloader", required_argument, 0, 'b' },
    { "config",     required_argument, 0, 'c' },
    { "kernel",     required_argument, 0, 'k' },
    { "initrd",     required_argument, 0, 'i' },
    { "type",       required_argument, 0, 't' },
    { "verbose",    no_argument,       0, 'v' },
    { "help",       no_argument,       0, 'h' },
    { 0, 0, 0, 0 }
};

int media_main(int argc, char **argv) {
    memset(&g_media_opts, 0, sizeof(g_media_opts));
    g_media_opts.type = "iso";

    int opt;
    while ((opt = getopt_long(argc, argv, "o:b:c:k:i:t:vh", media_long_opts, NULL)) != -1) {
        switch (opt) {
            case 'o': g_media_opts.output = optarg; break;
            case 'b': g_media_opts.bootloader = optarg; break;
            case 'c': g_media_opts.config = optarg; break;
            case 'k': g_media_opts.kernel = optarg; break;
            case 'i': g_media_opts.initrd = optarg; break;
            case 't': g_media_opts.type = optarg; break;
            case 'v': g_media_opts.verbose = 1; log_set_level(LOG_DEBUG); break;
            case 'h': print_media_usage(argv[0]); return 0;
            default:  print_media_usage(argv[0]); return 1;
        }
    }

    if (g_media_opts.verbose) {
        log_set_level(LOG_DEBUG);
    }

    if (!g_media_opts.output) {
        log_error("Output required (--output)");
        return 1;
    }

    if (strcmp(g_media_opts.type, "iso") == 0) {
        return create_iso(g_media_opts.output);
    } else if (strcmp(g_media_opts.type, "usb") == 0) {
        return create_usb(g_media_opts.output);
    } else {
        log_error("Unknown media type: %s", g_media_opts.type);
        return 1;
    }
}