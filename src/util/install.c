/*
 * Install Command - Install limoboot to ESP
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
#include <dirent.h>
#include "common.c"

/* ============================================================================
 * Install Options
 * ============================================================================ */

typedef struct {
    const char *esp_path;
    const char *bootloader_path;
    const char *config_path;
    int force;
    int verbose;
} install_options_t;

static install_options_t g_install_opts;

/* ============================================================================
 * Usage
 * ============================================================================ */

static void print_install_usage(const char *prog) {
    printf("Usage: %s install [options]\n\n", prog);
    printf("Install limoboot to EFI System Partition\n\n");
    printf("Options:\n");
    printf("  --esp PATH           Path to ESP mount point (default: /boot/efi)\n");
    printf("  --bootloader PATH    Path to limoboot-<arch>.efi (auto-detected)\n");
    printf("  --config PATH        Path to limoboot.conf (default: /boot/limoboot.conf)\n");
    printf("  --force              Overwrite existing files\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help\n");
}

/* ============================================================================
 * Find ESP
 * ============================================================================ */

static const char *find_esp(void) {
    static const char *candidates[] = {
        "/boot/efi",
        "/boot",
        "/efi",
        "/mnt/esp",
        "/esp",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        struct stat st;
        if (stat(candidates[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Check for EFI directory */
            char *efi_path = strdup_printf("%s/EFI", candidates[i]);
            if (efi_path) {
                if (stat(efi_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                    free(efi_path);
                    return candidates[i];
                }
                free(efi_path);
            }
        }
    }
    return NULL;
}

/* ============================================================================
 * Find Bootloader
 * ============================================================================ */

static char *find_bootloader(const char *search_dir) {
    DIR *dir = opendir(search_dir);
    if (!dir) return NULL;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (str_starts_with(ent->d_name, "limoboot-") && str_ends_with(ent->d_name, ".efi")) {
            closedir(dir);
            return strdup_printf("%s/%s", search_dir, ent->d_name);
        }
    }
    closedir(dir);
    return NULL;
}

/* ============================================================================
 * Copy File
 * ============================================================================ */

static int copy_file(const char *src, const char *dst, int force) {
    struct stat st;
    if (stat(dst, &st) == 0 && !force) {
        log_warn("%s already exists (use --force to overwrite)", dst);
        return 0;
    }

    void *data;
    size_t size;
    if (read_file(src, &data, &size) < 0) {
        return -1;
    }

    if (write_file(dst, data, size) < 0) {
        free(data);
        return -1;
    }

    free(data);
    log_info("Installed %s -> %s", src, dst);
    return 0;
}

/* ============================================================================
 * Ensure Directory
 * ============================================================================ */

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }

    char *copy = strdup(path);
    char *p = copy;
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(copy, 0755);
            *p = '/';
        }
        p++;
    }
    mkdir(copy, 0755);
    free(copy);
    return 0;
}

/* ============================================================================
 * Install Main
 * ============================================================================ */

static struct option install_long_opts[] = {
    { "esp",        required_argument, 0, 'e' },
    { "bootloader", required_argument, 0, 'b' },
    { "config",     required_argument, 0, 'c' },
    { "force",      no_argument,       0, 'f' },
    { "verbose",    no_argument,       0, 'v' },
    { "help",       no_argument,       0, 'h' },
    { 0, 0, 0, 0 }
};

int install_main(int argc, char **argv) {
    memset(&g_install_opts, 0, sizeof(g_install_opts));
    g_install_opts.esp_path = "/boot/efi";

    int opt;
    while ((opt = getopt_long(argc, argv, "e:b:c:fvh", install_long_opts, NULL)) != -1) {
        switch (opt) {
            case 'e': g_install_opts.esp_path = optarg; break;
            case 'b': g_install_opts.bootloader_path = optarg; break;
            case 'c': g_install_opts.config_path = optarg; break;
            case 'f': g_install_opts.force = 1; break;
            case 'v': g_install_opts.verbose = 1; log_set_level(LOG_DEBUG); break;
            case 'h': print_install_usage(argv[0]); return 0;
            default:  print_install_usage(argv[0]); return 1;
        }
    }

    if (g_install_opts.verbose) {
        log_set_level(LOG_DEBUG);
    }

    /* Find ESP */
    const char *esp = g_install_opts.esp_path;
    if (!esp) {
        esp = find_esp();
        if (!esp) {
            log_error("Could not find ESP. Use --esp to specify path.");
            return 1;
        }
        log_info("Found ESP at %s", esp);
    }

    /* Find bootloader */
    char *bootloader = g_install_opts.bootloader_path ? strdup(g_install_opts.bootloader_path) : find_bootloader(".");
    if (!bootloader) {
        log_error("Could not find limoboot-<arch>.efi. Use --bootloader to specify path.");
        return 1;
    }
    log_info("Using bootloader: %s", bootloader);

    /* Target paths */
    char *efi_dir = strdup_printf("%s/EFI/limoboot", esp);
    char *target_bootloader = strdup_printf("%s/limoboot.efi", efi_dir);
    char *target_config = g_install_opts.config_path ? strdup(g_install_opts.config_path) : strdup_printf("%s/limoboot.conf", esp);

    /* Create EFI directory */
    if (ensure_dir(efi_dir) < 0) {
        log_error("Failed to create %s", efi_dir);
        free(efi_dir);
        free(target_bootloader);
        free(target_config);
        free(bootloader);
        return 1;
    }

    /* Copy bootloader */
    if (copy_file(bootloader, target_bootloader, g_install_opts.force) < 0) {
        log_error("Failed to install bootloader");
        free(efi_dir);
        free(target_bootloader);
        free(target_config);
        free(bootloader);
        return 1;
    }

    /* Copy config if provided */
    if (g_install_opts.config_path) {
        if (copy_file(g_install_opts.config_path, target_config, g_install_opts.force) < 0) {
            log_warn("Failed to install config (will use defaults)");
        }
    } else {
        /* Create default config */
        const char *default_config =
            "# limoboot configuration\n"
            "timeout = 5\n"
            "\n"
            "entry \"Linux\" {\n"
            "    protocol = \"linux\"\n"
            "    kernel = \"/boot/vmlinuz\"\n"
            "    cmdline = \"root=/dev/sda2 rw\"\n"
            "}\n"
            "\n"
            "entry \"Custom Kernel\" {\n"
            "    protocol = \"limoboot\"\n"
            "    kernel = \"/boot/kernel.elf\"\n"
            "    cmdline = \"loglevel=4\"\n"
            "}\n";

        if (write_file(target_config, default_config, strlen(default_config)) < 0) {
            log_warn("Failed to create default config");
        } else {
            log_info("Created default config at %s", target_config);
        }
    }

    /* Create boot entry using efibootmgr if available */
    log_info("Installation complete!");
    log_info("Run 'efibootmgr -c -d /dev/sdX -p Y -L \"limoboot\" -l \"\\EFI\\limoboot\\limoboot.efi\"' to add boot entry");

    free(efi_dir);
    free(target_bootloader);
    free(target_config);
    free(bootloader);

    return 0;
}