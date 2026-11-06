/*
 * limoboot Host Utility
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include "common.c"
#include "install.c"
#include "enroll.c"
#include "config.c"
#include "media.c"

/* ============================================================================
 * Version
 * ============================================================================ */

#define LIMBOOT_VERSION "0.1.0"

/* ============================================================================
 * Usage
 * ============================================================================ */

static void print_usage(const char *prog) {
    printf("limoboot %s - Pure 64-bit bootloader utility\n\n", LIMBOOT_VERSION);
    printf("Usage: %s <command> [options]\n\n", prog);
    printf("Commands:\n");
    printf("  install       Install limoboot to ESP\n");
    printf("  enroll-hash   Enroll config hash for Secure Boot\n");
    printf("  config        Manage limoboot configuration\n");
    printf("  media         Create bootable media (ISO, USB)\n");
    printf("  version       Show version\n");
    printf("  help          Show this help\n\n");
    printf("Run '%s <command> --help' for command-specific help\n", prog);
}

static void print_version(void) {
    printf("limoboot %s\n", LIMBOOT_VERSION);
    printf("Pure 64-bit bootloader for modern systems\n");
    printf("License: BSD-2-Clause\n");
}

/* ============================================================================
 * Command Table
 * ============================================================================ */

typedef struct {
    const char *name;
    int (*func)(int argc, char **argv);
    const char *help;
} command_t;

static int cmd_install(int argc, char **argv);
static int cmd_enroll_hash(int argc, char **argv);
static int cmd_config(int argc, char **argv);
static int cmd_media(int argc, char **argv);
static int cmd_help(int argc, char **argv);

static command_t commands[] = {
    { "install",       cmd_install,       "Install limoboot to ESP" },
    { "enroll-hash",   cmd_enroll_hash,   "Enroll config hash for Secure Boot" },
    { "config",        cmd_config,        "Manage limoboot configuration" },
    { "media",         cmd_media,         "Create bootable media" },
    { "version",       NULL,              "Show version" },
    { "help",          cmd_help,          "Show help" },
    { NULL, NULL, NULL }
};

/* ============================================================================
 * Command Implementations
 * ============================================================================ */

static int cmd_help(int argc, char **argv) {
    if (argc > 1) {
        for (command_t *c = commands; c->name; c++) {
            if (strcmp(c->name, argv[1]) == 0) {
                printf("%s - %s\n\n", c->name, c->help);
                /* Would call command-specific help */
                return 0;
            }
        }
        log_error("Unknown command: %s", argv[1]);
        return 1;
    }
    print_usage(argv[0]);
    return 0;
}

static int cmd_install(int argc, char **argv) {
    return install_main(argc, argv);
}

static int cmd_enroll_hash(int argc, char **argv) {
    return enroll_main(argc, argv);
}

static int cmd_config(int argc, char **argv) {
    return config_main(argc, argv);
}

static int cmd_media(int argc, char **argv) {
    return media_main(argc, argv);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "version") == 0) {
        print_version();
        return 0;
    }

    for (command_t *c = commands; c->name; c++) {
        if (strcmp(c->name, argv[1]) == 0) {
            if (c->func) {
                return c->func(argc - 1, argv + 1);
            }
            return 0;
        }
    }

    log_error("Unknown command: %s", argv[1]);
    print_usage(argv[0]);
    return 1;
}