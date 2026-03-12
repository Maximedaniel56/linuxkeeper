#include "config.h"
#include "audio.h"
#include "daemon.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>

#define LK_VERSION "1.0.0"

static void print_version(void) {
    printf("linuxkeeper %s\n", LK_VERSION);
    printf("Audio backends:");
#ifdef WITH_PIPEWIRE
    printf(" pipewire");
#endif
#ifdef WITH_PULSEAUDIO
    printf(" pulseaudio");
#endif
#ifdef WITH_ALSA
    printf(" alsa");
#endif
#ifdef WITH_OSS
    printf(" oss");
#endif
    printf("\n");
}

static void print_help(const char *prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Linux audio keep-alive daemon - prevents Bluetooth/USB audio\n");
    printf("devices from disconnecting by playing continuous silent audio.\n\n");
    printf("Options:\n");
    printf("  -c, --config PATH     Path to config file\n");
    printf("  -d, --daemon          Run as background daemon\n");
    printf("  -D, --device NAME     Target device (overrides config)\n");
    printf("  -b, --backend NAME    Force audio backend (auto|pipewire|pulseaudio|alsa|oss)\n");
    printf("  -v, --verbose         Increase log verbosity\n");
    printf("  -l, --list-devices    List available audio output devices and exit\n");
    printf("  -s, --status          Show daemon status\n");
    printf("  -k, --kill            Stop running daemon\n");
    printf("      --version         Show version and build info\n");
    printf("      --help            Show this help\n");
}

static int do_status(const Config *cfg) {
    int pid = lk_pidfile_read(cfg->pid_file);
    if (pid <= 0) {
        printf("linuxkeeper is not running (no PID file)\n");
        return 1;
    }

    /* Check if process exists */
    if (kill((pid_t)pid, 0) == 0) {
        printf("linuxkeeper is running (PID %d)\n", pid);
        return 0;
    } else {
        printf("linuxkeeper is not running (stale PID file, was PID %d)\n", pid);
        lk_pidfile_remove(cfg->pid_file);
        return 1;
    }
}

static int do_kill(const Config *cfg) {
    int pid = lk_pidfile_read(cfg->pid_file);
    if (pid <= 0) {
        fprintf(stderr, "linuxkeeper is not running (no PID file)\n");
        return 1;
    }

    if (kill((pid_t)pid, 0) != 0) {
        fprintf(stderr, "linuxkeeper is not running (stale PID file)\n");
        lk_pidfile_remove(cfg->pid_file);
        return 1;
    }

    printf("Stopping linuxkeeper (PID %d)...\n", pid);
    kill((pid_t)pid, SIGTERM);

    /* Wait up to 5 seconds for clean shutdown */
    for (int i = 0; i < 50; i++) {
        usleep(100000);
        if (kill((pid_t)pid, 0) != 0) {
            printf("Stopped.\n");
            lk_pidfile_remove(cfg->pid_file);
            return 0;
        }
    }

    fprintf(stderr, "Warning: process did not stop cleanly, sending SIGKILL\n");
    kill((pid_t)pid, SIGKILL);
    lk_pidfile_remove(cfg->pid_file);
    return 0;
}

int main(int argc, char **argv) {
    Config cfg;
    lk_config_defaults(&cfg);

    char *config_path = NULL;
    int daemonize = 0;
    int list_devices = 0;
    int show_status = 0;
    int kill_daemon = 0;
    int verbose = 0;
    char *cli_device = NULL;
    char *cli_backend = NULL;

    static struct option long_opts[] = {
        { "config",       required_argument, NULL, 'c' },
        { "daemon",       no_argument,       NULL, 'd' },
        { "device",       required_argument, NULL, 'D' },
        { "backend",      required_argument, NULL, 'b' },
        { "verbose",      no_argument,       NULL, 'v' },
        { "list-devices", no_argument,       NULL, 'l' },
        { "status",       no_argument,       NULL, 's' },
        { "kill",         no_argument,       NULL, 'k' },
        { "version",      no_argument,       NULL, 'V' },
        { "help",         no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:dD:b:vlskh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'c': config_path = optarg; break;
        case 'd': daemonize = 1; break;
        case 'D': cli_device = optarg; break;
        case 'b': cli_backend = optarg; break;
        case 'v': verbose++; break;
        case 'l': list_devices = 1; break;
        case 's': show_status = 1; break;
        case 'k': kill_daemon = 1; break;
        case 'V': print_version(); return 0;
        case 'h': print_help(argv[0]); return 0;
        default:  print_help(argv[0]); return 1;
        }
    }

    /* Load config */
    if (config_path) {
        if (lk_config_load(&cfg, config_path) != 0) {
            fprintf(stderr, "Error: cannot load config file '%s'\n", config_path);
            return 1;
        }
    } else {
        char *found = lk_config_find();
        if (found) {
            lk_config_load(&cfg, found);
            free(found);
        }
    }

    /* CLI overrides */
    if (cli_device) lk_config_set_device(&cfg, cli_device);
    if (cli_backend) lk_config_set_backend(&cfg, cli_backend);

    /* Initialize logging */
    LkLogLevel log_level = lk_log_level_from_str(cfg.log_level);
    if (verbose) {
        log_level = LK_LOG_DEBUG;
    }
    lk_log_init(log_level, cfg.log_file);

    /* Handle --status */
    if (show_status) {
        int ret = do_status(&cfg);
        lk_log_cleanup();
        return ret;
    }

    /* Handle --kill */
    if (kill_daemon) {
        int ret = do_kill(&cfg);
        lk_log_cleanup();
        return ret;
    }

    /* Handle --list-devices */
    if (list_devices) {
        lk_audio_list_backends();
        printf("\n");
        AudioContext *ctx = lk_audio_create(&cfg);
        if (ctx) {
            ctx->backend->list_devices(ctx);
            lk_audio_free(ctx);
        }
        lk_log_cleanup();
        return 0;
    }

    /* Check if already running */
    int existing_pid = lk_pidfile_read(cfg.pid_file);
    if (existing_pid > 0 && kill((pid_t)existing_pid, 0) == 0) {
        fprintf(stderr, "linuxkeeper is already running (PID %d)\n", existing_pid);
        lk_log_cleanup();
        return 1;
    }

    LK_INFO("linuxkeeper %s starting", LK_VERSION);
    print_version();

    /* Setup signals */
    lk_signals_setup();

    /* Create audio context */
    AudioContext *ctx = lk_audio_create(&cfg);
    if (!ctx) {
        LK_ERROR("Failed to initialize any audio backend");
        lk_log_cleanup();
        return 1;
    }

    /* Daemonize if requested */
    if (daemonize) {
        if (lk_daemonize(&cfg) != 0) {
            LK_ERROR("Failed to daemonize");
            lk_audio_free(ctx);
            lk_log_cleanup();
            return 1;
        }
    }

    /* Write PID file */
    lk_pidfile_write(cfg.pid_file);

    /* Run main loop */
    int ret = lk_daemon_run(ctx, &cfg);

    /* Cleanup */
    lk_pidfile_remove(cfg.pid_file);
    lk_audio_free(ctx);
    lk_log_cleanup();

    LK_INFO("linuxkeeper stopped cleanly");
    return ret;
}
