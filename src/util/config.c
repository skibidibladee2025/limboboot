/*
 * Config Command - Manage limoboot configuration
 * Copyright (c) 2024, limoboot contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include "common.c"

/* ============================================================================
 * Config Options
 * ============================================================================ */

typedef struct {
    const char *config_path;
    const char *action;  /* show, set, add-entry, remove-entry, validate */
    const char *key;
    const char *value;
    int verbose;
} config_options_t;

static config_options_t g_config_opts;

/* ============================================================================
 * Usage
 * ============================================================================ */

static void print_config_usage(const char *prog) {
    printf("Usage: %s config [options] <action>\n\n", prog);
    printf("Manage limoboot configuration\n\n");
    printf("Actions:\n");
    printf("  show               Show current configuration\n");
    printf("  set KEY VALUE      Set a global option\n");
    printf("  add-entry NAME     Add a new boot entry\n");
    printf("  remove-entry NAME  Remove a boot entry\n");
    printf("  validate           Validate configuration file\n\n");
    printf("Options:\n");
    printf("  --config PATH      Path to limoboot.conf (default: /boot/limoboot.conf)\n");
    printf("  -v, --verbose      Verbose output\n");
    printf("  -h, --help         Show this help\n");
}

/* ============================================================================
 * Show Config
 * ============================================================================ */

static int config_show(const char *path) {
    void *data;
    size_t size;
    if (read_file(path, &data, &size) < 0) {
        return -1;
    }

    printf("# %s\n", path);
    printf("%.*s", (int)size, (char *)data);
    free(data);
    return 0;
}

/* ============================================================================
 * Set Option
 * ============================================================================ */

static int config_set(const char *path, const char *key, const char *value) {
    void *data;
    size_t size;
    if (read_file(path, &data, &size) < 0) {
        /* Create new config */
        char *new_config = strdup_printf("%s = \"%s\"\n", key, value);
        if (write_file(path, new_config, strlen(new_config)) < 0) {
            free(new_config);
            return -1;
        }
        free(new_config);
        log_info("Created new config with %s = \"%s\"", key, value);
        return 0;
    }

    /* Simple line-based replacement */
    char *content = (char *)data;
    char *line = strtok(content, "\n");
    int found = 0;
    size_t new_size = 0;
    char *new_content = malloc(size + 256);

    line = strtok(content, "\n");
    while (line) {
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (str_starts_with(trimmed, key) && trimmed[strlen(key)] == '=') {
            new_size += snprintf(new_content + new_size, 256, "%s = \"%s\"\n", key, value);
            found = 1;
        } else {
            new_size += snprintf(new_content + new_size, strlen(line) + 2, "%s\n", line);
        }
        line = strtok(NULL, "\n");
    }

    if (!found) {
        new_size += snprintf(new_content + new_size, 256, "%s = \"%s\"\n", key, value);
    }

    if (write_file(path, new_content, new_size) < 0) {
        free(new_content);
        free(data);
        return -1;
    }

    free(new_content);
    free(data);
    log_info("Set %s = \"%s\"", key, value);
    return 0;
}

/* ============================================================================
 * Add Entry
 * ============================================================================ */

static int config_add_entry(const char *path, const char *name) {
    void *data;
    size_t size;
    if (read_file(path, &data, &size) < 0) {
        return -1;
    }

    char *entry = strdup_printf(
        "\nentry \"%s\" {\n"
        "    protocol = \"limoboot\"\n"
        "    kernel = \"/boot/kernel.elf\"\n"
        "    cmdline = \"\"\n"
        "}\n", name);

    char *new_content = malloc(size + strlen(entry) + 1);
    memcpy(new_content, data, size);
    memcpy(new_content + size, entry, strlen(entry) + 1);

    if (write_file(path, new_content, size + strlen(entry)) < 0) {
        free(new_content);
        free(entry);
        free(data);
        return -1;
    }

    free(new_content);
    free(entry);
    free(data);
    log_info("Added entry: %s", name);
    return 0;
}

/* ============================================================================
 * Remove Entry
 * ============================================================================ */

static int config_remove_entry(const char *path, const char *name) {
    log_warn("remove-entry not yet implemented");
    return 0;
}

/* ============================================================================
 * Validate Config
 * ============================================================================ */

static int config_validate(const char *path) {
    void *data;
    size_t size;
    if (read_file(path, &data, &size) < 0) {
        return -1;
    }

    /* Basic validation - check for balanced braces */
    int brace_count = 0;
    char *p = (char *)data;
    while (*p) {
        if (*p == '{') brace_count++;
        else if (*p == '}') brace_count--;
        p++;
    }

    if (brace_count != 0) {
        log_error("Unbalanced braces in config");
        free(data);
        return -1;
    }

    free(data);
    log_info("Configuration is valid");
    return 0;
}

/* ============================================================================
 * Config Main
 * ============================================================================ */

static struct option config_long_opts[] = {
    { "config",  required_argument, 0, 'c' },
    { "verbose", no_argument,       0, 'v' },
    { "help",    no_argument,       0, 'h' },
    { 0, 0, 0, 0 }
};

int config_main(int argc, char **argv) {
    memset(&g_config_opts, 0, sizeof(g_config_opts));
    g_config_opts.config_path = "/boot/limoboot.conf";

    int opt;
    while ((opt = getopt_long(argc, argv, "c:vh", config_long_opts, NULL)) != -1) {
        switch (opt) {
            case 'c': g_config_opts.config_path = optarg; break;
            case 'v': g_config_opts.verbose = 1; log_set_level(LOG_DEBUG); break;
            case 'h': print_config_usage(argv[0]); return 0;
            default:  print_config_usage(argv[0]); return 1;
        }
    }

    if (g_config_opts.verbose) {
        log_set_level(LOG_DEBUG);
    }

    if (optind >= argc) {
        print_config_usage(argv[0]);
        return 1;
    }

    g_config_opts.action = argv[optind++];

    if (strcmp(g_config_opts.action, "show") == 0) {
        return config_show(g_config_opts.config_path);
    } else if (strcmp(g_config_opts.action, "set") == 0) {
        if (optind + 1 >= argc) {
            log_error("set requires KEY and VALUE");
            return 1;
        }
        return config_set(g_config_opts.config_path, argv[optind], argv[optind + 1]);
    } else if (strcmp(g_config_opts.action, "add-entry") == 0) {
        if (optind >= argc) {
            log_error("add-entry requires NAME");
            return 1;
        }
        return config_add_entry(g_config_opts.config_path, argv[optind]);
    } else if (strcmp(g_config_opts.action, "remove-entry") == 0) {
        if (optind >= argc) {
            log_error("remove-entry requires NAME");
            return 1;
        }
        return config_remove_entry(g_config_opts.config_path, argv[optind]);
    } else if (strcmp(g_config_opts.action, "validate") == 0) {
        return config_validate(g_config_opts.config_path);
    } else {
        log_error("Unknown action: %s", g_config_opts.action);
        return 1;
    }
}