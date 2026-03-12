#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#endif

static LkLogLevel g_min_level = LK_LOG_INFO;
static FILE *g_log_fp = NULL;
#ifdef HAVE_SYSTEMD
static int g_use_journald = 0;
#endif

static const char *level_names[] = { "DEBUG", "INFO", "WARN", "ERROR" };

#ifdef HAVE_SYSTEMD
static int level_to_journal_priority[] = { 7, 6, 4, 3 }; /* LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR */
#endif

void lk_log_init(LkLogLevel level, const char *log_file) {
    g_min_level = level;

    if (log_file && log_file[0] != '\0') {
        g_log_fp = fopen(log_file, "a");
        if (!g_log_fp) {
            fprintf(stderr, "linuxkeeper: failed to open log file '%s', using stderr\n", log_file);
            g_log_fp = stderr;
        }
    } else {
#ifdef HAVE_SYSTEMD
        /* Try journald first if stderr is not a terminal (running as service) */
        if (!isatty(fileno(stderr))) {
            g_use_journald = 1;
        }
#endif
        g_log_fp = stderr;
    }
}

void lk_log_set_level(LkLogLevel level) {
    g_min_level = level;
}

LkLogLevel lk_log_level_from_str(const char *str) {
    if (!str) return LK_LOG_INFO;
    if (strcasecmp(str, "debug") == 0) return LK_LOG_DEBUG;
    if (strcasecmp(str, "info") == 0)  return LK_LOG_INFO;
    if (strcasecmp(str, "warn") == 0)  return LK_LOG_WARN;
    if (strcasecmp(str, "error") == 0) return LK_LOG_ERROR;
    return LK_LOG_INFO;
}

void lk_log_cleanup(void) {
    if (g_log_fp && g_log_fp != stderr) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
}

void lk_log(LkLogLevel level, const char *fmt, ...) {
    if (level < g_min_level) return;

    va_list args;
    va_start(args, fmt);

#ifdef HAVE_SYSTEMD
    if (g_use_journald) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        sd_journal_print(level_to_journal_priority[level], "%s", buf);
        va_end(args);
        return;
    }
#endif

    if (!g_log_fp) g_log_fp = stderr;

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    fprintf(g_log_fp, "[%s] [%-5s] ", timebuf, level_names[level]);
    vfprintf(g_log_fp, fmt, args);
    fprintf(g_log_fp, "\n");
    fflush(g_log_fp);

    va_end(args);
}
