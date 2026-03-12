#include "config.h"
#include "log.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

void lk_config_defaults(Config *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    strncpy(cfg->backend, "auto", sizeof(cfg->backend) - 1);
    cfg->sample_rate = 44100;
    cfg->channels = 2;
    cfg->buffer_size = 2048;
    cfg->num_target_devices = 1;
    strncpy(cfg->target_devices[0], "default", sizeof(cfg->target_devices[0]) - 1);

    strncpy(cfg->log_level, "info", sizeof(cfg->log_level) - 1);
    cfg->log_file[0] = '\0';

    /* Default PID file: /run/user/UID/linuxkeeper.pid */
    uid_t uid = getuid();
    snprintf(cfg->pid_file, sizeof(cfg->pid_file), "/run/user/%u/linuxkeeper.pid", (unsigned)uid);

    cfg->watchdog_max_interval = 30;
    cfg->reconnect_on_device_change = 1;
    cfg->keep_all_bt_devices = 0;
}

char *lk_config_find(void) {
    char path[LK_MAX_PATH];
    struct stat st;

    /* $XDG_CONFIG_HOME/linuxkeeper/config.toml */
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(path, sizeof(path), "%s/linuxkeeper/config.toml", xdg);
        if (stat(path, &st) == 0) return strdup(path);
    }

    /* ~/.config/linuxkeeper/config.toml */
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(path, sizeof(path), "%s/.config/linuxkeeper/config.toml", home);
        if (stat(path, &st) == 0) return strdup(path);
    }

    /* /etc/linuxkeeper/config.toml */
    snprintf(path, sizeof(path), "/etc/linuxkeeper/config.toml");
    if (stat(path, &st) == 0) return strdup(path);

    return NULL;
}

int lk_config_load(Config *cfg, const char *path) {
    TomlTable *t = toml_parse_file(path);
    if (!t) {
        LK_ERROR("Failed to parse config file: %s", path);
        return -1;
    }

    LK_INFO("Loading config from: %s", path);

    /* [audio] */
    const char *s;
    s = toml_get_string(t, "audio", "backend", NULL);
    if (s) snprintf(cfg->backend, sizeof(cfg->backend), "%s", s);

    long v;
    v = toml_get_int(t, "audio", "sample_rate", 0);
    if (v > 0) cfg->sample_rate = (unsigned int)v;

    v = toml_get_int(t, "audio", "channels", 0);
    if (v > 0) cfg->channels = (unsigned int)v;

    v = toml_get_int(t, "audio", "buffer_size", 0);
    if (v > 0) cfg->buffer_size = (unsigned int)v;

    const TomlEntry *e = toml_find(t, "audio", "target_devices");
    if (e && e->type == TOML_STRING_ARRAY) {
        cfg->num_target_devices = 0;
        for (int i = 0; i < e->arr_count && i < LK_MAX_DEVICES; i++) {
            snprintf(cfg->target_devices[i], sizeof(cfg->target_devices[i]),
                     "%s", e->arr_vals[i]);
            cfg->num_target_devices++;
        }
    }

    /* [daemon] */
    s = toml_get_string(t, "daemon", "log_level", NULL);
    if (s) snprintf(cfg->log_level, sizeof(cfg->log_level), "%s", s);

    s = toml_get_string(t, "daemon", "log_file", NULL);
    if (s) snprintf(cfg->log_file, sizeof(cfg->log_file), "%s", s);

    s = toml_get_string(t, "daemon", "pid_file", NULL);
    if (s) snprintf(cfg->pid_file, sizeof(cfg->pid_file), "%s", s);

    v = toml_get_int(t, "daemon", "watchdog_max_interval", 0);
    if (v > 0) cfg->watchdog_max_interval = (unsigned int)v;

    /* [stream] */
    const TomlEntry *re = toml_find(t, "stream", "reconnect_on_device_change");
    if (re && re->type == TOML_BOOLEAN)
        cfg->reconnect_on_device_change = re->bool_val;

    const TomlEntry *bt = toml_find(t, "stream", "keep_all_bt_devices");
    if (bt && bt->type == TOML_BOOLEAN)
        cfg->keep_all_bt_devices = bt->bool_val;

    toml_free(t);
    return 0;
}

void lk_config_set_device(Config *cfg, const char *device) {
    cfg->num_target_devices = 1;
    snprintf(cfg->target_devices[0], sizeof(cfg->target_devices[0]), "%s", device);
}

void lk_config_set_backend(Config *cfg, const char *backend) {
    snprintf(cfg->backend, sizeof(cfg->backend), "%s", backend);
}
