#ifndef LK_CONFIG_H
#define LK_CONFIG_H

#include <stdint.h>

#define LK_MAX_DEVICES 32
#define LK_MAX_PATH 4096

typedef struct Config {
    /* [audio] */
    char backend[32];
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int buffer_size;
    char target_devices[LK_MAX_DEVICES][256];
    int num_target_devices;

    /* [daemon] */
    char log_level[16];
    char log_file[LK_MAX_PATH];
    char pid_file[LK_MAX_PATH];
    unsigned int watchdog_max_interval;

    /* [stream] */
    int reconnect_on_device_change;
    int keep_all_bt_devices;
} Config;

/* Initialize config with defaults */
void lk_config_defaults(Config *cfg);

/* Load config from file. Returns 0 on success, -1 on error. */
int lk_config_load(Config *cfg, const char *path);

/* Find config file in standard locations. Returns allocated path or NULL. */
char *lk_config_find(void);

/* Apply CLI overrides */
void lk_config_set_device(Config *cfg, const char *device);
void lk_config_set_backend(Config *cfg, const char *backend);

#endif
