#include "daemon.h"
#include "log.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

static volatile sig_atomic_t g_quit = 0;
static volatile sig_atomic_t g_reload = 0;

static void sig_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT)
        g_quit = 1;
    else if (sig == SIGHUP)
        g_reload = 1;
}

void lk_signals_setup(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    /* Ignore SIGPIPE */
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

int lk_should_quit(void) { return g_quit; }
int lk_should_reload(void) { return g_reload; }
void lk_clear_reload(void) { g_reload = 0; }

int lk_daemonize(const Config *cfg) {
    (void)cfg;

    pid_t pid = fork();
    if (pid < 0) {
        LK_ERROR("First fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid > 0) _exit(0); /* Parent exits */

    if (setsid() < 0) {
        LK_ERROR("setsid failed: %s", strerror(errno));
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        LK_ERROR("Second fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid > 0) _exit(0); /* First child exits */

    /* Close stdin, redirect stdout/stderr to /dev/null */
    close(STDIN_FILENO);
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        if (devnull != STDIN_FILENO)
            dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        /* Keep stderr if logging to it; otherwise redirect */
        if (cfg->log_file[0] != '\0')
            dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO)
            close(devnull);
    }

    umask(0022);

    LK_INFO("Daemonized, PID=%d", (int)getpid());
    return 0;
}

int lk_pidfile_write(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        LK_WARN("Cannot write PID file '%s': %s", path, strerror(errno));
        return -1;
    }
    fprintf(fp, "%d\n", (int)getpid());
    fclose(fp);
    return 0;
}

int lk_pidfile_read(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    int pid = 0;
    if (fscanf(fp, "%d", &pid) != 1) pid = -1;
    fclose(fp);
    return pid;
}

void lk_pidfile_remove(const char *path) {
    unlink(path);
}

int lk_daemon_run(AudioContext *ctx, Config *cfg) {
    unsigned int backoff = 1;
    int stream_open = 0;

    while (!lk_should_quit()) {
        /* Handle config reload */
        if (lk_should_reload()) {
            LK_INFO("Reloading configuration...");
            lk_clear_reload();
            char *cfgpath = lk_config_find();
            if (cfgpath) {
                Config new_cfg;
                lk_config_defaults(&new_cfg);
                if (lk_config_load(&new_cfg, cfgpath) == 0) {
                    /* Preserve runtime overrides that matter */
                    *cfg = new_cfg;
                    lk_log_set_level(lk_log_level_from_str(cfg->log_level));
                    /* Close and reopen stream with new config */
                    if (stream_open) {
                        ctx->backend->close_stream(ctx);
                        stream_open = 0;
                    }
                }
                free(cfgpath);
            }
        }

        /* Open stream if not open */
        if (!stream_open) {
            const char *dev = (cfg->num_target_devices > 0) ?
                              cfg->target_devices[0] : "default";
            LK_INFO("Opening audio stream on device: %s", dev);

            if (ctx->backend->open_stream(ctx, dev) == 0) {
                stream_open = 1;
                backoff = 1;
                LK_INFO("Audio stream opened successfully via %s", ctx->backend->name);
            } else {
                LK_WARN("Failed to open stream, retrying in %u seconds", backoff);
                for (unsigned int i = 0; i < backoff && !lk_should_quit(); i++) {
                    sleep(1);
                }
                if (backoff < cfg->watchdog_max_interval) {
                    backoff *= 2;
                    if (backoff > cfg->watchdog_max_interval)
                        backoff = cfg->watchdog_max_interval;
                }
                continue;
            }
        }

        /* Write silence */
        if (stream_open) {
            int ret = ctx->backend->write_silence(ctx);
            if (ret != 0) {
                LK_WARN("Audio write failed (error %d), reconnecting...", ret);
                ctx->backend->close_stream(ctx);
                stream_open = 0;
                /* Small delay before retry */
                struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 }; /* 100ms */
                nanosleep(&ts, NULL);
            }
        }
    }

    /* Clean shutdown */
    LK_INFO("Shutting down...");
    if (stream_open) {
        ctx->backend->close_stream(ctx);
    }

    return 0;
}
