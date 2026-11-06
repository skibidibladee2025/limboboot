/*
 * Enroll Hash Command - Enroll config hash for Secure Boot
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include "common.c"

/* ============================================================================
 * Enroll Options
 * ============================================================================ */

typedef struct {
    const char *config_path;
    const char *hash_algorithm;
    int use_mok;
    int verbose;
} enroll_options_t;

static enroll_options_t g_enroll_opts;

/* ============================================================================
 * Usage
 * ============================================================================ */

static void print_enroll_usage(const char *prog) {
    printf("Usage: %s enroll-hash [options]\n\n", prog);
    printf("Enroll config hash for Secure Boot (MOK or db)\n\n");
    printf("Options:\n");
    printf("  --config PATH        Path to limoboot.conf (default: /boot/limoboot.conf)\n");
    printf("  --algorithm ALGO     Hash algorithm: sha256, sha384, sha512 (default: sha256)\n");
    printf("  --mok                Use MOK (Machine Owner Key) enrollment\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help\n");
}

/* ============================================================================
 * Compute Hash
 * ============================================================================ */

static int compute_hash(const char *path, const char *algo, uint8_t *hash, size_t *hash_len) {
    void *data;
    size_t size;
    if (read_file(path, &data, &size) < 0) {
        return -1;
    }

    const EVP_MD *md = NULL;
    if (strcmp(algo, "sha256") == 0) md = EVP_sha256();
    else if (strcmp(algo, "sha384") == 0) md = EVP_sha384();
    else if (strcmp(algo, "sha512") == 0) md = EVP_sha512();
    else {
        log_error("Unsupported algorithm: %s", algo);
        free(data);
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        free(data);
        return -1;
    }

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        EVP_DigestUpdate(ctx, data, size) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, (unsigned int *)hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        free(data);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    free(data);
    return 0;
}

/* ============================================================================
 * Print Hash
 * ============================================================================ */

static void print_hash(const uint8_t *hash, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

/* ============================================================================
 * Enroll via MOK
 * ============================================================================ */

static int enroll_mok(const uint8_t *hash, size_t len) {
    /* This would use mokutil or direct EFI variable access */
    log_info("MOK enrollment not yet implemented");
    log_info("Run: mokutil --import-hash <hash>");
    print_hash(hash, len);
    return 0;
}

/* ============================================================================
 * Enroll via db (direct EFI variable)
 * ============================================================================ */

static int enroll_db(const uint8_t *hash, size_t len) {
    /* This would write to the db EFI variable */
    log_info("Direct db enrollment not yet implemented");
    log_info("Requires root and efivarfs mounted");
    print_hash(hash, len);
    return 0;
}

/* ============================================================================
 * Enroll Main
 * ============================================================================ */

static struct option enroll_long_opts[] = {
    { "config",     required_argument, 0, 'c' },
    { "algorithm",  required_argument, 0, 'a' },
    { "mok",        no_argument,       0, 'm' },
    { "verbose",    no_argument,       0, 'v' },
    { "help",       no_argument,       0, 'h' },
    { 0, 0, 0, 0 }
};

int enroll_main(int argc, char **argv) {
    memset(&g_enroll_opts, 0, sizeof(g_enroll_opts));
    g_enroll_opts.config_path = "/boot/limoboot.conf";
    g_enroll_opts.hash_algorithm = "sha256";

    int opt;
    while ((opt = getopt_long(argc, argv, "c:a:mvh", enroll_long_opts, NULL)) != -1) {
        switch (opt) {
            case 'c': g_enroll_opts.config_path = optarg; break;
            case 'a': g_enroll_opts.hash_algorithm = optarg; break;
            case 'm': g_enroll_opts.use_mok = 1; break;
            case 'v': g_enroll_opts.verbose = 1; log_set_level(LOG_DEBUG); break;
            case 'h': print_enroll_usage(argv[0]); return 0;
            default:  print_enroll_usage(argv[0]); return 1;
        }
    }

    if (g_enroll_opts.verbose) {
        log_set_level(LOG_DEBUG);
    }

    /* Check config exists */
    struct stat st;
    if (stat(g_enroll_opts.config_path, &st) != 0) {
        log_error("Config not found: %s", g_enroll_opts.config_path);
        return 1;
    }

    log_info("Computing %s hash of %s", g_enroll_opts.hash_algorithm, g_enroll_opts.config_path);

    uint8_t hash[EVP_MAX_MD_SIZE];
    size_t hash_len = 0;

    if (compute_hash(g_enroll_opts.config_path, g_enroll_opts.hash_algorithm, hash, &hash_len) < 0) {
        log_error("Failed to compute hash");
        return 1;
    }

    log_info("Hash: ");
    print_hash(hash, hash_len);

    if (g_enroll_opts.use_mok) {
        return enroll_mok(hash, hash_len);
    } else {
        return enroll_db(hash, hash_len);
    }
}